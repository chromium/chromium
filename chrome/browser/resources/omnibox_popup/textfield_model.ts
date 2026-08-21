// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface SelectionRange {
  start: number;
  end: number;
}

export enum MergeType {
  DO_NOT_MERGE,
  MERGEABLE,
  FORCE_MERGE,
}

export type EditType = 'insert'|'delete'|'replace';

// Edit holds state information to undo/redo editing changes. Editing operations
// are merged when possible, like when characters are typed in sequence. Calling
// `commit()` marks an edit as an independent operation that shouldn't be
// merged.
export interface Edit {
  readonly type: EditType;
  // The type of merging allowed.
  mergeType: MergeType;
  // Commits the edit and marks as un-mergeable.
  commit(): void;
  // Can `other` be merged into this edit?
  canMerge(other: Edit): boolean;
  // Tries to merge `other` into this edit and returns true on success.
  merge(other: Edit): boolean;
  // If defined, merges the replace edit into the current edit.
  mergeReplace?(other: Edit): void;
  // Reverts the change made by this edit.
  undo(currentText: string): {text: string, selection: SelectionRange};
  // Applies the change of this edit.
  redo(currentText: string): {text: string, selection: SelectionRange};
}

// Insert text at a given position. Assumes 1) no previous selection and 2) the
// insertion is at the caret, which will advance by the insertion length.
export class InsertEdit implements Edit {
  readonly type: EditType = 'insert';
  // The type of merging allowed.
  mergeType: MergeType;
  // Added text.
  newText: string;
  // The index of `new_text_`.
  newTextStart: number;
  // New cursor position.
  newCursorPos: number;

  constructor(mergeable: boolean, newText: string, at: number) {
    this.mergeType = mergeable ? MergeType.MERGEABLE : MergeType.DO_NOT_MERGE;
    this.newText = newText;
    this.newTextStart = at;
    this.newCursorPos = at + newText.length;
  }

  commit(): void {
    this.mergeType = MergeType.DO_NOT_MERGE;
  }

  // Merge if `other` is an insertion continuing forward where `this` ended.
  // E.g. If `this` changed "ab|c" to "abX|c", an edit to "abXY|c" can be
  // merged.
  canMerge(other: Edit): boolean {
    // Reject other edit types.
    if (other.type !== 'insert') {
      return false;
    }
    const otherInsert = other as InsertEdit;
    if (this.mergeType !== MergeType.MERGEABLE ||
        otherInsert.mergeType !== MergeType.MERGEABLE) {
      return false;
    }
    // Reject inserts starting somewhere other than where this insert ended.
    return (this.newTextStart + this.newText.length) ===
        otherInsert.newTextStart;
  }

  // Merge the replace edit into the current edit. This handles the special case
  // where an omnibox autocomplete string is set after a new character is typed.
  mergeReplace(edit: Edit): void {
    const otherReplace = edit as ReplaceEdit;
    this.mergeType = MergeType.DO_NOT_MERGE;
    this.newCursorPos = otherReplace.newCursorPos;
    this.newText = otherReplace.newText;
    this.newTextStart = 0;
  }

  merge(other: Edit): boolean {
    if (other.mergeType === MergeType.FORCE_MERGE) {
      this.mergeReplace(other);
      return true;
    }
    if (!this.canMerge(other)) {
      return false;
    }
    const otherInsert = other as InsertEdit;
    this.newText += otherInsert.newText;
    this.newCursorPos = otherInsert.newCursorPos;
    return true;
  }

  undo(currentText: string): {text: string, selection: SelectionRange} {
    const textBefore = currentText.substring(0, this.newTextStart);
    const textAfter =
        currentText.substring(this.newTextStart + this.newText.length);
    const text = textBefore + textAfter;
    const selection = {start: this.newTextStart, end: this.newTextStart};
    return {text, selection};
  }

  redo(currentText: string): {text: string, selection: SelectionRange} {
    const textBefore = currentText.substring(0, this.newTextStart);
    const textAfter = currentText.substring(this.newTextStart);
    const text = textBefore + this.newText + textAfter;
    const selection = {start: this.newCursorPos, end: this.newCursorPos};
    return {text, selection};
  }
}

