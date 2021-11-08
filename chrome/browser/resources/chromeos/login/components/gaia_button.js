// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element wrapping gaia styled button for login/oobe.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 */
const GaiaButtonBase = Polymer.mixinBehaviors([], Polymer.Element);

/**
 * @typedef {{
 *   button: CrButtonElement,
 * }}
 */
GaiaButtonBase.$;

class GaiaButton extends GaiaButtonBase {
  static get is() {
    return 'gaia-button';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      link: {
        type: Boolean,
        reflectToAttribute: true,
        observer: 'onLinkChanged_',
        value: false,
      },
    };
  }

  constructor() {
    super();
  }

  focus() {
    this.$.button.focus();
  }

  /**
   * @private
   */
  onLinkChanged_() {
    this.$.button.classList.toggle('action-button', !this.link);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onClick_(e) {
    if (this.disabled) {
      e.stopPropagation();
    }
  }
}

customElements.define(GaiaButton.is, GaiaButton);
