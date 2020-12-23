// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @fileoverview Element that displays a header inside a module. */

class ModuleHeaderElement extends PolymerElement {
  static get is() {
    return 'ntp-module-header';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The title to be displayed.
       * @type {!string}
       */
      title: String,

      /**
       * The chip text showing on the header.
       * @type {string}
       */
      chipText: String,

      /**
       * True if the header should display an info button.
       * @type {boolean}
       */
      showInfoButton: {
        type: Boolean,
        value: false,
      },

      /**
       * True if the header should display a dismiss button.
       * @type {boolean}
       */
      showDismissButton: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** @private */
  onInfoButtonClick_() {
    this.dispatchEvent(new CustomEvent('info-button-click', {bubbles: true}));
  }

  /** @private */
  onDismissButtonClick_() {
    this.dispatchEvent(
        new CustomEvent('dismiss-button-click', {bubbles: true}));
  }
}

customElements.define(ModuleHeaderElement.is, ModuleHeaderElement);
