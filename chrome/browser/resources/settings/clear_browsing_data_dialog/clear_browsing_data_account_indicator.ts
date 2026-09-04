// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-clear-browsing-data-account-indicator' is an
 * indicator that informs users of the primary signed-in account.
 */

import {assert} from '//resources/js/assert.js';
import type {StoredAccount, SyncBrowserProxy, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './clear_browsing_data_account_indicator.css.js';
import {getHtml} from './clear_browsing_data_account_indicator.html.js';
import {canDeleteAccountData} from './clear_browsing_data_signin_util.js';

const SettingsClearBrowsingDataAccountIndicatorElementBase =
    WebUiListenerMixinLit(CrLitElement);

export type ClearBrowsingDataAccountIndicatorElement =
    SettingsClearBrowsingDataAccountIndicatorElement;

export class SettingsClearBrowsingDataAccountIndicatorElement extends
    SettingsClearBrowsingDataAccountIndicatorElementBase {
  static get is() {
    return 'settings-clear-browsing-data-account-indicator';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      shouldShowAccountIndicator_: {type: Boolean},

      /**
       * The primary signed-in account.
       */
      shownAccount_: {type: Object},

      /**
       * The current sync status, supplied by SyncBrowserProxy.
       */
      syncStatus_: {type: Object},
    };
  }

  protected accessor shouldShowAccountIndicator_: boolean = false;
  protected accessor shownAccount_: StoredAccount|null = null;
  private accessor syncStatus_: SyncStatus|undefined = undefined;

  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.syncBrowserProxy_.getStoredAccounts().then(
        this.handleStoredAccounts_.bind(this));
    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));

    this.addWebUiListener(
        'stored-accounts-updated', this.handleStoredAccounts_.bind(this));
    this.addWebUiListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('shownAccount_') ||
        changedPrivateProperties.has('syncStatus_')) {
      this.shouldShowAccountIndicator_ = this.computeShouldShowAvatarRow_();
    }
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

  protected getProfileImageSrc_(): string {
    assert(this.shownAccount_);

    // image can be undefined if the account has not set an avatar photo.
    return this.shownAccount_.avatarImage ||
        'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-clear-browsing-data-account-indicator':
        SettingsClearBrowsingDataAccountIndicatorElement;
  }
}

customElements.define(
    SettingsClearBrowsingDataAccountIndicatorElement.is,
    SettingsClearBrowsingDataAccountIndicatorElement);
