// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LineFocus} from './read_anything_types.js';

export class LineFocusModel {
  // The min y position allowed for the line focus element.
  private minY_: number = 0;
  // The max y position allowed for the line focus element.
  private maxY_: number = 0;

  // The y position of the line focus element. If the element is a line, this is
  // where the top of the line should go. If the element is a window, this is
  // where the bottom of the window should go.
  private y_: number = 0;

  // The top value of the line focus element. If the element is a line, this is
  // equal to y_. If the element is a window, this is the top of the window.
  private top_: number = 0;

  // The height of the line focus element if it is a window. This should be 0
  // otherwise.
  private windowHeight_: number = 0;
  // The default window height to use if the window is being moved with the
  // mouse. Since mouse movement has to be continuous, we can't measure the
  // exact window height based on location.
  private defaultWindowHeight_: number = 0;

  // The current line focus mode.
  private currentLineFocus_?: LineFocus;

  // The index of the current line in textLineBottoms_ being focused. Null if
  // line focus is moving continuously with the mouse instead of discretely.
  private currentLineIndex_: number|null = null;
  // The precomputed bottom positions of each line of text.
  private textLineBottoms_: number[] = [];

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

  getDefaultWindowHeight(): number {
    return this.defaultWindowHeight_;
  }

  setDefaultWindowHeight(height: number): void {
    this.defaultWindowHeight_ = height;
  }

  getCurrentLineFocus(): LineFocus|null {
    return this.currentLineFocus_ || null;
  }

  setCurrentLineFocus(lineFocus: LineFocus): void {
    this.currentLineFocus_ = lineFocus;
  }

  getCurrentLineIndex(): number|null {
    return this.currentLineIndex_;
  }

  setCurrentLineIndex(index: number|null): void {
    this.currentLineIndex_ = index;
  }

  getTextLineBottoms(): number[] {
    return this.textLineBottoms_;
  }

  setTextLineBottoms(bottoms: number[]): void {
    this.textLineBottoms_ = bottoms;
  }
}
