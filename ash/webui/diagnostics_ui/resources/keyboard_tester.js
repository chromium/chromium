// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {KeyboardInfo} from './diagnostics_types.js'

/**
 * @fileoverview
 * 'keyboard-tester' displays a tester UI for a keyboard.
 */

Polymer({
  is: 'keyboard-tester',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * The keyboard being tested, or null if none is being tested at the moment.
     * @type {?KeyboardInfo}
     */
    keyboard: KeyboardInfo,
  },

  /** Shows the tester's dialog. */
  show() {
    this.$.dialog.showModal();
  },

  /**
   * Returns whether the tester is currently open.
   * @return {boolean}
   */
  isOpen() {
    return this.$.dialog.open;
  },
});
