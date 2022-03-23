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
import './internet_shared_css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/cr_elements/policy/cr_policy_indicator.m.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.m.js';

import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from '//resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
import {OncMojo} from '//resources/cr_components/chromeos/network/onc_mojo.m.js';
import {assert} from '//resources/js/assert.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {PrefsBehavior} from '../prefs_behavior.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'cellular-roaming-toggle-button',

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
  ],

  properties: {
    disabled: {
      type: Boolean,
      value: false,
    },

    /** @type {!chromeos.networkConfig.mojom.ManagedProperties|undefined} */
    managedProperties: {
      type: Object,
      observer: 'managedPropertiesChanged_',
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
    },
  },

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @override */
  created() {
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  },

  /**
   * Returns the text sub-label for testing.
   * @return {string}
   */
  getSubLabelForTesting() {
    return this.$$('#cellularRoamingToggleSubLabel').innerText;
  },

  /**
   * Returns the child element responsible for controlling cellular roaming.
   * @return {?CrToggleElement}
   */
  getCellularRoamingToggle() {
    return /** @type {?CrToggleElement} */ (this.$$('#control'));
  },

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
      roaming: {allowRoaming: this.isRoamingAllowedForNetwork_}
    };
    this.networkConfig_.setProperties(this.managedProperties.guid, config)
        .then(response => {
          if (!response.success) {
            console.warn('Unable to set properties: ' + JSON.stringify(config));
          }
        });
    recordSettingChange();
  },

  /**
   * @return {boolean} The value derived from the network state reported by
   * managed properties and whether we are under policy enforcement.
   * @private
   */
  getRoamingAllowedForNetwork_() {
    return !!OncMojo.getActiveValue(
               this.managedProperties.typeProperties.cellular.allowRoaming) &&
        !this.isRoamingProhibitedByPolicy_();
  },

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
  },

  /** @private */
  managedPropertiesChanged_() {
    if (!this.managedProperties ||
        !this.managedProperties.typeProperties.cellular.allowRoaming) {
      return;
    }
    this.isRoamingAllowedForNetwork_ = this.getRoamingAllowedForNetwork_();
  },

  /** @private */
  onCellularRoamingRowClicked_(event) {
    event.stopPropagation();
    if (this.isPerNetworkToggleDisabled_()) {
      return;
    }
    this.isRoamingAllowedForNetwork_ = !this.isRoamingAllowedForNetwork_;
  },

  /** @private */
  isRoamingProhibitedByPolicy_() {
    const dataRoamingEnabled = this.getPref('cros.signed.data_roaming_enabled');
    return !dataRoamingEnabled.value &&
        dataRoamingEnabled.controlledBy ===
        chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
  },

  /** @private */
  isPerNetworkToggleDisabled_() {
    return this.disabled || this.isRoamingProhibitedByPolicy_();
  },

  /** @private */
  showPerNetworkAllowRoamingToggle_() {
    return this.isRoamingAllowedForNetwork_ !== undefined;
  },
});
