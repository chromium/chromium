// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './keyboard_icons.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'keyboard-key' provides a visual representation of a single key for the
 * 'keyboard-diagram' component.
 */

export class KeyboardKeyElement extends PolymerElement {
  static get is() {
    return 'keyboard-key';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The text to show on the key, if any.
       * @type {?string}
       */
      mainGlyph: String,

      /**
       * The name of the icon to use, if any. The name should be of the form:
       * `iconset_name:icon_name`.
       * @type {?string}
       */
      icon: String,
    };
  }
}

customElements.define(KeyboardKeyElement.is, KeyboardKeyElement);
