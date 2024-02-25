// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for network configuration input fields.
 */
import '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import './cr_policy_network_indicator_mojo.js';
import './network_shared.css.js';

import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyNetworkBehaviorMojo} from './cr_policy_network_behavior_mojo.js';
import {NetworkConfigElementBehavior} from './network_config_element_behavior.js';
import {getTemplate} from './network_config_input.html.js';

Polymer({
  _template: getTemplate(),
  is: 'network-config-input',

  behaviors: [
    CrPolicyNetworkBehaviorMojo,
    NetworkConfigElementBehavior,
  ],

  properties: {
    label: String,

    hidden: {
      type: Boolean,
      reflectToAttribute: true,
    },

    invalid: {
      type: Boolean,
      value: false,
    },
  },

  focus() {
    this.$$('cr-input').focus();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onKeypress_(event) {
    if (event.key !== 'Enter') {
      return;
    }
    event.stopPropagation();
    this.fire('enter');
  },
});
