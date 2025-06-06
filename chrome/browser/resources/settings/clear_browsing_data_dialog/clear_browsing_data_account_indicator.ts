// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-clear-browsing-data-account-indicator' is an
 * indicator that informs users of the primary signed-in account.
 */
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';

import {assert} from '//resources/js/assert.js';
import type {StoredAccount, SyncBrowserProxy, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './clear_browsing_data_account_indicator.html.js';
import {canDeleteAccountData} from './clear_browsing_data_signin_util.js';

const SettingsClearBrowsingDataAccountIndicatorBase =
    WebUiListenerMixin(PolymerElement);

export class SettingsClearBrowsingDataAccountIndicator extends
    SettingsClearBrowsingDataAccountIndicatorBase {
  static get is() {
    return 'settings-clear-browsing-data-account-indicator';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      shouldShowAccountIndicator_: {
        type: Boolean,
        value: false,
        computed: 'computeShouldShowAvatarRow_(shownAccount_,' +
            'syncStatus_.signedInState)',
      },

      /**
       * The primary signed-in account.
       */
      shownAccount_: String,

      /**
       * The current sync status, supplied by SyncBrowserProxy.
       */
      syncStatus_: Object,
    };
  }

  declare private shouldShowAccountIndicator_: boolean;
  declare private shownAccount_: StoredAccount|null;
  declare private syncStatus_: SyncStatus|undefined;

  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.syncBrowserProxy_.getStoredAccounts().then(
        this.handleStoredAccounts_.bind(this));
    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));

    this.addWebUiListener(
        'stored-accounts-updated', this.handleStoredAccounts_.bind(this));
    this.addWebUiListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));
  }

  /**
   * Computes the shown account from the StoredAccounts list. The shown account
   * is the primary account which is the first element in the StoredAccounts
   * list.
   */
  private handleStoredAccounts_(accounts: StoredAccount[]) {
    if (!accounts) {
      this.shownAccount_ = null;
      return;
    }

    this.shownAccount_ = (accounts.length > 0) ? accounts[0] : null;
  }

  private handleSyncStatus_(syncStatus: SyncStatus) {
    this.syncStatus_ = syncStatus;
  }

  /**
   * Determines when the account indicator should be shown, in the case where
   * account data would be deleted.
   */
  private computeShouldShowAvatarRow_() {
    if (!this.shownAccount_) {
      return false;
    }
    return canDeleteAccountData(this.syncStatus_);
  }

  private getProfileImageSrc_(): string {
    assert(this.shownAccount_);

    // image can be undefined if the account has not set an avatar photo.
    return this.shownAccount_.avatarImage ||
        'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-clear-browsing-data-account-indicator':
        SettingsClearBrowsingDataAccountIndicator;
  }
}

customElements.define(
    SettingsClearBrowsingDataAccountIndicator.is,
    SettingsClearBrowsingDataAccountIndicator);
