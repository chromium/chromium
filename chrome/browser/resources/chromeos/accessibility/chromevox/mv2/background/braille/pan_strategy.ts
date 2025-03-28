// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic for panning a braille display within a line of braille
 * content that might not fit on a single display.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {CURSOR_DOTS} from './cursor_dots.js';

interface CursorPosition {
  start: number;
  end: number;
}

interface DisplaySize {
  rows: number;
  columns: number;
}

interface Offsets {
  brailleOffset: number;
  textOffset: number;
}

interface Range {
  firstRow: number;
  lastRow: number;
}

export class PanStrategy {
  private displaySize_: DisplaySize = {rows: 1, columns: 40};
  /** Start and end are both inclusive. */
  private viewPort_: Range = {firstRow: 0, lastRow: 0};
  /**
   * The ArrayBuffer holding the braille cells after it's been processed to
   * wrap words that are cut off by the column boundaries.
   */
  private wrappedBuffer_ = new ArrayBuffer(0);
  /**
   * The original text that corresponds with the braille buffers. There is
   * only one textBuffer that correlates with both fixed and wrapped buffers.
   */
  private textBuffer_ = '';
  /**
   * The ArrayBuffer holding the original braille cells, without being
   * processed to wrap words.
   */
  private fixedBuffer_ = new ArrayBuffer(0);
  /**
   * The updated mapping from braille cells to text characters for the wrapped
   * buffer.
   */
  private wrappedBrailleToText_: number[] = [];
  /** The original mapping from braille cells to text characters. */
  private fixedBrailleToText_: number[] = [];
  /**
   * Indicates whether the pan strategy is wrapped or fixed. It is wrapped
   * when true.
   */
  private panStrategyWrapped_ = false;
  private cursor_: CursorPosition = {start: -1, end: -1};
  private wrappedCursor_: CursorPosition = {start: -1, end: -1};

  /**
   * Gets the current viewport which is never larger than the current
   * display size and whose end points are always within the limits of
   * the current content.
   */
  get viewPort(): Range {
    return this.viewPort_;
  }

  /** Gets the current displaySize. */
  get displaySize(): DisplaySize {
    return this.displaySize_;
  }

  /** @return The offset of braille and text indices of the current slice. */
  get offsetsForSlices(): Offsets {
    return {
      brailleOffset: this.viewPort_.firstRow * this.displaySize_.columns,
      textOffset: this.brailleToText
                      [this.viewPort_.firstRow * this.displaySize_.columns],
    };
  }

  /** @return The number of lines in the fixedBuffer. */
  get fixedLineCount(): number {
    return Math.ceil(this.fixedBuffer_.byteLength / this.displaySize_.columns);
  }

  /** @return The number of lines in the wrappedBuffer. */
  get wrappedLineCount(): number {
    return Math.ceil(
        this.wrappedBuffer_.byteLength / this.displaySize_.columns);
  }

  /**
   * @return The map of Braille cells to the first index of the corresponding
   *     text character.
   */
  get brailleToText(): number[] {
    if (this.panStrategyWrapped_) {
      return this.wrappedBrailleToText_;
    } else {
      return this.fixedBrailleToText_;
    }
  }

  /**
   * @return Buffer of the slice of braille cells within the bounds of the
   * viewport.
   */
  getCurrentBrailleViewportContents(showCursor: boolean = true): ArrayBuffer {
    const buf =
        this.panStrategyWrapped_ ? this.wrappedBuffer_ : this.fixedBuffer_;

    let startIndex;
    let endIndex;
    if (this.panStrategyWrapped_) {
      startIndex = this.wrappedCursor_.start;
      endIndex = this.wrappedCursor_.end;
    } else {
      startIndex = this.cursor_.start;
      endIndex = this.cursor_.end;
    }

    if (startIndex >= 0 && startIndex < buf.byteLength &&
        endIndex >= startIndex && endIndex <= buf.byteLength) {
      const dataView = new DataView(buf);
      while (startIndex < endIndex) {
        let value = dataView.getUint8(startIndex);
        if (showCursor) {
          value |= CURSOR_DOTS;
        } else {
          value &= ~CURSOR_DOTS;
        }
        dataView.setUint8(startIndex, value);
        startIndex++;
      }
    }

    return buf.slice(
        this.viewPort_.firstRow * this.displaySize_.columns,
        (this.viewPort_.lastRow + 1) * this.displaySize_.columns);
  }

