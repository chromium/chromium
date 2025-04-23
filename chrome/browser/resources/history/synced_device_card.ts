// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './searched_label.js';
import '/strings.m.js';

import type {CrCollapseElement} from 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import {FocusRow} from 'chrome://resources/js/focus_row.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {BrowserServiceImpl} from './browser_service.js';
import {SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram} from './constants.js';
import type {ForeignSessionTab} from './externs.js';
import {getCss} from './synced_device_card.css.js';
import {getHtml} from './synced_device_card.html.js';

declare global {
  interface HTMLElementTagNameMap {
    'history-synced-device-card': HistorySyncedDeviceCardElement;
  }
}

export interface HistorySyncedDeviceCardElement {
  $: {
    'card-heading': HTMLElement,
    'collapse': CrCollapseElement,
    'collapse-button': HTMLElement,
    'menu-button': HTMLElement,
  };
}

export class HistorySyncedDeviceCardElement extends CrLitElement {
  static get is() {
    return 'history-synced-device-card';
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
       * The list of tabs open for this device.
       */
      tabs: {type: Array},

      // Name of the synced device.
      device: {type: String},

      // When the device information was last updated.
      lastUpdateTime: {type: String},

      // Whether the card is open.
      opened: {
        type: Boolean,
        notify: true,
      },

      searchTerm: {type: String},

      /**
       * The indexes where a window separator should be shown. The use of a
       * separate array here is necessary for window separators to appear
       * correctly in search. See http://crrev.com/2022003002 for more details.
       */
      separatorIndexes: {type: Array},

      // Internal identifier for the device.
      sessionTag: {type: String},
    };
  }

  accessor device: string = '';
  accessor lastUpdateTime: string = '';
  accessor tabs: ForeignSessionTab[] = [];
  accessor opened: boolean = true;
  accessor searchTerm: string;
  accessor separatorIndexes: number[] = [];
  accessor sessionTag: string = '';

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('tabs')) {
      this.notifyFocusUpdate_();
      this.updateIcons_();
    }
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
      this.shadowRoot.querySelectorAll<HTMLElement>('.item-container')
          .forEach(function(el) {
            const row = new FocusRow(el, null);
            row.addItem('link', '.website-link');
            rows.push(row);
          });
    }
    return rows;
  }

  /** Open a single synced tab. */
  protected openTab_(e: MouseEvent) {
    const browserService = BrowserServiceImpl.getInstance();
    browserService.recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.LINK_CLICKED,
        SyncedTabsHistogram.LIMIT);
    browserService.openForeignSessionTab(
        this.sessionTag,
        Number((e.currentTarget as HTMLElement).dataset['sessionId']), e);
    e.preventDefault();
  }

  /**
   * Toggles the dropdown display of synced tabs for each device card.
   */
  async toggleTabCard() {
    const histogramValue = this.opened ? SyncedTabsHistogram.COLLAPSE_SESSION :
                                         SyncedTabsHistogram.EXPAND_SESSION;

    BrowserServiceImpl.getInstance().recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, histogramValue, SyncedTabsHistogram.LIMIT);

    this.opened = !this.opened;

    await this.updateComplete;  // Wait until focusable elements are updated.
    this.fire('update-focus-grid');
  }

  private notifyFocusUpdate_() {
    // Refresh focus after all rows are rendered.
    this.fire('update-focus-grid');
  }

  /**
   * When the synced tab information is set, the icon associated with the tab
   * website is also set.
   */
  private updateIcons_() {
    setTimeout(() => {
      const icons =
          this.shadowRoot.querySelectorAll<HTMLElement>('.website-icon');

      for (let i = 0; i < this.tabs.length; i++) {
        // Entries on this UI are coming strictly from sync, so we can set
        // |isSyncedUrlForHistoryUi| to true on the getFavicon call below.
        icons[i].style.backgroundImage = getFaviconForPageURL(
            this.tabs[i].url, true, this.tabs[i].remoteIconUrlForUma);
      }
    }, 0);
  }

  protected isWindowSeparatorIndex_(index: number): boolean {
    return this.separatorIndexes.indexOf(index) !== -1;
  }

  protected getCollapseIcon_(): string {
    return this.opened ? 'cr:expand-less' : 'cr:expand-more';
  }

  protected getCollapseTitle_(): string {
    return this.opened ? loadTimeData.getString('collapseSessionButton') :
                         loadTimeData.getString('expandSessionButton');
  }

  protected onMenuButtonClick_(e: Event) {
    this.fire('synced-device-card-open-menu', {
      target: e.target,
      tag: this.sessionTag,
    });
    e.stopPropagation();  // Prevent cr-collapse.
  }

  protected onLinkRightClick_() {
    BrowserServiceImpl.getInstance().recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.LINK_RIGHT_CLICKED,
        SyncedTabsHistogram.LIMIT);
  }

  protected onOpenedChanged_(e: CustomEvent<{value: boolean}>) {
    this.opened = e.detail.value;
  }
}

// Exported to be used in the autogenerated Lit template file
export type SyncedDeviceCardElement = HistorySyncedDeviceCardElement;

declare global {
  interface HTMLElementTagNameMap {
    'history-synced-device-card': HistorySyncedDeviceCardElement;
  }
}

customElements.define(
    HistorySyncedDeviceCardElement.is, HistorySyncedDeviceCardElement);
