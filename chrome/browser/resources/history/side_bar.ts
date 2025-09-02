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
  geminiAppsActivity: boolean;
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
      /**
       * Used to display Google Account section in the footer. This section
       * contains links to Google My Activity and/or Gemini Apps Activity.
       *
       * When this property is true, `showFooter_` property should also be true.
       */
      showGoogleAccountFooter_: {type: Boolean},
      /**
       * Mutually exclusive flags that determine which message to show in the
       * Google Account footer:
       *   - message with Google My Activity (GMA) link
       *   - message with Gemini Apps Activity (GAA) link
       *   - message with both Google My Activity (GMA) and
       *     Gemini Apps Activity (GAA) links.
       *
       * At most one of these can be true.
       */
      showGMAOnly_: {type: Boolean},
      showGAAOnly_: {type: Boolean},
      showGMAAndGAA_: {type: Boolean},

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
  protected accessor showGoogleAccountFooter_: boolean = false;
  protected accessor showGMAOnly_: boolean = false;
  protected accessor showGAAOnly_: boolean = false;
  protected accessor showGMAAndGAA_: boolean = false;
  private accessor showHistoryClusters_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.addEventListener('keydown', e => this.onKeydown_(e));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('footerInfo')) {
      this.updateFooterVisibility();
    }
    if (changedProperties.has('historyClustersEnabled') ||
        changedProperties.has('historyClustersVisible')) {
      this.showHistoryClusters_ =
          this.historyClustersEnabled && this.historyClustersVisible;
    }
  }

  private updateFooterVisibility() {
    // At most one of these values can be true.
    this.showGMAOnly_ = false;
    this.showGAAOnly_ = false;
    this.showGMAAndGAA_ = false;

    if (this.footerInfo.otherFormsOfHistory &&
        this.footerInfo.geminiAppsActivity) {
      this.showGMAAndGAA_ = true;
    } else if (this.footerInfo.otherFormsOfHistory) {
      this.showGMAOnly_ = true;
    } else if (this.footerInfo.geminiAppsActivity) {
      this.showGAAOnly_ = true;
    }

    this.showGoogleAccountFooter_ =
        this.showGMAAndGAA_ || this.showGMAOnly_ || this.showGAAOnly_;
    this.showFooter_ = this.footerInfo.managed || this.showGoogleAccountFooter_;
  }

  protected onGoogleAccountFooterClick_(e: Event) {
    if ((e.target as HTMLElement).tagName !== 'A') {
      // Do nothing if a link is not clicked.
      return;
    }

    e.preventDefault();

    // Proxy URL navigation to fix CI failures in
    // `js_code_coverage_browser_tests`. The tests fail because Chrome attempts
    // to open real URLs.

    const browserService = BrowserServiceImpl.getInstance();
    switch ((e.target as HTMLElement).id) {
      case 'footerGoogleMyActivityLink':
        browserService.recordAction('SideBarFooterGoogleMyActivityClick');
        browserService.navigateToUrl(
            loadTimeData.getString('sidebarFooterGMALink'), '_blank',
            e as MouseEvent);
        break;
      case 'footerGeminiAppsActivityLink':
        browserService.recordAction('SideBarFooterGeminiAppsActivityClick');
        browserService.navigateToUrl(
            loadTimeData.getString('sidebarFooterGAALink'), '_blank',
            e as MouseEvent);
        break;
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

declare global {
  interface HTMLElementTagNameMap {
    'history-side-bar': HistorySideBarElement;
  }
}

customElements.define(HistorySideBarElement.is, HistorySideBarElement);
