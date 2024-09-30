// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import '../tab_search_item.js';

import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {normalizeURL, TabData, TabItemType} from '../tab_data.js';
import type {Tab} from '../tab_search.mojom-webui.js';
import type {TabSearchApiProxy} from '../tab_search_api_proxy.js';
import {TabSearchApiProxyImpl} from '../tab_search_api_proxy.js';

import {getCss} from './declutter_page.css.js';
import {getHtml} from './declutter_page.html.js';

const MAX_SCROLLABLE_HEIGHT: number = 280;

function getEventTargetIndex(e: Event): number {
  return Number((e.currentTarget as HTMLElement).dataset['index']);
}

export interface DeclutterPageElement {
  $: {
    scrollable: HTMLElement,
  };
}

export class DeclutterPageElement extends CrLitElement {
  static get is() {
    return 'declutter-page';
  }

  static override get properties() {
    return {
      showBackButton: {type: Boolean},
      staleTabDatas_: {type: Array},
    };
  }

  showBackButton: boolean = false;

  protected staleTabDatas_: TabData[] = [];
  private apiProxy_: TabSearchApiProxy = TabSearchApiProxyImpl.getInstance();
  private listenerIds_: number[] = [];

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.apiProxy_.getStaleTabs().then(({tabs}) => this.setStaleTabs_(tabs));
    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerIds_.push(callbackRouter.staleTabsChanged.addListener(
        this.setStaleTabs_.bind(this)));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('staleTabDatas_')) {
      this.updateScroll_();
    }
  }

  override firstUpdated() {
    this.$.scrollable.addEventListener('scroll', this.updateScroll_.bind(this));
  }

  private async updateScroll_() {
    await this.updateComplete;
    const scrollable = this.$.scrollable;
    scrollable.classList.toggle(
        'can-scroll', scrollable.clientHeight < scrollable.scrollHeight);
    scrollable.classList.toggle('is-scrolled', scrollable.scrollTop > 0);
    scrollable.classList.toggle(
        'scrolled-to-bottom',
        scrollable.scrollTop + MAX_SCROLLABLE_HEIGHT >=
            scrollable.scrollHeight);
  }

  protected onBackClick_() {
    this.fire('back-click');
  }

  protected onCloseTabsClick_() {
    const tabIds = this.staleTabDatas_.map((tabData) => tabData.tab.tabId);
    this.apiProxy_.declutterTabs(tabIds);
  }

  protected onTabRemove_(e: Event) {
    const index = getEventTargetIndex(e);
    const tabData = this.staleTabDatas_[index]!;
    this.apiProxy_.excludeFromStaleTabs(tabData.tab.tabId);
  }

  private setStaleTabs_(tabs: Tab[]): void {
    this.staleTabDatas_ = tabs.map((tab) => this.tabDataFromTab_(tab));
  }

  private tabDataFromTab_(tab: Tab): TabData {
    return new TabData(
        tab, TabItemType.OPEN_TAB, new URL(normalizeURL(tab.url.url)).hostname);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'declutter-page': DeclutterPageElement;
  }
}

customElements.define(DeclutterPageElement.is, DeclutterPageElement);
