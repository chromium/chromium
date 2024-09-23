// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Translates text to braille, optionally with some parts
 * uncontracted.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {Spannable} from '../../common/spannable.js';

import {LibLouis} from './liblouis.js';
import {BrailleTextStyleSpan, ExtraCellsSpan, ValueSelectionSpan, ValueSpan} from './spans.js';

interface Chunk {
  translator: LibLouis.Translator | null;
  start: number;
  end: number;
  cells?: ArrayBuffer;
  textToBraille?: number[];
  brailleToText?: number[];
}

/** Like LibLouis.TranslateCallback, but all values are mandatory. */
type RequiredTranslateCallback =
    (cells: ArrayBuffer, textToBraille: number[], brailleToText: number[]) =>
        void;

/**
 * A wrapper around one or two braille translators that uses contracted
 * braille or not based on the selection start- and end-points (if any) in the
 * translated text.  If only one translator is provided, then that translator
 * is used for all text regardless of selection.  If two translators
 * are provided, then the uncontracted translator is used for some text
 * around the selection end-points and the contracted translator is used
 * for all other text.  When determining what text to use uncontracted
 * translation for around a position, a region surrounding that position
 * containing either only whitespace characters or only non-whitespace
 * characters is used.
 */
export class ExpandingBrailleTranslator {
  /**
   * @param defaultTranslator The translator for all text when the uncontracted
   *     translator is not used.
   * @param uncontractedTranslator Translator to use for uncontracted braille
   *     translation.
   */
  constructor(
      private defaultTranslator_: LibLouis.Translator,
      private uncontractedTranslator_?: LibLouis.Translator | null) {}

  /**
   * Translates text to braille using the translator(s) provided to the
   * constructor.  See LibLouis.Translator for further details.
   * @param text Text to translate.
   * @param expansionType Indicates how the text marked by a value span,
   *    if any, is expanded.
   * @param callback Called when the translation is done.  It takes resulting
   *    braille cells and positional mappings as parameters.
   */
  translate(
      text: Spannable, expansionType: ExpandingBrailleTranslator.ExpansionType,
      callback: RequiredTranslateCallback): void {
    const expandRanges = this.findExpandRanges_(text, expansionType);
    const extraCellsSpans = text.getSpansInstanceOf(ExtraCellsSpan)
                                .filter(span => span.cells.byteLength > 0);
    const extraCellsPositions =
        extraCellsSpans.map(span => text.getSpanStart(span));
    const formTypeMap: number[] = new Array(text.length).fill(0);
    text.getSpansInstanceOf(BrailleTextStyleSpan).forEach(
        (span: BrailleTextStyleSpan) => {
          const start = text.getSpanStart(span);
          const end = text.getSpanEnd(span);
          for (let i = start; i < end; i++) {
            formTypeMap[i] |= span.formType;
          }
        });

    if (expandRanges.length === 0 && extraCellsSpans.length === 0) {
      this.defaultTranslator_.translate(
          text.toString(), formTypeMap,
          ExpandingBrailleTranslator.nullParamsToEmptyAdapter_(
              text.length, callback));
      return;
    }

    const chunks: Chunk[] = [];
    function maybeAddChunkToTranslate(
        translator: LibLouis.Translator, start: number, end: number): void {
      if (start < end) {
        chunks.push({translator, start, end});
      }
    }
    function addExtraCellsChunk(pos: number, cells: ArrayBuffer): void {
      const chunk = {
        translator: null,
        start: pos,
        end: pos,
        cells,
        textToBraille: [],
        brailleToText: new Array(cells.byteLength),
      };
      for (let i = 0; i < cells.byteLength; ++i) {
        chunk.brailleToText[i] = 0;
      }
      chunks.push(chunk);
    }
    function addChunk(
        translator: LibLouis.Translator, start: number, end: number): void {
      while (extraCellsSpans.length > 0 && extraCellsPositions[0] <= end) {
        maybeAddChunkToTranslate(translator, start, extraCellsPositions[0]);
        // TODO(b/314203187): Not null asserted, check that this is correct.
        start = extraCellsPositions.shift()!;
        addExtraCellsChunk(start, extraCellsSpans.shift().cells);
      }
      maybeAddChunkToTranslate(translator, start, end);
    }
    let lastEnd = 0;
    for (let i = 0; i < expandRanges.length; ++i) {
      const range = expandRanges[i];
      if (lastEnd < range.start) {
        addChunk(this.defaultTranslator_, lastEnd, range.start);
      }
      // TODO(b/314203187): Not null asserted, check that this is correct.
      addChunk(this.uncontractedTranslator_!, range.start, range.end);
      lastEnd = range.end;
    }
    addChunk(this.defaultTranslator_, lastEnd, text.length);

    const chunksToTranslate = chunks.filter((chunk: Chunk) => chunk.translator);
    let numPendingCallbacks = chunksToTranslate.length;

    function chunkTranslated(
        chunk: Chunk, cells: ArrayBuffer, textToBraille: number[],
        brailleToText: number[]): void {
      chunk.cells = cells;
      chunk.textToBraille = textToBraille;
      chunk.brailleToText = brailleToText;
      if (--numPendingCallbacks <= 0) {
        finish();
      }
    }

    function finish(): void {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      const totalCells = chunks.reduce(
          (accum: number, chunk: Chunk) => accum + chunk.cells!.byteLength, 0);
      const cells = new Uint8Array(totalCells);
      let cellPos = 0;
      const textToBraille: number[] = [];
      const brailleToText: number[] = [];
      function appendAdjusted(
          array: number[], toAppend: number[], adjustment: number): void {
        array.push.apply(array, toAppend.map(elem => adjustment + elem));
      }
      for (let i = 0, chunk; chunk = chunks[i]; ++i) {
        // TODO(b/314203187): Not null asserted, check that this is correct.
        cells.set(new Uint8Array(chunk.cells!), cellPos);
        appendAdjusted(textToBraille, chunk.textToBraille!, cellPos);
        appendAdjusted(brailleToText, chunk.brailleToText!, chunk.start);
        cellPos += chunk.cells!.byteLength;
      }
      callback(cells.buffer, textToBraille, brailleToText);
    }

    if (chunksToTranslate.length > 0) {
      chunksToTranslate.forEach((chunk: Chunk) => {
        // TODO(b/314203187): Not null asserted, check that this is correct.
        chunk.translator!.translate(
            text.toString().substring(chunk.start, chunk.end),
            formTypeMap.slice(chunk.start, chunk.end),
            ExpandingBrailleTranslator.nullParamsToEmptyAdapter_(
                chunk.end - chunk.start,
                (cells: ArrayBuffer, textToBraille: number[],
                    brailleToText: number[]) =>
                  chunkTranslated(chunk, cells, textToBraille, brailleToText)));
      });
    } else {
      finish();
    }
  }

