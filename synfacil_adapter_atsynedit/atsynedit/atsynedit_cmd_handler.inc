{$ifdef none}begin end;{$endif}

procedure TATSynEdit.KeyDown(var Key: Word; Shift: TShiftState);
begin
  if not FWantTabs then
    if Key=vk_tab then Exit;

  DoPaintModeStatic;
  try
    DoCommand(GetCommandFromKey(Key, Shift));
  finally
    DoPaintModeBlinking;
  end;
end;

procedure TATSynEdit.UTF8KeyPress(var UTF8Key: TUTF8Char);
var
  Str: atString;
begin
  inherited;

  //skip control Ascii chars
  if Ord(UTF8Key[1])<32 then Exit;

  Str:= Utf8Decode(Utf8Key);
  DoCommand(cCommand_TextInsert, Str);
  Utf8Key:= '';
end;

function TATSynEdit.GetCommandFromKey(var Key: Word; Shift: TShiftState): integer;
var
  Shortcut: TShortcut;
begin
  Result:= 0;

  if (Key=VK_TAB) and (Shift=[]) then
  begin
    Result:= cCommand_KeyTab;
    Key:= 0;
    Exit;
  end;

  if IsEditableTextKey(Key) and ((Shift=[]) or (Shift=[ssShift])) then
    Exit;

  if not Assigned(FKeymap) then
  begin
    ShowMessage('Keymap not assigned: cannot input keys');
    Exit
  end;

  Shortcut:= KeyToShortCut(Key, Shift);
  Result:= FKeymap.GetCommandFromShortcut(Shortcut);
  if Result>0 then
    Key:= 0;
end;

procedure TATSynEdit.DoCommandResults(Res: TATCommandResults);
begin
  if cResultText in Res then
  begin
    FWrapUpdateNeeded:= true;
    UpdateWrapInfo;

    if FOptMouseHideCursor then
      if PtInRect(FRectMain, ScreenToClient(Mouse.CursorPos)) then
        SetCursor(crNone);
  end;

  DoCaretsSort;
  UpdateCaretsCoords;

  if cResultCaretLeft in Res then DoGotoCaret(cEdgeLeft);
  if cResultCaretRight in Res then DoGotoCaret(cEdgeRight);
  if cResultCaretTop in Res then DoGotoCaret(cEdgeTop);
  if cResultCaretBottom in Res then DoGotoCaret(cEdgeBottom);

  if Res*[cResultCaretAny,
    cResultCaretLeft, cResultCaretRight,
    cResultCaretTop, cResultCaretBottom]<>[] then
  begin
    if not (cResultKeepColumnSel in Res) then
      FSelRect:= cRectEmpty;
    DoEventCarets;
  end;

  if cResultText in Res then
  begin
    FSelRect:= cRectEmpty;
    DoEventChange;
  end;

  if cResultScroll in Res then
    DoEventScroll;

  if cResultState in Res then
    DoEventState;

  Update;
end;

function TATSynEdit.DoCommand_FoldUnfoldAll(ADoFold: boolean): TATCommandResults;
var
  Ar: TATIntArray;
  R: TATSynRange;
  i: integer;
begin
  if ADoFold then
  begin
    Ar:= Fold.FindRangesContainingLines(
      0, Strings.Count-1, nil,
      false{OnlyFolded}, true{TopLevelOnly}, cRngHasAnyOfLines);
    for i:= Low(Ar) to High(Ar) do
    begin
      R:= Fold.Items[Ar[i]];
      if not R.Folded then
        DoRangeFold(R);
    end;
  end
  else
  begin
    for i:= 0 to Fold.Count-1 do
    begin
      R:= Fold.Items[i];
      if R.Folded then
        DoRangeUnfold(R);
    end;
  end;

  Result:= [cResultCaretAny, cResultScroll];
end;

procedure TATSynEdit.DoCommand(ACmd: integer; const AText: atString = '');
var
  Caret: TATCaretItem;
  Res: TATCommandResults;
  i: integer;
