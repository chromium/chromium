// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import './account_manager_shared_css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {InlineLoginBrowserProxyImpl} from './inline_login_browser_proxy.js';

Polymer({
  is: 'welcome-page-app',

  _template: html`{__html_template__}`,

  /** @override */
  ready() {
    this.$$('#osSettingsLink')
        .addEventListener(
            'click',
            () => this.dispatchEvent(new CustomEvent('opened-new-window')));
    const incognitoLink = this.$$('#incognitoLink');
    if (incognitoLink) {
      incognitoLink.addEventListener('click', () => this.openIncognitoLink_());
    }
  },

  /** @return {boolean} */
  isSkipCheckboxChecked() {
    return this.$.checkbox.checked;
  },

  /**
   * @return {string}
   * @private
   */
  getWelcomeTitle_() {
    return loadTimeData.getStringF(
        'accountManagerDialogWelcomeTitle', loadTimeData.getString('userName'));
  },

  /** @private */
  openIncognitoLink_() {
    InlineLoginBrowserProxyImpl.getInstance().showIncognito();
    this.dispatchEvent(new CustomEvent('opened-new-window'));
  },
});
