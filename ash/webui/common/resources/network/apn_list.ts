// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a list of cellular
 * APNs
 */

import '//resources/ash/common/cr_elements/localized_link/localized_link.js';
import './network_shared.css.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import '//resources/ash/common/network/apn_list_item.js';
import '//resources/ash/common/network/apn_detail_dialog.js';
import '//resources/ash/common/network/apn_selection_dialog.js';
import '//resources/ash/common/cr_elements/icons.html.js';

import {assert} from '//resources/js/assert.js';
import {ApnProperties, ApnSource, ApnState, ManagedCellularProperties} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {PortalState} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {afterNextRender, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';

import {ApnDetailDialog} from './apn_detail_dialog.js';
import {getTemplate} from './apn_list.html.js';
import {ApnDetailDialogMode, ApnEventData, isAttachApn, isDefaultApn} from './cellular_utils.js';

const SHILL_INVALID_APN_ERROR = 'invalid-apn';

const ApnListElementBase = I18nMixin(PolymerElement);

export class ApnListElement extends ApnListElementBase {
  static get is() {
    return 'apn-list' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** The GUID of the network to display details for. */
      guid: String,

      managedCellularProperties: {
        type: Object,
      },

      errorState: String,

      portalState: {
        type: Object,
      },

      shouldOmitLinks: {
        type: Boolean,
        value: false,
      },

      shouldDisallowApnModification: {
        type: Boolean,
        value: false,
      },

      apns_: {
        type: Object,
        value: [],
        computed: 'computeApns_(managedCellularProperties)',
      },

      hasEnabledDefaultCustomApn_: {
        type: Boolean,
        computed:
            'computeHasEnabledDefaultCustomApn_(managedCellularProperties)',
      },

      shouldShowApnDetailDialog_: {
        type: Boolean,
        value: false,
      },

      apnDetailDialogMode_: {
        type: Object,
        value: ApnDetailDialogMode.CREATE,
      },

      shouldShowApnSelectionDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  guid: string;
  errorState: string;
  managedCellularProperties: ManagedCellularProperties;
  portalState: PortalState|null;
  shouldOmitLinks: boolean;
  shouldDisallowApnModification: boolean;
  private apns_: [];
  private hasEnabledDefaultCustomApn_: boolean;
  private shouldShowApnDetailDialog_: boolean;
  private apnDetailDialogMode_: ApnDetailDialogMode;
  private shouldShowApnSelectionDialog_: boolean;

  private onZeroStateCreateApnLinkClicked_(e: CustomEvent<{event: Event}>):
      void {
    // A place holder href with the value "#" is used to have a compliant link.
    // This prevents the browser from navigating the window to "#"
    e.detail.event.preventDefault();
    e.stopPropagation();
    this.openApnDetailDialogInCreateMode();
  }

  openApnDetailDialogInCreateMode(): void {
    this.showApnDetailDialog_(ApnDetailDialogMode.CREATE, /* apn= */ undefined);
  }

  openApnSelectionDialog(): void {
    this.shouldShowApnSelectionDialog_ = true;
  }

  private shouldShowZeroStateContent_(): boolean {
    if (!this.managedCellularProperties) {
      return true;
    }

    if (this.managedCellularProperties.connectedApn) {
      return false;
    }

    // Don't display the zero-state text if there's an APN-related error.
    if (this.errorState === SHILL_INVALID_APN_ERROR) {
      return false;
    }

    return !this.getCustomApnList_().length;
  }

  private shouldShowErrorMessage_(): boolean {
    // In some instances, there can be an |errorState| and also a connected APN.
    // Don't show the error message as the network is actually connected.
    if (this.managedCellularProperties &&
        this.managedCellularProperties.connectedApn) {
      return false;
    }

    return this.errorState === SHILL_INVALID_APN_ERROR;
  }

  private getErrorMessage_(): string {
    if (!this.managedCellularProperties || !this.errorState) {
      return '';
    }

    if (this.getCustomApnList_().some(apn => apn.state === ApnState.kEnabled)) {
      return this.i18n('apnSettingsCustomApnsErrorMessage');
    }

    return this.i18n('apnSettingsDatabaseApnsErrorMessage');
  }

  /**
   * Returns an array with all the APN properties that need to be displayed.
   * TODO(b/162365553): Handle managedCellularProperties.apnList.policyValue
   * when policies are included.
   */
  private computeApns_(): ApnProperties[] {
    if (!this.managedCellularProperties) {
      return [];
    }

    const {connectedApn} = this.managedCellularProperties;
    const customApnList = this.getCustomApnList_();

    // Move the connected APN to the front if it exists
    if (connectedApn) {
      const customApnsWithoutConnectedApn =
          customApnList.filter(apn => apn.id !== connectedApn.id);
      return [connectedApn, ...customApnsWithoutConnectedApn];
    }
    return customApnList;
  }

  /**
   * Returns true if the APN on this index is connected.
   */
  private isApnConnected_(index: number): boolean {
    return !!this.managedCellularProperties &&
        !!this.managedCellularProperties.connectedApn && index === 0;
  }

  /**
   * Returns true if currentApn is the only enabled default APN and there is
   * at least one enabled attach APN.
   */
  private shouldDisallowDisablingRemoving_(currentApn: ApnProperties): boolean {
    assert(this.managedCellularProperties);
    if (!currentApn.id) {
      return true;
    }

    const customApnList = this.getCustomApnList_();
    if (!customApnList.some(
            apn => isAttachApn(apn) && !isDefaultApn(apn) &&
                apn.state === ApnState.kEnabled)) {
      return false;
    }

    const defaultEnabledApnList = customApnList.filter(
        apn => isDefaultApn(apn) && apn.state === ApnState.kEnabled);

    return defaultEnabledApnList.length === 1 &&
        currentApn.id === defaultEnabledApnList[0].id;
  }

  /**
   * Returns true if there are no enabled default APNs and the current APN has
   * only an attach APN type.
   */
  private shouldDisallowEnabling_(currentApn: ApnProperties): boolean {
    assert(this.managedCellularProperties);
    if (!currentApn.id) {
      return true;
    }

    if (this.hasEnabledDefaultCustomApn_) {
      return false;
    }

    return isAttachApn(currentApn) && !isDefaultApn(currentApn);
  }

  private onShowApnDetailDialog_(event: CustomEvent<ApnEventData>): void {
    event.stopPropagation();
    if (this.shouldShowApnDetailDialog_) {
      return;
    }
    const eventData = event.detail;
    this.showApnDetailDialog_(eventData.mode, eventData.apn);
  }

  private showApnDetailDialog_(
      mode: ApnDetailDialogMode, apn: ApnProperties|undefined): void {
    this.shouldShowApnDetailDialog_ = true;
    this.apnDetailDialogMode_ = mode;
    // Added to ensure dom-if stamping.
    afterNextRender(this, () => {
      const apnDetailDialog =
          this.shadowRoot!.querySelector<ApnDetailDialog>('#apnDetailDialog');
      assert(!!apnDetailDialog);
      apnDetailDialog.apnProperties = apn;
    });
  }

  private onApnDetailDialogClose_(): void {
    this.shouldShowApnDetailDialog_ = false;
  }

  private onApnSelectionDialogClose_(): void {
    this.shouldShowApnSelectionDialog_ = false;
  }

  private getCustomApnList_(): ApnProperties[] {
    return this.managedCellularProperties?.customApnList ?? [];
  }

  private computeHasEnabledDefaultCustomApn_(): boolean {
    return this.getCustomApnList_().some(
        (apn) => apn.state === ApnState.kEnabled && isDefaultApn(apn));
  }

  private getValidDatabaseApnList_(): ApnProperties[] {
    const databaseApnList =
        this.managedCellularProperties?.apnList?.activeValue ?? [];
    return databaseApnList.filter((apn) => {
      if (apn.source !== ApnSource.kModb) {
        return false;
      }

      // Only APNs that have type default are allowed unless an enabled custom
      // APN of type default already exists. In that case, APNs that are of type
      // attach are also permitted.
      return isDefaultApn(apn) ||
          (this.hasEnabledDefaultCustomApn_ && isAttachApn(apn));
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ApnListElement.is]: ApnListElement;
  }
}

customElements.define(ApnListElement.is, ApnListElement);
