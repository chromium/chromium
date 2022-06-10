// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * EditingUtil provides utility and helper methods for editing-related
 * operations.
 */
export class EditingUtil {
  /**
   * Replaces a phrase to the left of the text caret with another phrase. If
   * multiple instances of `deletePhrase` are present, this function will
   * replace the one closest to the text caret.
   * @param {string} value
   * @param {number} caretIndex
   * @param {string} deletePhrase The phrase to be deleted.
   * @param {string} insertPhrase The phrase to be inserted.
   * @return {string}
   */
  static replacePhrase(value, caretIndex, deletePhrase, insertPhrase) {
    const leftOfCaret = value.substring(0, caretIndex);
    const rightOfCaret = value.substring(caretIndex);
    const performingDelete = insertPhrase === '';
    deletePhrase = deletePhrase.trim();
    insertPhrase = insertPhrase.trim();

    // Find the right-most occurrence of `deletePhrase`. Require `deletePhrase`
    // to be separated by word boundaries. If we're deleting text, prefer
    // the RegExps that include a leading/trailing space to preserve spacing.
    const re = EditingUtil.getPhraseRegex_(deletePhrase);
    const reWithLeadingSpace =
        EditingUtil.getPhraseRegexLeadingSpace_(deletePhrase);
    const reWithTrailingSpace =
        EditingUtil.getPhraseRegexTrailingSpace_(deletePhrase);

    let newLeft;
    if (performingDelete && reWithLeadingSpace.test(leftOfCaret)) {
      newLeft = leftOfCaret.replace(reWithLeadingSpace, insertPhrase);
    } else if (performingDelete && reWithTrailingSpace.test(leftOfCaret)) {
      newLeft = leftOfCaret.replace(reWithTrailingSpace, insertPhrase);
    } else {
      newLeft = leftOfCaret.replace(re, insertPhrase);
    }

    return newLeft + rightOfCaret;
  }

  /**
   * Inserts `insertPhrase` directly before `beforePhrase` (and separates them
   * with a space). This function operates on the text to the left of the caret.
   * If multiple instances of `beforePhrase` are present, this function will
   * use the one closest to the text caret.
   * @param {string} value
   * @param {number} caretIndex
   * @param {string} insertPhrase
   * @param {string} beforePhrase
   * @return {string}
   */
  static insertBefore(value, caretIndex, insertPhrase, beforePhrase) {
    const leftOfCaret = value.substring(0, caretIndex);
    const rightOfCaret = value.substring(caretIndex);
    insertPhrase = insertPhrase.trim();
    beforePhrase = beforePhrase.trim();

    const re = EditingUtil.getPhraseRegex_(beforePhrase);
    // Runs when a regex match occurs and returns the replacement string.
    const replacer = () => `${insertPhrase} ${beforePhrase}`;
    const newLeft = leftOfCaret.replace(re, replacer);
    return newLeft + rightOfCaret;
  }

  /**
   * Returns a RegExp that matches on the right-most occurrence of a phrase.
   * The returned RegExp is case insensitive and requires that `phrase` is
   * separated by word boundaries.
   * @param {string} phrase
   * @return {!RegExp}
   * @private
   */
  static getPhraseRegex_(phrase) {
    return new RegExp(`(\\b${phrase}\\b)(?!.*\\b\\1\\b)`, 'i');
  }

  /**
   * Similar to above, but accounts for a leading space.
   * @param {string} phrase
   * @return {!RegExp}
   * @private
   */
  static getPhraseRegexLeadingSpace_(phrase) {
    return new RegExp(`( \\b${phrase}\\b)(?!.*\\b\\1\\b)`, 'i');
  }

  /**
   * Similar to above, but accounts for a trailing space.
   * @param {string} phrase
   * @return {!RegExp}
   * @private
   */
  static getPhraseRegexTrailingSpace_(phrase) {
    return new RegExp(`(\\b${phrase}\\b )(?!.*\\b\\1\\b)`, 'i');
  }
}
