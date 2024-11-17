// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for policy controlled network properties.
 * Note: Many of these methods may be called from HTML, so they support
 * optional properties (which may be null|undefined).
 */

import {ApnProperties, ManagedApnList, ManagedBoolean, ManagedInt32, ManagedString, ManagedStringList} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {OncSource, PolicySource} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';

import {CrPolicyIndicatorType} from '../cr_policy_indicator_behavior.js';

import {OncMojo} from './onc_mojo.js';

/** @polymerBehavior */
export const CrPolicyNetworkBehaviorMojo = {
  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean} True if the property is controlled by network policy.
   */
  isNetworkPolicyControlled(property) {
    if (!property) {
      return false;
    }
    return property.policySource !== PolicySource.kNone &&
        property.policySource !== PolicySource.kActiveExtension;
  },

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean} True if the property is controlled by an extension.
   */
  isExtensionControlled(property) {
    if (!property) {
      return false;
    }
    return property.policySource === PolicySource.kActiveExtension;
  },

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is controlled by a network
   *     policy or an extension.
   */
  isControlled(property) {
    if (!property) {
      return false;
    }
    return property.policySource !== PolicySource.kNone;
  },

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is editable.
   */
  isEditable(property) {
    if (!property) {
      return false;
    }
    return property.policySource !== PolicySource.kUserPolicyEnforced &&
        property.policySource !== PolicySource.kDevicePolicyEnforced &&
        property.policySource !== PolicySource.kActiveExtension;
  },

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is enforced by a policy.
   */
  isNetworkPolicyEnforced(property) {
    if (!property) {
      return false;
    }
    return property.policySource === PolicySource.kUserPolicyEnforced ||
        property.policySource === PolicySource.kDevicePolicyEnforced;
  },

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is recommended by a policy.
   */
  isNetworkPolicyRecommended(property) {
    if (!property) {
      return false;
    }
    return property.policySource === PolicySource.kUserPolicyRecommended ||
        property.policySource === PolicySource.kDevicePolicyRecommended;
  },

  /**
   * @param {!ManagedBoolean|
   *         !ManagedInt32|
   *         !ManagedString|
   *         !ManagedStringList|
   *         !ManagedApnList} property
   * @return {boolean|number|string|!Array<string>|
   *          !Array<!ApnProperties>|null}
   *         |property.policyValue| if the property is policy-enforced or null
   *         otherwise.
   */
  getEnforcedPolicyValue(property) {
    if (!property || !this.isNetworkPolicyEnforced(property)) {
      return null;
    }
    return property.policyValue === undefined ? null : property.policyValue;
  },

  /**
   * @param {!ManagedBoolean|
   *         !ManagedInt32|
   *         !ManagedString|
   *         !ManagedStringList|
   *         !ManagedApnList} property
   * @return {boolean|number|string|!Array<string>|
   *          !Array<!ApnProperties>|null}
   *         |property.policyValue| if the property is policy-recommended or
   *         null otherwise.
   */
  getRecommendedPolicyValue(property) {
    if (!property || !this.isNetworkPolicyRecommended(property)) {
      return null;
    }
    return property.policyValue === undefined ? null : property.policyValue;
  },

  /**
   * @param {!OncSource} source
   * @return {boolean}
   * @protected
   */
  isPolicySource(source) {
    return source === OncSource.kDevicePolicy ||
        source === OncSource.kUserPolicy;
  },

  /**
   * @param {!OncSource} source
   * @return {!CrPolicyIndicatorType}
   * @protected
   */
  getIndicatorTypeForSource(source) {
    if (source === OncSource.kDevicePolicy) {
      return CrPolicyIndicatorType.DEVICE_POLICY;
    }
    if (source === OncSource.kUserPolicy) {
      return CrPolicyIndicatorType.USER_POLICY;
    }
    return CrPolicyIndicatorType.NONE;
  },

  /**
   * Get policy indicator type for the setting at |path|.
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {CrPolicyIndicatorType}
   */
  getPolicyIndicatorType(property) {
    if (!property) {
      return CrPolicyIndicatorType.NONE;
    }
    if (property.policySource === PolicySource.kUserPolicyEnforced ||
        property.policySource === PolicySource.kUserPolicyRecommended) {
      return CrPolicyIndicatorType.USER_POLICY;
    }
    if (property.policySource === PolicySource.kDevicePolicyEnforced ||
        property.policySource === PolicySource.kDevicePolicyRecommended) {
      return CrPolicyIndicatorType.DEVICE_POLICY;
    }
    if (property.policySource === PolicySource.kActiveExtension) {
      return CrPolicyIndicatorType.EXTENSION;
    }
    return CrPolicyIndicatorType.NONE;
  },
};

/** @interface */
export class CrPolicyNetworkBehaviorMojoInterface {
  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean}
   */
  isNetworkPolicyControlled(property) {}

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean}
   */
  isExtensionControlled(property) {}

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean}
   */
  isControlled(property) {}

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean}
   */
  isEditable(property) {}

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean}
   */
  isNetworkPolicyEnforced(property) {}

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean}
   */
  isNetworkPolicyRecommended(property) {}

  /**
   * @param {!ManagedBoolean|
   *         !ManagedInt32|
   *         !ManagedString|
   *         !ManagedStringList|
   *         !ManagedApnList} property
   * @return {boolean|number|string|!Array<string>|
   *          !Array<!ApnProperties>|null}
   */
  getEnforcedPolicyValue(property) {}

  /**
   * @param {!ManagedBoolean|
   *         !ManagedInt32|
   *         !ManagedString|
   *         !ManagedStringList|
   *         !ManagedApnList} property
   * @return {boolean|number|string|!Array<string>|
   *          !Array<!ApnProperties>|null}
   */
  getRecommendedPolicyValue(property) {}

  /**
   * @param {!OncSource} source
   * @return {boolean}
   * @protected
   */
  isPolicySource(source) {}

  /**
   * @param {!OncSource} source
   * @return {!CrPolicyIndicatorType}
   * @protected
   */
  getIndicatorTypeForSource(source) {}

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {CrPolicyIndicatorType}
   */
  getPolicyIndicatorType(property) {}
}