  /**
   * @return String of the slice of text letters corresponding with the current
   * braille slice.
   */
  getCurrentTextViewportContents(): string {
    const brailleToText = this.brailleToText;
    // Index of last braille character in slice.
    let index = (this.viewPort_.lastRow + 1) * this.displaySize_.columns - 1;
    // Index of first text character that the last braille character points
    // to.
    const end = brailleToText[index];
    // Increment index until brailleToText[index] points to a different char.
    // This is the cutoff point, as substring cuts up to, but not including,
    // brailleToText[index].
    while (index < brailleToText.length && brailleToText[index] === end) {
      index++;
    }
    return this.textBuffer_.substring(
        brailleToText[this.viewPort_.firstRow * this.displaySize_.columns],
        brailleToText[index]);
  }

  /** Sets the current pan strategy and resets the viewport. */
  setPanStrategy(wordWrap: boolean): void {
    this.panStrategyWrapped_ = wordWrap;
    this.panToPosition_(0);
  }

  /**
   * Sets the display size.  This call may update the viewport.
   * @param rowCount the new row size, or 0 if no display is present.
   * @param columnCount the new column size, or 0 if no display is
   * present.
   */
  setDisplaySize(rowCount: number, columnCount: number): void {
    this.displaySize_ = {rows: rowCount, columns: columnCount};
    this.setContent(
        this.textBuffer_, this.fixedBuffer_, this.fixedBrailleToText_, 0);
  }

