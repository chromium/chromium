// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A round icon containing the avatar of the signed-in user, or
 * the placeholder avatar if the user is not signed-in.
 */

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {StoredAccount, SyncBrowserProxyImpl} from '../people_page/sync_browser_proxy.js';

import {getTemplate} from './avatar_icon.html.js';

const SettingsAvatarIconElementBase = WebUiListenerMixin(PolymerElement);

class SettingsAvatarIconElement extends SettingsAvatarIconElementBase {
  static get is() {
    return 'settings-avatar-icon';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The URL for the currently selected profile icon.
       */
      avatarUrl_: {
        type: String,
        value: '',
      },
    };
  }

  private avatarUrl_: string;

  override connectedCallback() {
    super.connectedCallback();

    const setAvatarUrl = (accounts: StoredAccount[]) => {
      this.avatarUrl_ = (accounts.length > 0 && !!accounts[0].avatarImage) ?
          accounts[0].avatarImage :
          'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';
    };
    SyncBrowserProxyImpl.getInstance().getStoredAccounts().then(setAvatarUrl);
    this.addWebUiListener('stored-accounts-updated', setAvatarUrl);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-avatar-icon': SettingsAvatarIconElement;
  }
}

customElements.define(SettingsAvatarIconElement.is, SettingsAvatarIconElement);
