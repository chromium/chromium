// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import './synced_device_card.js';
import '/strings.m.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FocusGrid} from 'chrome://resources/js/focus_grid.js';
import type {FocusRow} from 'chrome://resources/js/focus_row.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {BrowserServiceImpl} from './browser_service.js';
import {SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram} from './constants.js';
import type {ForeignSession, ForeignSessionTab} from './externs.js';
import type {HistorySyncedDeviceCardElement} from './synced_device_card.js';
import {getCss} from './synced_device_manager.css.js';
import {getHtml} from './synced_device_manager.html.js';

interface ForeignDeviceInternal {
  device: string;
  lastUpdateTime: string;
  opened: boolean;
  separatorIndexes: number[];
  timestamp: number;
  tabs: ForeignSessionTab[];
  tag: string;
}

declare global {
  interface HTMLElementEventMap {
    'synced-device-card-open-menu':
        CustomEvent<{tag: string, target: HTMLElement}>;
  }
}

export interface HistorySyncedDeviceManagerElement {
  $: {
    'menu': CrLazyRenderLitElement<CrActionMenuElement>,
    'no-synced-tabs': HTMLElement,
    'sign-in-guide': HTMLElement,
  };
}

export class HistorySyncedDeviceManagerElement extends CrLitElement {
  static get is() {
    return 'history-synced-device-manager';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      sessionList: {type: Array},
      searchTerm: {type: String},
      /**
       * An array of synced devices with synced tab data.
       */
      syncedDevices_: {type: Array},
      signInState: {type: Boolean},
      guestSession_: {type: Boolean},
      signInAllowed_: {type: Boolean},
      fetchingSyncedTabs_: {type: Boolean},
      hasSeenForeignData_: {type: Boolean},
      /**
       * The session ID referring to the currently active action menu.
       */
      actionMenuModel_: {type: String},
    };
  }

  private focusGrid_: FocusGrid|null = null;
  private focusGridUpdateTimeout_: number|null = null;
  protected accessor syncedDevices_: ForeignDeviceInternal[] = [];
  private accessor hasSeenForeignData_: boolean = false;
  private accessor fetchingSyncedTabs_: boolean = false;
  private accessor actionMenuModel_: string|null = null;
  private accessor guestSession_: boolean =
      loadTimeData.getBoolean('isGuestSession');
  private accessor signInAllowed_: boolean =
      loadTimeData.getBoolean('isSignInAllowed');

  accessor signInState: boolean = false;
  accessor searchTerm: string = '';
  accessor sessionList: ForeignSession[] = [];

  override firstUpdated() {
    this.addEventListener('synced-device-card-open-menu', this.onOpenMenu_);
    this.addEventListener('update-focus-grid', this.updateFocusGrid_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('sessionList')) {
      this.updateSyncedDevices_();
    }
    if (changedProperties.has('searchTerm')) {
      this.searchTermChanged_();
    }
    if (changedProperties.has('signInState')) {
      this.signInStateChanged_(changedProperties.get('signInState'));
    }
  }

  override connectedCallback() {
    super.connectedCallback();
    this.focusGrid_ = new FocusGrid();

    // Update the sign in state.
    BrowserServiceImpl.getInstance().otherDevicesInitialized();
    BrowserServiceImpl.getInstance().recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.INITIALIZED,
        SyncedTabsHistogram.LIMIT);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.focusGrid_!.destroy();
  }

  configureSignInForTest(data: {
    signInState: boolean,
    signInAllowed: boolean,
    guestSession: boolean,
  }) {
    this.signInState = data.signInState;
    this.signInAllowed_ = data.signInAllowed;
    this.guestSession_ = data.guestSession;
  }

  private createInternalDevice_(session: ForeignSession):
      ForeignDeviceInternal {
    let tabs: ForeignSessionTab[] = [];
    const separatorIndexes = [];
    for (let i = 0; i < session.windows.length; i++) {
      const windowId = session.windows[i].sessionId;
      const newTabs = session.windows[i].tabs;
      if (newTabs.length === 0) {
        continue;
      }

      newTabs.forEach(function(tab) {
        tab.windowId = windowId;
      });

      let windowAdded = false;
      if (!this.searchTerm) {
        // Add all the tabs if there is no search term.
        tabs = tabs.concat(newTabs);
        windowAdded = true;
      } else {
        const searchText = this.searchTerm.toLowerCase();
        for (let j = 0; j < newTabs.length; j++) {
          const tab = newTabs[j];
          if (tab.title.toLowerCase().indexOf(searchText) !== -1) {
            tabs.push(tab);
            windowAdded = true;
          }
        }
      }
      if (windowAdded && i !== session.windows.length - 1) {
        separatorIndexes.push(tabs.length - 1);
      }
    }
    return {
      device: session.name,
      lastUpdateTime: '– ' + session.modifiedTime,
      opened: true,
      separatorIndexes: separatorIndexes,
      timestamp: session.timestamp,
      tabs: tabs,
      tag: session.tag,
    };
  }

  protected onTurnOnSyncClick_() {
    BrowserServiceImpl.getInstance().startTurnOnSyncFlow();
  }

  private onOpenMenu_(e: CustomEvent<{tag: string, target: HTMLElement}>) {
    this.actionMenuModel_ = e.detail.tag;
    this.$.menu.get().showAt(e.detail.target);
    BrowserServiceImpl.getInstance().recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.SHOW_SESSION_MENU,
        SyncedTabsHistogram.LIMIT);
  }

  protected onOpenAllClick_() {
    const menu = this.$.menu.getIfExists();
    assert(menu);
    const browserService = BrowserServiceImpl.getInstance();
    browserService.recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.OPEN_ALL,
        SyncedTabsHistogram.LIMIT);
    assert(this.actionMenuModel_);
    browserService.openForeignSessionAllTabs(this.actionMenuModel_);
    this.actionMenuModel_ = null;
    menu.close();
  }

  private updateFocusGrid_() {
    if (!this.focusGrid_) {
      return;
    }

    this.focusGrid_.destroy();

    if (this.focusGridUpdateTimeout_) {
      clearTimeout(this.focusGridUpdateTimeout_);
    }
    this.focusGridUpdateTimeout_ = setTimeout(() => {
      const cards =
          this.shadowRoot.querySelectorAll('history-synced-device-card');
      Array.from(cards)
          .reduce(
              (prev: FocusRow[], cur: HistorySyncedDeviceCardElement) =>
                  prev.concat(cur.createFocusRows()),
              [])
          .forEach((row) => {
            this.focusGrid_!.addRow(row);
          });
      this.focusGrid_!.ensureRowActive(1);
      this.focusGridUpdateTimeout_ = null;
    });
  }

  protected onDeleteSessionClick_() {
    const menu = this.$.menu.getIfExists();
    assert(menu);
    const browserService = BrowserServiceImpl.getInstance();
    browserService.recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.HIDE_FOR_NOW,
        SyncedTabsHistogram.LIMIT);
    assert(this.actionMenuModel_);
    browserService.deleteForeignSession(this.actionMenuModel_);
    this.actionMenuModel_ = null;
    menu.close();
  }

  clearSyncedDevicesForTest() {
    this.clearDisplayedSyncedDevices_();
  }

  private clearDisplayedSyncedDevices_() {
    this.syncedDevices_ = [];
  }

  /**
   * Decide whether or not should display no synced tabs message.
   */
  protected showNoSyncedMessage_(): boolean {
    if (this.guestSession_) {
      return true;
    }

    return this.signInState && this.syncedDevices_.length === 0;
  }

  /**
   * Shows the signin guide when the user is not signed in, signin is allowed
   * and not in a guest session.
   */
  protected showSignInGuide_(): boolean {
    const show =
        !this.signInState && !this.guestSession_ && this.signInAllowed_;
    if (show) {
      BrowserServiceImpl.getInstance().recordAction(
          'Signin_Impression_FromRecentTabs');
    }

    return show;
  }

  /**
   * Decide what message should be displayed when user is logged in and there
   * are no synced tabs.
   */
  protected noSyncedTabsMessage_(): string {
    let stringName = this.fetchingSyncedTabs_ ? 'loading' : 'noSyncedResults';
    if (this.searchTerm !== '') {
      stringName = 'noSearchResults';
    }
    return loadTimeData.getString(stringName);
  }

  /**
   * Replaces the currently displayed synced tabs with |sessionList|. It is
   * common for only a single session within the list to have changed, We try to
   * avoid doing extra work in this case. The logic could be more intelligent
   * about updating individual tabs rather than replacing whole sessions, but
   * this approach seems to have acceptable performance.
   */
  private updateSyncedDevices_() {
    this.fetchingSyncedTabs_ = false;

    if (!this.sessionList) {
      return;
    }

    if (this.sessionList.length > 0 && !this.hasSeenForeignData_) {
      this.hasSeenForeignData_ = true;
      BrowserServiceImpl.getInstance().recordHistogram(
          SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.HAS_FOREIGN_DATA,
          SyncedTabsHistogram.LIMIT);
    }

    const devices: ForeignDeviceInternal[] = [];
    this.sessionList.forEach((session) => {
      const device = this.createInternalDevice_(session);
      if (device.tabs.length !== 0) {
        devices.push(device);
      }
    });

    this.syncedDevices_ = devices;
  }

  /**
   * Get called when user's sign in state changes, this will affect UI of synced
   * tabs page. Sign in promo gets displayed when user is signed out, and
   * different messages are shown when there are no synced tabs.
   */
  private signInStateChanged_(previous?: boolean) {
    if (previous === undefined) {
      return;
    }

    this.dispatchEvent(new CustomEvent(
        'history-view-changed', {bubbles: true, composed: true}));

    // User signed out, clear synced device list and show the sign in promo.
    if (!this.signInState) {
      this.clearDisplayedSyncedDevices_();
      return;
    }
    // User signed in, show the loading message when querying for synced
    // devices.
    this.fetchingSyncedTabs_ = true;
  }

  private searchTermChanged_() {
    this.clearDisplayedSyncedDevices_();
    this.updateSyncedDevices_();
  }

  protected onCardOpenedChanged_(e: CustomEvent<{value: boolean}>) {
    const currentTarget = e.currentTarget as HTMLElement;
    const index = Number(currentTarget.dataset['index']);
    const device = this.syncedDevices_[index];
    device.opened = e.detail.value;
    this.requestUpdate();
  }
}

// Exported to be used in the autogenerated Lit template file
export type SyncedDeviceManagerElement = HistorySyncedDeviceManagerElement;

declare global {
  interface HTMLElementTagNameMap {
    'history-synced-device-manager': HistorySyncedDeviceManagerElement;
  }
}

customElements.define(
    HistorySyncedDeviceManagerElement.is, HistorySyncedDeviceManagerElement);
