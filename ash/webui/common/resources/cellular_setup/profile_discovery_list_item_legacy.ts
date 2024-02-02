// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Item in the profile-discovery-list-page-legacy list displaying details of
 * an eSIM profile.
 */

import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './cellular_setup_icons.html.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {ESimProfileProperties, ESimProfileRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';

import {getTemplate} from './profile_discovery_list_item_legacy.html.js';

export class ProfileDiscoveryListItemLegacyElement extends PolymerElement {
  static get is() {
    return 'profile-discovery-list-item-legacy' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      profile: {
        type: Object,
        value: null,
        observer: 'onProfileChanged_',
      },

      selected: {
        type: Boolean,
        reflectToAttribute: true,
      },

      showLoadingIndicator: Boolean,

      profileProperties_: {
        type: Object,
        value: null,
        notify: true,
      },

      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },

    };
  }

  profile: ESimProfileRemote|null;
  selected: boolean;
  showLoadingIndicator: boolean;
  private profileProperties_: ESimProfileProperties|null;
  private isDarkModeActive_: boolean;

  private async onProfileChanged_(): Promise<void> {
    if (!this.profile) {
      this.profileProperties_ = null;
      return;
    }
    const response = await this.profile.getProperties();
    this.profileProperties_ = response.properties;
  }

  private getProfileName_(): string {
    if (!this.profileProperties_) {
      return '';
    }
    return mojoString16ToString(this.profileProperties_.name);
  }
}

customElements.define(
    ProfileDiscoveryListItemLegacyElement.is,
    ProfileDiscoveryListItemLegacyElement);
