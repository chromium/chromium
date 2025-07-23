// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-account-page' is the settings subpage containing controls to
 * manage features related to the user's primary account, such as sync
 * controls and advanced sync settings.
 */
import './sync_account_control.js';
import '../settings_page/settings_subpage.js';

import type {SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SignedInState, SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './account_page.html.js';

const SettingsAccountPageElementBase =
    SettingsViewMixin(WebUiListenerMixin(PolymerElement));

export class SettingsAccountPageElement extends SettingsAccountPageElementBase {
  static get is() {
    return 'settings-account-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The current sync status.
       */
      syncStatus_: {type: Object},
    };
  }

  declare private syncStatus_: SyncStatus|null;

  override connectedCallback() {
    super.connectedCallback();

    assert(loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos'));

    SyncBrowserProxyImpl.getInstance().getSyncStatus().then(
        this.onSyncStatusChanged_.bind(this));
    this.addWebUiListener(
        'sync-status-changed', this.onSyncStatusChanged_.bind(this));
  }

  private onSyncStatusChanged_(syncStatus: SyncStatus) {
    this.syncStatus_ = syncStatus;

    if (Router.getInstance().getCurrentRoute() !== routes.ACCOUNT) {
      return;
    }

    // Don't show this page if the user is not signed in.
    if (!this.syncStatus_ ||
        this.syncStatus_.signedInState !== SignedInState.SIGNED_IN) {
      Router.getInstance().navigateTo(routes.PEOPLE);
    }
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-account-page': SettingsAccountPageElement;
  }
}

customElements.define(
    SettingsAccountPageElement.is, SettingsAccountPageElement);
