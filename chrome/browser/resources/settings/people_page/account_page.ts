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

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {SyncBrowserProxy, SyncPrefs, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SignedInState, SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import {getCss} from './account_page.css.js';
import {getHtml} from './account_page.html.js';

const SettingsAccountPageElementBase =
    SettingsViewMixinLit(WebUiListenerMixinLit(I18nMixinLit(CrLitElement)));

export type AccountPageElement = SettingsAccountPageElement;

export class SettingsAccountPageElement extends SettingsAccountPageElementBase {
  static get is() {
    return 'settings-account-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The current sync status.
       */
      syncStatus_: {type: Object},

      /**
       * The current sync preferences, supplied by SyncBrowserProxy.
       */
      syncPrefs: {type: Object},

      isEeaChoiceCountry_: {type: Boolean},

      personalizationCollapseExpanded_: {type: Boolean},

      existingPassphraseLabel_: {type: String},

      dataEncrypted_: {type: Boolean},

      encryptionExpanded_: {type: Boolean},
    };
  }

  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();
  protected accessor syncStatus_: SyncStatus|null = null;
  accessor syncPrefs: SyncPrefs|null = null;

  protected accessor isEeaChoiceCountry_: boolean =
      loadTimeData.getBoolean('isEeaChoiceCountry');
  protected accessor personalizationCollapseExpanded_: boolean = false;
  protected accessor dataEncrypted_: boolean = false;
  protected accessor encryptionExpanded_: boolean = false;
  protected accessor existingPassphraseLabel_: string = '';

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

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('syncPrefs')) {
      const wasDataEncrypted = this.dataEncrypted_;
      this.dataEncrypted_ = this.computeDataEncrypted_();
      this.existingPassphraseLabel_ = this.computeExistingPassphraseLabel_();
      if (wasDataEncrypted !== this.dataEncrypted_) {
        this.expandEncryptionIfNeeded_();
      }
    }
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

  protected onSyncEncryptionOptionsSyncPrefsChanged_(
      e: CustomEvent<{value: SyncPrefs}>) {
    this.syncPrefs = e.detail.value;
  }

  protected onPersonalizationCollapseExpandedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.personalizationCollapseExpanded_ = e.detail.value;
  }

  protected onEncryptionExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.encryptionExpanded_ = e.detail.value;
  }

  protected onActivityControlsClick_() {
    this.syncBrowserProxy_.openActivityControlsUrl();
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('activityControlsUrl'));
  }

  protected onLinkedServicesClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('linkedServicesUrl'));
  }

  protected onSyncDashboardLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('syncDashboardUrl'));
  }

  protected onResetSyncClick_(event: Event) {
    if ((event.target as HTMLElement).tagName === 'A') {
      // Stop the propagation of events as the |cr-expand-button|
      // prevents the default which will prevent the navigation to the link.
      event.stopPropagation();
    }
  }

  protected onManageGoogleAccountClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('googleAccountUrl'));
  }

  // <if expr="is_chromeos">
  protected onManageDeviceAccountsClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('osSettingsAccountsPageUrl'));
  }
  //</if>

  private computeExistingPassphraseLabel_(): string {
    if (!this.syncPrefs || !this.syncPrefs.encryptAllData) {
      return '';
    }

    if (!this.syncPrefs.explicitPassphraseTime) {
      return this.i18n('existingPassphraseLabel');
    }

    return this.i18n(
        'existingPassphraseLabelWithDate',
        this.syncPrefs.explicitPassphraseTime);
  }

  private computeDataEncrypted_(): boolean {
    return !!this.syncPrefs && this.syncPrefs.encryptAllData;
  }

  private expandEncryptionIfNeeded_() {
    this.encryptionExpanded_ = this.dataEncrypted_;
  }

  protected shouldShowPageContents_() {
    return this.syncStatus_ &&
        this.syncStatus_.signedInState === SignedInState.SIGNED_IN;
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-account-page': SettingsAccountPageElement;
  }
}

customElements.define(
    SettingsAccountPageElement.is, SettingsAccountPageElement);
