// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-display-overscan-dialog' is the dialog for display overscan
 * adjustments.
 */

Polymer({
  is: 'settings-display-overscan-dialog',

  properties: {
    /** Id of the display for which overscan is being applied (or empty). */
    displayId: {
      type: String,
      notify: true,
      observer: 'displayIdChanged_',
    },

    /** Set to true once changes are saved to avoid a reset/cancel on close. */
    committed_: Boolean,
  },

  /**
   * Keyboard event handler for overscan adjustments.
   * @type {?function(!Event)}
   * @private
   */
  keyHandler_: null,

  open() {
    this.keyHandler_ = this.handleKeyEvent_.bind(this);
    // We need to attach the event listener to |window|, not |this| so that
    // changing focus does not prevent key events from occurring.
    window.addEventListener('keydown', this.keyHandler_);
    this.committed_ = false;
    this.$.dialog.showModal();
    // Don't focus 'reset' by default. 'Tab' will focus 'OK'.
    this.$$('#reset').blur();
  },

  close() {
    window.removeEventListener('keydown', this.keyHandler_);

    this.displayId = '';  // Will trigger displayIdChanged_.

    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  },

  /** @private */
  displayIdChanged_(newValue, oldValue) {
    if (oldValue && !this.committed_) {
      settings.getDisplayApi().overscanCalibrationReset(oldValue);
      settings.getDisplayApi().overscanCalibrationComplete(oldValue);
    }
    if (!newValue) {
      return;
    }
    this.committed_ = false;
    settings.getDisplayApi().overscanCalibrationStart(newValue);
  },

  /** @private */
  onResetTap_() {
    settings.getDisplayApi().overscanCalibrationReset(this.displayId);
  },

  /** @private */
  onSaveTap_() {
    settings.getDisplayApi().overscanCalibrationComplete(this.displayId);
    this.committed_ = true;
    this.close();
  },

  /**
   * @param {!Event} event
   * @private
   */
  handleKeyEvent_(event) {
    if (event.altKey || event.ctrlKey || event.metaKey) {
      return;
    }
    switch (event.keyCode) {
      case 37:  // left arrow
        if (event.shiftKey) {
          this.move_(-1, 0);
        } else {
          this.resize_(1, 0);
        }
        break;
      case 38:  // up arrow
        if (event.shiftKey) {
          this.move_(0, -1);
        } else {
          this.resize_(0, -1);
        }
        break;
      case 39:  // right arrow
        if (event.shiftKey) {
          this.move_(1, 0);
        } else {
          this.resize_(-1, 0);
        }
        break;
      case 40:  // down arrow
        if (event.shiftKey) {
          this.move_(0, 1);
        } else {
          this.resize_(0, 1);
        }
        break;
      default:
        // Allow unhandled key events to propagate.
        return;
    }
    event.preventDefault();
  },

  /**
   * @param {number} x
   * @param {number} y
   * @private
   */
  move_(x, y) {
    /** @type {!chrome.system.display.Insets} */ const delta = {
      left: x,
      top: y,
      right: x ? -x : 0,  // negating 0 will produce a double.
      bottom: y ? -y : 0,
    };
    settings.getDisplayApi().overscanCalibrationAdjust(this.displayId, delta);
  },

  /**
   * @param {number} x
   * @param {number} y
   * @private
   */
  resize_(x, y) {
    /** @type {!chrome.system.display.Insets} */ const delta = {
      left: x,
      top: y,
      right: x,
      bottom: y,
    };
    settings.getDisplayApi().overscanCalibrationAdjust(this.displayId, delta);
  }
});