  /**
   * Expands a position to a range that covers the consecutive range of
   * either whitespace or non whitespace characters around it.
   * @param str Text to look in.
   * @param pos Position to start looking at.
   * @param start Minimum value for the start position of the returned range.
   * @param end Maximum value for the end position of the returned range.
   * @return The calculated range.
   */
  private static rangeForPosition_(
      str: string, pos: number, start: number, end: number): Range {
    if (start < 0 || end > str.length) {
      throw RangeError(
          'End-points out of range looking for braille expansion range');
    }
    if (pos < start || pos >= end) {
      throw RangeError(
          'Position out of range looking for braille expansion range');
    }
    // Find the last chunk of either whitespace or non-whitespace before and
    // including pos.
    start = str.substring(start, pos + 1).search(/(\s+|\S+)$/) + start;
    // Find the characters to include after pos, starting at pos so that
    // they are the same kind (either whitespace or not) as the
    // characters starting at start.
    // TODO(b/314203187): Not null asserted, check that this is correct.
    end = pos + /^(\s+|\S+)/.exec(str.substring(pos, end))![0].length;
    return {start, end};
  }

  /**
   * Finds the ranges in which contracted braille should not be used.
   * @param text Text to find expansion ranges in.
   * @param expansionType Indicates how the text marked up as the value is
   *     expanded.
   * @return The calculated ranges.
   */
  private findExpandRanges_(
      text: Spannable, expansionType: ExpandingBrailleTranslator.ExpansionType):
          Range[] {
    const result: Range[] = [];
    if (this.uncontractedTranslator_ &&
        expansionType !== ExpandingBrailleTranslator.ExpansionType.NONE) {
      const value = text.getSpanInstanceOf(ValueSpan);
      if (value) {
        const valueStart = text.getSpanStart(value);
        const valueEnd = text.getSpanEnd(value);
        switch (expansionType) {
          case ExpandingBrailleTranslator.ExpansionType.SELECTION:
            this.addRangesForSelection_(text, valueStart, valueEnd, result);
            break;
          case ExpandingBrailleTranslator.ExpansionType.ALL:
            result.push({start: valueStart, end: valueEnd});
            break;
        }
      }
    }

    return result;
  }

