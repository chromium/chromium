// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Item in the profile-discovery-list-page list displaying details of an eSIM
 * profile.
 */

import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './cellular_setup_icons.html.js';

import {mojoString16ToString} from '//resources/js/mojo_type_util.js';
import {ESimProfileProperties} from '//resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './profile_discovery_list_item.html.js';

export class ProfileDiscoveryListItemElement extends PolymerElement {
  static get is() {
    return 'profile-discovery-list-item' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      profileProperties: {
        type: Object,
        value: null,
        notify: true,
      },

      selected: {
        type: Boolean,
        reflectToAttribute: true,
      },

      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },
    };
  }

  profileProperties: ESimProfileProperties|null;
  selected: boolean;
  private isDarkModeActive_: boolean;

  private getProfileName_(): string {
    if (!this.profileProperties) {
      return '';
    }
    return mojoString16ToString(this.profileProperties.name);
  }
}

customElements.define(ProfileDiscoveryListItemElement.is,
    ProfileDiscoveryListItemElement);
