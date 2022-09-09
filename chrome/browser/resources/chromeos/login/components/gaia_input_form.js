// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element wrapping gaia styled input form for login/oobe.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 */
const GaiaInputFormBase = Polymer.mixinBehaviors([], Polymer.Element);

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

  /* #html_template_placeholder */

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
    return Polymer.dom(this.$.inputs).getDistributedNodes();
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
    return controls.concat(Polymer.dom(this).querySelectorAll('gaia-button'));
  }

  /** @private */
  onDisabledChanged_(disabled) {
    this.getControls_().forEach(function(control) {
      control.disabled = disabled;
    });
  }
}

customElements.define(GaiaInputForm.is, GaiaInputForm);
