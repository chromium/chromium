// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '../settings_shared_css.js';

import {CrPolicyPrefBehavior} from '//resources/cr_elements/policy/cr_policy_pref_behavior.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {PrefControlBehavior} from './pref_control_behavior.js';

Polymer({
  is: 'controlled-button',

  _template: html`{__html_template__}`,

  behaviors: [
    CrPolicyPrefBehavior,
    PrefControlBehavior,
  ],

  properties: {
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
  },

  /** @override */
  attached() {
    if (this.classList.contains('action-button')) {
      this.actionClass_ = 'action-button';
    }
  },

  /** Focus on the inner cr-button. */
  focus() {
    this.$$('cr-button').focus();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onIndicatorClick_(e) {
    // Disallow <controlled-button on-click="..."> when controlled.
    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * @param {!boolean} enforced
   * @param {!boolean} disabled
   * @return {boolean} True if the button should be enabled.
   * @private
   */
  buttonEnabled_(enforced, disabled) {
    return !enforced && !disabled;
  }
});
