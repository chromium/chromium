// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A round icon containing the avatar of the signed-in user, or
 * the placeholder avatar if the user is not signed-in.
 */

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {StoredAccount, SyncBrowserProxyImpl} from '../people_page/sync_browser_proxy.js';

Polymer({
  is: 'settings-avatar-icon',

  _template: html`{__html_template__}`,

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * The URL for the currently selected profile icon.
     * @private
     */
    avatarUrl_: {
      type: String,
      value: '',
    },
  },

  /** @override */
  attached() {
    /** @param {!Array<!StoredAccount>} accounts */
    const setAvatarUrl = accounts => {
      this.avatarUrl_ = (accounts.length > 0 && !!accounts[0].avatarImage) ?
          /** @type {string} */ (accounts[0].avatarImage) :
          'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';
    };
    SyncBrowserProxyImpl.getInstance().getStoredAccounts().then(setAvatarUrl);
    this.addWebUIListener('stored-accounts-updated', setAvatarUrl);
  },

});
