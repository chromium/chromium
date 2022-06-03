// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * Refers to the state of an 'input-key' item.
 * @enum {string}
 */
export const KeyInputState = {
  NOT_SELECTED: 'not-selected',
  MODIFIER_SELECTED: 'modifier-selected',
  ALPHANUMERIC_SELECTED: 'alpha-numeric-selected',
};

/**
 * @fileoverview
 * 'input-key' is a component wrapper for a single input key. Responsible for
 * handling dynamic styling of a single key.
 */
export class InputKeyElement extends PolymerElement {
  static get is() {
    return 'input-key';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      key: {
        type: String,
        value: '',
      },

      keyState: {
        type: String,
        value: KeyInputState.NOT_SELECTED,
        reflectToAttribute: true,
      },
    };
  }
}

customElements.define(InputKeyElement.is, InputKeyElement);