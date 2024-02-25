// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './shared_style.css.js';
import './synced_device_card.js';
import './strings.m.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FocusGrid} from 'chrome://resources/js/focus_grid.js';
import type {FocusRow} from 'chrome://resources/js/focus_row.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {Debouncer, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserServiceImpl} from './browser_service.js';
import {SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram} from './constants.js';
import type {ForeignSession, ForeignSessionTab} from './externs.js';
import type {HistorySyncedDeviceCardElement} from './synced_device_card.js';
import {getTemplate} from './synced_device_manager.html.js';

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
    'menu': CrLazyRenderElement<CrActionMenuElement>,
    'no-synced-tabs': HTMLElement,
    'sign-in-guide': HTMLElement,
  };
}

export class HistorySyncedDeviceManagerElement extends PolymerElement {
  static get is() {
    return 'history-synced-device-manager';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      sessionList: {
        type: Array,
        observer: 'updateSyncedDevices',
      },

      searchTerm: {
        type: String,
        observer: 'searchTermChanged',
      },

      /**
       * An array of synced devices with synced tab data.
       */
      syncedDevices_: Array,

      signInState: {
        type: Boolean,
        observer: 'signInStateChanged_',
      },

      guestSession_: Boolean,
      signInAllowed_: Boolean,
      fetchingSyncedTabs_: Boolean,
      hasSeenForeignData_: Boolean,

      /**
       * The session ID referring to the currently active action menu.
       */
      actionMenuModel_: String,
    };
  }

  private focusGrid_: FocusGrid|null = null;
  private syncedDevices_: ForeignDeviceInternal[] = [];
  private hasSeenForeignData_: boolean;
  private fetchingSyncedTabs_: boolean = false;
  private actionMenuModel_: string|null = null;
  private guestSession_: boolean = loadTimeData.getBoolean('isGuestSession');
  private signInAllowed_: boolean = loadTimeData.getBoolean('isSignInAllowed');
  private debouncer_: Debouncer|null = null;

  signInState: boolean;
  searchTerm: string;
  sessionList: ForeignSession[];

  override ready() {
    super.ready();
    this.addEventListener('synced-device-card-open-menu', this.onOpenMenu_);
    this.addEventListener('update-focus-grid', this.updateFocusGrid_);
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

  getContentScrollTarget(): HTMLElement {
    return this;
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

  private onTurnOnSyncClick_() {
    BrowserServiceImpl.getInstance().startTurnOnSyncFlow();
  }

  private onOpenMenu_(e: CustomEvent<{tag: string, target: HTMLElement}>) {
    this.actionMenuModel_ = e.detail.tag;
    this.$.menu.get().showAt(e.detail.target);
    BrowserServiceImpl.getInstance().recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.SHOW_SESSION_MENU,
        SyncedTabsHistogram.LIMIT);
  }

  private onOpenAllClick_() {
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

    this.debouncer_ = Debouncer.debounce(this.debouncer_, microTask, () => {
      const cards =
          this.shadowRoot!.querySelectorAll('history-synced-device-card');
      Array.from(cards)
          .reduce(
              (prev: FocusRow[], cur: HistorySyncedDeviceCardElement) =>
                  prev.concat(cur.createFocusRows()),
              [])
          .forEach((row) => {
            this.focusGrid_!.addRow(row);
          });
      this.focusGrid_!.ensureRowActive(1);
    });
  }

  private onDeleteSessionClick_() {
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
  showNoSyncedMessage(
      signInState: boolean, syncedDevicesLength: number,
      guestSession: boolean): boolean {
    if (guestSession) {
      return true;
    }

    return signInState && syncedDevicesLength === 0;
  }

  /**
   * Shows the signin guide when the user is not signed in, signin is allowed
   * and not in a guest session.
   */
  showSignInGuide(
      signInState: boolean, guestSession: boolean,
      signInAllowed: boolean): boolean {
    const show = !signInState && !guestSession && signInAllowed;
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
  noSyncedTabsMessage(): string {
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
  updateSyncedDevices(sessionList: ForeignSession[]) {
    this.fetchingSyncedTabs_ = false;

    if (!sessionList) {
      return;
    }

    if (sessionList.length > 0 && !this.hasSeenForeignData_) {
      this.hasSeenForeignData_ = true;
      BrowserServiceImpl.getInstance().recordHistogram(
          SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.HAS_FOREIGN_DATA,
          SyncedTabsHistogram.LIMIT);
    }

    const devices: ForeignDeviceInternal[] = [];
    sessionList.forEach((session) => {
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
  private signInStateChanged_(_current: boolean, previous?: boolean) {
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

  searchTermChanged() {
    this.clearDisplayedSyncedDevices_();
    this.updateSyncedDevices(this.sessionList);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-synced-device-manager': HistorySyncedDeviceManagerElement;
  }
}

customElements.define(
    HistorySyncedDeviceManagerElement.is, HistorySyncedDeviceManagerElement);
