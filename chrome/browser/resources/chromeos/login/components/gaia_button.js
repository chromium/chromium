// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element wrapping gaia styled button for login/oobe.
 */

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/polymer/v3_0/paper-styles/color.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';


/**
 * @constructor
 * @extends {PolymerElement}
 */
const GaiaButtonBase = mixinBehaviors([], PolymerElement);

/**
 * @typedef {{
 *   button: CrButtonElement,
 * }}
 */
GaiaButtonBase.$;

/** @polymer */
class GaiaButton extends GaiaButtonBase {
  static get is() {
    return 'gaia-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

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
