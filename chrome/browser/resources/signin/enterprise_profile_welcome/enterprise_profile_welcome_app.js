// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './strings.m.js';
import './signin_shared_css.js';
import './signin_vars_css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EnterpriseProfileInfo, EnterpriseProfileWelcomeBrowserProxy, EnterpriseProfileWelcomeBrowserProxyImpl} from './enterprise_profile_welcome_browser_proxy.js';

Polymer({
  is: 'enterprise-profile-welcome-app',

  _template: html`{__html_template__}`,

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {
    /** Whether the account is managed */
    showEnterpriseBadge_: {
      type: Boolean,
      value: false,
    },

    /** URL for the profile picture */
    pictureUrl_: {
      type: String,
    },

    /** The title about enterprise management */
    enterpriseTitle_: {
      type: String,
    },

    /** The detailed info about enterprise management */
    enterpriseInfo_: {
      type: String,
    },

    /** The label for the button to proceed with the flow */
    proceedLabel_: {
      type: String,
    },
  },

  /** @private {?EnterpriseProfileWelcomeBrowserProxy} */
  enterpriseProfileWelcomeBrowserProxy_: null,

  /** @override */
  ready() {
    this.enterpriseProfileWelcomeBrowserProxy_ =
        EnterpriseProfileWelcomeBrowserProxyImpl.getInstance();
    this.addWebUIListener(
        'on-profile-info-changed',
        (/** @type {!EnterpriseProfileInfo} */ info) =>
            this.setProfileInfo_(info));
    this.enterpriseProfileWelcomeBrowserProxy_.initialized().then(
        info => this.setProfileInfo_(info));
  },

  /**
   * Called when the proceed button is clicked.
   * @private
   */
  onProceed_() {
    this.enterpriseProfileWelcomeBrowserProxy_.proceed();
  },

  /**
   * Called when the cancel button is clicked.
   * @private
   */
  onCancel_() {
    this.enterpriseProfileWelcomeBrowserProxy_.cancel();
  },

  /**
   * @param {!EnterpriseProfileInfo} info
   * @private
   */
  setProfileInfo_(info) {
    this.style.setProperty('--header-background-color', info.backgroundColor);
    this.pictureUrl_ = info.pictureUrl;
    this.showEnterpriseBadge_ = info.showEnterpriseBadge;
    this.enterpriseTitle_ = info.enterpriseTitle;
    this.enterpriseInfo_ = info.enterpriseInfo;
    this.proceedLabel_ = info.proceedLabel;
  },
});