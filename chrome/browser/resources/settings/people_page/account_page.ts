// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-account-page' is the settings subpage containing controls to
 * manage features related to the user's primary account, such as sync
 * controls and advanced sync settings.
 */
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import './sync_account_control.js';
import './sync_controls.js';
import './sync_encryption_options.js';
import '../settings_page/settings_subpage.js';

import type {SyncBrowserProxy, SyncPrefs, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {ChromeSigninAccessPoint, SignedInState, SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './account_page.html.js';

const SettingsAccountPageElementBase =
    SettingsViewMixin(WebUiListenerMixin(I18nMixin(PolymerElement)));

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

      /**
       * The current sync preferences, supplied by SyncBrowserProxy.
       */
      syncPrefs: Object,

      isEeaChoiceCountry_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isEeaChoiceCountry');
        },
      },

      personalizationCollapseExpanded_: {
        type: Boolean,
        value: false,
      },

      existingPassphraseLabel_: {
        type: String,
        computed: 'computeExistingPassphraseLabel_(syncPrefs.encryptAllData,' +
            'syncPrefs.explicitPassphraseTime)',
      },

      dataEncrypted_: {
        type: Boolean,
        computed: 'computeDataEncrypted_(syncPrefs.encryptAllData)',
      },

      encryptionExpanded_: {
        type: Boolean,
        value: false,
      },

      // Exposes ChromeSigninAccessPoint enum to HTML bindings.
      accessPointEnum_: {
        type: Object,
        value: ChromeSigninAccessPoint,
      },
    };
  }

  static get observers() {
    return [
      'expandEncryptionIfNeeded_(dataEncrypted_)',
    ];
  }

  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();
  declare private syncStatus_: SyncStatus|null;
  declare syncPrefs?: SyncPrefs;

  declare private isEeaChoiceCountry_: boolean;
  declare private personalizationCollapseExpanded_: boolean;
  declare private dataEncrypted_: boolean;
  declare private encryptionExpanded_: boolean;
  declare private existingPassphraseLabel_: TrustedHTML;

  override connectedCallback() {
    super.connectedCallback();

    assert(loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos'));

    this.syncBrowserProxy_.getSyncStatus().then(
        this.onSyncStatusChanged_.bind(this));
    this.addWebUiListener(
        'sync-status-changed', this.onSyncStatusChanged_.bind(this));
    this.addWebUiListener(
        'sync-prefs-changed', this.onSyncPrefsChanged_.bind(this));
  }

  private onSyncStatusChanged_(syncStatus: SyncStatus) {
    this.syncStatus_ = syncStatus;

    if (Router.getInstance().getCurrentRoute() !== routes.ACCOUNT) {
      return;
    }

    // Don't show this page if the user is not signed in.
    if (!this.shouldShowPageContents_()) {
      Router.getInstance().navigateTo(routes.PEOPLE);
    }
  }

  /**
   * Handler for when the sync preferences are updated.
   */
  private onSyncPrefsChanged_(syncPrefs: SyncPrefs) {
    this.syncPrefs = syncPrefs;
  }

  private onActivityControlsClick_() {
    this.syncBrowserProxy_.openActivityControlsUrl();
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('activityControlsUrl'));
  }

  private onLinkedServicesClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('linkedServicesUrl'));
  }

  private onSyncDashboardLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('syncDashboardUrl'));
  }

  private onResetSyncClick_(event: Event) {
    if ((event.target as HTMLElement).tagName === 'A') {
      // Stop the propagation of events as the |cr-expand-button|
      // prevents the default which will prevent the navigation to the link.
      event.stopPropagation();
    }
  }

  private onManageGoogleAccountClicked_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('googleAccountUrl'));
  }

  private computeExistingPassphraseLabel_(): TrustedHTML {
    if (!this.syncPrefs || !this.syncPrefs.encryptAllData) {
      return window.trustedTypes!.emptyHTML;
    }

    if (!this.syncPrefs.explicitPassphraseTime) {
      return this.i18nAdvanced('existingPassphraseLabel');
    }

    return this.i18nAdvanced('existingPassphraseLabelWithDate', {
      substitutions: [this.syncPrefs.explicitPassphraseTime],
    });
  }

  private computeDataEncrypted_(): boolean {
    return !!this.syncPrefs && this.syncPrefs.encryptAllData;
  }

  private expandEncryptionIfNeeded_() {
    this.encryptionExpanded_ = this.dataEncrypted_;
  }

  private shouldShowPageContents_() {
    return this.syncStatus_ &&
        this.syncStatus_.signedInState === SignedInState.SIGNED_IN;
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
