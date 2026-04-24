// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import {getRectIndexAtY} from '../shared/dom_queries.js';

import type {LineFocusModel} from './line_focus_model.js';
import type {LineFocusStyle} from './read_anything_types.js';

// Used to prevent microadjustments of the line focus window when adjusting to
// new line heights as it can be distracting for no functional difference.
// Determined by experimentation and should be tweaked as needed.
export const WINDOW_DIFF_THRESHOLD = 5;

// Base class for the visual style of the line focus element (e.g. a
// single line vs a larger window).
export abstract class LineFocusStyleMode {
  constructor(
      protected style_: LineFocusStyle, protected model_: LineFocusModel) {}

  // Returns the style of this style strategy.
  getStyle(): LineFocusStyle {
    return this.style_;
  }

  // Returns how far from the center the current focal point is if it is outside
  // the viewport. Returns 0 otherwise.
  getOffScreenDiff(targetIndex: number): number {
    const textBounds = this.model_.getTextBounds();
    const {topRect, bottomRect} =
        this.getFocusWindowBounds(textBounds, targetIndex);
    if (bottomRect.bottom > this.model_.getMaxY() ||
        topRect.top < this.model_.getMinY()) {
      const center = this.getFocalPoint(topRect, bottomRect);
      return center - (this.model_.getMaxY() / 2);
    }
    return 0;
  }

  // Returns where the center of the focus element should be in the focus area
  // outlined by the given rects.
  getDesiredCenter(targetIndex: number): number {
    const textBounds = this.model_.getTextBounds();
    const {topRect, bottomRect} =
        this.getFocusWindowBounds(textBounds, targetIndex);
    return this.getFocalPoint(topRect, bottomRect);
  }

  // Updates the top position and height of the focus element in the model.
  abstract updateFocusBounds(): void;

  // Returns the new focal point Y position based on the given bounding rect.
  abstract getFocalPointForRect(bounds: DOMRect): number;

  // Returns the index of the last line of the focus area given the index of the
  // center of the focus area.
  abstract getBottomIndex(focalIndex: number): number;

  // Clamps the line index to a valid range for this style.
  abstract clampLineIndex(index: number): number;

  // Returns true if after a line-focus-initiated scroll, this focus area
  // calculates a change in position or height.
  abstract updateAfterScroll(): boolean;

  // Returns the bounding rects for the top and bottom lines of the focus area.
  protected abstract getFocusWindowBounds(
      lines: DOMRect[],
      targetIndex: number): {topRect: DOMRect, bottomRect: DOMRect};

  // Returns the desired focal point of the given rects for this style mode.
  protected abstract getFocalPoint(topRect: DOMRect, bottomRect: DOMRect):
      number;
}

// Style strategy for focusing on a single line with an underline effect.
export class LineFocusLineStyleMode extends LineFocusStyleMode {
  constructor(style: LineFocusStyle, model: LineFocusModel) {
    super(style, model);
  }

  updateFocusBounds(): void {
    this.model_.setTop(this.model_.getFocalPoint());
    this.model_.setWindowHeight(0);
  }

  getFocalPointForRect(bounds: DOMRect): number {
    return bounds.bottom;
  }

  getBottomIndex(focalIndex: number): number {
    return focalIndex;
  }

  clampLineIndex(index: number): number {
    return index;
  }

  updateAfterScroll(): boolean {
    // No need to update the focus area in line mode since the size of the line
    // does not affect the underline size.
    return false;
  }

  // The focus "window" is just the line itself.
  protected getFocusWindowBounds(lines: DOMRect[], targetIndex: number):
      {topRect: DOMRect, bottomRect: DOMRect} {
    assert(targetIndex >= 0 && targetIndex < lines.length);
    const rect = lines[targetIndex]!;
    return {
      topRect: rect,
      bottomRect: rect,
    };
  }

  protected getFocalPoint(_topRect: DOMRect, bottomRect: DOMRect) {
    return bottomRect.bottom;
  }
}

// Style strategy for focusing on a window of one or more lines.
export class LineFocusWindowStyleMode extends LineFocusStyleMode {
  constructor(style: LineFocusStyle, model: LineFocusModel) {
    super(style, model);
  }

