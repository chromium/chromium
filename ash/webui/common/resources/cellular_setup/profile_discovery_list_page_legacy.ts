// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Page in eSIM Setup flow that displays a choice of available eSIM Profiles.
 */

import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import './base_page.js';
import './profile_discovery_list_item_legacy.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {ESimProfileRemote} from '//resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './profile_discovery_list_page_legacy.html.js';

const ProfileDiscoveryListPageLegacyElementBase = I18nMixin(PolymerElement);

export class ProfileDiscoveryListPageLegacyElement extends
    ProfileDiscoveryListPageLegacyElementBase {
  static get is() {
    return 'profile-discovery-list-page-legacy' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      pendingProfiles: Array,

      selectedProfile: {
        type: Object,
        notify: true,
      },

      /**
       * Indicates the UI is busy with an operation and cannot be interacted
       * with.
       */
      showBusy: {
        type: Boolean,
        value: false,
      },
    };
  }

  pendingProfiles: ESimProfileRemote[];
  selectedProfile: ESimProfileRemote|null;
  showBusy: boolean;

  private isProfileSelected_(profile: ESimProfileRemote): boolean {
    return this.selectedProfile === profile;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ProfileDiscoveryListPageLegacyElement.is]:
        ProfileDiscoveryListPageLegacyElement;
  }
}


customElements.define(
    ProfileDiscoveryListPageLegacyElement.is,
    ProfileDiscoveryListPageLegacyElement);
