// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assert} from '//resources/js/assert.js';

import {type LineFocus, LineFocusType} from '../content/read_anything_types.js';

import {LineFocusModel} from './line_focus_model.js';

export interface LineFocusListener {
  onLineFocusMove(): void;
}

// Handles the business logic for managing the line focus feature.
export class LineFocusController {
  private readonly listeners_: LineFocusListener[] = [];
  private model_: LineFocusModel = new LineFocusModel();

  getTop(): number {
    return this.model_.getTop();
  }

  getHeight(): number|null {
    return (this.getCurrentLineFocusType() === LineFocusType.WINDOW) ?
        this.model_.getWindowHeight() :
        null;
  }

  getCurrentLineFocusType(): LineFocusType|undefined {
    return this.model_.getCurrentLineFocus()?.type;
  }

  addListener(listener: LineFocusListener) {
    this.listeners_.push(listener);
  }

  isEnabled(): boolean {
    return (
        chrome.readingMode.isLineFocusEnabled &&
        !!this.model_.getCurrentLineFocus() &&
        this.getCurrentLineFocusType() !== LineFocusType.NONE);
  }

  onMouseMove(y: number) {
    if (this.isEnabled()) {
      this.setY_(Math.max(this.model_.getMinY(), y));
    }
  }

  onTextLocationsChange(container: HTMLElement, height: number) {
    if (this.isEnabled()) {
      this.calculateNewPositions_(container, height);
      this.setY_(this.model_.getY());
    }
  }

  onLineFocusChange(
      lineFocus: LineFocus, container: HTMLElement, height: number) {
    this.model_.setCurrentLineFocus(lineFocus);
    if (lineFocus.type === LineFocusType.NONE) {
      this.model_.setMinY(0);
      this.model_.setMaxY(0);
      this.model_.setY(0);
      this.model_.setTop(0);
      this.model_.setWindowHeight(0);
      this.model_.setDefaultWindowHeight(0);
    } else {
      this.calculateNewPositions_(container, height);
      this.setY_(Math.max(this.model_.getMinY(), this.model_.getY()));
    }
  }

  private setY_(y: number) {
    const oldY = this.model_.getY();
    this.model_.setY(y);

    const oldHeight = this.model_.getWindowHeight();
    this.calculateHeight_();

    if (oldY === this.model_.getY() &&
        oldHeight === this.model_.getWindowHeight()) {
      return;
    }

    this.listeners_.forEach(l => l.onLineFocusMove());
  }

  private calculateHeight_() {
    if (!this.model_.getCurrentLineFocus()) {
      return;
    }

    if (this.getCurrentLineFocusType() === LineFocusType.LINE) {
      this.model_.setTop(this.model_.getY());
      return;
    }

    // If the line focus is a window being controlled with smooth mouse movement
    // then use the default window height.
    if (this.getCurrentLineFocusType() === LineFocusType.WINDOW) {
      this.model_.setWindowHeight(this.model_.getDefaultWindowHeight());
      this.model_.setTop(Math.max(
          this.model_.getMinY(),
          this.model_.getY() - this.model_.getWindowHeight()));
    }
  }

  private calculateNewPositions_(container: HTMLElement, height: number) {
    const currentLineFocus = this.model_.getCurrentLineFocus();
    assert(!!currentLineFocus);
    this.model_.setMinY(container.offsetTop);
    this.model_.setMaxY(this.model_.getMinY() + height);

    const range = document.createRange();
    range.selectNodeContents(container);
    const visibleLines =
        Array.from(range.getClientRects())
            .map(rect => rect.bottom)
            .filter(
                y => y >= this.model_.getMinY() && y <= this.model_.getMaxY());
    const uniqueLines = Array.from(new Set(visibleLines));
    this.model_.setDefaultWindowHeight(
        currentLineFocus.lines * (uniqueLines.at(-1)! - uniqueLines.at(0)!) /
        (uniqueLines.length - 1));
  }

  static getInstance(): LineFocusController {
    return instance || (instance = new LineFocusController());
  }

  static setInstance(obj: LineFocusController) {
    instance = obj;
  }
}

let instance: LineFocusController|null = null;
