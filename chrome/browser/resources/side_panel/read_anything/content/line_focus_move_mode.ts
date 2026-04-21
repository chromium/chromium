// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LineFocusModel} from './line_focus_model.js';
import type {LineFocusStyleMode} from './line_focus_style_mode.js';
import {LineFocusMovement} from './read_anything_types.js';

// Interface for communicating notifications back to the main
// LineFocusController.
export interface MoveModeDelegate {
  // Notifies that a line focus session has ended.
  onSessionEnd(): void;
}

// Base class for line focus movement strategies.
export abstract class LineFocusMoveMode {
  constructor(
      protected model_: LineFocusModel,
      protected styleMode_: LineFocusStyleMode,
      protected delegate_: MoveModeDelegate) {}

  // Returns the movement type of this movement strategy.
  abstract getMovement(): LineFocusMovement;

  // Called when this movement mode becomes the active strategy.
  abstract onActivated(): void;

  // Common setup logic for when a movement mode that enables line focus is
  // activated.
  protected setupEnabledMode(): void {
    this.model_.setLastEnabledLineFocusStyle(this.styleMode_.getStyle());
    if (!this.model_.isSessionActive()) {
      chrome.readingMode.startLineFocusSession();
      this.model_.setSessionActive(true);
    }
  }
}

// Movement strategy where the focus element stays centered in the view,
// scrolling the view when needed.
export class LineFocusStaticMoveMode extends LineFocusMoveMode {
  getMovement(): LineFocusMovement {
    return LineFocusMovement.STATIC;
  }

  onActivated(): void {
    this.setupEnabledMode();
  }
}

// Movement strategy where the focus element follows the mouse cursor.
export class LineFocusCursorMoveMode extends LineFocusMoveMode {
  getMovement(): LineFocusMovement {
    return LineFocusMovement.CURSOR;
  }

  onActivated(): void {
    this.setupEnabledMode();
  }
}

// Movement strategy for when line focus is disabled.
export class LineFocusNoneMoveMode extends LineFocusMoveMode {
  constructor(
      model: LineFocusModel, styleMode: LineFocusStyleMode,
      delegate: MoveModeDelegate, private movement_: LineFocusMovement) {
    super(model, styleMode, delegate);
  }

  getMovement(): LineFocusMovement {
    return this.movement_;
  }

  onActivated(): void {
    if (this.model_.isSessionActive()) {
      this.delegate_.onSessionEnd();
    }
    this.model_.reset();
  }
}
