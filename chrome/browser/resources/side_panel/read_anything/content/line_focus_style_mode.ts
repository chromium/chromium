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

  // Calculates and sets the height of the focus element in the model.
  abstract calculateHeight(): void;

  // Returns the new focal point Y position based on the given bounding rect.
  abstract getFocalPointForRect(bounds: DOMRect): number;

  // Clamps the line index to a valid range for this style.
  abstract clampLineIndex(index: number): number;

  // Returns the bounding rects for the top and bottom lines of the focus area.
  abstract getFocusWindowBounds(lines: DOMRect[], targetIndex: number):
      {topRect: DOMRect, bottomRect: DOMRect};

  // Returns where the center of the focus element should be in the focus area
  // outlined by the given rects.
  abstract getDesiredCenter(lines: DOMRect[], targetIndex: number): number;

  // Returns whether the focal point should be refreshed after a scroll.
  abstract shouldRefreshFocalPoint(oldHeight: number, oldTop: number): boolean;
}

// Style strategy for focusing on a single line with an underline effect.
export class LineFocusLineStyleMode extends LineFocusStyleMode {
  constructor(style: LineFocusStyle, model: LineFocusModel) {
    super(style, model);
  }

  calculateHeight(): void {
    this.model_.setTop(this.model_.getY());
    this.model_.setWindowHeight(0);
  }

  getFocalPointForRect(bounds: DOMRect): number {
    return bounds.bottom;
  }

  clampLineIndex(index: number): number {
    return index;
  }

  // The focus "window" is just the line itself.
  getFocusWindowBounds(lines: DOMRect[], targetIndex: number):
      {topRect: DOMRect, bottomRect: DOMRect} {
    assert(targetIndex >= 0 && targetIndex < lines.length);
    const rect = lines[targetIndex]!;
    return {
      topRect: rect,
      bottomRect: rect,
    };
  }

  getDesiredCenter(lines: DOMRect[], targetIndex: number): number {
    const {bottomRect} = this.getFocusWindowBounds(lines, targetIndex);
    return bottomRect.bottom;
  }

  shouldRefreshFocalPoint(_oldHeight: number, _oldTop: number): boolean {
    return true;
  }
}

// Style strategy for focusing on a window of one or more lines.
export class LineFocusWindowStyleMode extends LineFocusStyleMode {
  constructor(style: LineFocusStyle, model: LineFocusModel) {
    super(style, model);
  }

  calculateHeight(): void {
    const bounds = this.model_.getTextBounds();
    if (bounds.length === 0) {
      return;
    }

    const currentLineIndex = this.model_.getCurrentLineIndex() ??
        getRectIndexAtY(this.model_.getY(), this.model_.getTextBounds(), true);
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

  // The focus window spans multiple lines around the center index.
  getFocusWindowBounds(lines: DOMRect[], targetIndex: number):
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

  getDesiredCenter(lines: DOMRect[], targetIndex: number): number {
    const {topRect, bottomRect} = this.getFocusWindowBounds(lines, targetIndex);
    return (topRect.top + bottomRect.bottom) / 2;
  }

  shouldRefreshFocalPoint(oldHeight: number, oldTop: number): boolean {
    const heightDiff = Math.abs(oldHeight - this.model_.getWindowHeight());
    const topDiff = Math.abs(oldTop - this.model_.getTop());
    return heightDiff > WINDOW_DIFF_THRESHOLD ||
        topDiff > WINDOW_DIFF_THRESHOLD;
  }
}

// Style strategy for when line focus is disabled.
export class LineFocusNoneStyleMode extends LineFocusStyleMode {
  constructor(style: LineFocusStyle, model: LineFocusModel) {
    super(style, model);
  }

  calculateHeight(): void {
    // No-op.
  }

  getFocalPointForRect(_bounds: DOMRect): number {
    return 0;
  }

  clampLineIndex(_index: number): number {
    return 0;
  }

  getFocusWindowBounds(_lines: DOMRect[], _targetIndex: number):
      {topRect: DOMRect, bottomRect: DOMRect} {
    const emptyRect = new DOMRect();
    return {
      topRect: emptyRect,
      bottomRect: emptyRect,
    };
  }

  getDesiredCenter(_lines: DOMRect[], _targetIndex: number): number {
    return 0;
  }

  shouldRefreshFocalPoint(_oldHeight: number, _oldTop: number): boolean {
    return false;
  }
}
