// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Page in eSIM Setup flow that displays a choice of available eSIM Profiles.
 */

import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import './base_page.js';
import './profile_discovery_list_item_legacy.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ESimProfileRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';

import {getTemplate} from './profile_discovery_list_page_legacy.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ProfileDiscoveryListPageLegacyElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class ProfileDiscoveryListPageLegacyElement extends
    ProfileDiscoveryListPageLegacyElementBase {
  static get is() {
    return 'profile-discovery-list-page-legacy';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * @type {Array<!ESimProfileRemote>}
       * @private
       */
      pendingProfiles: Array,

      /**
       * @type {?ESimProfileRemote}
       * @private
       */
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

  /**
   * @param {ESimProfileRemote} profile
   * @private
   */
  isProfileSelected_(profile) {
    return this.selectedProfile === profile;
  }
}

customElements.define(
    ProfileDiscoveryListPageLegacyElement.is,
    ProfileDiscoveryListPageLegacyElement);