  /**
   * Sets the internal data structures that hold the fixed and wrapped buffers
   * and maps.
   * @param textBuffer Text of the shown braille.
   * @param translatedContent The new braille content.
   * @param fixedBrailleToText Map of Braille cells to the first index of
   *     corresponding text letter.
   * @param targetPosition Target position.  The viewport is changed to overlap
   *     this position.
   */
  setContent(
      textBuffer: string, translatedContent: ArrayBuffer,
      fixedBrailleToText: number[], targetPosition: number): void {
    this.viewPort_.firstRow = 0;
    this.viewPort_.lastRow = this.displaySize_.rows - 1;
    this.fixedBrailleToText_ = fixedBrailleToText;
    this.wrappedBrailleToText_ = [];
    this.textBuffer_ = textBuffer;
    this.fixedBuffer_ = translatedContent;

    // Convert the cells to Unicode braille pattern characters.
    const view = new Uint8Array(translatedContent);

    // Check for a multi-line display and encode with horizontal and vertical
    // spacing if so.
    //
    // We currently insert one column of dots horizontally between cells and two
    // rows of dots vertically between lines.
    //
    // TODO(accessibility): extend this to work with word wrapping below.
    if (!this.panStrategyWrapped_ && this.displaySize_.rows > 1) {
      // All known displays have even number of rows and columns.
      // TODO(accessibility): this check should move elsewhere.
      if (this.displaySize_.rows % 2 !== 0 ||
          this.displaySize_.columns % 2 !== 0) {
        return;
      }

      // Iterate in two-byte groupings.
      const horizontalSpacedBraille: number[] = [];
      for (let index = 0; index < translatedContent.byteLength; index += 2) {
        const first = view[index];
        const second = view[index + 1];

        // The first cell is always written.
        horizontalSpacedBraille.push(first);

        // The second cell turns into two cells.
        // The initial cell has one vertical blank dot column on the left.
        horizontalSpacedBraille.push(
            (second & 0b111) << 3 | (second & 0b1000000) << 1);

        // The next cell has one vertical blank dot column on the right.
        horizontalSpacedBraille.push(
            (second & 0b111000) >> 3 | (second & 0b10000000) >> 1);
      }

      // Now, space the lines vertically by inserting two blank dot rows.
      let spacedBraille: number[] = [];

      // Iterate by two cell rows at once.
      for (let row = 0; row < this.displaySize_.rows; row += 2) {
        // The first row gets added verbatim.
        for (let index = 0; index < this.displaySize_.columns; index++) {
          const rowIndex = row * this.displaySize_.columns + index;
          spacedBraille.push(horizontalSpacedBraille[rowIndex]);
        }

        // The second cell row turns into two cell rows: an upper row with
        // spacing a blank dot row above, and a lower cell row with blank dot
        // row spacing below. The upper cell row can be pushed below; store the
        // lower cell row for after.
        const nextRow = row + 1;
        const lowerRow: number[] = [];
        for (let index = 0; index < this.displaySize_.columns; index++) {
          const rowIndex = nextRow * this.displaySize_.columns + index;
          const value = horizontalSpacedBraille[rowIndex];

          // Downshift the top two dots by two positions.
          // e.g. dot 1 goes to dot 3, dot 4 to dot 6.
          let upperRowValue = (value & 0b1001) << 2;

          // Downshift dot 2 to dot 7.
          upperRowValue |= (value & 0b010) << 5;

          // Downshift dot 5 to dot 8.
          upperRowValue |= (value & 0b10000) << 3;

          spacedBraille.push(upperRowValue);

          // Lower row.
          // Upshift the bottom two dots by two positions.
          // e.g. dot 3 to dot 1, dot 6 to dot 4.
          let lowerRowValue = (value & 0b100100) >> 2;

          // Upshift dot 7 to dot 2.
          lowerRowValue |= (value & 0b1000000) >> 5;

          // Upshift dot 8 to dot 5.
          lowerRowValue |= (value & 0b10000000) >> 3;

          lowerRow.push(lowerRowValue);
        }

        spacedBraille = spacedBraille.concat(lowerRow);
      }

      const brailleUint8Array = new Uint8Array(spacedBraille);
      this.fixedBuffer_ = new ArrayBuffer(brailleUint8Array.length);
      new Uint8Array(this.fixedBuffer_).set(brailleUint8Array);
    }

    const wrappedBrailleArray: number[] = [];

    let lastBreak = 0;
    let cellsPadded = 0;
    let index;
    for (index = 0; index < translatedContent.byteLength + cellsPadded;
         index++) {
      // Is index at the beginning of a new line?
      if (index !== 0 && index % this.displaySize_.columns === 0) {
        if (view[index - cellsPadded] === 0) {
          // Delete all empty cells at the beginning of this line.
          while (index - cellsPadded < view.length &&
                 view[index - cellsPadded] === 0) {
            cellsPadded--;
          }
          index--;
          lastBreak = index;
        } else if (
            view[index - cellsPadded - 1] !== 0 &&
            lastBreak % this.displaySize_.columns !== 0) {
          // If first cell is not empty, we need to move the whole word down
          // to this line and padd to previous line with 0's, from |lastBreak|
          // to index. The braille to text map is also updated. If lastBreak
          // is at the beginning of a line, that means the current word is
          // bigger than |this.displaySize_.columns| so we shouldn't wrap.
          for (let j = lastBreak + 1; j < index; j++) {
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
        if (view[index - cellsPadded] === 0) {
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
    const wrappedBrailleUint8Array = new Uint8Array(wrappedBrailleArray);
    this.wrappedBuffer_ = new ArrayBuffer(wrappedBrailleUint8Array.length);
    new Uint8Array(this.wrappedBuffer_).set(wrappedBrailleUint8Array);
    this.panToPosition_(targetPosition);
  }

  setCursor(startIndex: number, endIndex: number): void {
    this.cursor_ = {start: startIndex, end: endIndex};
  }

  getCursor(): CursorPosition {
    return this.cursor_;
  }

  /**
   * Refreshes the wrapped cursor given a mapping from an unwrapped index to a
   * wrapped index.
   */
  private maybeSetWrappedCursor_(unwrappedIndex: number, wrappedIndex: number):
      void {
    // We only care about the bounds of the index start/end.
    if (this.cursor_.start !== unwrappedIndex &&
        this.cursor_.end !== unwrappedIndex) {
      return;
    }
    if (this.cursor_.start === unwrappedIndex) {
      this.wrappedCursor_.start = wrappedIndex;
    } else if (this.cursor_.end === unwrappedIndex) {
      this.wrappedCursor_.end = wrappedIndex;
    }
  }

  /**
   * If possible, changes the viewport to a part of the line that follows
   * the current viewport.
   * @return true if the viewport was changed.
   */
  next(): boolean {
    const contentLength =
        this.panStrategyWrapped_ ? this.wrappedLineCount : this.fixedLineCount;
    const newStart = this.viewPort_.lastRow + 1;
    let newEnd;
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
  }

  /**
   * If possible, changes the viewport to a part of the line that precedes
   * the current viewport.
   * @return true if the viewport was changed.
   */
  previous(): boolean {
    const contentLength =
        this.panStrategyWrapped_ ? this.wrappedLineCount : this.fixedLineCount;
    if (this.viewPort_.firstRow > 0) {
      let newStart;
      let newEnd;
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
  }

  /**
   * Moves the viewport so that it overlaps a target position without taking
   * the current viewport position into consideration.
   * @param position Target position.
   */
  private panToPosition_(position: number): void {
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
  }
}

TestImportManager.exportForTesting(PanStrategy);
