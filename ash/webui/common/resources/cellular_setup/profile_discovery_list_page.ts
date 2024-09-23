// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Page in eSIM Setup flow that displays a choice of available eSIM Profiles.
 */

import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import '//resources/ash/common/cr_elements/localized_link/localized_link.js';
import './base_page.js';
import './profile_discovery_list_item.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {MojoInterfaceProviderImpl} from '//resources/ash/common/network/mojo_interface_provider.js';
import {assert} from '//resources/js/assert.js';
import {ESimProfileProperties} from '//resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {NetworkType} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ProfileDiscoveryListItemElement} from './profile_discovery_list_item.js';
import {getTemplate} from './profile_discovery_list_page.html.js';

const ProfileDiscoveryListPageElementBase = I18nMixin(PolymerElement);

export class ProfileDiscoveryListPageElement extends
    ProfileDiscoveryListPageElementBase {
  static get is() {
    return 'profile-discovery-list-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      pendingProfileProperties: Array,

      selectedProfileProperties: {
        type: Object,
        notify: true,
      },
      /**
       * If true, device is locked to specific cellular operator.
       */
      isDeviceCarrierLocked_: {
        type: Boolean,
        value: false,
      },
    };
  }

  pendingProfileProperties: ESimProfileProperties[];
  selectedProfileProperties: ESimProfileProperties|null;
  private isDeviceCarrierLocked_: boolean;

  attemptToFocusOnFirstProfile(): boolean {
    if (!this.pendingProfileProperties ||
        this.pendingProfileProperties.length === 0) {
      return false;
    }

    const items =
        this.shadowRoot!.querySelectorAll('profile-discovery-list-item');
    const item = items[0] as ProfileDiscoveryListItemElement;
    assert(items.length > 0);
    item.focus();
    item.setAttribute('selected', 'true');
    this.selectedProfileProperties = item.profileProperties;
    return true;
  }

  private isProfilePropertiesSelected_(profileProperties:
                                           ESimProfileProperties): boolean {
    return this.selectedProfileProperties === profileProperties;
  }

  constructor() {
    super();

    const networkConfig =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
    networkConfig!.getDeviceStateList().then(response => {
      const devices = response.result;
      const deviceState =
          devices.find(device => device.type === NetworkType.kCellular) || null;
      if (deviceState) {
        this.isDeviceCarrierLocked_ = deviceState.isCarrierLocked;
      }
    });
  }

  private shouldShowCarrierLockWarning_(): boolean {
    return this.isDeviceCarrierLocked_;
  }

  private enterManuallyClicked_(e: CustomEvent): void {
    e.detail.event.preventDefault();
    e.stopPropagation();
    this.selectedProfileProperties = null;
    this.dispatchEvent(new CustomEvent('forward-navigation-requested', {
      bubbles: true,
      composed: true,
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ProfileDiscoveryListPageElement.is]: ProfileDiscoveryListPageElement;
  }
}

customElements.define(
    ProfileDiscoveryListPageElement.is, ProfileDiscoveryListPageElement);
