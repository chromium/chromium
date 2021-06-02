// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

/**
 * Manages focus restoration for modal dialogs. After the final dialog in a
 * stack is closed, restores focus to the element which was focused when the
 * first dialog was opened.
 */
export class DialogFocusManager {
  constructor() {
    /** @private {HTMLElement} */
    this.previousFocusElement_ = null;

    /** @private {Set<HTMLDialogElement>} */
    this.dialogs_ = new Set();
  }

  /**
   * @param {HTMLDialogElement} dialog
   * @param {function()=} showFn
   */
  showDialog(dialog, showFn) {
    if (!showFn) {
      showFn = function() {
        dialog.showModal();
      };
    }

    // Update the focus if there are no open dialogs or if this is the only
    // dialog and it's getting reshown.
    if (!this.dialogs_.size ||
        (this.dialogs_.has(dialog) && this.dialogs_.size === 1)) {
      this.updatePreviousFocus_();
    }

    if (!this.dialogs_.has(dialog)) {
      dialog.addEventListener('close', this.getCloseListener_(dialog));
      this.dialogs_.add(dialog);
    }

    showFn();
  }

  /**
   * @return {boolean} True if the document currently has an open dialog.
   */
  hasOpenDialog() {
    return this.dialogs_.size > 0;
  }

  /**
   * Clears the stored focus element, so that focus does not restore when all
   * dialogs are closed.
   */
  clearFocus() {
    this.previousFocusElement_ = null;
  }

  /** @private */
  updatePreviousFocus_() {
    this.previousFocusElement_ = this.getFocusedElement_();
  }

  /**
   * @return {HTMLElement}
   * @private
   */
  getFocusedElement_() {
    let focus = document.activeElement;
    while (focus.root && focus.root.activeElement) {
      focus = focus.root.activeElement;
    }

    return focus;
  }

  /**
   * @param {HTMLDialogElement} dialog
   * @return {function(Event)}
   * @private
   */
  getCloseListener_(dialog) {
    const closeListener = (e) => {
      // If the dialog is open, then it got reshown immediately and we
      // shouldn't clear it until it is closed again.
      if (dialog.open) {
        return;
      }

      assert(this.dialogs_.delete(dialog));
      // Focus the originally focused element if there are no more dialogs.
      if (!this.hasOpenDialog() && this.previousFocusElement_) {
        this.previousFocusElement_.focus();
      }

      dialog.removeEventListener('close', closeListener);
    };

    return closeListener;
  }

  /** @return {!DialogFocusManager} */
  static getInstance() {
    return instance || (instance = new DialogFocusManager());
  }

  /** @param {?DialogFocusManager} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?DialogFocusManager} */
let instance = null;
