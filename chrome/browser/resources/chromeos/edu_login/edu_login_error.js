// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './error_screen.js';
import './edu_login_css.js';
import './edu_login_template.js';
import './edu_login_button.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EduAccountLoginBrowserProxyImpl} from './browser_proxy.js';
import {EduLoginErrorType} from './edu_login_util.js';

Polymer({
  is: 'edu-login-error',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {EduLoginErrorType} */
    errorType: String,
  },

  /**
   * @returns {string} error title
   * @private
   */
  getErrorTitle_() {
    switch (this.errorType) {
      case EduLoginErrorType.NO_INTERNET:
        return loadTimeData.getString('accountManagerErrorNoInternetTitle');
      case EduLoginErrorType.CANNOT_ADD_ACCOUNT:
        return loadTimeData.getString(
            'accountManagerErrorCannotAddAccountTitle');
      default:
        return '';
    }
  },

  /**
   * @returns {string} error body
   * @private
   */
  getErrorBody_() {
    switch (this.errorType) {
      case EduLoginErrorType.NO_INTERNET:
        return loadTimeData.getString('accountManagerErrorNoInternetBody');
      case EduLoginErrorType.CANNOT_ADD_ACCOUNT:
        return loadTimeData.getString(
            'accountManagerErrorCannotAddAccountBody');
      default:
        return '';
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  handleCloseButtonClick_(e) {
    e.stopPropagation();
    EduAccountLoginBrowserProxyImpl.getInstance().dialogClose();
  }
});
