// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * Helper functions for an input with timeout.
 * @polymerBehavior
 */
export const InputBehavior = {
  properties: {
    /** @private {?string} */
    lastValue_: {
      type: String,
      value: '',
    },
  },

  /**
   * Timeout used to delay processing of the input, in ms.
   * @private {?number}
   */
  timeout_: null,

  /** @override */
  ready() {
    this.getInput().addEventListener('input', () => this.resetTimeout_());
    this.getInput().addEventListener(
        'keydown', e => this.onKeyDown_(/** @type {!KeyboardEvent} */ (e)));
  },

  /**
   * @return {(!CrInputElement|!HTMLInputElement)} The cr-input or input
   *     element the behavior should use. Should be overridden by elements
   *     using this behavior.
   */
  getInput() {},

  /**
   * @return {number} The delay to use for the timeout, in ms. Elements using
   *     this behavior must set this delay as data-timeout-delay on the input
   *     element returned by getInput().
   * @private
   */
  getTimeoutDelayMs_() {
    const delay = parseInt(
        /** @type {{timeoutDelay: number}} */ (this.getInput().dataset)
            .timeoutDelay,
        10);
    assert(!Number.isNaN(delay));
    return delay;
  },

  /**
   * Called when a key is pressed on the input.
   * @param {!KeyboardEvent} event Contains the key that was pressed.
   * @private
   */
  onKeyDown_(event) {
    if (event.code !== 'Enter' && event.code !== 'Tab') {
      return;
    }

    this.resetAndUpdate();
  },

  /**
   * Called when a input event occurs on the textfield. Starts an input
   * timeout.
   * @private
   */
  resetTimeout_() {
    if (this.timeout_) {
      clearTimeout(this.timeout_);
    }
    this.timeout_ =
        setTimeout(() => this.onTimeout_(), this.getTimeoutDelayMs_());
  },

  /**
   * Called after a timeout after user input into the textfield.
   * @private
   */
  onTimeout_() {
    this.timeout_ = null;
    const value = this.getInput().value;
    if (this.lastValue_ !== value) {
      this.lastValue_ = value;
      this.fire('input-change', value);
    }
  },

  // Resets the lastValue_ so that future inputs trigger a change event.
  resetString() {
    this.lastValue_ = null;
  },

  // Called to clear the timeout and update the value.
  resetAndUpdate() {
    if (this.timeout_) {
      clearTimeout(this.timeout_);
    }
    this.onTimeout_();
  },
};

/** @interface */
export class InputBehaviorInterface {
  /**
   * @return {(!CrInputElement|!HTMLInputElement)} The cr-input or input
   *     element the behavior should use. Should be overridden by elements
   *     using this behavior.
   */
  getInput() {}

  // Resets the lastValue_ so that future inputs trigger a change event.
  resetString() {}

  // Called to clear the timeout and update the value.
  resetAndUpdate() {}
}