  /**
   * Finds ranges to expand around selection end points inside the value of
   * a string.  If any ranges are found, adds them to outRanges.
   * @param text Text to find ranges in.
   * @param valueStart Start of the value in text.
   * @param valueEnd End of the value in text.
   * @param outRanges Destination for the expansion ranges. Untouched if no
   *     ranges are found. Note that ranges may be coalesced.
   */
  private addRangesForSelection_(
      text: Spannable, valueStart: number, valueEnd: number,
      outRanges: Range[]): void {
    const selection = text.getSpanInstanceOf(ValueSelectionSpan);
    if (!selection) {
      return;
    }
    const selectionStart = text.getSpanStart(selection);
    const selectionEnd = text.getSpanEnd(selection);
    if (selectionStart < valueStart || selectionEnd > valueEnd) {
      return;
    }
    const expandPositions: number[] = [];
    if (selectionStart === valueEnd) {
      if (selectionStart > valueStart) {
        expandPositions.push(selectionStart - 1);
      }
    } else {
      if (selectionStart === selectionEnd && selectionStart > valueStart) {
        expandPositions.push(selectionStart - 1);
      }
      expandPositions.push(selectionStart);
      // Include the selection end if the length of the selection is
      // greater than one (otherwise this position would be redundant).
      if (selectionEnd > selectionStart + 1) {
        // Look at the last actual character of the selection, not the
        // character at the (exclusive) end position.
        expandPositions.push(selectionEnd - 1);
      }
    }

    let lastRange = outRanges[outRanges.length - 1] || null;
    for (let i = 0; i < expandPositions.length; ++i) {
      const range = ExpandingBrailleTranslator.rangeForPosition_(
          text.toString(), expandPositions[i], valueStart, valueEnd);
      if (lastRange && lastRange.end >= range.start) {
        lastRange.end = range.end;
      } else {
        outRanges.push(range);
        lastRange = range;
      }
    }
  }

  /**
   * Adapts callback to accept null arguments and treat them as if the
   * translation result is empty.
   * @param inputLength Length of the input to the translation.
   *     Used for populating textToBraille if null.
   * @param callback The callback to adapt.
   * @return An adapted version of the callback.
   */
  private static nullParamsToEmptyAdapter_(
      inputLength: number, callback: RequiredTranslateCallback):
          LibLouis.TranslateCallback {
    return function(
        cells: ArrayBuffer | null, textToBraille: number[] | null,
        brailleToText: number[] | null): void {
      if (!textToBraille) {
        textToBraille = new Array(inputLength);
        for (let i = 0; i < inputLength; ++i) {
          textToBraille[i] = 0;
        }
      }
      callback(cells || new ArrayBuffer(0), textToBraille, brailleToText || []);
    };
  }
}

export namespace ExpandingBrailleTranslator {
  /**
   * What expansion to apply to the part of the translated string marked by the
   * ValueSpan spannable.
   */
  export enum ExpansionType {
    /**
     * Use the default translator all of the value, regardless of any selection.
     * This is typically used when the user is in the middle of typing and the
     * typing started outside of a word.
     */
    NONE = 0,
    /**
     * Expand text around the selection end-points if any.  If the selection is
     * a cursor, expand the text that occupies the positions right before and
     * after the cursor.  This is typically used when the user hasn't started
     * typing contracted braille or when editing inside a word.
     */
    SELECTION = 1,
    /**
     * Expand all text covered by the value span.  this is typically used when
     * the user is editing a text field where it doesn't make sense to use
     * contracted braille (such as a url or email address).
     */
    ALL = 2,
  }
}

// Local to module.

/** A character range with inclusive start and exclusive end positions. */
interface Range {
  start: number;
  end: number;
}

TestImportManager.exportForTesting(ExpandingBrailleTranslator);
