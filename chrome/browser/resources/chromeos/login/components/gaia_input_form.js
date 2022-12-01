// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element wrapping gaia styled input form for login/oobe.
 *
 * A simple input form with a button. Being used for typing email or password.
 * User should put one or more <gaia-input slot="inputs">s inside.
 *
 * Example:
 *   <gaia-input-form button-text="Submit">
 *     <gaia-input slot="inputs" label="Email" type="email"></gaia-input>
 *     <gaia-input slot="inputs" label="Password" type="password"></gaia-input>
 *     <gaia-input slot="inputs" label="OTP"></gaia-input>
 *   </gaia-input-form>
 *
 * Attributes:
 *   'button-text' - text on the button.
 * Methods:
 *   'reset' - resets all the inputs to the initial state.
 * Events:
 *   'submit' - fired on button click or "Enter" press inside input field.
 *
 */

import './common_styles/oobe_common_styles.m.js';
import './gaia_button.js';

import {dom, html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @constructor
 * @extends {PolymerElement}
 */
const GaiaInputFormBase = mixinBehaviors([], PolymerElement);

/**
 * @typedef {{
 *   inputs: HTMLSlotElement,
 * }}
 */
GaiaInputFormBase.$;

class GaiaInputForm extends GaiaInputFormBase {
  static get is() {
    return 'gaia-input-form';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        observer: 'onDisabledChanged_',
      },

      buttonText: {
        type: String,
        value: '',
      },
    };
  }

  /** @public */
  reset() {
    var inputs = this.getInputs_();
    for (var i = 0; i < inputs.length; ++i) {
      inputs[i].value = '';
      inputs[i].isInvalid = false;
    }
  }

  submit() {
    this.dispatchEvent(
        new CustomEvent('submit', {bubbles: true, composed: true}));
  }

  /** @private */
  onButtonClicked_() {
    this.submit();
  }

  /**
   * @private
   * @return {!Array<!Node>}
   */
  getInputs_() {
    return dom(this.$.inputs).getDistributedNodes();
  }

  /** @private */
  onKeyDown_(e) {
    if (e.keyCode != 13 || this.$.button.disabled) {
      return;
    }
    if (this.getInputs_().indexOf(e.target) == -1) {
      return;
    }
    this.onButtonClicked_();
  }

  /**
   * @private
   * @return {!Array<!Element>}
   */
  getControls_() {
    var controls = this.getInputs_();
    controls.push(this.$.button);
    return controls.concat(dom(this).querySelectorAll('gaia-button'));
  }

  /** @private */
  onDisabledChanged_(disabled) {
    this.getControls_().forEach(function(control) {
      control.disabled = disabled;
    });
  }
}

customElements.define(GaiaInputForm.is, GaiaInputForm);
