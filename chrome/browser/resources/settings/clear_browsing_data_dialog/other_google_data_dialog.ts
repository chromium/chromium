// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-other-google-data-dialog' is a subpage
 * shown within the Clear Browsing Data dialog to provide links
 * for managing other Google data like passwords and activity.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';

import type {SyncBrowserProxy, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PasswordManagerImpl, PasswordManagerPage} from '../autofill_page/passwords/password_manager_proxy.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';

import type {ClearBrowsingDataBrowserProxy, UpdateSyncStateEvent} from './clear_browsing_data_browser_proxy.js';
import {ClearBrowsingDataBrowserProxyImpl} from './clear_browsing_data_browser_proxy.js';
import {isSignedIn} from './clear_browsing_data_signin_util.js';
import {getCss} from './other_google_data_dialog.css.js';
import {getHtml} from './other_google_data_dialog.html.js';

export interface SettingsOtherGoogleDataDialogElement {
  $: {
    dialog: CrDialogElement,
    passwordManagerLink: CrLinkRowElement,
  };
}

const SettingsOtherGoogleDataDialogElementBase =
    WebUiListenerMixinLit(CrLitElement);

export type OtherGoogleDataDialogElement = SettingsOtherGoogleDataDialogElement;

export class SettingsOtherGoogleDataDialogElement extends
    SettingsOtherGoogleDataDialogElementBase {
  static get is() {
    return 'settings-other-google-data-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
  static override get properties() {
    return {
      isGoogleDse_: {type: Boolean},
      nonGoogleSearchHistorySubLabel_: {type: String},
      syncStatus_: {type: Object},
    };
  }

  protected accessor isGoogleDse_: boolean = false;
  protected accessor nonGoogleSearchHistorySubLabel_: TrustedHTML =
      window.trustedTypes!.emptyHTML;
  private accessor syncStatus_: SyncStatus|undefined = undefined;

  private clearBrowsingDataBrowserProxy_: ClearBrowsingDataBrowserProxy =
      ClearBrowsingDataBrowserProxyImpl.getInstance();
  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));
    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));

    this.addWebUiListener(
        'update-sync-state', this.updateDseStatus_.bind(this));
    this.clearBrowsingDataBrowserProxy_.getSyncState().then(
        this.updateDseStatus_.bind(this));
  }

  private updateDseStatus_(event: UpdateSyncStateEvent) {
    this.isGoogleDse_ = !event.isNonGoogleDse;
    this.nonGoogleSearchHistorySubLabel_ =
        sanitizeInnerHtml(event.nonGoogleSearchHistoryString);
  }

  private handleSyncStatus_(syncStatus: SyncStatus) {
    this.syncStatus_ = syncStatus;
  }

  protected computeDialogTitle_() {
    return this.isGoogleDse_ ? loadTimeData.getString('otherGoogleDataTitle') :
                               loadTimeData.getString('otherDataTitle');
  }

  protected onBackOrCancelClick_() {
    this.$.dialog.cancel();
  }

  protected onPasswordManagerClick_() {
    PasswordManagerImpl.getInstance().showPasswordManager(
        PasswordManagerPage.PASSWORDS);

    this.metricsBrowserProxy_.recordAction(
        'Settings.DeleteBrowsingData.PasswordManagerLinkClick');
  }

  protected onMyActivityLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('deleteBrowsingDataMyActivityUrl'));

    this.metricsBrowserProxy_.recordAction(
        'Settings.DeleteBrowsingData.MyActivityLinkClick');
  }

  protected onGoogleSearchHistoryLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('deleteBrowsingDataSearchHistoryUrl'));

    this.metricsBrowserProxy_.recordAction(
        'Settings.DeleteBrowsingData.GoogleSearchHistoryLinkClick');
  }

  protected onGeminiAppsActivityClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('myActivityGeminiAppsUrl'));

    this.metricsBrowserProxy_.recordAction(
        'Settings.DeleteBrowsingData.GeminiAppsActivityLinkClick');
  }

  protected shouldShowMyActivityLink_() {
    return isSignedIn(this.syncStatus_);
  }

  protected shouldShowGoogleSearchHistoryLink_() {
    return isSignedIn(this.syncStatus_) && this.isGoogleDse_;
  }

  protected shouldShowGeminiAppsActivityLink_() {
    return isSignedIn(this.syncStatus_) &&
        loadTimeData.getBoolean('showGlicSettings');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-other-google-data-dialog': SettingsOtherGoogleDataDialogElement;
  }
}

customElements.define(
    SettingsOtherGoogleDataDialogElement.is,
    SettingsOtherGoogleDataDialogElement);
