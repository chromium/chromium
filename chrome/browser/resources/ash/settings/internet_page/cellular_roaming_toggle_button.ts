// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cellular-roaming-toggle-button' is responsible for encapsulating the
 * cellular roaming configuration logic, and in particular the details behind
 * the transition to a more granular approach to roaming configuration.
 */

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {CrPolicyNetworkBehaviorMojo} from 'chrome://resources/ash/common/network/cr_policy_network_behavior_mojo.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrosNetworkConfigInterface, ManagedProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {PolicySource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';

import {getTemplate} from './cellular_roaming_toggle_button.html.js';

const CellularRoamingToggleButtonElementBase =
    PrefsMixin(I18nMixin(PolymerElement));

export class CellularRoamingToggleButtonElement extends
    CellularRoamingToggleButtonElementBase {
  static get is() {
    return 'cellular-roaming-toggle-button' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      managedProperties: {
        type: Object,
      },

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

  disabled: boolean;
  managedProperties: ManagedProperties|undefined;
  private isRoamingAllowedForNetwork_: boolean;
  private networkConfig_: CrosNetworkConfigInterface;

  constructor() {
    super();

    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  /**
   * Returns the child element responsible for controlling cellular roaming.
   */
  getCellularRoamingToggle(): CrToggleElement|null {
    return this.shadowRoot!.querySelector<CrToggleElement>(
        '#cellularRoamingToggle');
  }

  private isRoamingAllowedForNetworkChanged_(): void {
    assert(this.networkConfig_);
    if (!this.managedProperties ||
        !this.managedProperties.typeProperties.cellular!.allowRoaming) {
      return;
    }
    const config =
        OncMojo.getDefaultConfigProperties(this.managedProperties.type);
    config.typeConfig.cellular = {
      roaming: {
        allowRoaming: this.isRoamingAllowedForNetwork_,
      },
      apn: undefined,
      textMessageAllowState: undefined,
    };
    this.networkConfig_.setProperties(this.managedProperties.guid, config)
        .then(response => {
          if (response.success) {
            recordSettingChange(
                Setting.kCellularRoaming,
                {boolValue: this.isRoamingAllowedForNetwork_});
          } else {
            console.warn('Unable to set properties: ' + JSON.stringify(config));
          }
        });
  }

  /**
   * @return The value derived from the network state reported by
   * managed properties and whether we are under policy enforcement.
   */
  private getRoamingAllowedForNetwork_(): boolean {
    return !!OncMojo.getActiveValue(
               this.managedProperties!.typeProperties.cellular!.allowRoaming) &&
        !this.isRoamingProhibitedByPolicy_();
  }

  private getRoamingDetails_(): string {
    if (this.managedProperties!.typeProperties.cellular!.roamingState ===
        'Required') {
      return this.i18n('networkAllowDataRoamingRequired');
    }
    if (!this.getRoamingAllowedForNetwork_()) {
      return this.i18n('networkAllowDataRoamingDisabled');
    }
    return this.managedProperties!.typeProperties.cellular!.roamingState ===
            'Roaming' ?
        this.i18n('networkAllowDataRoamingEnabledRoaming') :
        this.i18n('networkAllowDataRoamingEnabledHome');
  }

  private managedPropertiesChanged_(): void {
    if (!this.managedProperties ||
        !this.managedProperties!.typeProperties.cellular!.allowRoaming) {
      return;
    }

    // We override the enforcement of the managed property here so that we can
    // have the toggle show the policy enforcement icon when the global policy
    // prohibits roaming.
    if (this.isRoamingProhibitedByPolicy_()) {
      this.set(
          'managedProperties.typeProperties.cellular.allowRoaming.policySource',
          PolicySource.kDevicePolicyEnforced);
    }
    this.isRoamingAllowedForNetwork_ = this.getRoamingAllowedForNetwork_();
  }

  private onCellularRoamingRowClicked_(event: Event): void {
    event.stopPropagation();
    if (this.isPerNetworkToggleDisabled_()) {
      return;
    }
    this.isRoamingAllowedForNetwork_ = !this.isRoamingAllowedForNetwork_;
  }

  private isRoamingProhibitedByPolicy_(): boolean {
    const dataRoamingEnabled = this.getPref('cros.signed.data_roaming_enabled');
    return !dataRoamingEnabled.value &&
        dataRoamingEnabled.controlledBy ===
        chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
  }

  private isPerNetworkToggleDisabled_(): boolean {
    return this.disabled || this.isRoamingProhibitedByPolicy_() ||
        CrPolicyNetworkBehaviorMojo.isNetworkPolicyEnforced(
            this.managedProperties!.typeProperties.cellular!.allowRoaming!);
  }

  private showPerNetworkAllowRoamingToggle_(): boolean {
    return this.isRoamingAllowedForNetwork_ !== undefined;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CellularRoamingToggleButtonElement.is]: CellularRoamingToggleButtonElement;
  }
}

customElements.define(
    CellularRoamingToggleButtonElement.is, CellularRoamingToggleButtonElement);
