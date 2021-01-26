/* Copyright 2021 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './strings.m.js';
import './signin_shared_css.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {WorkProfileConfirmationBrowserProxy, WorkProfileConfirmationBrowserProxyImpl} from './work_profile_confirmation_browser_proxy.js';

Polymer({
  is: 'work-profile-confirmation-app',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @private {string} */
    accountImageSrc_: {
      type: String,
      value() {
        return loadTimeData.getString('accountPictureUrl');
      },
    },
  },

  /** @private {?WorkProfileConfirmationBrowserProxy} */
  workProfileConfirmationBrowserProxy_: null,

  /** @override */
  ready() {
    this.workProfileConfirmationBrowserProxy_ =
        WorkProfileConfirmationBrowserProxyImpl.getInstance();
    this.addWebUIListener(
        'account-image-changed', this.handleAccountImageChanged_.bind(this));
    this.workProfileConfirmationBrowserProxy_.requestAccountImage();
  },

  /** @private */
  onConfirm_() {
    this.workProfileConfirmationBrowserProxy_.confirm();
  },

  /** @private */
  onCancel_() {
    this.workProfileConfirmationBrowserProxy_.cancel();
  },

  /**
   * Called when the account image changes.
   * @param {string} imageSrc
   * @private
   */
  handleAccountImageChanged_(imageSrc) {
    this.accountImageSrc_ = imageSrc;
  },
});
