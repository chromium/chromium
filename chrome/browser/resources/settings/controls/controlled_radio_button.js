// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_radio_button/cr_radio_button_style_css.m.js';
import '//resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import '//resources/polymer/v3_0/iron-a11y-keys-behavior/iron-a11y-keys-behavior.js';
import '../settings_shared_css.js';

import {CrRadioButtonBehavior} from '//resources/cr_elements/cr_radio_button/cr_radio_button_behavior.m.js';
import {assert} from '//resources/js/assert.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {prefToString} from '../prefs/pref_util.js';

import {PrefControlBehavior} from './pref_control_behavior.js';

Polymer({
  is: 'controlled-radio-button',

  _template: html`{__html_template__}`,

  behaviors: [
    PrefControlBehavior,
    CrRadioButtonBehavior,
  ],

  observers: [
    'updateDisabled_(pref.enforcement)',
  ],

  /** @private */
  updateDisabled_() {
    this.disabled =
        this.pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /**
   * @return {boolean}
   * @private
   */
  showIndicator_() {
    return this.disabled && this.name === prefToString(assert(this.pref));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onIndicatorTap_(e) {
    // Disallow <controlled-radio-button on-click="..."> when disabled.
    e.preventDefault();
    e.stopPropagation();
  },
});
