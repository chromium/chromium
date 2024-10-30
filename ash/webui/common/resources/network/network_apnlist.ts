// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/md_select.css.js';
import '//resources/ash/common/cr_elements/policy/cr_tooltip_icon.js';
import '../i18n_behavior.js';
import './onc_mojo.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './network_property_list_mojo.js';
import './network_shared.css.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {ApnAuthenticationType, ApnIpType, ApnProperties, ApnSource, ApnState, ApnType, ManagedApnProperties, ManagedProperties} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OncMojo} from '../network/onc_mojo.js';

import {getTemplate} from './network_apnlist.html.js';

const kDefaultAccessPointName = 'NONE';
const kOtherAccessPointName = 'Other';

const USE_ATTACH_APN_ON_SAVE_METRIC_NAME =
    'Network.Cellular.Apn.UseAttachApnOnSave';

const NetworkApnListElementBase = I18nMixin(PolymerElement);

export interface NetworkApnListElement {
  $: {
    selectApn: HTMLSelectElement,
  };
}

export class NetworkApnListElement extends NetworkApnListElementBase {
  static get is() {
    return 'network-apnlist' as const;
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
        observer: 'managedPropertiesChanged',
      },

      /**
       * The name property of the selected APN. If a name property is empty, the
       * accessPointName property will be used. We use 'name' so that multiple
       * APNs with the same accessPointName can be supported, so long as they
       * have a unique 'name' property. This is necessary to allow custom
       * 'other' entries (which are always named 'Other') that match an existing
       * accessPointName but provide a different username/password.
       */
      selectedApn_: {
        type: String,
        value: '',
      },

      /**
       * Selectable list of APN dictionaries for the UI. Includes an entry
       * corresponding to |otherApn| (see below).
       */
      apnSelectList_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * The user settable properties for a new ('other') APN. The values are
       * set to default and will be set to the currently active APN if it does
       * not match an existing list entry.
       */
      otherApn_: {
        type: Object,
        value() {
          return {
            accessPointName: kDefaultAccessPointName,
            name: kOtherAccessPointName,
            state: ApnState.kEnabled,
            authentication: ApnAuthenticationType.kAutomatic,
            ipType: ApnIpType.kAutomatic,
            apnTypes: [ApnType.kDefault],
            source: ApnSource.kUi,
          };
        },
      },

      /**
       * Array of property names to pass to the Other APN property list.
       */
      otherApnFields_: {
        type: Array,
        value() {
          return ['accessPointName', 'username', 'password'];
        },
        readOnly: true,
      },

      /**
       * Edit types to pass to the Other APN property list.
       */
      otherApnEditTypes_: {
        type: Object,
        value() {
          return {
            'accessPointName': 'String',
            'username': 'String',
            'password': 'Password',
          };
        },
        readOnly: true,
      },

