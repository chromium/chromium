// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './bookmark_bar.js';
import './icons.html.js';
import '/strings.m.js';
import './tab_strip.js';
import './webview.js';
import '//resources/cr_components/searchbox/searchbox.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {BookmarkBar} from './bookmark_bar.js';
import {BookmarkBarController} from './bookmark_bar_controller.js';
import {BrowserProxy} from './browser_proxy.js';
import type {LayoutManager} from './tab_strip_controller.js';
import {TabStripController} from './tab_strip_controller.js';

export interface WebuiBrowserAppElement {
  $: {bookmarkBar: BookmarkBar};
}

export class WebuiBrowserAppElement extends CrLitElement implements
    LayoutManager {
  static get is() {
    return 'webui-browser-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  private bookmarkBarController_: BookmarkBarController;
  private tabStripController_: TabStripController;
  protected backButtonDisabled_: boolean = true;
  protected forwardButtonDisabled_: boolean = true;

  constructor() {
    super();

    this.bookmarkBarController_ = new BookmarkBarController();
    this.tabStripController_ = new TabStripController(this);
  }

  override connectedCallback() {
    // Important. Properties are not reactive without calling
    // super.connectedCallback().
    super.connectedCallback();
  }

  static override get properties() {
    return {
      guestId_: {type: Number},
      backButtonDisabled_: {state: true, type: Boolean},
      forwardButtonDisabled_: {state: true, type: Boolean},
    };
  }

  // LayoutManager:
  refreshLayout() {
    this.updateToolbarButtons_();
  }

  protected accessor guestId_: number = loadTimeData.getInteger('testGuestId');

  protected onLaunchDevtoolsClick_(_: Event) {
    BrowserProxy.getPageHandler().launchDevToolsForBrowser();
  }

  protected onAppMenuClick_(_: Event) {
    BrowserProxy.getPageHandler().openAppMenu();
  }

  protected onAvatarClick_(_: Event) {
    BrowserProxy.getPageHandler().openProfileMenu();
  }

  protected onMinimizeClick_(_: Event) {
    BrowserProxy.getPageHandler().minimize();
  }

  protected onMaximizeClick_(_: Event) {
    BrowserProxy.getPageHandler().maximize();
  }

  protected onRestoreClick_(_: Event) {
    BrowserProxy.getPageHandler().restore();
  }

  protected onCloseClick_(_: Event) {
    BrowserProxy.getPageHandler().close();
  }

  protected onBackClick_(_: Event) {
    /* TODO(webium): Once ContentRegion is implemented:
    if (this.$.contentRegion.activeWebview_) {
      this.$.contentRegion.activeWebview_.goBack();
    }*/
  }

  protected onForwardClick_(_: Event) {
    /* TODO(webium): Once ContentRegion is implemented:
    if (this.$.contentRegion.activeWebview_) {
      this.$.contentRegion.activeWebview_.goForward();
    }*/
  }

  private async updateToolbarButtons_() {
    /* TODO(webium): Once ContentRegion is implemented:
    const webview = this.$.contentRegion.activeWebview_;
    if (webview) {
      const [canGoBack, canGoForward] =
          await Promise.all([webview.canGoBack(), webview.canGoForward()]);
      this.backButtonDisabled_ = !canGoBack;
      this.forwardButtonDisabled_ = !canGoForward;
    } else {
      this.backButtonDisabled_ = true;
      this.forwardButtonDisabled_ = true;
    }*/
  }

  protected onTabstripAdded_(e: CustomEvent) {
    this.tabStripController_.init(e.detail.tabstrip);
  }

  protected onTabClick_(e: CustomEvent) {
    this.tabStripController_.onTabClick(e);
  }

  protected onTabDragOutOfBounds_(e: CustomEvent) {
    this.tabStripController_.onTabDragOutOfBounds(e);
  }

  protected onTabClosed_(e: CustomEvent) {
    const tabId = e.detail.tabId;
    this.tabStripController_.removeTab(tabId);
  }

  protected onAddTabClick_(_: Event) {
    this.tabStripController_.addNewTab();
  }

  protected override firstUpdated() {
    this.bookmarkBarController_.init(this.$.bookmarkBar);
  }

  protected onShowBookmarkBar_() {
    this.$.bookmarkBar.style.display = 'flex';
  }

  protected onHideBookmarkBar_() {
    this.$.bookmarkBar.style.display = 'none';
  }

  protected onBookmarkButtonClick_(e: CustomEvent) {
    const bookmarkId = e.detail.bookmarkId;
    this.bookmarkBarController_.launchBookmark(bookmarkId);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-app': WebuiBrowserAppElement;
  }
}

customElements.define(WebuiBrowserAppElement.is, WebuiBrowserAppElement);
