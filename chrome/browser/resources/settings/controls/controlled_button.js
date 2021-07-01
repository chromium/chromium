// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '../settings_shared_css.js';

import {CrPolicyPrefBehavior} from '//resources/cr_elements/policy/cr_policy_pref_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {PrefControlBehavior} from './pref_control_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 */
const ControlledButtonElementBase =
    mixinBehaviors([CrPolicyPrefBehavior, PrefControlBehavior], PolymerElement);

/** @polymer */
class ControlledButtonElement extends ControlledButtonElementBase {
  static get is() {
    return 'controlled-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      endJustified: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      label: String,

      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** @private */
      actionClass_: {type: String, value: ''},

      /** @private */
      enforced_: {
        type: Boolean,
        computed: 'isPrefEnforced(pref.*)',
        reflectToAttribute: true,
      },
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    if (this.classList.contains('action-button')) {
      this.actionClass_ = 'action-button';
    }
  }

  /** Focus on the inner cr-button. */
  focus() {
    this.shadowRoot.querySelector('cr-button').focus();
  }

  /**
   * @param {!Event} e
   * @private
   */
  onIndicatorClick_(e) {
    // Disallow <controlled-button on-click="..."> when controlled.
    e.preventDefault();
    e.stopPropagation();
  }

  /**
   * @param {!boolean} enforced
   * @param {!boolean} disabled
   * @return {boolean} True if the button should be enabled.
   * @private
   */
  buttonEnabled_(enforced, disabled) {
    return !enforced && !disabled;
  }
}

customElements.define(ControlledButtonElement.is, ControlledButtonElement);
