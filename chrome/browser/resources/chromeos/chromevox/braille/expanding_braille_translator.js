// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Translates text to braille, optionally with some parts
 * uncontracted.
 */

goog.provide('ExpandingBrailleTranslator');

goog.require('Spannable');
goog.require('ExtraCellsSpan');
goog.require('LibLouis');
goog.require('ValueSelectionSpan');
goog.require('ValueSpan');


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
 * @param {!LibLouis.Translator} defaultTranslator The translator for all
 *     text when the uncontracted translator is not used.
 * @param {LibLouis.Translator=} opt_uncontractedTranslator
 *     Translator to use for uncontracted braille translation.
 * @constructor
 */
ExpandingBrailleTranslator = function(
    defaultTranslator, opt_uncontractedTranslator) {
  /**
   * @type {!LibLouis.Translator}
   * @private
   */
  this.defaultTranslator_ = defaultTranslator;
  /**
   * @type {LibLouis.Translator}
   * @private
   */
  this.uncontractedTranslator_ = opt_uncontractedTranslator || null;
};


/**
 * What expansion to apply to the part of the translated string marked by the
 * {@code ValueSpan} spannable.
 * @enum {number}
 */
ExpandingBrailleTranslator.ExpansionType = {
  /**
   * Use the default translator all of the value, regardless of any selection.
   * This is typically used when the user is in the middle of typing and the
   * typing started outside of a word.
   */
  NONE: 0,
  /**
   * Expand text around the selection end-points if any.  If the selection is
   * a cursor, expand the text that occupies the positions right before and
   * after the cursor.  This is typically used when the user hasn't started
   * typing contracted braille or when editing inside a word.
   */
  SELECTION: 1,
  /**
   * Expand all text covered by the value span.  this is typically used when
   * the user is editing a text field where it doesn't make sense to use
   * contracted braille (such as a url or email address).
   */
  ALL: 2
};


/**
 * Translates text to braille using the translator(s) provided to the
 * constructor.  See {@code LibLouis.Translator} for further details.
 * @param {!Spannable} text Text to translate.
 * @param {ExpandingBrailleTranslator.ExpansionType} expansionType
 *     Indicates how the text marked by a value span, if any, is expanded.
 * @param {function(!ArrayBuffer, !Array<number>, !Array<number>)}
 *     callback Called when the translation is done.  It takes resulting
 *         braille cells and positional mappings as parameters.
 */
ExpandingBrailleTranslator.prototype.translate = function(
    text, expansionType, callback) {
  var expandRanges = this.findExpandRanges_(text, expansionType);
  var extraCellsSpans =
      text.getSpansInstanceOf(ExtraCellsSpan).filter(function(span) {
        return span.cells.byteLength > 0;
      });
  var extraCellsPositions = extraCellsSpans.map(function(span) {
    return text.getSpanStart(span);
  });
  var formTypeMap = new Array(text.length).fill(0);
  text.getSpansInstanceOf(BrailleTextStyleSpan).forEach(function(span) {
    var start = text.getSpanStart(span);
    var end = text.getSpanEnd(span);
    for (var i = start; i < end; i++)
      formTypeMap[i] |= span.formType;
  });

  if (expandRanges.length == 0 && extraCellsSpans.length == 0) {
    this.defaultTranslator_.translate(
        text.toString(), formTypeMap,
        ExpandingBrailleTranslator.nullParamsToEmptyAdapter_(
            text.length, callback));
    return;
  }

  var chunks = [];
  function maybeAddChunkToTranslate(translator, start, end) {
    if (start < end) {
      chunks.push({translator: translator, start: start, end: end});
    }
  }
  function addExtraCellsChunk(pos, cells) {
    var chunk = {
      translator: null,
      start: pos,
      end: pos,
      cells: cells,
      textToBraille: [],
      brailleToText: new Array(cells.byteLength)
    };
    for (var i = 0; i < cells.byteLength; ++i)
      chunk.brailleToText[i] = 0;
    chunks.push(chunk);
  }
  function addChunk(translator, start, end) {
    while (extraCellsSpans.length > 0 && extraCellsPositions[0] <= end) {
      maybeAddChunkToTranslate(translator, start, extraCellsPositions[0]);
      start = extraCellsPositions.shift();
      addExtraCellsChunk(start, extraCellsSpans.shift().cells);
    }
    maybeAddChunkToTranslate(translator, start, end);
  }
  var lastEnd = 0;
  for (var i = 0; i < expandRanges.length; ++i) {
    var range = expandRanges[i];
    if (lastEnd < range.start) {
      addChunk(this.defaultTranslator_, lastEnd, range.start);
    }
    addChunk(this.uncontractedTranslator_, range.start, range.end);
    lastEnd = range.end;
  }
  addChunk(this.defaultTranslator_, lastEnd, text.length);

  var chunksToTranslate = chunks.filter(function(chunk) {
    return chunk.translator;
  });
  var numPendingCallbacks = chunksToTranslate.length;

  function chunkTranslated(chunk, cells, textToBraille, brailleToText) {
    chunk.cells = cells;
    chunk.textToBraille = textToBraille;
    chunk.brailleToText = brailleToText;
    if (--numPendingCallbacks <= 0) {
      finish();
    }
  }

  function finish() {
    var totalCells = chunks.reduce(function(accum, chunk) {
      return accum + chunk.cells.byteLength;
    }, 0);
    var cells = new Uint8Array(totalCells);
    var cellPos = 0;
    var textToBraille = [];
    var brailleToText = [];
    function appendAdjusted(array, toAppend, adjustment) {
      array.push.apply(array, toAppend.map(function(elem) {
        return adjustment + elem;
      }));
    }
    for (var i = 0, chunk; chunk = chunks[i]; ++i) {
      cells.set(new Uint8Array(chunk.cells), cellPos);
      appendAdjusted(textToBraille, chunk.textToBraille, cellPos);
      appendAdjusted(brailleToText, chunk.brailleToText, chunk.start);
      cellPos += chunk.cells.byteLength;
    }
    callback(cells.buffer, textToBraille, brailleToText);
  }

  if (chunksToTranslate.length > 0) {
    chunksToTranslate.forEach(function(chunk) {
      chunk.translator.translate(
          text.toString().substring(chunk.start, chunk.end),
          formTypeMap.slice(chunk.start, chunk.end),
          ExpandingBrailleTranslator.nullParamsToEmptyAdapter_(
              chunk.end - chunk.start, goog.partial(chunkTranslated, chunk)));
    });
  } else {
    finish();
  }
};


