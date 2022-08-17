// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button_style.css.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.m.js';
import 'chrome://resources/polymer/v3_0/iron-a11y-keys-behavior/iron-a11y-keys-behavior.js';
import '../../settings_shared.css.js';

import {CrRadioButtonBehavior, CrRadioButtonBehaviorInterface} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrRadioButtonBehaviorInterface}
 */
const MultideviceRadioButtonElementBase =
    mixinBehaviors([CrRadioButtonBehavior], PolymerElement);

/** @polymer */
class MultideviceRadioButtonElement extends MultideviceRadioButtonElementBase {
  static get is() {
    return 'multidevice-radio-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      ariaChecked: {
        type: String,
        notify: true,
        reflectToAttribute: true,
        computed: 'getAriaChecked_(checked)',
      },
      ariaDisabled: {
        type: String,
        notify: true,
        reflectToAttribute: true,
        computed: 'getAriaDisabled_(disabled)',

      },
      ariaLabel: {
        type: String,
        notify: true,
        reflectToAttribute: true,
        computed: 'getLabel_(label)',
      },
    };
  }

  ready() {
    super.ready();
    this.setAttribute('role', 'radio');
  }

  getLabel_(label) {
    return label;
  }

  /**
   * Prevents on-click handles on the control from being activated when the
   * indicator is clicked.
   * @param {!Event} e The click event.
   * @private
   */
  onIndicatorTap_(e) {
    e.preventDefault();
    e.stopPropagation();
  }
}

customElements.define(
    MultideviceRadioButtonElement.is, MultideviceRadioButtonElement);
