// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {LineFocusMovement} from '../content/read_anything_types.js';

// Base class for line focus movement strategies.
export abstract class LineFocusMoveMode {
  // Returns the movement type of this movement strategy.
  abstract getMovement(): LineFocusMovement;
}

// Movement strategy where the focus element stays centered in the view,
// scrolling the view when needed.
export class LineFocusStaticMoveMode extends LineFocusMoveMode {
  getMovement(): LineFocusMovement {
    return LineFocusMovement.STATIC;
  }
}

// Movement strategy where the focus element follows the mouse cursor.
export class LineFocusCursorMoveMode extends LineFocusMoveMode {
  getMovement(): LineFocusMovement {
    return LineFocusMovement.CURSOR;
  }
}

// Movement strategy for when line focus is disabled.
export class LineFocusNoneMoveMode extends LineFocusMoveMode {
  constructor(private movement_: LineFocusMovement) {
    super();
  }

  getMovement(): LineFocusMovement {
    return this.movement_;
  }
}
