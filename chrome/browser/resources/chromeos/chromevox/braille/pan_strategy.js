// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic for panning a braille display within a line of braille
 * content that might not fit on a single display.
 */

goog.provide('PanStrategy');

/**
 * @constructor
 *
 * A stateful class that keeps track of the current 'viewport' of a braille
 * display in a line of content.
 */
PanStrategy = function() {
  /**
   * @type {{rows: number, columns: number}}
   * @private
   */
  this.displaySize_ = {rows: 1, columns: 40};

  /**
   * Start and end are both inclusive.
   * @type {!PanStrategy.Range}
   * @private
   */
  this.viewPort_ = {firstRow: 0, lastRow: 0};

  /**
   * The ArrayBuffer holding the braille cells after it's been processed to
   * wrap words that are cut off by the column boundaries.
   * @type {!ArrayBuffer}
   * @private
   */
  this.wrappedBuffer_ = new ArrayBuffer(0);

  /**
   * The original text that corresponds with the braille buffers. There is only
   * one textBuffer that correlates with both fixed and wrapped buffers.
   * @type {string}
   * @private
   */
  this.textBuffer_ = '';

  /**
   * The ArrayBuffer holding the original braille cells, without being
   * processed to wrap words.
   * @type {!ArrayBuffer}
   * @private
   */
  this.fixedBuffer_ = new ArrayBuffer(0);

  /**
   * The updated mapping from braille cells to text characters for the wrapped
   * buffer.
   * @type {Array<number>}
   * @private
   */
  this.wrappedBrailleToText_ = [];

  /**
   * The original mapping from braille cells to text characters.
   * @type {Array<number>}
   * @private
   */
  this.fixedBrailleToText_ = [];

  /**
   * Indicates whether the pan strategy is wrapped or fixed. It is wrapped when
   * true.
   * @type {boolean}
   * @private
   */
  this.panStrategyWrapped_ = false;

  /** @type {{start: (number), end: (number)}} */
  this.cursor_ = {start: -1, end: -1};

  /** @type {{start: (number), end: (number)}} */
  this.wrappedCursor_ = {start: -1, end: -1};
};

/**
 * A range used to represent the viewport with inclusive start and xclusive
 * end position.
 * @typedef {{firstRow: number, lastRow: number}}
 */
PanStrategy.Range;

