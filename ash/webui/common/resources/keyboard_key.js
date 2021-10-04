// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
       * The glyph to show in the center of the key (if topGlyph is unset) or in
       * the bottom half (otherwise).
       * @type {string}
       */
      mainGlyph: String,
    };
  }
}

customElements.define(KeyboardKeyElement.is, KeyboardKeyElement);
