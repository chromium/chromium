// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/strings.m.js';
import '../tab_search_item.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {normalizeURL, TabData, TabItemType} from '../tab_data.js';
import type {Tab} from '../tab_search.mojom-webui.js';
import {DeclutterCTREvent} from '../tab_search.mojom-webui.js';
import type {TabSearchApiProxy} from '../tab_search_api_proxy.js';
import {TabSearchApiProxyImpl} from '../tab_search_api_proxy.js';
import {TabSearchItemElement} from '../tab_search_item.js';

import {getCss} from './declutter_page.css.js';
import {getHtml} from './declutter_page.html.js';

const MINIMUM_SCROLLABLE_MAX_HEIGHT: number = 238;
const NON_SCROLLABLE_VERTICAL_SPACING: number = 164;

export class DeclutterPageElement extends CrLitElement {
  static get is() {
    return 'declutter-page';
  }

  static override get properties() {
    return {
      availableHeight: {type: Number},
      showBackButton: {type: Boolean},
      staleTabDatas_: {type: Array},
      duplicateTabDatas_: {type: Object},
      dedupeEnabled_: {type: Boolean},
    };
  }

  availableHeight: number = 0;
  showBackButton: boolean = false;

  protected staleTabDatas_: TabData[] = [];
  protected duplicateTabDatas_: Map<string, TabData[]> = new Map();
  protected dedupeEnabled_: boolean = loadTimeData.getBoolean('dedupeEnabled');
  private apiProxy_: TabSearchApiProxy = TabSearchApiProxyImpl.getInstance();
  private listenerIds_: number[] = [];
  private visibilityChangedListener_: () => void;

  static override get styles() {
    return getCss();
  }

