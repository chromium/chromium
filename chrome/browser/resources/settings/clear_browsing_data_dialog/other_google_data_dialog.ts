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
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import type {SyncBrowserProxy, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerPage} from '../autofill_page/password_manager_proxy.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';

import type {ClearBrowsingDataBrowserProxy, UpdateSyncStateEvent} from './clear_browsing_data_browser_proxy.js';
import {ClearBrowsingDataBrowserProxyImpl} from './clear_browsing_data_browser_proxy.js';
import {isSignedIn} from './clear_browsing_data_signin_util.js';
import {getTemplate} from './other_google_data_dialog.html.js';

export interface SettingsOtherGoogleDataDialogElement {
  $: {
    dialog: CrDialogElement,
    passwordManagerLink: CrLinkRowElement,
  };
}

const SettingsOtherGoogleDataDialogElementBase =
    WebUiListenerMixin(PolymerElement);

export class SettingsOtherGoogleDataDialogElement extends
    SettingsOtherGoogleDataDialogElementBase {
  static get is() {
    return 'settings-other-google-data-dialog';
  }

  static get template() {
    return getTemplate();
  }
  static get properties() {
    return {
      dialogTitle_: {
        type: String,
        computed: 'computeDialogTitle_(isGoogleDse_)',
      },

      isGoogleDse_: {
        type: Boolean,
        value: false,
      },

      nonGoogleSearchHistorySubLabel_: String,

      syncStatus_: Object,
    };
  }

  declare private dialogTitle_: string;
  declare private isGoogleDse_: boolean;
  declare private nonGoogleSearchHistorySubLabel_: TrustedHTML;
  declare private syncStatus_: SyncStatus|undefined;

  private clearBrowsingDataBrowserProxy_: ClearBrowsingDataBrowserProxy =
      ClearBrowsingDataBrowserProxyImpl.getInstance();
  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

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

  private computeDialogTitle_() {
    return this.isGoogleDse_ ? loadTimeData.getString('otherGoogleDataTitle') :
                               loadTimeData.getString('otherDataTitle');
  }

  private onBackOrCancelClick_() {
    this.$.dialog.cancel();
  }

  private onPasswordManagerClick_() {
    PasswordManagerImpl.getInstance().showPasswordManager(
        PasswordManagerPage.PASSWORDS);

    this.metricsBrowserProxy_.recordAction(
        'Settings.DeleteBrowsingData.PasswordManagerLinkClick');
  }

  private onMyActivityLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('deleteBrowsingDataMyActivityUrl'));

    this.metricsBrowserProxy_.recordAction(
        'Settings.DeleteBrowsingData.MyActivityLinkClick');
  }

  private onGoogleSearchHistoryLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('deleteBrowsingDataSearchHistoryUrl'));

    this.metricsBrowserProxy_.recordAction(
        'Settings.DeleteBrowsingData.GoogleSearchHistoryLinkClick');
  }

  private onGeminiAppsActivityClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('myActivityGeminiAppsUrl'));

    this.metricsBrowserProxy_.recordAction(
        'Settings.DeleteBrowsingData.GeminiAppsActivityLinkClick');
  }

  private onGeminiPersonalContextClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('geminiPersonalContextUrl'));
  }

  private shouldShowMyActivityLink_() {
    return isSignedIn(this.syncStatus_);
  }

  private shouldShowGoogleSearchHistoryLink_() {
    return isSignedIn(this.syncStatus_) && this.isGoogleDse_;
  }

  private shouldShowGeminiAppsActivityLink_() {
    return isSignedIn(this.syncStatus_) &&
        loadTimeData.getBoolean('showGlicSettings') &&
        loadTimeData.getBoolean('enableBrowsingHistoryActorIntegrationM1');
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
