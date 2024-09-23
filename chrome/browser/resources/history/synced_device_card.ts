// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './searched_label.js';
import './shared_style.css.js';
import './shared_vars.css.js';
import './strings.m.js';

import type {CrCollapseElement} from 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import {FocusRow} from 'chrome://resources/js/focus_row.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserServiceImpl} from './browser_service.js';
import {SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram} from './constants.js';
import type {ForeignSessionTab} from './externs.js';
import {getTemplate} from './synced_device_card.html.js';

interface OpenTabEvent {
  model: {tab: ForeignSessionTab};
}

declare global {
  interface HTMLElementTagNameMap {
    'history-synced-device-card': HistorySyncedDeviceCardElement;
  }
}

export interface HistorySyncedDeviceCardElement {
  $: {
    'card-heading': HTMLDivElement,
    'collapse': CrCollapseElement,
    'collapse-button': HTMLElement,
    'menu-button': HTMLElement,
  };
}

export class HistorySyncedDeviceCardElement extends PolymerElement {
  static get is() {
    return 'history-synced-device-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The list of tabs open for this device.
       */
      tabs: {type: Array, observer: 'updateIcons_'},

      // Name of the synced device.
      device: String,

      // When the device information was last updated.
      lastUpdateTime: String,

      // Whether the card is open.
      opened: Boolean,

      searchTerm: String,

      /**
       * The indexes where a window separator should be shown. The use of a
       * separate array here is necessary for window separators to appear
       * correctly in search. See http://crrev.com/2022003002 for more details.
       */
      separatorIndexes: Array,

      // Internal identifier for the device.
      sessionTag: String,
    };
  }

  tabs: ForeignSessionTab[] = [];
  opened: boolean;
  separatorIndexes: number[];
  sessionTag: string;

  override ready() {
    super.ready();
    this.addEventListener('dom-change', this.notifyFocusUpdate_);
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  /**
   * Create FocusRows for this card. One is always made for the card heading and
   * one for each result if the card is open.
   */
  createFocusRows(): FocusRow[] {
    const titleRow = new FocusRow(this.$['card-heading'], null);
    titleRow.addItem('menu', '#menu-button');
    titleRow.addItem('collapse', '#collapse-button');
    const rows = [titleRow];
    if (this.opened) {
      this.shadowRoot!.querySelectorAll<HTMLElement>('.item-container')
          .forEach(function(el) {
            const row = new FocusRow(el, null);
            row.addItem('link', '.website-link');
            rows.push(row);
          });
    }
    return rows;
  }

  /** Open a single synced tab. */
  private openTab_(e: MouseEvent) {
    const model = (e as unknown as OpenTabEvent).model;
    const tab = model.tab;
    const browserService = BrowserServiceImpl.getInstance();
    browserService.recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.LINK_CLICKED,
        SyncedTabsHistogram.LIMIT);
    browserService.openForeignSessionTab(this.sessionTag, tab.sessionId, e);
    e.preventDefault();
  }

  /**
   * Toggles the dropdown display of synced tabs for each device card.
   */
  toggleTabCard() {
    const histogramValue = this.$.collapse.opened ?
        SyncedTabsHistogram.COLLAPSE_SESSION :
        SyncedTabsHistogram.EXPAND_SESSION;

    BrowserServiceImpl.getInstance().recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, histogramValue, SyncedTabsHistogram.LIMIT);

    this.$.collapse.toggle();

    this.fire_('update-focus-grid');
  }

  private notifyFocusUpdate_() {
    // Refresh focus after all rows are rendered.
    this.fire_('update-focus-grid');
  }

  /**
   * When the synced tab information is set, the icon associated with the tab
   * website is also set.
   */
  private updateIcons_() {
    setTimeout(() => {
      const icons =
          this.shadowRoot!.querySelectorAll<HTMLDivElement>('.website-icon');

      for (let i = 0; i < this.tabs.length; i++) {
        // Entries on this UI are coming strictly from sync, so we can set
        // |isSyncedUrlForHistoryUi| to true on the getFavicon call below.
        icons[i].style.backgroundImage = getFaviconForPageURL(
            this.tabs[i].url, true, this.tabs[i].remoteIconUrlForUma);
      }
    }, 0);
  }

  private isWindowSeparatorIndex_(index: number): boolean {
    return this.separatorIndexes.indexOf(index) !== -1;
  }

  private getCollapseIcon_(opened: boolean): string {
    return opened ? 'cr:expand-less' : 'cr:expand-more';
  }

  private getCollapseTitle_(opened: boolean): string {
    return opened ? loadTimeData.getString('collapseSessionButton') :
                    loadTimeData.getString('expandSessionButton');
  }

  private onMenuButtonClick_(e: Event) {
    this.fire_('synced-device-card-open-menu', {
      target: e.target,
      tag: this.sessionTag,
    });
    e.stopPropagation();  // Prevent cr-collapse.
  }

  private onLinkRightClick_() {
    BrowserServiceImpl.getInstance().recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.LINK_RIGHT_CLICKED,
        SyncedTabsHistogram.LIMIT);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-synced-device-card': HistorySyncedDeviceCardElement;
  }
}

customElements.define(
    HistorySyncedDeviceCardElement.is, HistorySyncedDeviceCardElement);
