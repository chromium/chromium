// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cellular-roaming-toggle-button' is responsible for encapsulating the
 * cellular roaming configuration logic, and in particular the details behind
 * the transition to a more granular approach to roaming configuration.
 */

import '../../prefs/prefs.js';
import '../../settings_shared.css.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import {CrPolicyNetworkBehaviorMojo} from 'chrome://resources/cr_components/chromeos/network/cr_policy_network_behavior_mojo.m.js';
import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {PrefsBehaviorInterface}
 */
const CellularRoamingToggleButtonElementBase =
    mixinBehaviors([I18nBehavior, PrefsBehavior], PolymerElement);

/** @polymer */
class CellularRoamingToggleButtonElement extends
    CellularRoamingToggleButtonElementBase {
  static get is() {
    return 'cellular-roaming-toggle-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** @type {!chromeos.networkConfig.mojom.ManagedProperties|undefined} */
      managedProperties: {
        type: Object,
      },

      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * The allow roaming state.
       * @private
       */
      isRoamingAllowedForNetwork_: {
        type: Boolean,
        observer: 'isRoamingAllowedForNetworkChanged_',
        notify: true,
      },
    };
  }

  static get observers() {
    return [
      `managedPropertiesChanged_(
          prefs.cros.signed.data_roaming_enabled.*,
          managedProperties.*)`,
    ];
  }

  /** @override */
  constructor() {
    super();

    /** @private {!chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  /**
   * Returns the child element responsible for controlling cellular roaming.
   * @return {?CrToggleElement}
   */
  getCellularRoamingToggle() {
    return /** @type {?CrToggleElement} */ (
        this.shadowRoot.querySelector('#cellularRoamingToggle'));
  }

  /** @private */
  isRoamingAllowedForNetworkChanged_() {
    assert(this.networkConfig_);
    if (!this.managedProperties ||
        !this.managedProperties.typeProperties.cellular.allowRoaming) {
      return;
    }
    const config =
        OncMojo.getDefaultConfigProperties(this.managedProperties.type);
    config.typeConfig.cellular = {
      roaming: {allowRoaming: this.isRoamingAllowedForNetwork_},
    };
    this.networkConfig_.setProperties(this.managedProperties.guid, config)
        .then(response => {
          if (!response.success) {
            console.warn('Unable to set properties: ' + JSON.stringify(config));
          }
        });
    recordSettingChange();
  }

  /**
   * @return {boolean} The value derived from the network state reported by
   * managed properties and whether we are under policy enforcement.
   * @private
   */
  getRoamingAllowedForNetwork_() {
    return !!OncMojo.getActiveValue(
               this.managedProperties.typeProperties.cellular.allowRoaming) &&
        !this.isRoamingProhibitedByPolicy_();
  }

  /**
   * @return {string} The text to display with roaming details.
   * @private
   */
  getRoamingDetails_() {
    if (this.managedProperties.typeProperties.cellular.roamingState ===
        'Required') {
      return this.i18n('networkAllowDataRoamingRequired');
    }
    if (!this.getRoamingAllowedForNetwork_()) {
      return this.i18n('networkAllowDataRoamingDisabled');
    }
    return this.managedProperties.typeProperties.cellular.roamingState ===
            'Roaming' ?
        this.i18n('networkAllowDataRoamingEnabledRoaming') :
        this.i18n('networkAllowDataRoamingEnabledHome');
  }

  /** @private */
  managedPropertiesChanged_() {
    if (!this.managedProperties ||
        !this.managedProperties.typeProperties.cellular.allowRoaming) {
      return;
    }

    // We override the enforcement of the managed property here so that we can
    // have the toggle show the policy enforcement icon when the global policy
    // prohibits roaming.
    if (this.isRoamingProhibitedByPolicy_()) {
      this.set(
          'managedProperties.typeProperties.cellular.allowRoaming.policySource',
          chromeos.networkConfig.mojom.PolicySource.kDevicePolicyEnforced);
    }
    this.isRoamingAllowedForNetwork_ = this.getRoamingAllowedForNetwork_();
  }

  /** @private */
  onCellularRoamingRowClicked_(event) {
    event.stopPropagation();
    if (this.isPerNetworkToggleDisabled_()) {
      return;
    }
    this.isRoamingAllowedForNetwork_ = !this.isRoamingAllowedForNetwork_;
  }

  /** @private */
  isRoamingProhibitedByPolicy_() {
    const dataRoamingEnabled = this.getPref('cros.signed.data_roaming_enabled');
    return !dataRoamingEnabled.value &&
        dataRoamingEnabled.controlledBy ===
        chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
  }

  /** @private */
  isPerNetworkToggleDisabled_() {
    return this.disabled || this.isRoamingProhibitedByPolicy_() ||
        CrPolicyNetworkBehaviorMojo.isNetworkPolicyEnforced(
            this.managedProperties.typeProperties.cellular.allowRoaming);
  }

  /** @private */
  showPerNetworkAllowRoamingToggle_() {
    return this.isRoamingAllowedForNetwork_ !== undefined;
  }
}

customElements.define(
    CellularRoamingToggleButtonElement.is, CellularRoamingToggleButtonElement);
