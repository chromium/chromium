// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LineFocusStyle} from './read_anything_types.js';

// Base class for the visual style of the line focus element (e.g. a
// single line vs a larger window).
export abstract class LineFocusStyleMode {
  constructor(protected style_: LineFocusStyle) {}

  // Returns the style of this style strategy.
  getStyle(): LineFocusStyle {
    return this.style_;
  }
}

// Style strategy for focusing on a single line with an underline effect.
export class LineFocusLineStyleMode extends LineFocusStyleMode {
  constructor(style: LineFocusStyle) {
    super(style);
  }
}

// Style strategy for focusing on a window of one or more lines.
export class LineFocusWindowStyleMode extends LineFocusStyleMode {
  constructor(style: LineFocusStyle) {
    super(style);
  }
}

// Style strategy for when line focus is disabled.
export class LineFocusNoneStyleMode extends LineFocusStyleMode {
  constructor(style: LineFocusStyle) {
    super(style);
  }
}
