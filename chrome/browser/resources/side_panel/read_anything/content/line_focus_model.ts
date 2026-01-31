// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LineFocusMovement, LineFocusStyle} from './read_anything_types.js';

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

  // The current line focus mode.
  private currentLineFocusStyle_: LineFocusStyle = LineFocusStyle.OFF;
  private currentLineFocusMovement_: LineFocusMovement =
      LineFocusMovement.STATIC;
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

  getCurrentLineFocusStyle(): LineFocusStyle {
    return this.currentLineFocusStyle_;
  }

  setCurrentLineFocusStyle(style: LineFocusStyle): void {
    this.currentLineFocusStyle_ = style;
  }

  getCurrentLineFocusMovement(): LineFocusMovement {
    return this.currentLineFocusMovement_;
  }

  setCurrentLineFocusMovement(movement: LineFocusMovement): void {
    this.currentLineFocusMovement_ = movement;
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
