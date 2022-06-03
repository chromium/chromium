// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './account_manager_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AccountManagerBrowserProxyImpl} from './account_manager_browser_proxy.js';

Polymer({
  is: 'account-manager-welcome',

  _template: html`{__html_template__}`,

  /** @private */
  closeDialog_() {
    AccountManagerBrowserProxyImpl.getInstance().closeDialog();
  },
});
