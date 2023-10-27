// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './strings.m.js';
import './tab_organization_shared_style.css.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './tab_organization_not_started.html.js';
import {AccountInfo, SyncInfo, TabSearchSyncBrowserProxy, TabSearchSyncBrowserProxyImpl} from './tab_search_sync_browser_proxy.js';

enum SyncState {
  SIGNED_OUT,
  UNSYNCED,
  UNSYNCED_HISTORY,
  SYNC_PAUSED,
  SYNCED,
}

const TabOrganizationNotStartedElementBase = WebUiListenerMixin(PolymerElement);

// Not started state for the tab organization UI.
export class TabOrganizationNotStartedElement extends
    TabOrganizationNotStartedElementBase {
  static get is() {
    return 'tab-organization-not-started';
  }

  static get properties() {
    return {
      showFre: Boolean,
      account_: Object,
      sync_: Object,
    };
  }

  showFre: boolean;

  private account_?: AccountInfo;
  private sync_?: SyncInfo;
  private syncBrowserProxy_: TabSearchSyncBrowserProxy =
      TabSearchSyncBrowserProxyImpl.getInstance();

  static get template() {
    return getTemplate();
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

  private getTitle_(): string {
    if (this.showFre) {
      return loadTimeData.getString('notStartedTitleFRE');
    } else {
      return loadTimeData.getString('notStartedTitle');
    }
  }

  private getBody_(): string {
    switch (this.getSyncState_()) {
      case SyncState.SIGNED_OUT:
      case SyncState.UNSYNCED:
      case SyncState.SYNC_PAUSED:
        return loadTimeData.getString('notStartedBodyUnsynced');
      case SyncState.UNSYNCED_HISTORY:
        return loadTimeData.getString('notStartedBodyUnsyncedHistory');
      case SyncState.SYNCED: {
        if (this.showFre) {
          return loadTimeData.getString('notStartedBodyFRE');
        } else {
          return loadTimeData.getString('notStartedBody');
        }
      }
    }
  }

  private shouldShowAccountInfo_(): boolean {
    return !!this.account_ &&
        (!this.sync_ || !this.sync_.syncing || this.sync_.paused ||
         !this.sync_.syncingHistory);
  }

  private getAccountImageSrc_(image: string|null): string {
    // image can be undefined if the account has not set an avatar photo.
    return image || 'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';
  }

  private getButtonText_(): string {
    switch (this.getSyncState_()) {
      case SyncState.SIGNED_OUT:
      case SyncState.UNSYNCED:
        return loadTimeData.getString('notStartedButtonUnsynced');
      case SyncState.SYNC_PAUSED:
        return loadTimeData.getString('notStartedButtonPaused');
      case SyncState.UNSYNCED_HISTORY:
        return loadTimeData.getString('notStartedButtonUnsyncedHistory');
      case SyncState.SYNCED:
        return loadTimeData.getString('notStartedButton');
    }
  }

  private onButtonClick_() {
    switch (this.getSyncState_()) {
      case SyncState.SIGNED_OUT:
        // TODO(emshack): Trigger sign in & sync flow
        break;
      case SyncState.UNSYNCED:
        // TODO(emshack): Trigger sync flow
        break;
      case SyncState.SYNC_PAUSED:
        // TODO(emshack): Trigger sign in flow
        break;
      case SyncState.UNSYNCED_HISTORY:
        // TODO(emshack): Trigger opening sync settings
        break;
      case SyncState.SYNCED:
        // Start a tab organization
        this.dispatchEvent(new CustomEvent(
            'organize-tabs-click', {bubbles: true, composed: true}));
        break;
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
