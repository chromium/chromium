// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_radio_button/cr_radio_button_style_css.m.js';
import '//resources/cr_elements/policy/cr_policy_indicator.m.js';
import '//resources/polymer/v3_0/iron-a11y-keys-behavior/iron-a11y-keys-behavior.js';
import '../../settings_shared_css.js';

import {CrRadioButtonBehavior} from '//resources/cr_elements/cr_radio_button/cr_radio_button_behavior.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'multidevice-radio-button',

  behaviors: [
    CrRadioButtonBehavior,
  ],

  hostAttributes: {'role': 'radio'},

  properties: {
    ariaChecked: {
      type: String,
      notify: true,
      reflectToAttribute: true,
      computed: 'getAriaChecked_(checked)'
    },
    ariaDisabled: {
      type: String,
      notify: true,
      reflectToAttribute: true,
      computed: 'getAriaDisabled_(disabled)'

    },
    ariaLabel: {
      type: String,
      notify: true,
      reflectToAttribute: true,
      computed: 'getLabel_(label)'
    }
  },

  getLabel_(label) {
    return label;
  },

  /**
   * Prevents on-click handles on the control from being activated when the
   * indicator is clicked.
   * @param {!Event} e The click event.
   * @private
   */
  onIndicatorTap_(e) {
    e.preventDefault();
    e.stopPropagation();
  },
});