/**
 * Expands a position to a range that covers the consecutive range of
 * either whitespace or non whitespace characters around it.
 * @param {string} str Text to look in.
 * @param {number} pos Position to start looking at.
 * @param {number} start Minimum value for the start position of the returned
 *     range.
 * @param {number} end Maximum value for the end position of the returned
 *     range.
 * @return {!ExpandingBrailleTranslator.Range_} The claculated range.
 * @private
 */
ExpandingBrailleTranslator.rangeForPosition_ = function(str, pos, start, end) {
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
  end = pos + /^(\s+|\S+)/.exec(str.substring(pos, end))[0].length;
  return {start: start, end: end};
};


/**
 * Finds the ranges in which contracted braille should not be used.
 * @param {!Spannable} text Text to find expansion ranges in.
 * @param {ExpandingBrailleTranslator.ExpansionType} expansionType
 *     Indicates how the text marked up as the value is expanded.
 * @return {!Array<ExpandingBrailleTranslator.Range_>} The calculated
 *     ranges.
 * @private
 */
ExpandingBrailleTranslator.prototype.findExpandRanges_ = function(
    text, expansionType) {
  var result = [];
  if (this.uncontractedTranslator_ &&
      expansionType != ExpandingBrailleTranslator.ExpansionType.NONE) {
    var value = text.getSpanInstanceOf(ValueSpan);
    if (value) {
      var valueStart = text.getSpanStart(value);
      var valueEnd = text.getSpanEnd(value);
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
};


/**
 * Finds ranges to expand around selection end points inside the value of
 * a string.  If any ranges are found, adds them to {@code outRanges}.
 * @param {Spannable} text Text to find ranges in.
 * @param {number} valueStart Start of the value in {@code text}.
 * @param {number} valueEnd End of the value in {@code text}.
 * @param {Array<ExpandingBrailleTranslator.Range_>} outRanges
 *     Destination for the expansion ranges.  Untouched if no ranges
 *     are found.  Note that ranges may be coalesced.
 * @private
 */
ExpandingBrailleTranslator.prototype.addRangesForSelection_ = function(
    text, valueStart, valueEnd, outRanges) {
  var selection = text.getSpanInstanceOf(ValueSelectionSpan);
  if (!selection) {
    return;
  }
  var selectionStart = text.getSpanStart(selection);
  var selectionEnd = text.getSpanEnd(selection);
  if (selectionStart < valueStart || selectionEnd > valueEnd) {
    return;
  }
  var expandPositions = [];
  if (selectionStart == valueEnd) {
    if (selectionStart > valueStart) {
      expandPositions.push(selectionStart - 1);
    }
  } else {
    if (selectionStart == selectionEnd && selectionStart > valueStart) {
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

  var lastRange = outRanges[outRanges.length - 1] || null;
  for (var i = 0; i < expandPositions.length; ++i) {
    var range = ExpandingBrailleTranslator.rangeForPosition_(
        text.toString(), expandPositions[i], valueStart, valueEnd);
    if (lastRange && lastRange.end >= range.start) {
      lastRange.end = range.end;
    } else {
      outRanges.push(range);
      lastRange = range;
    }
  }
};


/**
 * Adapts {@code callback} to accept null arguments and treat them as if the
 * translation result is empty.
 * @param {number} inputLength Length of the input to the translation.
 *     Used for populating {@code textToBraille} if null.
 * @param {function(!ArrayBuffer, !Array<number>, !Array<number>)} callback
 *     The callback to adapt.
 * @return {function(ArrayBuffer, Array<number>, Array<number>)}
 *     An adapted version of the callback.
 * @private
 */
ExpandingBrailleTranslator.nullParamsToEmptyAdapter_ = function(
    inputLength, callback) {
  return function(cells, textToBraille, brailleToText) {
    if (!textToBraille) {
      textToBraille = new Array(inputLength);
      for (var i = 0; i < inputLength; ++i) {
        textToBraille[i] = 0;
      }
    }
    callback(cells || new ArrayBuffer(0), textToBraille, brailleToText || []);
  };
};


/**
 * A character range with inclusive start and exclusive end positions.
 * @typedef {{start: number, end: number}}
 * @private
 */
ExpandingBrailleTranslator.Range_;