// Delete one or more characters and do a single insertion. The insertion need
// not be adjacent to the deletions (e.g. drag & drop).
export class ReplaceEdit implements Edit {
  readonly type: EditType = 'replace';
  // The type of merging allowed.
  mergeType: MergeType;
  // Deleted texts ordered with decreasing indices.
  oldTexts: string[];
  // The indices of `oldTexts`.
  oldTextStarts: number[];
  // `oldPrimarySelection` represents the selection associated with the cursor
  // prior to the edit.
  oldPrimarySelection: SelectionRange;
  // True if the deletion is made backward.
  deleteBackward: boolean;
  // New cursor position.
  newCursorPos: number;
  // Added text.
  newText: string;
  // The index of `newText`.
  newTextStart: number;

  constructor(
      mergeType: MergeType, oldTexts: string[], oldTextStarts: number[],
      oldPrimarySelection: SelectionRange, deleteBackward: boolean,
      newCursorPos: number, newText: string, newTextStart: number) {
    this.mergeType = mergeType;
    this.oldTexts = oldTexts;
    this.oldTextStarts = oldTextStarts;
    this.oldPrimarySelection = oldPrimarySelection;
    this.deleteBackward = deleteBackward;
    this.newCursorPos = newCursorPos;
    this.newText = newText;
    this.newTextStart = newTextStart;
  }

  commit(): void {
    this.mergeType = MergeType.DO_NOT_MERGE;
  }

  // Merge if `other` is an insertion or replacement continuing forward where
  // `this` ended. E.g. If `this` changed "a|bc" to "aX|c", edits to "aXY|" or
  // "aXY|c" can be merged.
  canMerge(other: Edit): boolean {
    // Reject deletions.
    if (other.type === 'delete') {
      return false;
    }
    if (this.mergeType !== MergeType.MERGEABLE ||
        other.mergeType !== MergeType.MERGEABLE) {
      return false;
    }

    const newTextEnd = this.newTextStart + this.newText.length;
    if (other.type === 'insert') {
      // Reject inserts starting somewhere other than where this edit ended.
      const otherInsert = other as InsertEdit;
      return newTextEnd === otherInsert.newTextStart;
    } else {
      // Reject replacements deleting multiple ranges or deleting text somewhere
      // other than where this edit ended.
      const otherReplace = other as ReplaceEdit;
      return otherReplace.oldTexts.length <= 1 &&
          newTextEnd === otherReplace.newTextStart &&
          (otherReplace.oldTextStarts.length === 0 ||
           newTextEnd === otherReplace.oldTextStarts[0]!);
    }
  }

  // Merge the replace edit into the current edit. This handles the special case
  // where an omnibox autocomplete string is set after a new character is typed.
  mergeReplace(edit: Edit): void {
    const otherReplace = edit as ReplaceEdit;
    // We need to compute the merged edit's old text by undoing this edit.
    // Otherwise, old text would be the autocompleted text following the
    // user input. E.g., given goo|[gle.com], when the user types 'g', the text
    // updates to goog|[le.com]. If we leave old text unchanged as 'gle.com',
    // then undoing will result in 'gle.com' instead of 'goo|[gle.com]'
    let oldText = otherReplace.oldTexts[0] || '';
    // Remove `newText`.
    oldText = oldText.substring(0, this.newTextStart) +
        oldText.substring(this.newTextStart + this.newText.length);
    // Add `oldTexts` in reverse order since we're undoing an edit.
    for (let i = this.oldTexts.length - 1; i >= 0; i--) {
      const start = this.oldTextStarts[i]!;
      const t = this.oldTexts[i]!;
      oldText = oldText.substring(0, start) + t + oldText.substring(start);
    }

    this.mergeType = MergeType.DO_NOT_MERGE;
    this.oldTexts = [oldText];
    this.oldTextStarts = [0];
    this.deleteBackward = false;
    this.newCursorPos = otherReplace.newCursorPos;
    this.newText = otherReplace.newText;
    this.newTextStart = 0;
  }