begin
  if ACmd<=0 then Exit;
  if DoEventCommand(ACmd) then Exit;

  Res:= [];
  FCaretSpecPos:= false;

  //---handle special command bits (cCmdNNNNN)
  if ACmd and cCmdSelKeep <> 0 then
    for i:= 0 to Carets.Count-1 do
    begin
      Caret:= Carets[i];
      if Caret.EndY<0 then Caret.EndY:= Caret.PosY;
      if Caret.EndX<0 then Caret.EndX:= Caret.PosX;
    end;

  if ACmd and cCmdSelReset <> 0 then
    DoSelect_None;

  if ACmd and cCmdCaret <> 0 then
  begin
    Strings.SetGroupMark;

    //save column for Up/Down movement
    Carets.UpdateColumnCoord(
      (ACmd=cCommand_KeyUp) or
      (ACmd=cCommand_KeyDown) or
      (ACmd=cCommand_KeyUp_Sel) or
      (ACmd=cCommand_KeyDown_Sel)
      );
  end;

  //debug
  //if FCaretMoved then Beep;
  //--------

  case ACmd of
    //most used commands
    cCommand_KeyLeft:             Res:= DoCommand_KeyLeft(false);
    cCommand_KeyLeft_Sel:         Res:= DoCommand_KeyLeft(true);
    cCommand_KeyRight:            Res:= DoCommand_KeyRight(false);
    cCommand_KeyRight_Sel:        Res:= DoCommand_KeyRight(true);
    cCommand_KeyUp,
    cCommand_KeyUp_Sel:           Res:= DoCommand_KeyUpDown(false, 1, false);
    cCommand_KeyDown,
    cCommand_KeyDown_Sel:         Res:= DoCommand_KeyUpDown(true, 1, false);
    cCommand_KeyHome,
    cCommand_KeyHome_Sel:         Res:= DoCommand_KeyHome;
    cCommand_KeyEnd,
    cCommand_KeyEnd_Sel:          Res:= DoCommand_KeyEnd;
    cCommand_KeyPageUp,
    cCommand_KeyPageUp_Sel:       Res:= DoCommand_KeyUpDown(false, GetPageLines, FOptKeyPageKeepsRelativePos);
    cCommand_KeyPageDown,
    cCommand_KeyPageDown_Sel:     Res:= DoCommand_KeyUpDown(true, GetPageLines, FOptKeyPageKeepsRelativePos);

    cCommand_ColSelectLeft:      Res:= DoCommand_SelectColumn(cDirLeft);
    cCommand_ColSelectRight:     Res:= DoCommand_SelectColumn(cDirRight);
    cCommand_ColSelectUp:        Res:= DoCommand_SelectColumn(cDirUp);
    cCommand_ColSelectDown:      Res:= DoCommand_SelectColumn(cDirDown);

    cCommand_TextInsert:          Res:= DoCommand_TextInsertAtCarets(AText, false, FOverwrite, false);
    cCommand_TextInsertTabChar:   Res:= DoCommand_TextInsertAtCarets(#9, false, FOverwrite, false);
    cCommand_KeyBackspace:        Res:= DoCommand_TextBackspace;
    cCommand_KeyDelete:           Res:= DoCommand_TextDelete;
    cCommand_KeyTab:              Res:= DoCommand_TextTabulation;
    cCommand_KeyEnter:            Res:= DoCommand_TextInsertEol(false);

    cCommand_Undo:                Res:= DoCommand_Undo;
    cCommand_Redo:                Res:= DoCommand_Redo;
    //end of most used

    cCommand_TextDeleteSelection:      Res:= DoCommand_TextDeleteSelection;
    cCommand_TextDeleteLine:           Res:= DoCommand_TextDeleteLines;
    cCommand_TextDuplicateLine:        Res:= DoCommand_TextDuplicateLine;
    cCommand_TextDeleteToLineBegin:    Res:= DoCommand_TextDeleteToLineBegin;
    cCommand_TextDeleteToLineEnd:      Res:= DoCommand_TextDeleteToLineEnd;
    cCommand_TextDeleteToTextEnd:      Res:= DoCommand_TextDeleteToFileEnd;
    cCommand_TextDeleteWordPrev:       Res:= DoCommand_TextDeleteWord(false);
    cCommand_TextDeleteWordNext:       Res:= DoCommand_TextDeleteWord(true);

    cCommand_SelectAll:                Res:= DoCommand_SelectAll;
    cCommand_SelectNone:               Res:= [cResultCaretAny];
    cCommand_SelectInverted:           Res:= DoCommand_SelectInverted;
    cCommand_SelectSplitToLines:       Res:= DoCommand_SelectSplitToLines;
    cCommand_SelectExtendByLine:       Res:= DoCommand_SelectExtendByLine;
    cCommand_SelectWords:              Res:= DoCommand_SelectWords;
    cCommand_SelectLines:              Res:= DoCommand_SelectLines;

    cCommand_GotoTextBegin,
    cCommand_GotoTextBegin_Sel:        Res:= DoCommand_GotoTextBegin;
    cCommand_GotoTextEnd,
    cCommand_GotoTextEnd_Sel:          Res:= DoCommand_GotoTextEnd;
    cCommand_GotoWordNext,
    cCommand_GotoWordNext_Sel:         Res:= DoCommand_GotoWord(true);
    cCommand_GotoWordPrev,
    cCommand_GotoWordPrev_Sel:         Res:= DoCommand_GotoWord(false);

    cCommand_ToggleOverwrite:          Res:= DoCommand_ToggleOverwrite;
    cCommand_ToggleReadOnly:           Res:= DoCommand_ToggleReadOnly;
    cCommand_ToggleWordWrap:           Res:= DoCommand_ToggleWordWrap;
    cCommand_ToggleUnprinted:          Res:= DoCommand_ToggleUnprinted;
    cCommand_ToggleUnprintedSpaces:    Res:= DoCommand_ToggleUnprintedSpaces;
    cCommand_ToggleUnprintedEnds:      Res:= DoCommand_ToggleUnprintedEnds;
    cCommand_ToggleUnprintedEndDetails: Res:= DoCommand_ToggleUnprintedEndDetails;
    cCommand_ToggleLineNums:           Res:= DoCommand_ToggleLineNums;
    cCommand_ToggleFolding:            Res:= DoCommand_ToggleFolding;
    cCommand_ToggleRuler:              Res:= DoCommand_ToggleRuler;
    cCommand_ToggleMinimap:            Res:= DoCommand_ToggleMinimap;

    cCommand_TextIndent:               Res:= DoCommand_TextIndentUnindent(true);
    cCommand_TextUnindent:             Res:= DoCommand_TextIndentUnindent(false);

    cCommand_ScrollLineUp:             Res:= DoCommand_ScrollVert(-1);
    cCommand_ScrollLineDown:           Res:= DoCommand_ScrollVert(1);
    cCommand_ScrollToCaretTop:         Res:= [cResultCaretTop];
    cCommand_ScrollToCaretBottom:      Res:= [cResultCaretBottom];
    cCommand_ScrollToCaretLeft:        Res:= [cResultCaretLeft];
    cCommand_ScrollToCaretRight:       Res:= [cResultCaretRight];

    cCommand_ClipboardCopy:            Res:= DoCommand_ClipboardCopy;
    cCommand_ClipboardCopyAdd:         Res:= DoCommand_ClipboardCopy(true);
    cCommand_ClipboardCut:             Res:= DoCommand_ClipboardCut;
    cCommand_ClipboardPaste:           Res:= DoCommand_ClipboardPaste(false, false);
    cCommand_ClipboardPaste_Select:    Res:= DoCommand_ClipboardPaste(false, true);
    cCommand_ClipboardPaste_KeepCaret: Res:= DoCommand_ClipboardPaste(true, false);
    cCommand_ClipboardPaste_Column:          Res:= DoCommand_ClipboardPasteColumnBlock(false);
    cCommand_ClipboardPaste_ColumnKeepCaret: Res:= DoCommand_ClipboardPasteColumnBlock(true);

    cCommand_MoveSelectionUp:          Res:= DoCommand_MoveSelectionUpDown(false);
    cCommand_MoveSelectiondown:        Res:= DoCommand_MoveSelectionUpDown(true);
    cCommand_TextInsertEmptyAbove:     Res:= DoCommand_TextInsertEmptyAboveBelow(false);
    cCommand_TextInsertEmptyBelow:     Res:= DoCommand_TextInsertEmptyAboveBelow(true);

    cCommand_TextCaseLower:            Res:= DoCommand_TextChangeCase(cCaseLower);
    cCommand_TextCaseUpper:            Res:= DoCommand_TextChangeCase(cCaseUpper);
    cCommand_TextCaseTitle:            Res:= DoCommand_TextChangeCase(cCaseTitle);
    cCommand_TextCaseInvert:           Res:= DoCommand_TextChangeCase(cCaseInvert);
    cCommand_TextCaseSentence:         Res:= DoCommand_TextChangeCase(cCaseSentence);

    cCommand_FoldAll:                  Res:= DoCommand_FoldUnfoldAll(true);
    cCommand_UnfoldAll:                Res:= DoCommand_FoldUnfoldAll(false);

    cCommand_Cancel:                   Res:= DoCommand_Cancel;
    cCommand_CaretsExtendUpLine:       Res:= DoCommand_CaretsExtend(false, 1);
    cCommand_CaretsExtendUpPage:       Res:= DoCommand_CaretsExtend(false, GetPageLines);
    cCommand_CaretsExtendUpToTop:      Res:= DoCommand_CaretsExtend(false, Strings.Count);
    cCommand_CaretsExtendDownLine:     Res:= DoCommand_CaretsExtend(true, 1);
    cCommand_CaretsExtendDownPage:     Res:= DoCommand_CaretsExtend(true, GetPageLines);
    cCommand_CaretsExtendDownToEnd:    Res:= DoCommand_CaretsExtend(true, Strings.Count);

    cCommand_ZoomIn:                   Res:= DoCommand_SizeChange(true);
    cCommand_ZoomOut:                  Res:= DoCommand_SizeChange(false);
    cCommand_RepeatTextCommand:        DoCommand(FLastTextCmd, FLastTextCmdText);
  end;

  if cResultText in Res then
    if ACmd<>cCommand_RepeatTextCommand then
    begin
      FLastTextCmd:= ACmd;
      FLastTextCmdText:= AText;
    end;

  DoCommandResults(Res);
end;



