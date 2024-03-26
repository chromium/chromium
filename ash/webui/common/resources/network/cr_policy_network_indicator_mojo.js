// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for indicating policies based on network
 * properties.
 */

import '//resources/ash/common/cr_elements/policy/cr_tooltip_icon.js';
import '//resources/ash/common/cr_elements/cr_hidden_style.css.js';

import {CrPolicyIndicatorBehavior, CrPolicyIndicatorType} from '//resources/ash/common/cr_policy_indicator_behavior.js';
import {ManagedBoolean, ManagedInt32, ManagedString} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {PolicySource} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyNetworkBehaviorMojo} from './cr_policy_network_behavior_mojo.js';
import {getTemplate} from './cr_policy_network_indicator_mojo.html.js';

Polymer({
  _template: getTemplate(),
  is: 'cr-policy-network-indicator-mojo',

  behaviors: [CrPolicyIndicatorBehavior, CrPolicyNetworkBehaviorMojo],

  properties: {
    /**
     * Network property associated with the indicator. Note: |property| may
     * be null or undefined, depending on how the properties dictionary is
     * generated.
     * @type {?ManagedBoolean|
     *        ?ManagedInt32|
     *        ?ManagedString|undefined}
     */
    property: Object,

    /** Property forwarded to the cr-tooltip-icon element. */
    tooltipPosition: String,

    /** @private */
    indicatorTooltip_: {
      type: String,
      computed: 'getNetworkIndicatorTooltip_(indicatorType, property.*)',
    },
  },

  observers: ['propertyChanged_(property.*)'],

  /** @private */
  propertyChanged_() {
    const property = this.property;
    if (property === null || property === undefined ||
        !this.isControlled(property)) {
      this.indicatorType = CrPolicyIndicatorType.NONE;
      return;
    }
    switch (property.policySource) {
      case PolicySource.kNone:
        this.indicatorType = CrPolicyIndicatorType.NONE;
        break;
      case PolicySource.kUserPolicyEnforced:
        this.indicatorType = CrPolicyIndicatorType.USER_POLICY;
        break;
      case PolicySource.kDevicePolicyEnforced:
        this.indicatorType = CrPolicyIndicatorType.DEVICE_POLICY;
        break;
      case PolicySource.kUserPolicyRecommended:
      case PolicySource.kDevicePolicyRecommended:
        this.indicatorType = CrPolicyIndicatorType.RECOMMENDED;
        break;
      case PolicySource.kActiveExtension:
        this.indicatorType = CrPolicyIndicatorType.EXTENSION;
        break;
    }
  },

  /**
   * @return {string} The tooltip text for |type|.
   * @private
   */
  getNetworkIndicatorTooltip_() {
    if (this.property === undefined) {
      return '';
    }

    const matches = !!this.property &&
        this.property.activeValue === this.property.policyValue;
    return this.getIndicatorTooltip(this.indicatorType, '', matches);
  },
});
