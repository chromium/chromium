// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './bookmark_bar.js';
import './content_region.js';
import './icons.html.js';
import '/strings.m.js';
import './tab_strip.js';
import './webview.js';
import '//resources/cr_components/searchbox/searchbox.js';

import {TrackedElementManager} from 'chrome://resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {BookmarkBar} from './bookmark_bar.js';
import {BookmarkBarController} from './bookmark_bar_controller.js';
import {BrowserProxy} from './browser_proxy.js';
import type {ContentRegion} from './content_region.js';
import {TabStrip} from './tab_strip.js';
import type {LayoutManager} from './tab_strip_controller.js';
import {TabStripController} from './tab_strip_controller.js';

export interface WebuiBrowserAppElement {
  $: {
    appMenuButton: HTMLElement,
    bookmarkBar: BookmarkBar,
    contentRegion: ContentRegion,
    tabstrip: TabStrip,
  };
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

  static override get properties() {
    return {
      backButtonDisabled_: {state: true, type: Boolean},
      forwardButtonDisabled_: {state: true, type: Boolean},
    };
  }

  private bookmarkBarController_: BookmarkBarController;
  private tabStripController_: TabStripController;
  private trackedElementManager_: TrackedElementManager;
  protected accessor backButtonDisabled_: boolean = true;
  protected accessor forwardButtonDisabled_: boolean = true;

  constructor() {
    super();

    this.bookmarkBarController_ = new BookmarkBarController();
    this.tabStripController_ =
        new TabStripController(this, this.$.tabstrip, this.$.contentRegion);
    this.trackedElementManager_ = new TrackedElementManager();
  }

  override connectedCallback() {
    // Important. Properties are not reactive without calling
    // super.connectedCallback().
    super.connectedCallback();
    this.trackedElementManager_.startTracking(
        this.$.appMenuButton, 'kToolbarAppMenuButtonElementId');
  }

  // LayoutManager:
  refreshLayout() {
    this.updateToolbarButtons_();
  }

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
    if (this.$.contentRegion.activeWebview) {
      this.$.contentRegion.activeWebview.goBack();
    }
  }

  protected onForwardClick_(_: Event) {
    if (this.$.contentRegion.activeWebview) {
      this.$.contentRegion.activeWebview.goForward();
    }
  }

  private async updateToolbarButtons_() {
    const webview = this.$.contentRegion.activeWebview;
    if (webview) {
      const [canGoBack, canGoForward] =
          await Promise.all([webview.canGoBack(), webview.canGoForward()]);
      this.backButtonDisabled_ = !canGoBack;
      this.forwardButtonDisabled_ = !canGoForward;
    } else {
      this.backButtonDisabled_ = true;
      this.forwardButtonDisabled_ = true;
    }
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

  protected onTabDragMouseDown_(e: MouseEvent) {
    if (e.target instanceof TabStrip) {
      this.$.tabstrip.dragMouseDown(e);
      this.addEventListener('mouseup', this.onTabDragMouseUp_);
      this.addEventListener('mousemove', this.onTabDragMouseMove_);
    }
  }

  protected onTabDragMouseUp_(_: MouseEvent) {
    this.$.tabstrip.closeDragElement();
    this.removeEventListener('mouseup', this.onTabDragMouseUp_);
    this.removeEventListener('mousemove', this.onTabDragMouseMove_);
  }

  protected onTabDragMouseMove_(e: MouseEvent) {
    this.$.tabstrip.elementDrag(e);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-app': WebuiBrowserAppElement;
  }
}

customElements.define(WebuiBrowserAppElement.is, WebuiBrowserAppElement);