  updateFocusBounds(): void {
    const bounds = this.model_.getTextBounds();
    if (bounds.length === 0) {
      return;
    }

    const currentLineIndex = this.model_.getCurrentLineIndex() ??
        getRectIndexAtY(this.model_.getFocalPoint(),
                        this.model_.getTextBounds(), true);
    const numLines = this.style_.lines;
    const topIndex = currentLineIndex - ((numLines - 1) / 2);
    const maxTopIndex = bounds.length - numLines;

    // Find first line that is visible.
    const findIndexResult =
        bounds.findIndex(rect => rect.top >= this.model_.getMinY());
    const minTopIndex = findIndexResult === -1 ? 0 : findIndexResult;

    const validTopIndex =
        Math.max(minTopIndex, Math.min(maxTopIndex, Math.floor(topIndex)));
    const topLine = bounds[validTopIndex];
    assert(topLine);
    this.model_.setTop(topLine.top);

    const bottomIndex = (validTopIndex + numLines - 1);
    const bottomRect = bounds[bottomIndex];
    const bottom = bottomRect ? bottomRect.bottom : bounds.at(-1)!.bottom;
    this.model_.setWindowHeight(bottom - this.model_.getTop());
  }

  getFocalPointForRect(bounds: DOMRect): number {
    return (bounds.top + bounds.bottom) / 2;
  }

  getBottomIndex(focalIndex: number): number {
    return focalIndex + ((this.style_.lines - 1) / 2);
  }

  // The bottom of the window should not go below the last line in the content
  // panel and the top should not go above the first line of the panel.
  clampLineIndex(index: number): number {
    const bounds = this.model_.getTextBounds();
    if (bounds.length === 0) {
      return 0;
    }
    const numLines = this.style_.lines;
    const offset = Math.floor((numLines - 1) / 2);
    const maxIndex = bounds.length - 1;
    const minIndex = maxIndex - offset;
    const clampedIndex = Math.max(offset, Math.min(minIndex, index));

    // Fallback safety to return a valid index in [0, maxIndex] even when the
    // requested window size is larger than the available lines.
    return Math.max(0, Math.min(maxIndex, clampedIndex));
  }

  updateAfterScroll(): boolean {
    if (this.style_.lines > 1) {
      return false;
    }

    // Always adapt the single line focus window height to the current text line
    // height, otherwise the text line might be much bigger than the focus area.
    // This isn't needed for larger window sizes.
    const oldHeight = this.model_.getWindowHeight();
    const oldTop = this.model_.getTop();
    this.updateFocusBounds();
    const heightDiff = Math.abs(oldHeight - this.model_.getWindowHeight());
    const topDiff = Math.abs(oldTop - this.model_.getTop());
    return heightDiff > WINDOW_DIFF_THRESHOLD ||
        topDiff > WINDOW_DIFF_THRESHOLD;
  }

  // The focus window spans multiple lines around the center index.
  protected getFocusWindowBounds(lines: DOMRect[], targetIndex: number):
      {topRect: DOMRect, bottomRect: DOMRect} {
    const numLines = this.style_.lines;
    const offset = Math.floor((numLines - 1) / 2);
    const topIndex = Math.max(0, targetIndex - offset);
    const bottomIndex = Math.min(lines.length - 1, topIndex + numLines - 1);
    const topRect = lines[Math.floor(topIndex)]!;
    const bottomRect = lines[Math.floor(bottomIndex)]!;

    return {
      topRect,
      bottomRect,
    };
  }

  protected getFocalPoint(topRect: DOMRect, bottomRect: DOMRect) {
    return (topRect.top + bottomRect.bottom) / 2;
  }
}

// Style strategy for when line focus is disabled.
export class LineFocusNoneStyleMode extends LineFocusStyleMode {
  constructor(style: LineFocusStyle, model: LineFocusModel) {
    super(style, model);
  }

  updateFocusBounds(): void {
    // No-op.
  }

  getFocalPointForRect(_bounds: DOMRect): number {
    // The focus area should never be drawn when line focus is disabled.
    return 0;
  }

  getBottomIndex(_focalIndex: number): number {
    // The focus area should never be drawn when line focus is disabled.
    return 0;
  }

  clampLineIndex(_index: number): number {
    // The focus area should never be drawn when line focus is disabled.
    return 0;
  }

  updateAfterScroll(): boolean {
    // Do nothing when line focus is disabled.
    return false;
  }

  protected getFocusWindowBounds(_lines: DOMRect[], _targetIndex: number):
      {topRect: DOMRect, bottomRect: DOMRect} {
    // The focus area should never be drawn when line focus is disabled.
    const emptyRect = new DOMRect();
    return {
      topRect: emptyRect,
      bottomRect: emptyRect,
    };
  }

  protected getFocalPoint(_topRect: DOMRect, _bottomRect: DOMRect) {
    // The focus area should never be drawn when line focus is disabled.
    return 0;
  }
}
