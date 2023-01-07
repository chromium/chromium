// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Page in eSIM Cellular Setup flow shown if an eSIM profile requires a
 * confirmation code to install. This element contains an input for the user to
 * enter the confirmation code.
 */
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './base_page.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ESimProfileProperties, ESimProfileRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';

import {getTemplate} from './confirmation_code_page.html.js';

Polymer({
  _template: getTemplate(),
  is: 'confirmation-code-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * @type {?ESimProfileRemote}
     */
    profile: {
      type: Object,
      observer: 'onProfileChanged_',
    },

    confirmationCode: {
      type: String,
      notify: true,
    },

    showError: {
      type: Boolean,
    },

    /**
     * Indicates the UI is busy with an operation and cannot be interacted with.
     */
    showBusy: {
      type: Boolean,
      value: false,
    },

    /**
     * @type {?ESimProfileProperties}
     * @private
     */
    profileProperties_: {
      type: Object,
      value: null,
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
  onProfileChanged_() {
    if (!this.profile) {
      this.profileProperties_ = null;
      return;
    }
    this.profile.getProperties().then(response => {
      this.profileProperties_ = response.properties;
    });
  },

  /**
   * @param {KeyboardEvent} e
   * @private
   */
  onKeyDown_(e) {
    if (e.key === 'Enter') {
      this.fire('forward-navigation-requested');
    }
    e.stopPropagation();
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowProfileDetails_() {
    return !!this.profile;
  },

  /**
   * @return {string}
   * @private
   */
  getProfileName_() {
    if (!this.profileProperties_) {
      return '';
    }
    return String.fromCharCode(...this.profileProperties_.name.data);
  },

  /**
   * @return {string}
   * @private
   */
  getProfileImage_() {
    return this.isDarkModeActive_ ?
        'chrome://resources/ash/common/cellular_setup/default_esim_profile_dark.svg' :
        'chrome://resources/ash/common/cellular_setup/default_esim_profile.svg';
  },
});