PanStrategy.prototype = {
  /**
   * Gets the current viewport which is never larger than the current
   * display size and whose end points are always within the limits of
   * the current content.
   * @type {!PanStrategy.Range}
   */
  get viewPort() {
    return this.viewPort_;
  },

  /**
   * Gets the current displaySize.
   * @type {{rows: number, columns: number}}
   */
  get displaySize() {
    return this.displaySize_;
  },

  /**
   * @return {{brailleOffset: number, textOffset: number}} The offset of
   * braille and text indices of the current slice.
   */
  get offsetsForSlices() {
    return {
      brailleOffset: this.viewPort_.firstRow * this.displaySize_.columns,
      textOffset: this.brailleToText
                      [this.viewPort_.firstRow * this.displaySize_.columns]
    };
  },

  /**
   * @return {number} The number of lines in the fixedBuffer.
   */
  get fixedLineCount() {
    return Math.ceil(this.fixedBuffer_.byteLength / this.displaySize_.columns);
  },

  /**
   * @return {number} The number of lines in the wrappedBuffer.
   */
  get wrappedLineCount() {
    return Math.ceil(
        this.wrappedBuffer_.byteLength / this.displaySize_.columns);
  },

  /**
   * @return {Array<number>} The map of Braille cells to the first index of the
   *    corresponding text character.
   */
  get brailleToText() {
    if (this.panStrategyWrapped_) {
      return this.wrappedBrailleToText_;
    } else {
      return this.fixedBrailleToText_;
    }
  },

  /**
   * @param {boolean=} opt_showCursor Defaults to true.
   * @return {ArrayBuffer} Buffer of the slice of braille cells within the
   *    bounds of the viewport.
   */
  getCurrentBrailleViewportContents: function(opt_showCursor) {
    opt_showCursor = opt_showCursor === undefined ? true : opt_showCursor;
    var buf =
        this.panStrategyWrapped_ ? this.wrappedBuffer_ : this.fixedBuffer_;

    var startIndex, endIndex;
    if (this.panStrategyWrapped_) {
      startIndex = this.wrappedCursor_.start;
      endIndex = this.wrappedCursor_.end;
    } else {
      startIndex = this.cursor_.start;
      endIndex = this.cursor_.end;
    }

    if (startIndex >= 0 && startIndex < buf.byteLength &&
        endIndex >= startIndex && endIndex <= buf.byteLength) {
      var dataView = new DataView(buf);
      while (startIndex < endIndex) {
        var value = dataView.getUint8(startIndex);
        if (opt_showCursor) {
          value |= BrailleDisplayManager.CURSOR_DOTS;
        } else {
          value &= ~BrailleDisplayManager.CURSOR_DOTS;
        }
        dataView.setUint8(startIndex, value);
        startIndex++;
      }
    }

    return buf.slice(
        this.viewPort_.firstRow * this.displaySize_.columns,
        (this.viewPort_.lastRow + 1) * this.displaySize_.columns);
  },

  /**
   * @return {string} String of the slice of text letters corresponding with
   *    the current braille slice.
   */
  getCurrentTextViewportContents: function() {
    var brailleToText = this.brailleToText;
    // Index of last braille character in slice.
    var index = (this.viewPort_.lastRow + 1) * this.displaySize_.columns - 1;
    // Index of first text character that the last braille character points to.
    var end = brailleToText[index];
    // Increment index until brailleToText[index] points to a different char.
    // This is the cutoff point, as substring cuts up to, but not including,
    // brailleToText[index].
    while (index < brailleToText.length && brailleToText[index] == end) {
      index++;
    }
    return this.textBuffer_.substring(
        brailleToText[this.viewPort_.firstRow * this.displaySize_.columns],
        brailleToText[index]);
  },

  /**
   * Sets the current pan strategy and resets the viewport.
   */
  setPanStrategy: function(wordWrap) {
    this.panStrategyWrapped_ = wordWrap;
    this.panToPosition_(0);
  },

  /**
   * Sets the display size.  This call may update the viewport.
   * @param {number} rowCount the new row size, or {@code 0} if no display is
   *     present.
   * @param {number} columnCount the new column size, or {@code 0}
   *    if no display is present.
   */
  setDisplaySize: function(rowCount, columnCount) {
    this.displaySize_ = {rows: rowCount, columns: columnCount};
    this.setContent(
        this.textBuffer_, this.fixedBuffer_, this.fixedBrailleToText_, 0);
  },

  /**
   * Sets the internal data structures that hold the fixed and wrapped buffers
   * and maps.
   * @param {string} textBuffer Text of the shown braille.
   * @param {!ArrayBuffer} translatedContent The new braille content.
   * @param {Array<number>} fixedBrailleToText Map of Braille cells to the first
   *     index of corresponding text letter.
   * @param {number} targetPosition Target position.  The viewport is changed
   *     to overlap this position.
   */
  setContent: function(
      textBuffer, translatedContent, fixedBrailleToText, targetPosition) {
    this.viewPort_.firstRow = 0;
    this.viewPort_.lastRow = this.displaySize_.rows - 1;
    this.fixedBrailleToText_ = fixedBrailleToText;
    this.wrappedBrailleToText_ = [];
    this.textBuffer_ = textBuffer;
    this.fixedBuffer_ = translatedContent;

    // Convert the cells to Unicode braille pattern characters.
    var view = new Uint8Array(translatedContent);
    var wrappedBrailleArray = [];

    var lastBreak = 0;
    var cellsPadded = 0;
    var index;
    for (index = 0; index < translatedContent.byteLength + cellsPadded;
         index++) {
      // Is index at the beginning of a new line?
      if (index != 0 && index % this.displaySize_.columns == 0) {
        if (view[index - cellsPadded] == 0) {
          // Delete all empty cells at the beginning of this line.
          while (index - cellsPadded < view.length &&
                 view[index - cellsPadded] == 0) {
            cellsPadded--;
          }
          index--;
          lastBreak = index;
        } else if (
            view[index - cellsPadded - 1] != 0 &&
            lastBreak % this.displaySize_.columns != 0) {
          // If first cell is not empty, we need to move the whole word down to
          // this line and padd to previous line with 0's, from |lastBreak| to
          // index. The braille to text map is also updated.
          // If lastBreak is at the beginning of a line, that means the current
          // word is bigger than |this.displaySize_.columns| so we shouldn't
          // wrap.
          for (var j = lastBreak + 1; j < index; j++) {
            wrappedBrailleArray[j] = 0;
            this.wrappedBrailleToText_[j] = this.wrappedBrailleToText_[j - 1];
            cellsPadded++;
          }
          lastBreak = index;
          index--;
        } else {
          // |lastBreak| is at the beginning of a line, so current word is
          // bigger than |this.displaySize_.columns| so we shouldn't wrap.
          this.maybeSetWrappedCursor_(
              index - cellsPadded, wrappedBrailleArray.length);
          wrappedBrailleArray.push(view[index - cellsPadded]);
          this.wrappedBrailleToText_.push(
              fixedBrailleToText[index - cellsPadded]);
        }
      } else {
        if (view[index - cellsPadded] == 0) {
          lastBreak = index;
        }
        this.maybeSetWrappedCursor_(
            index - cellsPadded, wrappedBrailleArray.length);
        wrappedBrailleArray.push(view[index - cellsPadded]);
        this.wrappedBrailleToText_.push(
            fixedBrailleToText[index - cellsPadded]);
      }
    }

    // It's possible the end of the wrapped cursor falls at the
    // |translatedContent.byteLength| exactly.
    this.maybeSetWrappedCursor_(
        index - cellsPadded, wrappedBrailleArray.length);

    // Convert the wrapped Braille Uint8 Array back to ArrayBuffer.
    var wrappedBrailleUint8Array = new Uint8Array(wrappedBrailleArray);
    this.wrappedBuffer_ = new ArrayBuffer(wrappedBrailleUint8Array.length);
    new Uint8Array(this.wrappedBuffer_).set(wrappedBrailleUint8Array);
    this.panToPosition_(targetPosition);
  },

  /**
   * Sets a braille cursor.
   * @param {number} startIndex
   * @param {number} endIndex
   */
  setCursor: function(startIndex, endIndex) {
    this.cursor_ = {start: startIndex, end: endIndex};
  },

  /**
   * Gets the current cursor.
   * @return {{start: (number), end: (number)}}
   */
  getCursor: function() {
    return this.cursor_;
  },

  /**
   * Refreshes the wrapped cursor given a mapping from an unwrapped index to a
   * wrapped index.
   * @param {number} unwrappedIndex
   * @param {number} wrappedIndex
   */
  maybeSetWrappedCursor_: function(unwrappedIndex, wrappedIndex) {
    // We only care about the bounds of the index start/end.
    if (this.cursor_.start != unwrappedIndex &&
        this.cursor_.end != unwrappedIndex)
      return;
    if (this.cursor_.start == unwrappedIndex) {
      this.wrappedCursor_.start = wrappedIndex;
    } else if (this.cursor_.end == unwrappedIndex) {
      this.wrappedCursor_.end = wrappedIndex;
    }
  },

  /**
   * If possible, changes the viewport to a part of the line that follows
   * the current viewport.
   * @return {boolean} {@code true} if the viewport was changed.
   */
  next: function() {
    var contentLength =
        this.panStrategyWrapped_ ? this.wrappedLineCount : this.fixedLineCount;
    var newStart = this.viewPort_.lastRow + 1;
    var newEnd;
    if (newStart + this.displaySize_.rows - 1 < contentLength) {
      newEnd = newStart + this.displaySize_.rows - 1;
    } else {
      newEnd = contentLength - 1;
    }
    if (newEnd >= newStart) {
      this.viewPort_ = {firstRow: newStart, lastRow: newEnd};
      return true;
    }
    return false;
  },

  /**
   * If possible, changes the viewport to a part of the line that precedes
   * the current viewport.
   * @return {boolean} {@code true} if the viewport was changed.
   */
  previous: function() {
    var contentLength =
        this.panStrategyWrapped_ ? this.wrappedLineCount : this.fixedLineCount;
    if (this.viewPort_.firstRow > 0) {
      var newStart, newEnd;
      if (this.viewPort_.firstRow < this.displaySize_.rows) {
        newStart = 0;
        newEnd = Math.min(this.displaySize_.rows, contentLength);
      } else {
        newEnd = this.viewPort_.firstRow - 1;
        newStart = newEnd - this.displaySize_.rows + 1;
      }
      if (newStart <= newEnd) {
        this.viewPort_ = {firstRow: newStart, lastRow: newEnd};
        return true;
      }
    }
    return false;
  },

  /**
   * Moves the viewport so that it overlaps a target position without taking
   * the current viewport position into consideration.
   * @param {number} position Target position.
   */
  panToPosition_: function(position) {
    if (this.displaySize_.rows * this.displaySize_.columns > 0) {
      this.viewPort_ = {firstRow: -1, lastRow: -1};
      while (this.next() &&
             (this.viewPort_.lastRow + 1) * this.displaySize_.columns <=
                 position) {
        // Nothing to do.
      }
    } else {
      this.viewPort_ = {firstRow: position, lastRow: position};
    }
  },
};
