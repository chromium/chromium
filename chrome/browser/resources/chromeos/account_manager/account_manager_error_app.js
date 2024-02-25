// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import './strings.m.js';
import './account_manager_shared.css.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AccountManagerBrowserProxyImpl} from './account_manager_browser_proxy.js';
import {getTemplate} from './account_manager_error_app.html.js';

Polymer({
  is: 'account-manager-error',

  _template: getTemplate(),

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
