// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './profile_picker_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ManageProfilesBrowserProxy, ManageProfilesBrowserProxyImpl, ProfileState} from './manage_profiles_browser_proxy.js';

Polymer({
  is: 'profile-switch',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {ProfileState} */
    profileState_: {
      type: Object,
    },

    /** @type {boolean} */
    isProfileStateInitialized_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {?ManageProfilesBrowserProxy} */
  manageProfilesBrowserProxy_: null,

  /** @override */
  ready() {
    this.manageProfilesBrowserProxy_ =
        ManageProfilesBrowserProxyImpl.getInstance();
    this.manageProfilesBrowserProxy_.getSwitchProfile().then(profileState => {
      this.profileState_ = profileState;
      this.isProfileStateInitialized_ = true;
    });
  },

  /** @private */
  onCancelClick_() {
    this.manageProfilesBrowserProxy_.cancelProfileSwitch();
  },

  /** @private */
  onSwitchClick_() {
    this.manageProfilesBrowserProxy_.confirmProfileSwitch(
        this.profileState_.profilePath);
  },
});
