// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Page in eSIM Setup flow that displays a choice of available eSIM Profiles.
 */

import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import './base_page.js';
import './profile_discovery_list_item.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ESimProfileProperties} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';

import {getTemplate} from './profile_discovery_list_page.html.js';

Polymer({
  _template: getTemplate(),
  is: 'profile-discovery-list-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * @type {Array<!ESimProfileProperties>}
     * @private
     */
    pendingProfileProperties: {
      type: Array,
    },

    /**
     * @type {?ESimProfileProperties}
     * @private
     */
    selectedProfileProperties: {
      type: Object,
      notify: true,
    },

    /**
     * Indicates the UI is busy with an operation and cannot be interacted with.
     */
    showBusy: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * @param {ESimProfileProperties} profileProperties
   * @private
   */
  isProfilePropertiesSelected_(profileProperties) {
    return this.selectedProfileProperties === profileProperties;
  },
});
