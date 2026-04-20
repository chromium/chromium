// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assert} from '//resources/js/assert.js';

import type {LineFocusMoveMode} from './line_focus_move_mode.js';
import type {LineFocusStyleMode} from './line_focus_style_mode.js';
import {LineFocusStyle} from './read_anything_types.js';

export class LineFocusModel {
  // The min y position allowed for the line focus element.
  private minY_: number = 0;
  // The max y position allowed for the line focus element.
  private maxY_: number = 0;

  // The y position of the line focus element. If the element is a line, this is
  // where the top of the line should go. If the element is a window, this is
  // where the center of the window should go.
  private y_: number = 0;

  // The top value of the line focus element. If the element is a line, this is
  // equal to y_. If the element is a window, this is the top of the window.
  private top_: number = 0;

  // The height of the line focus element if it is a window. This should be 0
  // otherwise.
  private windowHeight_: number = 0;

  // The current style and move mode strategy instances.
  private currentStyleMode_?: LineFocusStyleMode;
  private currentMoveMode_?: LineFocusMoveMode;

  // The last line focus mode that was used when it was on. Used for toggling on
  // line focus with the last used line focus mode.
  private lastEnabledLineFocusStyle_: LineFocusStyle =
      LineFocusStyle.defaultValue();

  // The index of the current line in textLineBottoms_ being focused. Null if
  // line focus is moving continuously with the mouse instead of discretely.
  private currentLineIndex_: number|null = null;
  // The precomputed bounding boxes of each line of text.
  private textBounds_: DOMRect[] = [];

  // Used for logging line focus session scroll distance.
  private lastScrollTop_: number = 0;

  // Whether line focus caused the latest scroll action.
  private initiatedScroll_: boolean = false;

  getMinY(): number {
    return this.minY_;
  }

  setMinY(y: number): void {
    this.minY_ = y;
  }

  getMaxY(): number {
    return this.maxY_;
  }

  setMaxY(y: number): void {
    this.maxY_ = y;
  }

  getY(): number {
    return this.y_;
  }

  setY(y: number): void {
    this.y_ = y;
  }

  getTop(): number {
    return this.top_;
  }

  setTop(top: number): void {
    this.top_ = top;
  }

  getWindowHeight(): number {
    return this.windowHeight_;
  }

  setWindowHeight(height: number): void {
    this.windowHeight_ = height;
  }

  getCurrentStyleMode(): LineFocusStyleMode {
    assert(this.currentStyleMode_, 'You must set the default style mode!');
    return this.currentStyleMode_;
  }

  setCurrentStyleMode(styleMode: LineFocusStyleMode): void {
    this.currentStyleMode_ = styleMode;
  }

  getCurrentMoveMode(): LineFocusMoveMode {
    assert(this.currentMoveMode_, 'You must set the default move mode!');
    return this.currentMoveMode_;
  }

  setCurrentMoveMode(moveMode: LineFocusMoveMode): void {
    this.currentMoveMode_ = moveMode;
  }

  getLastEnabledLineFocusStyle(): LineFocusStyle {
    return this.lastEnabledLineFocusStyle_;
  }

  setLastEnabledLineFocusStyle(style: LineFocusStyle): void {
    this.lastEnabledLineFocusStyle_ = style;
  }

  getCurrentLineIndex(): number|null {
    return this.currentLineIndex_;
  }

  setCurrentLineIndex(index: number|null): void {
    this.currentLineIndex_ = index;
  }

  getTextBounds(): DOMRect[] {
    return this.textBounds_;
  }

  setTextBounds(bounds: DOMRect[]): void {
    this.textBounds_ = bounds;
  }

  getLastScrollTop(): number {
    return this.lastScrollTop_;
  }

  setLastScrollTop(top: number): void {
    this.lastScrollTop_ = top;
  }

  getInitiatedScroll(): boolean {
    return this.initiatedScroll_;
  }

  setInitiatedScroll(initiated: boolean) {
    this.initiatedScroll_ = initiated;
  }
}
