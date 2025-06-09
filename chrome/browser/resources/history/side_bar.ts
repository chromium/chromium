// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/managed_footnote/managed_footnote.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import 'chrome://resources/cr_elements/cr_ripple/cr_ripple.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './shared_icons.html.js';
import './shared_vars.css.js';
import '/strings.m.js';

import type {CrMenuSelector} from 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {BrowserServiceImpl} from './browser_service.js';
import {Page, TABBED_PAGES} from './router.js';
import {getCss} from './side_bar.css.js';
import {getHtml} from './side_bar.html.js';

export interface FooterInfo {
  managed: boolean;
  otherFormsOfHistory: boolean;
}

export interface HistorySideBarElement {
  $: {
    'history': HTMLAnchorElement,
    'menu': CrMenuSelector,
    'syncedTabs': HTMLElement,
  };
}

export class HistorySideBarElement extends CrLitElement {
  static get is() {
    return 'history-side-bar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      footerInfo: {type: Object},

      historyClustersEnabled: {type: Boolean},

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

      guestSession_: {type: Boolean},

      historyClustersVisibleManagedByPolicy_: {type: Boolean},

      /**
       * Used to display notices for profile sign-in status and managed status.
       */
      showFooter_: {type: Boolean},

      showHistoryClusters_: {type: Boolean},
    };
  }

  accessor footerInfo: FooterInfo;
  accessor historyClustersEnabled: boolean = false;
  accessor historyClustersVisible: boolean = false;
  accessor selectedPage: string;
  accessor selectedTab: number;
  protected accessor guestSession_ = loadTimeData.getBoolean('isGuestSession');
  private accessor historyClustersVisibleManagedByPolicy_: boolean =
      loadTimeData.getBoolean('isHistoryClustersVisibleManagedByPolicy');
  protected accessor showFooter_: boolean = false;
  private accessor showHistoryClusters_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.addEventListener('keydown', e => this.onKeydown_(e));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('footerInfo')) {
      this.showFooter_ =
          this.footerInfo.otherFormsOfHistory || this.footerInfo.managed;
    }
    if (changedProperties.has('historyClustersEnabled') ||
        changedProperties.has('historyClustersVisible')) {
      this.showHistoryClusters_ =
          this.historyClustersEnabled && this.historyClustersVisible;
    }
  }

  private onKeydown_(e: KeyboardEvent) {
    if (e.key === ' ') {
      (e.composedPath()[0] as HTMLElement).click();
    }
  }

  protected onSelectorActivate_() {
    this.fire('history-close-drawer');
  }

  protected onSelectorSelectedChanged_(e: CustomEvent<{value: string}>) {
    this.selectedPage = e.detail.value;
  }

  /**
   * Relocates the user to the clear browsing data section of the settings page.
   */
  protected onClearBrowsingDataClick_(e: Event) {
    const browserService = BrowserServiceImpl.getInstance();
    browserService.recordAction('InitClearBrowsingData');
    browserService.handler.openClearBrowsingDataDialog();
    e.preventDefault();
  }

  /**
   * Prevent clicks on sidebar items from navigating. These are only links for
   * accessibility purposes, taps are handled separately.
   */
  protected onItemClick_(e: Event) {
    e.preventDefault();
  }

  /**
   * @returns The url to navigate to when the history menu item is clicked. It
   *     reflects the currently selected tab.
   */
  protected getHistoryItemHref_(): string {
    return this.showHistoryClusters_ &&
            TABBED_PAGES[this.selectedTab] === Page.HISTORY_CLUSTERS ?
        '/' + Page.HISTORY_CLUSTERS :
        '/';
  }

  /**
   * @returns The path that determines if the history menu item is selected. It
   *     reflects the currently selected tab.
   */
  protected getHistoryItemPath_(): string {
    return this.showHistoryClusters_ &&
            TABBED_PAGES[this.selectedTab] === Page.HISTORY_CLUSTERS ?
        Page.HISTORY_CLUSTERS :
        Page.HISTORY;
  }
}

// Exported to be used in the autogenerated Lit template file
export type SideBarElement = HistorySideBarElement;

declare global {
  interface HTMLElementTagNameMap {
    'history-side-bar': HistorySideBarElement;
  }
}

customElements.define(HistorySideBarElement.is, HistorySideBarElement);
