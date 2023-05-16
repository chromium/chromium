// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying network nameserver options.
 */

import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/cr_elements/policy/cr_policy_indicator.js';
import '//resources/cr_elements/md_select.css.js';
import './network_shared.css.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ManagedProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {IPConfigType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';

import {CrPolicyNetworkBehaviorMojo} from './cr_policy_network_behavior_mojo.js';
import {getTemplate} from './network_nameservers.html.js';
import {OncMojo} from './onc_mojo.js';

/**
 * UI configuration options for nameservers.
 * @enum {string}
 */
const NameserversType = {
  AUTOMATIC: 'automatic',
  CUSTOM: 'custom',
  GOOGLE: 'google',
};

Polymer({
  _template: getTemplate(),
  is: 'network-nameservers',

  behaviors: [I18nBehavior, CrPolicyNetworkBehaviorMojo],

  properties: {
    disabled: {
      type: Boolean,
      value: false,
    },

    /** @type {!ManagedProperties|undefined} */
    managedProperties: {
      type: Object,
      observer: 'managedPropertiesChanged_',
    },

    /**
     * Array of nameserver addresses stored as strings.
     * @private {!Array<string>}
     */
    nameservers_: {
      type: Array,
      value() {
        return [];
      },
    },

    /**
     * The selected nameserver type.
     * @private {!NameserversType}
     */
    nameserversType_: {
      type: String,
      value: NameserversType.AUTOMATIC,
    },

    /**
     * Enum values for |nameserversType_|.
     * @private {NameserversType}
     */
    nameserversTypeEnum_: {
      readOnly: true,
      type: Object,
      value: NameserversType,
    },

    /** @private */
    googleNameserversText_: {
      type: String,
      value() {
        return this
            .i18nAdvanced(
                'networkNameserversGoogle', {substitutions: [], tags: ['a']})
            .toString();
      },
    },

    /** @private */
    canChangeConfigType_: {
      type: Boolean,
      computed: 'computeCanChangeConfigType_(managedProperties)',
    },
  },

  /** @const */
  GOOGLE_NAMESERVERS: [
    '8.8.4.4',
    '8.8.8.8',
  ],

  /** @const */
  EMPTY_NAMESERVER: '0.0.0.0',

  /** @const */
  MAX_NAMESERVERS: 4,

  /**
   * Saved nameservers from the NameserversType.CUSTOM tab. If this is empty, it
   * means that the user has not entered any custom nameservers yet.
   * @private {!Array<string>}
   */
  savedCustomNameservers_: [],

  /**
   * The last manually performed selection of the nameserver type. If this is
   * null, no explicit selection has been done for this network yet.
   * @private {?NameserversType}
   */
  savedNameserversType_: null,

  /*
   * Returns the nameserver type CrRadioGroupElement.
   * @return {?HTMLElement}
   */
  getNameserverRadioButtons() {
    return /** @type {?HTMLElement} */ (this.$$('#nameserverType'));
  },

  /**
   * Returns true if the nameservers in |nameservers1| match the nameservers in
   * |nameservers2|, ignoring order and empty / 0.0.0.0 entries.
   * @param {!Array<string>} nameservers1
   * @param {!Array<string>} nameservers2
   */
  nameserversMatch_(nameservers1, nameservers2) {
    const nonEmptySortedNameservers1 =
        this.clearEmptyNameServers_(nameservers1).sort();
    const nonEmptySortedNameservers2 =
        this.clearEmptyNameServers_(nameservers2).sort();
    if (nonEmptySortedNameservers1.length !==
        nonEmptySortedNameservers2.length) {
      return false;
    }
    for (let i = 0; i < nonEmptySortedNameservers1.length; i++) {
      if (nonEmptySortedNameservers1[i] !== nonEmptySortedNameservers2[i]) {
        return false;
      }
    }
    return true;
  },

  /**
   * Returns true if |nameservers| contains any all google nameserver entries
   * and only google nameserver entries or empty entries.
   * @param {!Array<string>} nameservers
   * @private
   */
  isGoogleNameservers_(nameservers) {
    return this.nameserversMatch_(nameservers, this.GOOGLE_NAMESERVERS);
  },

  /**
   * Returns the nameservers enforced by policy. If nameservers are not being
   * enforced, returns null.
   * @return {Array<string>|null}
   */
  getPolicyEnforcedNameservers_() {
    const staticIpConfig =
        this.managedProperties && this.managedProperties.staticIpConfig;
    if (!staticIpConfig || !staticIpConfig.nameServers) {
      return null;
    }
    return /** @type {Array<string>|null} */ (
        this.getEnforcedPolicyValue(staticIpConfig.nameServers));
  },

  /**
   * Returns the nameservers recommended by policy. If nameservers are not being
   * recommended, returns null. Note: also returns null if nameservers are being
   * enforced by policy.
   * @return {Array<string>|null}
   */
  getPolicyRecommendedNameservers_() {
    const staticIpConfig =
        this.managedProperties && this.managedProperties.staticIpConfig;
    if (!staticIpConfig || !staticIpConfig.nameServers) {
      return null;
    }
    return /** @type {Array<string>|null} */ (
        this.getRecommendedPolicyValue(staticIpConfig.nameServers));
  },

  /** @private */
  managedPropertiesChanged_(newValue, oldValue) {
    if (!this.managedProperties) {
      return;
    }

    if (!oldValue || newValue.guid !== oldValue.guid) {
      this.savedCustomNameservers_ = [];
      this.savedNameserversType_ = null;
    }

    // Update the 'nameservers' property.
    let nameservers = [];
    const ipv4 =
        OncMojo.getIPConfigForType(this.managedProperties, IPConfigType.kIPv4);
    if (ipv4 && ipv4.nameServers) {
      nameservers = ipv4.nameServers.slice();
    }

    // Update the 'nameserversType' property.
    const configType =
        OncMojo.getActiveValue(this.managedProperties.nameServersConfigType);
    /** @type {NameserversType} */ let type;
    if (configType === 'Static') {
      if (this.isGoogleNameservers_(nameservers) &&
          this.savedNameserversType_ !== NameserversType.CUSTOM) {
        type = NameserversType.GOOGLE;
        nameservers = this.GOOGLE_NAMESERVERS;  // Use consistent order.
      } else {
        type = NameserversType.CUSTOM;
      }
    } else {
      type = NameserversType.AUTOMATIC;
      nameservers = this.clearEmptyNameServers_(nameservers);
    }
    this.setNameservers_(type, nameservers, false /* send */);
  },

  /**
   * @param {!NameserversType} nameserversType
   * @param {!Array<string>} nameservers
   * @param {boolean} sendNameservers If true, send the nameservers once they
   *     have been set in the UI.
   * @private
   */
  setNameservers_(nameserversType, nameservers, sendNameservers) {
    if (nameserversType === NameserversType.CUSTOM) {
      // Add empty entries for unset custom nameservers.
      for (let i = nameservers.length; i < this.MAX_NAMESERVERS; ++i) {
        nameservers[i] = this.EMPTY_NAMESERVER;
      }
    } else {
      nameservers = this.clearEmptyNameServers_(nameservers);
    }
    this.nameservers_ = nameservers;
    this.nameserversType_ = nameserversType;
    if (sendNameservers) {
      this.sendNameServers_();
    }
  },

  /**
   * @param {!ManagedProperties} managedProperties
   * @return {boolean} True if the nameservers config type type can be changed.
   * @private
   */
  computeCanChangeConfigType_(managedProperties) {
    if (!managedProperties) {
      return false;
    }
    if (this.isNetworkPolicyEnforced(managedProperties.nameServersConfigType)) {
      return false;
    }
    return true;
  },

  /**
   * @param {string} nameserversType
   * @param {!ManagedProperties} managedProperties
   * @return {boolean} True if the nameservers are editable.
   * @private
   */
  canEditCustomNameServers_(nameserversType, managedProperties) {
    if (!managedProperties) {
      return false;
    }
    if (nameserversType !== NameserversType.CUSTOM) {
      return false;
    }
    if (this.isNetworkPolicyEnforced(managedProperties.nameServersConfigType)) {
      return false;
    }
    if (managedProperties.staticIpConfig &&
        managedProperties.staticIpConfig.nameServers &&
        this.isNetworkPolicyEnforced(
            managedProperties.staticIpConfig.nameServers)) {
      return false;
    }
    return true;
  },

  /**
   * @param {NameserversType} nameserversType
   * @param {NameserversType} type
   * @param {!Array<string>} nameservers
   * @return {boolean}
   * @private
   */
  showNameservers_(nameserversType, type, nameservers) {
    if (nameserversType !== type) {
      return false;
    }
    return type === NameserversType.CUSTOM || nameservers.length > 0;
  },

  /**
   * @param {!Array<string>} nameservers
   * @return {string}
   * @private
   */
  getNameserversString_(nameservers) {
    return nameservers.join(', ');
  },

  /**
   * Returns currently configured custom nameservers, to be used when toggling
   * to 'custom' from 'automatic' or 'google', prefer nameservers in the
   * following priority:
   *
   * 1) policy-enforced nameservers,
   * 2) previously manually entered nameservers (|savedCustomNameservers_|),
   * 3) policy-recommended nameservers,
   * 4) active nameservers (e.g. from DHCP).
   * @return {!Array<string>} nameservers
   * @private
   */
  getCustomNameServers_() {
    const policyEnforcedNameservers = this.getPolicyEnforcedNameservers_();
    if (policyEnforcedNameservers !== null) {
      return policyEnforcedNameservers.slice();
    }

    if (this.savedCustomNameservers_.length > 0) {
      return this.savedCustomNameservers_;
    }

    const policyRecommendedNameservers =
        this.getPolicyRecommendedNameservers_();
    if (policyRecommendedNameservers !== null) {
      return policyRecommendedNameservers.slice();
    }

    return this.nameservers_;
  },

  /**
   * Event triggered when the selected type changes. Updates nameservers and
   * sends the change value if necessary.
   * @private
   */
  onTypeChange_() {
    const type = this.$$('#nameserverType').selected;
    this.nameserversType_ = type;
    this.savedNameserversType_ = type;
    if (type === NameserversType.CUSTOM) {
      this.setNameservers_(type, this.getCustomNameServers_(), true /* send */);
      return;
    }
    this.sendNameServers_();
  },

  /**
   * Event triggered when a |nameservers_| value changes through the custom
   * namservers UI.
   * This gets called after data-binding updates a |nameservers_[i]| entry.
   * This saves the custom nameservers and reflects that change to the backend
   * (sending the custom nameservers).
   * @private
   */
  onValueChange_() {
    this.savedCustomNameservers_ = this.nameservers_.slice();
    this.sendNameServers_();
  },

  /**
   * Sends the current nameservers type (for automatic) or value.
   * @private
   */
  sendNameServers_() {
    const type = this.nameserversType_;

    if (type === NameserversType.CUSTOM) {
      this.fire('nameservers-change', {
        field: 'nameServers',
        value: this.nameservers_,
      });
    } else if (type === NameserversType.GOOGLE) {
      this.nameservers_ = this.GOOGLE_NAMESERVERS;
      this.fire('nameservers-change', {
        field: 'nameServers',
        value: this.GOOGLE_NAMESERVERS,
      });
    } else {  // type === NameserversType.AUTOMATIC
      // If not connected, properties will clear. Otherwise they may or may not
      // change so leave them as-is.
      if (!OncMojo.connectionStateIsConnected(
              this.managedProperties.connectionState)) {
        this.nameservers_ = [];
      } else {
        this.nameservers_ = this.clearEmptyNameServers_(this.nameservers_);
      }
      this.fire('nameservers-change', {
        field: 'nameServersConfigType',
        value: 'DHCP',
      });
    }
  },

  /**
   * @param {!Array<string>} nameservers
   * @return {!Array<string>}
   * @private
   */
  clearEmptyNameServers_(nameservers) {
    return nameservers.filter(
        (nameserver) => (!!nameserver && nameserver !== this.EMPTY_NAMESERVER));
  },

  /**
   * @param {!Event} event
   * @private
   */
  doNothing_(event) {
    event.stopPropagation();
  },

  /**
   * @param {number} index
   * @return {string} Accessibility label for nameserver input with given index.
   * @private
   */
  getCustomNameServerInputA11yLabel_(index) {
    return this.i18n('networkNameserversCustomInputA11yLabel', index + 1);
  },
});
