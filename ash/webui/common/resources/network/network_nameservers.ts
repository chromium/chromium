// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying network nameserver options.
 */

import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import '//resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import '//resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import '//resources/ash/common/cr_elements/md_select.css.js';
import './network_shared.css.js';

import {assert} from '//resources/js/assert.js';
import {ManagedProperties} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {IPConfigType} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrRadioGroupElement} from 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';

import {CrPolicyNetworkBehaviorMojo, CrPolicyNetworkBehaviorMojoInterface} from './cr_policy_network_behavior_mojo.js';
import {getTemplate} from './network_nameservers.html.js';
import {OncMojo} from './onc_mojo.js';

/**
 * UI configuration options for nameservers.
 */
enum NameserversType {
  AUTOMATIC = 'automatic',
  CUSTOM = 'custom',
  GOOGLE = 'google',
}

const GOOGLE_NAMESERVERS = [
  '8.8.4.4',
  '8.8.8.8',
];

const EMPTY_NAMESERVER = '0.0.0.0';

const MAX_NAMESERVERS = 4;

const NetworkNameserversElementBase =
    mixinBehaviors([CrPolicyNetworkBehaviorMojo], I18nMixin(PolymerElement)) as
    {
      new (): PolymerElement & I18nMixinInterface &
          CrPolicyNetworkBehaviorMojoInterface,
    };

export class NetworkNameserversElement extends NetworkNameserversElementBase {
  static get is() {
    return 'network-nameservers' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        value: false,
      },

      managedProperties: {
        type: Object,
        observer: 'managedPropertiesChanged_',
      },

      /**
       * Array of nameserver addresses stored as strings.
       */
      nameservers_: {
        type: Array,
      },

      /**
       * The selected nameserver type.
       */
      nameserversType_: {
        type: String,
      },

      /**
       * Enum values for |nameserversType_|.
       */
      nameserversTypeEnum_: {
        readOnly: true,
        type: Object,
        value: NameserversType,
      },

      googleNameserversText_: {
        type: String,
      },

