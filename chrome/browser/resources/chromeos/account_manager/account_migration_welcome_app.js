// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import './strings.m.js';
import './account_manager_shared.css.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AccountManagerBrowserProxy, AccountManagerBrowserProxyImpl} from './account_manager_browser_proxy.js';
import {getTemplate} from './account_migration_welcome_app.html.js';

Polymer({
  is: 'account-migration-welcome',

  _template: getTemplate(),

  properties: {
    /** @private */
    title_: {
      type: String,
      value: '',
    },
    /** @private */
    body_: {
      type: String,
      value: '',
    },
  },

  /** @type {string} */
  userEmail_: '',

  /** @type {?AccountManagerBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = AccountManagerBrowserProxyImpl.getInstance();

    const dialogArgs = chrome.getVariableValue('dialogArguments');
    if (!dialogArgs) {
      // Only if the user navigates to the URL
      // chrome://account-migration-welcome to debug.
      console.warn('No arguments were provided to the dialog.');
      return;
    }
    const args = JSON.parse(dialogArgs);
    assert(args);
    assert(args.email);
    this.userEmail_ = args.email;

    this.title_ = loadTimeData.getStringF('welcomeTitle', this.userEmail_);
    this.body_ = loadTimeData.getStringF(
        'welcomeMessage', this.userEmail_,
        loadTimeData.getString('accountManagerLearnMoreUrl'));
  },

  /** @private */
  closeDialog_() {
    this.browserProxy_.closeDialog();
  },

  /** @private */
  reauthenticateAccount_() {
    this.browserProxy_.reauthenticateAccount(this.userEmail_);
  },
});
