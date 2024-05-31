// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './strings.m.js';
import './tab_organization_not_started_image.js';

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './tab_organization_not_started.css.js';
import {getHtml} from './tab_organization_not_started.html.js';
import type {AccountInfo, SyncInfo, TabSearchSyncBrowserProxy} from './tab_search_sync_browser_proxy.js';
import {TabSearchSyncBrowserProxyImpl} from './tab_search_sync_browser_proxy.js';

enum SyncState {
  SIGNED_OUT,
  UNSYNCED,
  UNSYNCED_HISTORY,
  SYNC_PAUSED,
  SYNCED,
}

const TabOrganizationNotStartedElementBase =
    WebUiListenerMixinLit(CrLitElement);

export interface TabOrganizationNotStartedElement {
  $: {
    header: HTMLElement,
  };
}

// Not started state for the tab organization UI.
export class TabOrganizationNotStartedElement extends
    TabOrganizationNotStartedElementBase {
  static get is() {
    return 'tab-organization-not-started';
  }

  static override get properties() {
    return {
      showFre: {type: Boolean},
      account_: {type: Object},
      sync_: {type: Object},
    };
  }

  showFre: boolean = false;

  protected account_?: AccountInfo;
  private sync_?: SyncInfo;
  private syncBrowserProxy_: TabSearchSyncBrowserProxy =
      TabSearchSyncBrowserProxyImpl.getInstance();

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.syncBrowserProxy_.getAccountInfo().then(this.setAccount_.bind(this));
    this.addWebUiListener('account-info-changed', this.setAccount_.bind(this));

    this.syncBrowserProxy_.getSyncInfo().then(this.setSync_.bind(this));
    this.addWebUiListener('sync-info-changed', this.setSync_.bind(this));
  }

  private setAccount_(account: AccountInfo) {
    this.account_ = account;
  }

  private setSync_(sync: SyncInfo) {
    this.sync_ = sync;
  }

  private getSyncState_(): SyncState {
    if (!this.account_) {
      return SyncState.SIGNED_OUT;
    } else if (!this.sync_?.syncing) {
      return SyncState.UNSYNCED;
    } else if (this.sync_.paused) {
      return SyncState.SYNC_PAUSED;
    } else if (!this.sync_.syncingHistory) {
      return SyncState.UNSYNCED_HISTORY;
    } else {
      return SyncState.SYNCED;
    }
  }

  protected getTitle_(): string {
    return loadTimeData.getString(
        this.showFre ? 'notStartedTitleFRE' : 'notStartedTitle');
  }

  protected getBody_(): string {
    switch (this.getSyncState_()) {
      case SyncState.SIGNED_OUT:
        return loadTimeData.getString('notStartedBodySignedOut');
      case SyncState.UNSYNCED:
        return loadTimeData.getString('notStartedBodyUnsynced');
      case SyncState.SYNC_PAUSED:
        return loadTimeData.getString('notStartedBodySyncPaused');
      case SyncState.UNSYNCED_HISTORY:
        return loadTimeData.getString('notStartedBodyUnsyncedHistory');
      case SyncState.SYNCED: {
        return loadTimeData.getString(
            this.showFre ? 'notStartedBodyFRE' : 'notStartedBody');
      }
    }
  }

  protected shouldShowBodyLink_(): boolean {
    return this.getSyncState_() === SyncState.SYNCED && this.showFre;
  }

  protected shouldShowAccountInfo_(): boolean {
    return !!this.account_ &&
        (!this.sync_ || !this.sync_.syncing || this.sync_.paused ||
         !this.sync_.syncingHistory);
  }

  protected getAccountImageSrc_(): string {
    // image can be undefined if the account has not set an avatar photo.
    return this.account_!.avatarImage ||
        'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';
  }

  protected getButtonAriaLabel_(): string {
    switch (this.getSyncState_()) {
      case SyncState.SIGNED_OUT:
      case SyncState.UNSYNCED:
        return loadTimeData.getString('notStartedButtonUnsyncedAriaLabel');
      case SyncState.SYNC_PAUSED:
        return loadTimeData.getString('notStartedButtonSyncPausedAriaLabel');
      case SyncState.UNSYNCED_HISTORY:
        return loadTimeData.getString(
            'notStartedButtonUnsyncedHistoryAriaLabel');
      case SyncState.SYNCED:
        return loadTimeData.getString(
            this.showFre ? 'notStartedButtonFREAriaLabel' :
                           'notStartedButtonAriaLabel');
    }
  }

  protected getButtonText_(): string {
    switch (this.getSyncState_()) {
      case SyncState.SIGNED_OUT:
      case SyncState.UNSYNCED:
        return loadTimeData.getString('notStartedButtonUnsynced');
      case SyncState.SYNC_PAUSED:
        return loadTimeData.getString('notStartedButtonSyncPaused');
      case SyncState.UNSYNCED_HISTORY:
        return loadTimeData.getString('notStartedButtonUnsyncedHistory');
      case SyncState.SYNCED:
        return loadTimeData.getString(
            this.showFre ? 'notStartedButtonFRE' : 'notStartedButton');
    }
  }

  protected onButtonClick_() {
    switch (this.getSyncState_()) {
      case SyncState.SIGNED_OUT:
      case SyncState.UNSYNCED:
        this.fire('sync-click');
        break;
      case SyncState.SYNC_PAUSED:
        this.fire('sign-in-click');
        break;
      case SyncState.UNSYNCED_HISTORY:
        this.fire('settings-click');
        break;
      case SyncState.SYNCED:
        // Start a tab organization
        this.fire('organize-tabs-click');
        chrome.metricsPrivate.recordBoolean(
            'Tab.Organization.AllEntrypoints.Clicked', true);
        chrome.metricsPrivate.recordBoolean(
            'Tab.Organization.TabSearch.Clicked', true);
        break;
    }
  }

  protected onLinkClick_() {
    this.fire('learn-more-click');
  }

  protected onLinkKeyDown_(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      this.onLinkClick_();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-not-started': TabOrganizationNotStartedElement;
  }
}

customElements.define(
    TabOrganizationNotStartedElement.is, TabOrganizationNotStartedElement);
