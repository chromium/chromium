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

import type {SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SignedInState} from '/shared/settings/people_page/sync_browser_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {getTemplate} from './account_page.html.js';

export class SettingsAccountPageElement extends PolymerElement {
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
      syncStatus: {
        type: Object,
        observer: 'syncStatusChanged_',
      },
    };
  }

  declare syncStatus: SyncStatus|null;

  private syncStatusChanged_() {
    // Don't show this page if the user is not signed in.
    if (!loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos') ||
        !this.syncStatus ||
        this.syncStatus.signedInState !== SignedInState.SIGNED_IN) {
      Router.getInstance().navigateTo(routes.PEOPLE);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-account-page': SettingsAccountPageElement;
  }
}

customElements.define(
    SettingsAccountPageElement.is, SettingsAccountPageElement);
