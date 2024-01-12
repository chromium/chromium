// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Item in the profile-discovery-list-page list displaying details of an eSIM
 * profile.
 */

import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './cellular_setup_icons.html.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {ESimProfileProperties} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';

import {getTemplate} from './profile_discovery_list_item.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ProfileDiscoveryListItemElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class ProfileDiscoveryListItemElement extends
    ProfileDiscoveryListItemElementBase {
  static get is() {
    return 'profile-discovery-list-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * @type {?ESimProfileProperties}
       */
      profileProperties: {
        type: Object,
        value: null,
        notify: true,
      },

      selected: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /**
       * @type {boolean}
       * @private
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },

    };
  }

  /** @private */
  getProfileName_() {
    if (!this.profileProperties) {
      return '';
    }
    return mojoString16ToString(this.profileProperties.name);
  }
}

customElements.define(
    ProfileDiscoveryListItemElement.is, ProfileDiscoveryListItemElement);
