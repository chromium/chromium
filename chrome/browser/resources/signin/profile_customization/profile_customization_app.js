// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/customize_themes/customize_themes.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './strings.m.js';
import './signin_shared_css.js';
import './signin_vars_css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ProfileCustomizationBrowserProxy, ProfileCustomizationBrowserProxyImpl, ProfileInfo} from './profile_customization_browser_proxy.js';

Polymer({
  is: 'profile-customization-app',

  _template: html`{__html_template__}`,

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {
    /** Whether the account is managed (Enterprise) */
    isManaged_: {
      type: Boolean,
      value: false,
    },

    /** Local profile name, editable by user input */
    profileName_: {
      type: String,
      value: '',
    },

    /** URL for the profile picture */
    pictureUrl_: {
      type: String,
    },

    /** Welcome title for the bubble */
    welcomeTitle_: {
      type: String,
    },
  },

  /** @private {?ProfileCustomizationBrowserProxy} */
  profileCustomizationBrowserProxy_: null,

  /** @override */
  ready() {
    // profileName_ is only set now, because it triggers a validation of the
    // input which crashes if it's done too early.
    this.profileName_ = loadTimeData.getString('profileName');
    this.profileCustomizationBrowserProxy_ =
        ProfileCustomizationBrowserProxyImpl.getInstance();
    this.addWebUIListener(
        'on-profile-info-changed',
        (/** @type {!ProfileInfo} */ info) => this.setProfileInfo_(info));
    this.profileCustomizationBrowserProxy_.initialized().then(
        info => this.setProfileInfo_(info));
  },

  /**
   * Called when the Done button is clicked. Sends the profile name back to
   * native.
   * @private
   */
  onDoneCustomizationClicked_() {
    this.profileCustomizationBrowserProxy_.done(this.profileName_);
  },

  /**
   * Returns whether the Done button should be disabled.
   * @return Boolean
   * @private
   */
  isDoneButtonDisabled_() {
    return !this.profileName_ || !this.$.nameInput.validate();
  },

  /**
   * @param {!ProfileInfo} profileInfo
   * @private
   */
  setProfileInfo_(profileInfo) {
    this.style.setProperty(
        '--header-background-color', profileInfo.backgroundColor);
    this.pictureUrl_ = profileInfo.pictureUrl;
    this.isManaged_ = profileInfo.isManaged;
    this.welcomeTitle_ = profileInfo.welcomeTitle;
  },
});
