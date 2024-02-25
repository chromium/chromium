// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for network configuration toggle.
 */
import '//resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import './cr_policy_network_indicator_mojo.js';
import './network_shared.css.js';

import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyNetworkBehaviorMojo} from './cr_policy_network_behavior_mojo.js';
import {NetworkConfigElementBehavior} from './network_config_element_behavior.js';
import {getTemplate} from './network_config_toggle.html.js';

Polymer({
  _template: getTemplate(),
  is: 'network-config-toggle',

  behaviors: [
    CrPolicyNetworkBehaviorMojo,
    NetworkConfigElementBehavior,
  ],

  properties: {
    label: String,

    subLabel: String,

    checked: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      notify: true,
    },

    /**
     * Uses Settings styling when true (policy icon is left of the toggle)
     */
    policyOnLeft: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  listeners: {
    'click': 'onHostTap_',
  },

  /** @override */
  focus() {
    this.$$('cr-toggle').focus();
  },

  /**
   * Handles non cr-toggle button clicks (cr-toggle handles its own click events
   * which don't bubble).
   * @param {!Event} e
   * @private
   */
  onHostTap_(e) {
    e.stopPropagation();
    if (this.getDisabled_(this.disabled, this.property)) {
      return;
    }
    this.checked = !this.checked;
    this.fire('change');
  },
});
