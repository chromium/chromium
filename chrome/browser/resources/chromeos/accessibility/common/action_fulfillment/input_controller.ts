// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

type AutomationNode = chrome.automation.AutomationNode;

export interface EditableNodeData {
  node: AutomationNode;
  value: string;
  selStart: number;
  selEnd: number;
}

/**
 * InputController handles interaction with input fields for Accessibility
 * features.
 */
export abstract class InputController {
  constructor() {}

  /** Whether this is an active InputController with a focused input */
  abstract isActive(): boolean;

  /**
   * Commits the given text to the active IME context.
   */
  abstract commitText(text: string): void;


  /**
   * Deletes the sentence to the left of the text caret. If the caret is in the
   * middle of a sentence, it will delete a portion of the sentence it
   * intersects.
   */
  abstract deletePrevSentence(): void;

  /**
   * Deletes a phrase to the left of the text caret. If multiple instances of
   * `phrase` are present, it deletes the one closest to the text caret.
   * @param phrase The phrase to be deleted.
   */
  abstract deletePhrase(phrase: string): void;

  /**
   * Replaces a phrase to the left of the text caret with another phrase. If
   * multiple instances of `deletePhrase` are present, this function will
   * replace the one closest to the text caret.
   * @param deletePhrase The phrase to be deleted.
   * @param insertPhrase The phrase to be inserted.
   */
  abstract replacePhrase(deletePhrase: string, insertPhrase: string):
      Promise<void>;

  /**
   * Inserts `insertPhrase` directly before `beforePhrase` (and separates them
   * with a space). This function operates on the text to the left of the caret.
   * If multiple instances of `beforePhrase` are present, this function will
   * use the one closest to the text caret.
   */
  abstract insertBefore(insertPhrase: string, beforePhrase: string):
      Promise<void>;

  /**
   * Sets selection starting at `startPhrase` and ending at `endPhrase`
   * (inclusive). The function operates on the text to the left of the text
   * caret. If multiple instances of `startPhrase` or `endPhrase` are present,
   * the function will use the ones closest to the text caret.
   */
  abstract selectBetween(startPhrase: string, endPhrase: string): void;

  /** Moves the text caret to the next sentence. */
  abstract navNextSent(): Promise<void>;

  /** Moves the text caret to the previous sentence. */
  abstract navPrevSent(): Promise<void>;

  /**
   * Returns the editable node, its value, the selection start, and the
   * selection end.
   * TODO(crbug.com/1353871): Only return text that is visible on-screen.
   */
  abstract getEditableNodeData(): EditableNodeData|null;
}
