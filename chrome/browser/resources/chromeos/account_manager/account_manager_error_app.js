// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './strings.m.js';
import './account_manager_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AccountManagerBrowserProxyImpl} from './account_manager_browser_proxy.js';

Polymer({
  is: 'account-manager-error',

  _template: html`{__html_template__}`,

  properties: {
    /** @private */
    errorTitle_: {
      type: String,
      value: '',
    },
    /** @private */
    errorMessage_: {
      type: String,
      value: '',
    },
    /** @private */
    imageUrl_: {
      type: String,
      value: '',
    },
  },

  /** @override */
  ready() {
    this.errorTitle_ =
        loadTimeData.getString('secondaryAccountsDisabledErrorTitle');
    this.errorMessage_ =
        loadTimeData.getString('secondaryAccountsDisabledErrorMessage');
    document.title = this.errorTitle_;
  },

  /** @private */
  closeDialog_() {
    AccountManagerBrowserProxyImpl.getInstance().closeDialog();
  },
});
