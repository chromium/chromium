// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assert} from '//resources/js/assert.js';

import {getRectIndexAtY} from '../shared/dom_queries.js';
import {calculateTextBounds} from '../shared/rect_calculations.js';

import type {LineFocusModel} from './line_focus_model.js';
import type {LineFocusStyleMode} from './line_focus_style_mode.js';
import {LineFocusMovement} from './read_anything_types.js';

// Used to prevent microadjustments of the line focus window when adjusting to
// new line heights as it can be distracting for no functional difference.
// Determined by experimentation and should be tweaked as needed.
const SCROLL_THRESHOLD = 10;

// Interface for communicating notifications back to the main
// LineFocusController.
export interface MoveModeDelegate {
  // Notifies that the focus element has moved.
  notifyMove(): void;

  // Notifies that the view needs to scroll.
  notifyScroll(scrollDiff: number, instant?: boolean): void;

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
  abstract onActivated(container: HTMLElement, height: number): void;

  // Updates the focal point Y position or scrolls the view to the given
  // rect, depending on the movement strategy.
  abstract moveToRect(rect: DOMRect): void;

  setFocalPoint(focalPointY: number, quietly: boolean = false): void {
    this.model_.setFocalPoint(focalPointY);
    this.styleMode_.calculateHeight();
    if (!quietly) {
      this.delegate_.notifyMove();
    }
  }

  scrollToCenter(
      lines: DOMRect[], targetIndex: number, instant: boolean = false) {
    const desiredCenter = this.styleMode_.getDesiredCenter(lines, targetIndex);
    const scrollDiff = desiredCenter - (this.model_.getMaxY() / 2);
    this.scroll(scrollDiff, instant);
  }

  scroll(scrollDiff: number, instant?: boolean): void {
    if (Math.abs(scrollDiff) < SCROLL_THRESHOLD) {
      return;
    }
    this.model_.setInitiatedScroll(true);
    this.delegate_.notifyScroll(scrollDiff, instant);
  }

  initializeSnapIndex(lines: DOMRect[], isForward: boolean) {
    const rawIndex = getRectIndexAtY(
        this.model_.getFocalPoint(), this.model_.getTextBounds(), isForward);
    const safeIndex = this.styleMode_.clampLineIndex(rawIndex);

    this.model_.setCurrentLineIndex(safeIndex);
    assert(safeIndex < lines.length);
    this.moveToRect(lines[safeIndex]!);
  }

  updatePositions(container: HTMLElement, height: number): void {
    const {minY, maxY, bounds} = calculateTextBounds(container, height);
    this.model_.setMinY(minY);
    this.model_.setMaxY(maxY);
    this.model_.setTextBounds(bounds);
  }

  // Common setup logic for when a movement mode that enables line focus is
  // activated.
  protected setupEnabledMode(container: HTMLElement, height: number): void {
    this.model_.setLastEnabledLineFocusStyle(this.styleMode_.getStyle());
    if (!this.model_.isSessionActive()) {
      chrome.readingMode.startLineFocusSession();
      this.model_.setSessionActive(true);
    }
    this.updatePositions(container, height);
  }
}

// Movement strategy where the focus element stays centered in the view,
// scrolling the view when needed.
export class LineFocusStaticMoveMode extends LineFocusMoveMode {
  getMovement(): LineFocusMovement {
    return LineFocusMovement.STATIC;
  }

  onActivated(container: HTMLElement, height: number): void {
    this.setupEnabledMode(container, height);
  }

  moveToRect(rect: DOMRect): void {
    const focalPoint = this.styleMode_.getFocalPointForRect(rect);
    const scrollDiff = focalPoint - this.model_.getFocalPoint();
    this.scroll(scrollDiff);
  }
}

// Movement strategy where the focus element follows the mouse cursor.
export class LineFocusCursorMoveMode extends LineFocusMoveMode {
  getMovement(): LineFocusMovement {
    return LineFocusMovement.CURSOR;
  }

  onActivated(container: HTMLElement, height: number): void {
    this.setupEnabledMode(container, height);
  }

  moveToRect(rect: DOMRect): void {
    const focalPoint = this.styleMode_.getFocalPointForRect(rect);
    this.setFocalPoint(focalPoint);
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

  onActivated(_container: HTMLElement, _height: number): void {
    if (this.model_.isSessionActive()) {
      this.delegate_.onSessionEnd();
    }
    this.model_.reset();
  }

  moveToRect(_rect: DOMRect): void {}
}