  merge(other: Edit): boolean {
    if (other.mergeType === MergeType.FORCE_MERGE) {
      this.mergeReplace(other);
      return true;
    }
    if (!this.canMerge(other)) {
      return false;
    }
    const otherReplace = other as ReplaceEdit;
    if (otherReplace.oldTexts?.length === 1) {
      this.oldTexts[0] = (this.oldTexts[0] || '') + otherReplace.oldTexts[0]!;
    }
    const otherEdit = other as InsertEdit | ReplaceEdit;
    this.newText += otherEdit.newText;
    this.newCursorPos = otherEdit.newCursorPos;
    return true;
  }

  undo(currentText: string): {text: string, selection: SelectionRange} {
    const textBefore = currentText.substring(0, this.newTextStart);
    const textAfter =
        currentText.substring(this.newTextStart + this.newText.length);
    let text = textBefore + textAfter;

    for (let i = this.oldTexts.length - 1; i >= 0; i--) {
      const start = this.oldTextStarts[i]!;
      const oldText = this.oldTexts[i]!;
      text = text.substring(0, start) + oldText + text.substring(start);
    }

    return {text, selection: {...this.oldPrimarySelection}};
  }

  redo(currentText: string): {text: string, selection: SelectionRange} {
    let text = currentText;
    for (let i = 0; i < this.oldTexts.length; i++) {
      const start = this.oldTextStarts[i]!;
      const length = this.oldTexts[i]!.length;
      text = text.substring(0, start) + text.substring(start + length);
    }

    text = text.substring(0, this.newTextStart) + this.newText +
        text.substring(this.newTextStart);

    const selection = {start: this.newCursorPos, end: this.newCursorPos};
    return {text, selection};
  }
}

// Delete text, either at/before the caret or over a selected range.
export class DeleteEdit implements Edit {
  readonly type: EditType = 'delete';
  // The type of merging allowed.
  mergeType: MergeType;
  // Deleted texts ordered with decreasing indices.
  oldTexts: string[];
  // The indices of `oldTexts`.
  oldTextStarts: number[];
  // `oldPrimarySelection` represents the selection associated with the cursor
  // prior to the edit.
  oldPrimarySelection: SelectionRange;
  // True if the deletion is made backward.
  deleteBackward: boolean;
  // New cursor position.
  newCursorPos: number;

  constructor(
      mergeable: boolean, oldTexts: string[], oldTextStarts: number[],
      oldPrimarySelection: SelectionRange, deleteBackward: boolean,
      newCursorPos: number) {
    this.mergeType = mergeable ? MergeType.MERGEABLE : MergeType.DO_NOT_MERGE;
    this.oldTexts = oldTexts;
    this.oldTextStarts = oldTextStarts;
    this.oldPrimarySelection = oldPrimarySelection;
    this.deleteBackward = deleteBackward;
    this.newCursorPos = newCursorPos;
  }

  commit(): void {
    this.mergeType = MergeType.DO_NOT_MERGE;
  }

  // Merge if `other` is a deletion continuing in the same direction and
  // position where `this` ended. E.g. If `this` changed "ab|c" to "a|c" an edit
  // to "|c" can be merged.
  canMerge(other: Edit): boolean {
    if (other.type !== 'delete') {
      return false;
    }
    const otherDelete = other as DeleteEdit;
    if (this.mergeType !== MergeType.MERGEABLE ||
        otherDelete.mergeType !== MergeType.MERGEABLE) {
      return false;
    }

    if (this.deleteBackward) {
      // Backspace can be merged only with backspace at the same position.
      return otherDelete.deleteBackward &&
          this.oldTextStarts[0]! ===
          (otherDelete.oldTextStarts[0]! + otherDelete.oldTexts[0]!.length);
    } else {
      // Delete can be merged only with delete at the same position.
      return !otherDelete.deleteBackward &&
          this.oldTextStarts[0]! === otherDelete.oldTextStarts[0]!;
    }
  }

