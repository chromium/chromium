// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Page in eSIM Setup flow that displays a choice of available eSIM Profiles.
 */

import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import '//resources/cr_components/localized_link/localized_link.js';
import './base_page.js';
import './profile_discovery_list_item.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ESimProfileProperties} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';

import {getTemplate} from './profile_discovery_list_page.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ProfileDiscoveryListPageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class ProfileDiscoveryListPageElement extends
    ProfileDiscoveryListPageElementBase {
  static get is() {
    return 'profile-discovery-list-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * @type {Array<!ESimProfileProperties>}
       * @private
       */
      pendingProfileProperties: Array,

      /**
       * @type {?ESimProfileProperties}
       * @private
       */
      selectedProfileProperties: {
        type: Object,
        notify: true,
      },

    };
  }

  /**
   * @param {ESimProfileProperties} profileProperties
   * @private
   */
  isProfilePropertiesSelected_(profileProperties) {
    return this.selectedProfileProperties === profileProperties;
  }

  /**
   * @param {Event} e
   * @private
   */
  enterManuallyClicked_(e) {
    e.detail.event.preventDefault();
    e.stopPropagation();
    this.selectedProfileProperties = null;
    this.dispatchEvent(new CustomEvent('forward-navigation-requested', {
      bubbles: true,
      composed: true,
    }));
  }
}

customElements.define(
    ProfileDiscoveryListPageElement.is, ProfileDiscoveryListPageElement);
