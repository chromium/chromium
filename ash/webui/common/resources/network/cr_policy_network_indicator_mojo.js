// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for indicating policies based on network
 * properties.
 */

import '//resources/ash/common/cr_elements/policy/cr_tooltip_icon.js';
import '//resources/ash/common/cr_elements/cr_hidden_style.css.js';

import {CrPolicyIndicatorBehavior, CrPolicyIndicatorBehaviorInterface, CrPolicyIndicatorType} from '//resources/ash/common/cr_policy_indicator_behavior.js';
import {PolicySource} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyNetworkBehaviorMojo, CrPolicyNetworkBehaviorMojoInterface} from './cr_policy_network_behavior_mojo.js';
import {getTemplate} from './cr_policy_network_indicator_mojo.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrPolicyIndicatorBehaviorInterface}
 * @implements {CrPolicyNetworkBehaviorMojoInterface}
 */
const CrPolicyNetworkIndicatorMojoElementBase = mixinBehaviors(
    [CrPolicyIndicatorBehavior, CrPolicyNetworkBehaviorMojo], PolymerElement);

/** @polymer */
class CrPolicyNetworkIndicatorMojoElement extends
    CrPolicyNetworkIndicatorMojoElementBase {
  static get is() {
    return 'cr-policy-network-indicator-mojo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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
    };
  }

  static get observers() {
    return ['propertyChanged_(property.*)'];
  }

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
  }

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
  }
}

customElements.define(
    CrPolicyNetworkIndicatorMojoElement.is,
    CrPolicyNetworkIndicatorMojoElement);
