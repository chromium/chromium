// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './input_key.js'

import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertNotReached} from 'chrome://resources/js/assert.m.js';

// Modifier values are based off of ui::Accelerator. Must be kept in sync with
// ui::Accelerator.
export const ModifierKeys = {
  SHIFT: /*EF_SHIFT_DOWN=*/ 1 << 1,
  CONTROL: /*EF_CONTROL_DOWN=*/ 1 << 2,
  ALT: /*EF_ALT_DOWN=*/ 1 << 3,
  COMMAND: /*EF_COMMAND_DOWN=*/ 1 << 4,
}

/**
 * Returns the converted modifier flag as a readable string.
 * TODO(jimmyxgong): Localize, replace with icon, or update strings.
 * @param {number} modifier
 * @return {string}
 */
function GetModifierString(modifier) {
  switch(modifier) {
    case ModifierKeys.SHIFT:
      return 'shift';
    case ModifierKeys.CONTROL:
      return 'ctrl';
    case ModifierKeys.ALT:
      return 'alt';
    case ModifierKeys.COMMAND:
      return 'meta';
    default:
      assertNotReached();
      return '';
  }
}

/**
 * @fileoverview
 * 'accelerator-view' is wrapper component for an accelerator. It maintains both
 * the read-only and editable state of an accelerator.
 * TODO(jimmyxgong): Implement the edit mode.
 */
export class AcceleratorViewElement extends PolymerElement {
  static get is() {
    return 'accelerator-view';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * TODO(jimmyxgong): Update this type to be the actual mojom::accelerator.
       * @type{!Object}
       */
      accelerator: {
        type: Object,
        value: {},
      },

      isEditable: {
        type: Boolean,
        value: false,
      },

      /**
       * @type{!Array<string>}
       * @private
       */
      modifiers_: {
        type: Array,
        computed: 'getModifiers_(accelerator)',
      },
    }
  }

  /**
   * @return {!Array<string>}
   * @private
   */
  getModifiers_() {
    let modifiers = [];
    for (const key in ModifierKeys) {
      const modifier = ModifierKeys[key];
      if (this.accelerator.modifiers & modifier) {
        modifiers.push(GetModifierString(modifier));
      }
    }
    return modifiers;
  }
}

customElements.define(AcceleratorViewElement.is, AcceleratorViewElement);