      isAttachApnToggleEnabled_: {
        type: Boolean,
        value: false,
      },

    };
  }

  disabled: boolean;
  managedProperties: ManagedProperties;
  private otherApn_: ApnProperties;
  private selectedApn_: string;
  private apnSelectList_: ApnProperties[];
  private isAttachApnToggleEnabled_: boolean;

  /*
   * Returns the select APN SelectElement.
   */
  getApnSelect(): HTMLSelectElement|null {
    return this.shadowRoot!.querySelector<HTMLSelectElement>('#selectApn');
  }

  getApnFromManaged(apn: ManagedApnProperties): ApnProperties {
    return {
      // authentication and language are ignored in this UI.
      accessPointName: OncMojo.getActiveString(apn.accessPointName),
      localizedName: OncMojo.getActiveString(apn.localizedName),
      name: OncMojo.getActiveString(apn.name),
      password: OncMojo.getActiveString(apn.password),
      username: OncMojo.getActiveString(apn.username),
      authentication: ApnAuthenticationType.kAutomatic,
      // Because this UI is for kApnRevamp=false, these are all default
      // values for fields to pass compilation. None of these fields
      // should be used by CrosNetworkConfig.
      state: ApnState.kEnabled,
      ipType: ApnIpType.kAutomatic,
      apnTypes: [ApnType.kDefault],
      source: ApnSource.kModem,
      id: undefined,
      language: undefined,
      attach: undefined,
    };
  }

  getActiveApnFromProperties(managedProperties: ManagedProperties):
      ApnProperties|undefined {
    const cellular = managedProperties.typeProperties.cellular;
    assert(cellular);
    let activeApn;
    // We show selectedAPN as the active entry in the select list but it may
    // not correspond to the currently "active" APN which is represented by
    // lastGoodApn.
    if (cellular.selectedApn) {
      activeApn = this.getApnFromManaged(cellular.selectedApn);
    } else if (cellular.lastGoodApn && cellular.lastGoodApn.accessPointName) {
      activeApn = cellular.lastGoodApn;
    }
    if (activeApn && !activeApn.accessPointName) {
      activeApn = undefined;
    }
    return activeApn;
  }

  shouldUpdateSelectList(oldManagedProperties: ManagedProperties): boolean {
    if (!oldManagedProperties) {
      return true;
    }

    const newActiveApn =
        this.getActiveApnFromProperties(this.managedProperties);
    const oldActiveApn = this.getActiveApnFromProperties(oldManagedProperties);
    if ((newActiveApn && oldActiveApn &&
         !OncMojo.apnMatch(newActiveApn, oldActiveApn)) ||
        (newActiveApn && !oldActiveApn) || (!newActiveApn && oldActiveApn)) {
      return true;
    }
    assert(this.managedProperties);

    const newApnList = this.managedProperties.typeProperties.cellular!.apnList;
    const oldApnList = oldManagedProperties.typeProperties.cellular!.apnList;
    if (!OncMojo.apnListMatch(
            oldApnList && oldApnList.activeValue,
            newApnList && newApnList.activeValue)) {
      return true;
    }

    const newCustomApnList =
        this.managedProperties.typeProperties.cellular!.customApnList;
    const oldCustomApnList =
        oldManagedProperties.typeProperties.cellular!.customApnList;
    if (!OncMojo.apnListMatch(oldCustomApnList, newCustomApnList)) {
      return true;
    }

    return false;
  }

  managedPropertiesChanged(
      managedProperties: ManagedProperties,
      oldManagedProperties: ManagedProperties): void {
    if (!this.shouldUpdateSelectList(oldManagedProperties)) {
      return;
    }
    this.setApnSelectList(this.getActiveApnFromProperties(managedProperties));
  }

  /**
   * Sets the list of selectable APNs for the UI. Appends an 'Other' entry
   * (see comments for |otherApn_| above).
   */
  setApnSelectList(activeApn: ApnProperties|undefined) {
    const apnList = this.generateApnList();
    if (apnList === undefined || apnList.length === 0) {
      // Show other APN when no APN list property is available.
      this.apnSelectList_ = [this.otherApn_];
      this.set('selectedApn_', kOtherAccessPointName);
      return;
    }
    // Get the list entry for activeApn if it exists. It will have 'name' set.
    let activeApnInList;
    if (activeApn) {
      activeApnInList = apnList.find(a => a.name === activeApn.name);
    }

    assert(this.managedProperties);
    assert(this.managedProperties.typeProperties.cellular);
    const customApnList =
        this.managedProperties.typeProperties.cellular.customApnList;
    let otherApn = this.otherApn_;
    if (customApnList && customApnList.length) {
      // If custom apn list exists, then use it's first entry as otherApn.
      otherApn = customApnList[0];
    } else if (!activeApnInList && activeApn && activeApn.accessPointName) {
      // If the active APN is not in the list, copy it to otherApn.
      otherApn = activeApn;
    }
    this.isAttachApnToggleEnabled_ =
        otherApn.attach === OncMojo.USE_ATTACH_APN_NAME;
    this.otherApn_ = {
      accessPointName: otherApn.accessPointName,
      name: otherApn.name,
      username: otherApn.username,
      password: otherApn.password,
      authentication: ApnAuthenticationType.kAutomatic,
      // Because this UI is for kApnRevamp=false, these are all default
      // values for fields to pass compilation. None of these fields
      // should be used by CrosNetworkConfig.
      state: ApnState.kEnabled,
      ipType: ApnIpType.kAutomatic,
      apnTypes: [ApnType.kDefault],
      source: ApnSource.kUi,
      id: otherApn.id,
      language: otherApn.language,
      localizedName: otherApn.localizedName,
      attach: otherApn.attach,
    };
    apnList.push(this.otherApn_);

    this.apnSelectList_ = apnList;
    const selectedApn =
        activeApnInList ? activeApnInList.name : kOtherAccessPointName;
    assert(selectedApn);
    this.set('selectedApn_', selectedApn);

    // Wait for the dom-repeat to populate the <option> entries then explicitly
    // set the selected value.
    this.setSelectedApn();
  }

  async setSelectedApn() {
    this.getApnSelect()!.value = this.selectedApn_;
  }

  /**
   * Returns a modified copy of the APN properties or undefined if the
   * property is not set. All entries in the returned copy will have nonempty
   * name and accessPointName properties.
   */
  generateApnList(): ApnProperties[]|undefined {
    if (!this.managedProperties) {
      return undefined;
    }
    assert(this.managedProperties.typeProperties.cellular);
    const apnList = this.managedProperties.typeProperties.cellular.apnList;
    if (!apnList) {
      return undefined;
    }
    return apnList.activeValue.filter(apn => !!apn.accessPointName).map(apn => {
      return {
        accessPointName: apn.accessPointName,
        localizedName: apn.localizedName,
        name: apn.name || apn.accessPointName || undefined,
        username: apn.username,
        password: apn.password,
        id: apn.id,
        authentication: apn.authentication,
        language: apn.language,
        attach: apn.attach,
        state: apn.state,
        ipType: apn.ipType,
        apnTypes: apn.apnTypes,
        source: apn.source,
      };
    });
  }

  /**
   * Event triggered when the selectApn selection changes.
   */
  private onSelectApnChange_(event: Event) {
    const target = (event.target) as HTMLSelectElement;
    const name = target!.value;
    // When selecting 'Other', don't send a change event unless a valid
    // non-default value has been set for Other.
    if (name === kOtherAccessPointName &&
        (!this.otherApn_.accessPointName ||
         this.otherApn_.accessPointName === kDefaultAccessPointName)) {
      this.selectedApn_ = name;
      return;
    }
    // The change will generate an update which will update selectedApn_ and
    // refresh the UI.
    this.sendApnChange(name);
  }

  /**
   * Event triggered when any 'Other' APN network property changes.
   */
  private onOtherApnChange_(event: CustomEvent<{field: string, value: string}>):
      void {
    // TODO(benchan/stevenjb): Move the toUpperCase logic to shill or
    // onc_translator_onc_to_shill.cc.
    const value = (event.detail.field === 'accessPointName') ?
        event.detail.value.toUpperCase() :
        event.detail.value;
    this.set('otherApn_.' + event.detail.field, value);
    // Don't send a change event for 'Other' until the 'Save' button is tapped.
  }

  /**
   * Event triggered when the Other APN 'Save' button is tapped.
   */
  private onSaveOtherTap_(): void {
    if (this.sendApnChange(this.selectedApn_)) {
      chrome.metricsPrivate.recordBoolean(
          USE_ATTACH_APN_ON_SAVE_METRIC_NAME, this.isAttachApnToggleEnabled_);
    }
  }

  /**
   * Attempts to send the apn-change event. Returns true if it succeeds.
   * @param name The APN name property.
   */
  sendApnChange(name: string): boolean {
    let apn: ApnProperties|undefined;
    if (name === kOtherAccessPointName) {
      if (!this.otherApn_.accessPointName ||
          this.otherApn_.accessPointName === kDefaultAccessPointName) {
        // No valid APN set, do nothing.
        return false;
      }
      apn = {
        accessPointName: this.otherApn_.accessPointName,
        username: this.otherApn_.username,
        password: this.otherApn_.password,
        attach: this.isAttachApnToggleEnabled_ ? OncMojo.USE_ATTACH_APN_NAME :
                                                 '',
        id: undefined,
        authentication: ApnAuthenticationType.kAutomatic,
        language: undefined,
        localizedName: undefined,
        name: undefined,
        state: ApnState.kEnabled,
        ipType: ApnIpType.kAutomatic,
        apnTypes: [ApnType.kDefault],
        source: ApnSource.kUi,
      };
    } else {
      apn = this.apnSelectList_.find(a => a.name === name);
      if (apn === undefined) {
        // Potential edge case if an update is received before this is invoked.
        console.error('Selected APN not in list');
        return false;
      }
    }
    // Add required field with a default value since it's unused when
    // kApnRevamp=false.
    apn!.apnTypes = [ApnType.kDefault];
    this.dispatchEvent(
        new CustomEvent<ApnProperties>('apn-change', {detail: apn}));
    return true;
  }

  private isDisabled_(): boolean {
    return this.disabled || this.selectedApn_ === '';
  }

  private showOtherApn_(): boolean {
    return this.selectedApn_ === kOtherAccessPointName;
  }

  private apnDesc_(apn: ApnProperties): string|undefined {
    assert(apn.name);
    return apn.localizedName || apn.name;
  }

  isApnItemSelected(item: ApnProperties): boolean {
    return item.accessPointName === this.selectedApn_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkApnListElement.is]: NetworkApnListElement;
  }
}

customElements.define(NetworkApnListElement.is, NetworkApnListElement);