      canChangeConfigType_: {
        type: Boolean,
        computed: 'computeCanChangeConfigType_(managedProperties)',
      },

    };
  }

  disabled: boolean;
  managedProperties: ManagedProperties|undefined;
  // Saved nameservers from the NameserversType.CUSTOM tab. If this is empty, it
  // that the user has not entered any custom nameservers yet.
  private nameservers_: string[] = [];
  private nameserversType_: NameserversType = NameserversType.AUTOMATIC;
  private readonly nameserversTypeEnum_: NameserversType[];
  private googleNameserversText_: string =
      this.i18nAdvanced(
              'networkNameserversGoogle', {substitutions: [], tags: ['a']})
          .toString();
  private canChangeConfigType_: boolean;
  private savedCustomNameservers_: string[] = [];
  // The last manually performed selection of the nameserver type. If this is
  // null, no explicit selection has been done for this network yet.
  private savedNameserversType_: NameserversType|null = null;

  /*
   * Returns the nameserver type CrRadioGroupElement.
   */
  getNameserverRadioButtons(): HTMLElement|null {
    return this.shadowRoot!.querySelector('#nameserverType');
  }

  /**
   * Returns true if the nameservers in |nameservers1| match the nameservers in
   * |nameservers2|, ignoring order and empty / 0.0.0.0 entries.
   */
  private nameserversMatch_(nameservers1: string[], nameservers2: string[]):
      boolean {
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
  }

  /**
   * Returns true if |nameservers| contains any all google nameserver entries
   * and only google nameserver entries or empty entries.
   */
  private isGoogleNameservers_(nameservers: string[]): boolean {
    return this.nameserversMatch_(nameservers, GOOGLE_NAMESERVERS);
  }

  /**
   * Returns the nameservers enforced by policy. If nameservers are not being
   * enforced, returns null.
   */
  private getPolicyEnforcedNameservers_(): string[]|null {
    const staticIpConfig =
        this.managedProperties && this.managedProperties.staticIpConfig;
    if (!staticIpConfig || !staticIpConfig.nameServers) {
      return null;
    }
    return this.getEnforcedPolicyValue(staticIpConfig.nameServers) as string[] |
        null;
  }

  /**
   * Returns the nameservers recommended by policy. If nameservers are not being
   * recommended, returns null. Note: also returns null if nameservers are being
   * enforced by policy.
   */
  private getPolicyRecommendedNameservers_(): string[]|null {
    const staticIpConfig =
        this.managedProperties && this.managedProperties.staticIpConfig;
    if (!staticIpConfig || !staticIpConfig.nameServers) {
      return null;
    }
    return this.getRecommendedPolicyValue(staticIpConfig.nameServers) as
        string[] |
        null;
  }

  private managedPropertiesChanged_(
      newValue: ManagedProperties, oldValue: ManagedProperties): void {
    if (!this.managedProperties) {
      return;
    }

    if (!oldValue || newValue.guid !== oldValue.guid) {
      this.savedCustomNameservers_ = [];
      this.savedNameserversType_ = null;
    }

    // Update the 'nameservers' property.
    let nameservers: string[] = [];
    const ipv4 =
        OncMojo.getIPConfigForType(this.managedProperties, IPConfigType.kIPv4);
    if (ipv4 && ipv4.nameServers) {
      nameservers = ipv4.nameServers.slice();
    }

    // Update the 'nameserversType' property.
    const configType =
        OncMojo.getActiveValue(this.managedProperties.nameServersConfigType);
    let nameserversType;
    if (configType === 'Static') {
      if (this.isGoogleNameservers_(nameservers) &&
          this.savedNameserversType_ !== NameserversType.CUSTOM) {
        nameserversType = NameserversType.GOOGLE;
        nameservers = GOOGLE_NAMESERVERS;  // Use consistent order.
      } else {
        nameserversType = NameserversType.CUSTOM;
      }
    } else {
      nameserversType = NameserversType.AUTOMATIC;
      nameservers = this.clearEmptyNameServers_(nameservers);
    }
    // When a network is connected, we receive connection strength updates and
    // that prevents users from making any custom updates to network
    // nameservers. These below conditions allow connection strength updates to
    // be applied only if network is not connected or if nameservers type is set
    // to auto or if we are receiving the update for the first time.
    if (nameserversType !== NameserversType.CUSTOM || !oldValue ||
        newValue.guid !== (oldValue && oldValue.guid) ||
        !OncMojo.connectionStateIsConnected(
            this.managedProperties.connectionState)) {
      this.setNameservers_(nameserversType, nameservers, false /* send */);
    }
  }

  /**
   * @param sendNameservers If true, send the nameservers once they have been
   *     set in the UI.
   */
  private setNameservers_(
      nameserversType: NameserversType, nameservers: string[],
      sendNameservers: boolean) {
    if (nameserversType === NameserversType.CUSTOM) {
      // Add empty entries for unset custom nameservers.
      for (let i = nameservers.length; i < MAX_NAMESERVERS; ++i) {
        nameservers[i] = EMPTY_NAMESERVER;
      }
    } else {
      nameservers = this.clearEmptyNameServers_(nameservers);
    }
    this.nameservers_ = nameservers;
    this.nameserversType_ = nameserversType;
    if (sendNameservers) {
      this.sendNameServers_();
    }
  }

  /**
   * @return True if the nameservers config type type can be changed.
   */
  private computeCanChangeConfigType_(managedProperties: ManagedProperties):
      boolean {
    if (!managedProperties) {
      return false;
    }
    if (this.isNetworkPolicyEnforced(managedProperties.nameServersConfigType)) {
      return false;
    }
    return true;
  }

  /**
   * @return True if the nameservers are editable.
   */
  private canEditCustomNameServers_(
      nameserversType: string, managedProperties: ManagedProperties): boolean {
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
  }

  private showNameservers_(
      nameserversType: NameserversType, buttonNameserverstype: NameserversType,
      nameservers: string[]): boolean {
    if (nameserversType !== buttonNameserverstype) {
      return false;
    }
    return buttonNameserverstype === NameserversType.CUSTOM ||
        nameservers.length > 0;
  }

  private getNameserversString_(nameservers: string[]): string {
    return nameservers.join(', ');
  }

  /**
   * Returns currently configured custom nameservers, to be used when toggling
   * to 'custom' from 'automatic' or 'google', prefer nameservers in the
   * following priority:
   *
   * 1) policy-enforced nameservers,
   * 2) previously manually entered nameservers (|savedCustomNameservers_|),
   * 3) policy-recommended nameservers,
   * 4) active nameservers (e.g. from DHCP).
   */
  private getCustomNameServers_(): string[] {
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
  }

  /**
   * Event triggered when the selected type changes. Updates nameservers and
   * sends the change value if necessary.
   */
  private onTypeChange_(): void {
    const el =
        this.shadowRoot!.querySelector<CrRadioGroupElement>('#nameserverType');
    assert(el);
    const nameserversType = el.selected as NameserversType;
    this.nameserversType_ = nameserversType;
    this.savedNameserversType_ = nameserversType;
    if (nameserversType === NameserversType.CUSTOM) {
      this.setNameservers_(
          nameserversType, this.getCustomNameServers_(), true /* send */);
      return;
    }
    this.sendNameServers_();
  }

  /**
   * Event triggered when a |nameservers_| value changes through the custom
   * namservers UI.
   * This gets called after data-binding updates a |nameservers_[i]| entry.
   * This saves the custom nameservers and reflects that change to the backend
   * (sending the custom nameservers).
   */
  private onValueChange_(): void {
    this.savedCustomNameservers_ = this.nameservers_.slice();
    this.sendNameServers_();
  }

  /**
   * Sends the current nameservers type (for automatic) or value.
   */
  private sendNameServers_(): void {
    let eventField;
    let eventValue;
    const nameserversType = this.nameserversType_;
    if (nameserversType === NameserversType.CUSTOM) {
      eventField = 'nameServers';
      eventValue = this.nameservers_;
    } else if (nameserversType === NameserversType.GOOGLE) {
      this.nameservers_ = GOOGLE_NAMESERVERS;
      eventField = 'nameServers';
      eventValue = GOOGLE_NAMESERVERS;
    } else {  // nameserversType === NameserversType.AUTOMATIC
      // If not connected, properties will clear. Otherwise they may or may not
      // change so leave them as-is.
      assert(this.managedProperties);
      if (!OncMojo.connectionStateIsConnected(
              this.managedProperties.connectionState)) {
        this.nameservers_ = [];
      } else {
        this.nameservers_ = this.clearEmptyNameServers_(this.nameservers_);
      }
      eventField = 'nameServersConfigType';
      eventValue = 'DHCP';
    }

    const event = new CustomEvent('nameservers-change', {
      bubbles: true,
      composed: true,
      detail: {
        field: eventField,
        value: eventValue,
      },
    });
    this.dispatchEvent(event);
  }

  private clearEmptyNameServers_(nameservers: string[]): string[] {
    return nameservers.filter(
        (nameserver) => (!!nameserver && nameserver !== EMPTY_NAMESERVER));
  }

  private doNothing_(event: Event): void {
    event.stopPropagation();
  }

  /**
   * @return Accessibility label for nameserver input with given index.
   */
  private getCustomNameServerInputA11yLabel_(index: number): string {
    return this.i18n('networkNameserversCustomInputA11yLabel', index + 1);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkNameserversElement.is]: NetworkNameserversElement;
  }
}

customElements.define(NetworkNameserversElement.is, NetworkNameserversElement);
