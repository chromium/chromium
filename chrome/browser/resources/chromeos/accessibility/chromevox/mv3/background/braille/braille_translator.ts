// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export type TranslateCallback =
    (cells: ArrayBuffer|null, textToBraille: number[]|null,
     brailleToText: number[]|null) => void;
export type BackTranslateCallback = (text: string|null) => void;

/** Interface for translating between text and braille cells. */
export interface BrailleTranslator {
  /**
   * Translates text into braille cells.
   * @param text Text to be translated.
   * @param callback Callback for result. Takes 3 parameters: the resulting
   *     cells, mapping from text to braille positions and mapping from
   *     braille to text positions. If translation fails for any reason, all
   *     parameters are null.
   */
  translate(
      text: string, formTypeMap: number[]|number,
      callback: TranslateCallback): void;

  /**
   * Translates braille cells into text.
   * @param cells Cells to be translated.
   * @param callback Callback for result.
   */
  backTranslate(cells: ArrayBuffer, callback: BackTranslateCallback): void;
}