  constructor() {
    super();

    this.visibilityChangedListener_ = () => {
      if (document.visibilityState === 'visible') {
        this.apiProxy_.getStaleTabs().then(
            ({tabs}) => this.setStaleTabs_(tabs));
      }
    };
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
    document.addEventListener(
        'visibilitychange', this.visibilityChangedListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));
    document.removeEventListener(
        'visibilitychange', this.visibilityChangedListener_);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('availableHeight')) {
      this.onAvailableHeightChange_();
    }
    if (changedPrivateProperties.has('staleTabDatas_')) {
      this.maybeAddScrollListener_();
      this.updateScroll_();
    }
  }

  override firstUpdated() {
    this.maybeAddScrollListener_();
  }

  override focus() {
    if (this.showBackButton) {
      const backButton = this.shadowRoot!.querySelector('cr-icon-button')!;
      backButton.focus();
    } else {
      super.focus();
    }
  }

  logCtrValue(event: DeclutterCTREvent) {
    chrome.metricsPrivate.recordEnumerationValue(
        'Tab.Organization.DeclutterCTR', event,
        DeclutterCTREvent.MAX_VALUE + 1);
  }

  private getMaxScrollableHeight_(): number {
    return Math.max(
        MINIMUM_SCROLLABLE_MAX_HEIGHT,
        (this.availableHeight - NON_SCROLLABLE_VERTICAL_SPACING));
  }

  private onAvailableHeightChange_() {
    const scrollable = this.shadowRoot!.querySelector('#scrollable');
    if (scrollable) {
      this.updateScroll_();
    }
  }

  private async maybeAddScrollListener_() {
    const scrollable = this.shadowRoot!.querySelector('#scrollable');
    if (scrollable) {
      scrollable.addEventListener('scroll', this.updateScroll_.bind(this));
    }
  }

  private async updateScroll_() {
    await this.updateComplete;
    const scrollable =
        this.shadowRoot!.querySelector<HTMLElement>('#scrollable');
    if (scrollable) {
      const maxHeight = this.getMaxScrollableHeight_();
      scrollable.style.maxHeight = maxHeight + 'px';
      scrollable.classList.toggle(
          'can-scroll', scrollable.clientHeight < scrollable.scrollHeight);
      scrollable.classList.toggle('is-scrolled', scrollable.scrollTop > 0);
      scrollable.classList.toggle(
          'scrolled-to-bottom',
          scrollable.scrollTop + maxHeight >= scrollable.scrollHeight);
    }
  }

  protected getBackButtonAriaLabel_(): string {
    return loadTimeData.getStringF(
        'backButtonAriaLabel', loadTimeData.getString('declutterTitle'));
  }

  protected getCloseButtonAriaLabel_(tabData: TabData): string {
    return loadTimeData.getStringF(
        'declutterCloseTabAriaLabel', tabData.tab.title);
  }

  protected getCloseButtonTooltip_(): string {
    return loadTimeData.getString('declutterCloseTabTooltip');
  }

  protected onBackClick_() {
    this.fire('back-click');
  }

  protected onCloseTabsClick_() {
    const tabIds = this.staleTabDatas_.map((tabData) => tabData.tab.tabId);
    this.apiProxy_.declutterTabs(tabIds);
    this.logCtrValue(DeclutterCTREvent.kCloseTabsClicked);
  }

  protected onTabFocus_(e: FocusEvent) {
    if (e.target instanceof TabSearchItemElement) {
      const tabSearchItem: TabSearchItemElement = e.target;
      tabSearchItem.classList.toggle('selected', true);
      const closeButton =
          tabSearchItem.shadowRoot!.querySelector('#closeButton')!;
      closeButton.setAttribute('aria-selected', 'true');
    } else {
      throw new Error('Invalid onTabFocus_ target type: ' + typeof e.target);
    }
  }

  protected onTabBlur_(e: FocusEvent) {
    if (e.target instanceof TabSearchItemElement) {
      const tabSearchItem: TabSearchItemElement = e.target;
      tabSearchItem.classList.toggle('selected', false);
      const closeButton =
          tabSearchItem.shadowRoot!.querySelector('#closeButton')!;
      closeButton.setAttribute('aria-selected', 'false');
    } else {
      throw new Error('Invalid onTabBlur_ target type: ' + typeof e.target);
    }
  }

  protected onTabKeyDown_(e: KeyboardEvent) {
    if ((e.key !== 'ArrowUp' && e.key !== 'ArrowDown')) {
      return;
    }
    const tabSearchItems =
        Array.from(this.shadowRoot!.querySelectorAll('tab-search-item'));
    const tabSearchItemCount = tabSearchItems.length;
    const focusedIndex =
        tabSearchItems.findIndex((element) => element.matches(':focus'));
    if (focusedIndex < 0) {
      return;
    }
    let nextFocusedIndex = 0;
    if (e.key === 'ArrowUp') {
      nextFocusedIndex =
          (focusedIndex + tabSearchItemCount - 1) % tabSearchItemCount;
    } else if (e.key === 'ArrowDown') {
      nextFocusedIndex = (focusedIndex + 1) % tabSearchItemCount;
    }
    const selectedItem = tabSearchItems[nextFocusedIndex]!;
    const focusableElement =
        selectedItem.shadowRoot!.querySelector(`cr-icon-button`)!;
    focusableElement.focus();
    e.preventDefault();
    e.stopPropagation();
  }

  protected onStaleTabRemove_(e: Event) {
    const tabData = (e.currentTarget as TabSearchItemElement).data;
    this.apiProxy_.excludeFromStaleTabs(tabData.tab.tabId);
  }

  protected onDuplicateTabRemove_(_e: Event) {
    // TODO(crbug.com/376739583): Implement this along with api proxy call
  }

  protected getDuplicateTabDataList_(this: DeclutterPageElement) {
    const tabDatas: TabData[] = [];
    this.duplicateTabDatas_.forEach((value: TabData[], key: string, _map) => {
      const primaryTabData = structuredClone(value[0])!;
      primaryTabData.tab.title = key;
      primaryTabData.tab.lastActiveElapsedText = value.length.toString();
      tabDatas.push(primaryTabData);
    });
    return tabDatas;
  }

  private setStaleTabs_(tabs: Tab[]): void {
    this.staleTabDatas_ = tabs.map((tab) => this.tabDataFromTab_(tab));
    // TODO(crbug.com/376739583): Placeholder, replace with api proxy calls
    if (this.dedupeEnabled_) {
      this.duplicateTabDatas_ = new Map();
      tabs.forEach((tab) => {
        this.duplicateTabDatas_.set(
            tab.url.url.toString(), [this.tabDataFromTab_(tab)]);
      });
    }
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