  merge(other: Edit): boolean {
    if (!this.canMerge(other)) {
      return false;
    }
    const otherDelete = other as DeleteEdit;
    if (this.deleteBackward) {
      this.oldTextStarts[0] = otherDelete.oldTextStarts[0]!;
      this.oldTexts[0] = otherDelete.oldTexts[0]! + this.oldTexts[0]!;
      this.newCursorPos = otherDelete.newCursorPos;
    } else {
      this.oldTexts[0] = this.oldTexts[0]! + otherDelete.oldTexts[0]!;
    }
    return true;
  }

  undo(currentText: string): {text: string, selection: SelectionRange} {
    const textBefore = currentText.substring(0, this.oldTextStarts[0]);
    const textAfter = currentText.substring(this.oldTextStarts[0]!);
    const text = textBefore + this.oldTexts[0]! + textAfter;
    return {text, selection: {...this.oldPrimarySelection}};
  }

  redo(currentText: string): {text: string, selection: SelectionRange} {
    const textBefore = currentText.substring(0, this.oldTextStarts[0]);
    const textAfter = currentText.substring(
        this.oldTextStarts[0]! + this.oldTexts[0]!.length);
    const text = textBefore + textAfter;
    const selection = {start: this.newCursorPos, end: this.newCursorPos};
    return {text, selection};
  }
}

// TextfieldModel manages the edit history stack (undo/redo operations,
// text state, and selection ranges) for the Omnibox popup searchbox.
// Sequential edits are merged when possible (e.g. typing characters), and
// undoing/redoing restores both the text and caret selection range.
// In essence, this class is intended to mirror the Views `TextfieldModel`
// class in terms of edit state tracking.
export class TextfieldModel {
  // The list of Edits. The oldest Edits are at the front of the list, and the
  // newest ones are at the back of the list.
  private history_: Edit[] = [];
  // An index that points to the current edit that can be undone.
  //
  // There is no edit to undo when:
  //   1) in initial state. (nothing to undo)
  //   2) very 1st edit is undone.
  //   3) all edit history is removed.
  // There is no edit to redo when:
  //   1) in initial state. (nothing to redo)
  //   2) new edit is added. (redo history is cleared)
  //   3) redone all undone edits.
  private currentEditIndex_: number = -1;
  // The current text whose edits are being recorded by this class.
  private text_: string = '';
  // The current selection range in `text_`.
  private selection_: SelectionRange = {start: 0, end: 0};

  // Returns the current text.
  get text(): string {
    return this.text_;
  }

  // Returns the current cursor position.
  getCursorPosition(): number {
    return this.selection_.end;
  }

  // Returns the current selection range in `text_`.
  get selection(): SelectionRange {
    return {...this.selection_};
  }

  // Sets the text.
  // Setting the same text, even with an updated `cursorPosition`, will neither
  // add edit history nor change the cursor because it's neither a user visible
  // change nor user-initiated change. This allows clients to set the same text
  // multiple times without messing up edit history. The resulting history edit
  // will have `newCursorPos` set to `cursorPosition`. This is important even
  // if subsequent calls will override the cursor position because updating the
  // cursor alone won't update the edit history. I.e. the cursor position after
  // applying or redoing the edit will be determined by `cursorPosition`.
  setText(text: string, cursorPosition: number = text.length): void {
    // TODO(b/522957982): Align with `TextfieldModel::SetText()` in terms of IME
    // parity.
    if (this.text_ !== text) {
      const oldText = this.text_;
      // Force merge the edit.
      this.addOrMergeEdit(new ReplaceEdit(
          MergeType.FORCE_MERGE, [oldText], [0], {...this.selection_},
          /*deleteBackward=*/ false, cursorPosition, text, 0));
      this.text_ = text;
    }
    this.selection_ = {start: cursorPosition, end: cursorPosition};
  }

  // Select a specific range of text.
  // The selection starts with the range's start position and ends with the
  // range's end position; therefore the cursor position becomes the end
  // position.
  selectRange(range: SelectionRange): void {
    // TODO(b/522957982): Align with `TextfieldModel::SelectRange()` in terms of
    // IME parity.
    this.selection_ = {start: range.start, end: range.end};
  }

