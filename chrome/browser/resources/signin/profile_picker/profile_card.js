// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './profile_card_menu.js';
import './profile_picker_shared_css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ManageProfilesBrowserProxy, ManageProfilesBrowserProxyImpl, ProfileState} from './manage_profiles_browser_proxy.js';

Polymer({
  is: 'profile-card',

  _template: html`{__html_template__}`,

  properties: {
    /**  @type {!ProfileState} */
    profileState: {
      type: Object,
    },

    pattern_: {
      type: String,
      value: '.*\\S.*',
    },
  },

  /** @private {ManageProfilesBrowserProxy} */
  manageProfilesBrowserProxy_: null,

  /** @override */
  ready() {
    this.manageProfilesBrowserProxy_ =
        ManageProfilesBrowserProxyImpl.getInstance();
  },

  /** @private */
  onProfileClick_() {
    this.manageProfilesBrowserProxy_.launchSelectedProfile(
        this.profileState.profilePath);
  },

  /**
   * Handler for when the profile name field is changed, then blurred.
   * @param {!Event} event
   * @private
   */
  onProfileNameChanged_(event) {
    if (event.target.invalid) {
      return;
    }

    this.manageProfilesBrowserProxy_.setProfileName(
        this.profileState.profilePath, event.target.value);

    event.target.blur();
  },

  /**
   * Handler for profile name keydowns.
   * @param {!Event} event
   * @private
   */
  onProfileNameKeydown_(event) {
    if (event.key === 'Escape' || event.key === 'Enter') {
      event.target.blur();
    }
  },

  /**
   * Handler for profile name blur.
   * @private
   */
  onProfileNameInputBlur_() {
    if (this.$.nameInput.invalid) {
      this.$.nameInput.value = this.profileState.localProfileName;
    }
  },
});
