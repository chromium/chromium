// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/keyboard_diagram.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {KeyboardInfo, MechanicalLayout, PhysicalLayout} from './diagnostics_types.js'

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

    /** @private */
    layoutIsKnown_: {
      type: Boolean,
      computed: 'computeLayoutIsKnown_(keyboard)',
    },
  },

  /**
   * @param {?KeyboardInfo} keyboard
   * @return {boolean}
   * @private
   */
  computeLayoutIsKnown_(keyboard) {
    if (!keyboard) {
      return false;
    }
    return keyboard.physicalLayout !== PhysicalLayout.kUnknown &&
        keyboard.mechanicalLayout !== MechanicalLayout.kUnknown;
    // Number pad presence can be unknown, as we can adapt on the fly if we get
    // a number pad event we weren't expecting.
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
