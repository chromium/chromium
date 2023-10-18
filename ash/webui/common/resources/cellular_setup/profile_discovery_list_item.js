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

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ESimProfileProperties} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';

import {getTemplate} from './profile_discovery_list_item.html.js';

Polymer({
  _template: getTemplate(),
  is: 'profile-discovery-list-item',

  behaviors: [I18nBehavior],

  properties: {
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
  },

  /** @private */
  getProfileName_() {
    if (!this.profileProperties) {
      return '';
    }
    return String.fromCharCode(...this.profileProperties.name.data);
  },
});
