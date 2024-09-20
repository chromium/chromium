// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/managed_footnote/managed_footnote.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import 'chrome://resources/cr_elements/cr_nav_menu_item_style.css.js';
import 'chrome://resources/cr_elements/cr_ripple/cr_ripple.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './shared_icons.html.js';
import './shared_vars.css.js';
import './strings.m.js';

import type {CrMenuSelector} from 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserServiceImpl} from './browser_service.js';
import {Page, TABBED_PAGES} from './router.js';
import {getTemplate} from './side_bar.html.js';

export interface FooterInfo {
  managed: boolean;
  otherFormsOfHistory: boolean;
}

export interface HistorySideBarElement {
  $: {
    'history': HTMLAnchorElement,
    'menu': CrMenuSelector,
    'toggle-history-clusters': HTMLElement,
    'syncedTabs': HTMLElement,
  };
}

export class HistorySideBarElement extends PolymerElement {
  static get is() {
    return 'history-side-bar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      footerInfo: Object,

      historyClustersEnabled: Boolean,

      historyClustersVisible: {
        type: Boolean,
        notify: true,
      },

      /* The id of the currently selected page. */
      selectedPage: {
        type: String,
        notify: true,
      },

      /* The index of the currently selected tab. */
      selectedTab: {
        type: Number,
        notify: true,
      },

      guestSession_: Boolean,

      historyClustersVisibleManagedByPolicy_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean(
              'isHistoryClustersVisibleManagedByPolicy');
        },
      },

      /**
       * Used to display notices for profile sign-in status and managed status.
       */
      showFooter_: {
        type: Boolean,
        computed: 'computeShowFooter_(' +
            'footerInfo.otherFormsOfHistory, footerInfo.managed)',
      },

      showHistoryClusters_: {
        type: Boolean,
        computed: 'computeShowHistoryClusters_(' +
            'historyClustersEnabled, historyClustersVisible)',
      },

      compareHistoryEnabled_: Boolean,
    };
  }

  footerInfo: FooterInfo;
  historyClustersEnabled: boolean;
  historyClustersVisible: boolean;
  selectedPage: string;
  selectedTab: number;
  private guestSession_ = loadTimeData.getBoolean('isGuestSession');
  private historyClustersVisibleManagedByPolicy_: boolean;
  private showFooter_: boolean;
  private showHistoryClusters_: boolean;
  private compareHistoryEnabled_: boolean =
      loadTimeData.getBoolean('compareHistoryEnabled');

  override ready() {
    super.ready();
    this.addEventListener('keydown', e => this.onKeydown_(e));
  }

  private onKeydown_(e: KeyboardEvent) {
    if (e.key === ' ') {
      (e.composedPath()[0] as HTMLElement).click();
    }
  }

  private onSelectorActivate_() {
    this.dispatchEvent(new CustomEvent(
        'history-close-drawer', {bubbles: true, composed: true}));
  }

  /**
   * Relocates the user to the clear browsing data section of the settings page.
   */
  private onClearBrowsingDataClick_(e: Event) {
    const browserService = BrowserServiceImpl.getInstance();
    browserService.recordAction('InitClearBrowsingData');
    browserService.openClearBrowsingData();
    e.preventDefault();
  }

  private computeClearBrowsingDataTabIndex_(): string {
    return this.guestSession_ ? '-1' : '';
  }

  /**
   * Prevent clicks on sidebar items from navigating. These are only links for
   * accessibility purposes, taps are handled separately.
   */
  private onItemClick_(e: Event) {
    e.preventDefault();
  }

  /**
   * @returns The url to navigate to when the history menu item is clicked. It
   *     reflects the currently selected tab.
   */
  private getHistoryItemHref_(): string {
    return this.showHistoryClusters_ &&
            TABBED_PAGES[this.selectedTab] === Page.HISTORY_CLUSTERS ?
        '/' + Page.HISTORY_CLUSTERS :
        '/';
  }

  /**
   * @returns The path that determines if the history menu item is selected. It
   *     reflects the currently selected tab.
   */
  private getHistoryItemPath_(): string {
    return this.showHistoryClusters_ &&
            TABBED_PAGES[this.selectedTab] === Page.HISTORY_CLUSTERS ?
        Page.HISTORY_CLUSTERS :
        Page.HISTORY;
  }

  private computeShowFooter_(
      includeOtherFormsOfBrowsingHistory: boolean, managed: boolean): boolean {
    return includeOtherFormsOfBrowsingHistory || managed;
  }

  private computeShowHistoryClusters_(): boolean {
    return this.historyClustersEnabled && this.historyClustersVisible;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-side-bar': HistorySideBarElement;
  }
}

customElements.define(HistorySideBarElement.is, HistorySideBarElement);