  // Clears all edit history.
  clearEditHistory(): void {
    this.history_ = [];
    this.currentEditIndex_ = -1;
  }

  // Initializes baseline text and selection state and clears edit history, so
  // subsequent user edits start from `text`.
  setInitialText(text: string, selection: SelectionRange = {
    start: text.length,
    end: text.length,
  }): void {
    this.clearEditHistory();
    this.text_ = text;
    this.selection_ = {...selection};
  }

  // Insert the given `insertedText` at the cursor. `mergeable` indicates if
  // this operation can be merged with previous edits in the history. Will
  // delete any selected text.
  insertText(insertedText: string, mergeable: boolean = false): void {
    // TODO(b/522957982): Align with `TextfieldModel::InsertTextInternal()` in
    // terms of IME parity.
    const selStart = Math.min(this.selection_.start, this.selection_.end);
    const selEnd = Math.max(this.selection_.start, this.selection_.end);
    const oldText = this.text_;
    const oldSelection = {...this.selection_};
    const mergeType = mergeable ? MergeType.MERGEABLE : MergeType.DO_NOT_MERGE;

    if (selStart !== selEnd) {
      // User selected a non-empty span of text and replaced it with some new
      // text.
      const replacedText = oldText.substring(selStart, selEnd);
      this.addOrMergeEdit(new ReplaceEdit(
          mergeType, [replacedText], [selStart], oldSelection,
          /*deleteBackward=*/ false, selStart + insertedText.length,
          insertedText, selStart));
    } else {
      // User inserted some text at the current caret position.
      this.addOrMergeEdit(new InsertEdit(mergeable, insertedText, selStart));
    }

    this.text_ = oldText.substring(0, selStart) + insertedText +
        oldText.substring(selEnd);
    const newPos = selStart + insertedText.length;
    this.selection_ = {start: newPos, end: newPos};
  }

  // Inserts a character at the current cursor position.
  insertChar(c: string): void {
    this.insertText(c, /*mergeable=*/ true);
  }

  // Deletes the first character after the current cursor position (as if, the
  // the user has pressed delete key in the textfield). Returns true if
  // the deletion is successful.
  delete(): boolean {
    return this.deleteInternal_(/*deleteBackward=*/ false);
  }

  // Deletes the first character before the current cursor position (as if, the
  // the user has pressed backspace key in the textfield). Returns true if
  // the removal is successful.
  backspace(): boolean {
    return this.deleteInternal_(/*deleteBackward=*/ true);
  }

  private deleteInternal_(deleteBackward: boolean): boolean {
    const selStart = Math.min(this.selection_.start, this.selection_.end);
    const selEnd = Math.max(this.selection_.start, this.selection_.end);
    const oldText = this.text_;
    const oldSelection = {...this.selection_};

    // TODO(b/522957982): Align with `TextfieldModel::Delete()` /
    // `TextfieldModel::Backspace()` in terms of IME parity.

    if (selStart !== selEnd) {
      const deletedText = oldText.substring(selStart, selEnd);
      if (!deletedText) {
        return false;
      }
      // User selected a non-empty span of text and deleted it.
      this.addOrMergeEdit(new DeleteEdit(
          /*mergeable=*/ false, [deletedText], [selStart], oldSelection,
          /*deleteBackward=*/ true, selStart));
      this.text_ = oldText.substring(0, selStart) + oldText.substring(selEnd);
      this.selection_ = {start: selStart, end: selStart};
      return true;
    }

    if (deleteBackward) {
      // Backspace: User deleted the character before the caret.
      if (selStart <= 0) {
        return false;
      }
      const deletedText = oldText.substring(selStart - 1, selStart);
      this.addOrMergeEdit(new DeleteEdit(
          /*mergeable=*/ true, [deletedText], [selStart - 1], oldSelection,
          /*deleteBackward=*/ true, selStart - 1));
      this.text_ =
          oldText.substring(0, selStart - 1) + oldText.substring(selStart);
      this.selection_ = {start: selStart - 1, end: selStart - 1};
      return true;
    } else {
      // Delete: User deleted the character at the caret.
      if (selStart >= oldText.length) {
        return false;
      }
      const deletedText = oldText.substring(selStart, selStart + 1);
      this.addOrMergeEdit(new DeleteEdit(
          /*mergeable=*/ true, [deletedText], [selStart], oldSelection,
          /*deleteBackward=*/ false, selStart));
      this.text_ =
          oldText.substring(0, selStart) + oldText.substring(selStart + 1);
      this.selection_ = {start: selStart, end: selStart};
      return true;
    }
  }

  // Cuts the currently selected text (without clipboard interaction).
  // Returns true if text has changed after cutting.
  cut(): boolean {
    if (this.selection_.start === this.selection_.end) {
      return false;
    }
    return this.delete();
  }

  // Pastes the given text at the current cursor position.
  // Returns true if any text is pasted.
  paste(pastedText: string): boolean {
    if (!pastedText) {
      return false;
    }

    // Leading/trailing whitespace is often selected accidentally, and is rarely
    // critical to include (e.g. when pasting into a find bar). Trim it. By
    // contrast, whitespace in the middle of the string may need exact
    // preservation to avoid changing the effect (e.g. converting a full-width
    // space to a regular space), so we leave that alone.
    let text = pastedText.trim();
    // If the provided text contains all whitespace, then paste a single space.
    if (!text) {
      text = ' ';
    }

    this.insertText(text, /*mergeable=*/ false);
    return true;
  }

  // Adds or merges `edit` into the edit history.
  addOrMergeEdit(edit: Edit): void {
    // Truncate redo history if new edit is performed after undo.
    if (this.currentEditIndex_ < this.history_.length - 1) {
      this.history_.splice(this.currentEditIndex_ + 1);
    }

    if (this.currentEditIndex_ >= 0) {
      const currentEdit = this.history_[this.currentEditIndex_];
      // If the new edit was successfully merged with an old one, don't add it
      // to the history.
      if (currentEdit && currentEdit.merge(edit)) {
        return;
      }
    }

    this.history_.push(edit);
    this.currentEditIndex_++;
  }

  commitCurrentEdit(): void {
    if (this.currentEditIndex_ >= 0) {
      const currentEdit = this.history_[this.currentEditIndex_];
      if (currentEdit) {
        currentEdit.commit();
      }
    }
  }

  // Returns true if there is an undoable edit.
  canUndo(): boolean {
    return this.currentEditIndex_ >= 0;
  }

  // Returns true if there is an redoable edit.
  canRedo(): boolean {
    return this.history_.length > 0 &&
        this.currentEditIndex_ < this.history_.length - 1;
  }

  // Undo edit.
  // Returns latest text and selection range if undo changed the text.
  undo(): {text: string, selection: SelectionRange}|null {
    if (!this.canUndo()) {
      return null;
    }
    // TODO(b/522957982): Align with `TextfieldModel::Undo()` in terms of IME
    // parity.

    const edit = this.history_[this.currentEditIndex_];
    if (!edit) {
      return null;
    }
    edit.commit();
    const result = edit.undo(this.text_);
    this.currentEditIndex_--;
    if (result) {
      this.text_ = result.text;
      this.selection_ = result.selection;
    }
    return result;
  }

  // Redo edit.
  // Returns latest text and selection range if redo changed the text.
  redo(): {text: string, selection: SelectionRange}|null {
    if (!this.canRedo()) {
      return null;
    }
    // TODO(b/522957982): Align with `TextfieldModel::Redo()` in terms of IME
    // parity.

    this.currentEditIndex_++;
    const edit = this.history_[this.currentEditIndex_];
    if (!edit) {
      return null;
    }
    const result = edit.redo(this.text_);
    if (result) {
      this.text_ = result.text;
      this.selection_ = result.selection;
    }
    return result;
  }

  // Resets all edit history model state.
  clear(): void {
    this.clearEditHistory();
    this.text_ = '';
    this.selection_ = {start: 0, end: 0};
  }

  get length(): number {
    return this.history_.length;
  }

  get currentEditIndex(): number {
    return this.currentEditIndex_;
  }
}
