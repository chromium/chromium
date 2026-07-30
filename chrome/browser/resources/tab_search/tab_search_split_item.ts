// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';

import type {CrTooltipElement} from 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {MouseHoverableMixinLit} from 'chrome://resources/cr_elements/mouse_hoverable_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getDisplayHostnameForUrl, normalizeURL, SplitViewData, TabItemType} from './tab_data.js';
import {getTabGroupColorVar} from './tab_group_color_helper.js';
import type {Tab} from './tab_search.mojom-webui.js';
import {SplitTabLayout} from './tab_search.mojom-webui.js';
import {getCss} from './tab_search_split_item.css.js';
import {getHtml} from './tab_search_split_item.html.js';
import {getFaviconUrlForTab, getMediaAlertImageClass, tabHasMediaAlerts} from './tab_search_utils.js';

const TabSearchSplitItemBase = MouseHoverableMixinLit(CrLitElement);

export class TabSearchSplitItemElement extends TabSearchSplitItemBase {
  static get is() {
    return 'tab-search-split-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      data: {type: Object},
      buttonRipples_: {type: Boolean},
      closeButtonIcon: {type: String},
      tabGroupColorRefresh_: {type: Boolean},
    };
  }

  accessor data: SplitViewData = new SplitViewData({
    splitView: {
      sessionId: -1,
      id: {high: 0n, low: 0n},
      tabCount: 0,
      lastActiveTime: {internalValue: 0n},
      lastActiveElapsedText: '',
      tabUrls: [],
      layout: SplitTabLayout.kSideBySide,
      groupId: null,
    },
  });
  protected accessor buttonRipples_: boolean =
      loadTimeData.getBoolean('useRipples');
  accessor closeButtonIcon: string =
      loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
      'tab-search:close' :
      'tab-search:close-old';
  protected accessor tabGroupColorRefresh_: boolean =
      loadTimeData.getBoolean('useTabGroupColorRefresh');

  protected getGroupColor_(): string {
    return this.data.tabGroup ?
        getTabGroupColorVar(
            this.data.tabGroup.color, this.tabGroupColorRefresh_) :
        '';
  }

  protected getFaviconUrl_(url: string, index: number): string {
    const tab = this.data.tabs ? this.data.tabs[index] : null;
    return getFaviconUrlForTab(url, tab);
  }

  get domainTexts_(): string[] {
    return this.data.tabUrls.map(url => this.getDomainTextForUrl_(url));
  }

  protected getDomainTextForUrl_(url: string): string {
    try {
      return getDisplayHostnameForUrl(new URL(normalizeURL(url)));
    } catch (e) {
      return 'about:blank';
    }
  }

  protected isCloseable_(): boolean {
    return this.data.type === TabItemType.OPEN_SPLIT;
  }

  protected tooltipForButton_(): string {
    return loadTimeData.getString('closeTab');
  }

  protected onCloseButtonClick_(e: Event) {
    e.stopPropagation();
    this.dispatchEvent(new CustomEvent('close'));
  }

  protected onCloseButtonFocus_() {
    const tooltip =
        this.shadowRoot.querySelector<CrTooltipElement>('cr-tooltip');
    assert(tooltip);
    tooltip.show();
  }

  protected onCloseButtonBlur_() {
    const tooltip =
        this.shadowRoot.querySelector<CrTooltipElement>('cr-tooltip');
    assert(tooltip);
    tooltip.hide();
  }

  protected hasMediaAlertForTab_(tab: Tab): boolean {
    return tabHasMediaAlerts(tab);
  }

  protected getMediaAlertImageClassForTab_(tab: Tab): string {
    return getMediaAlertImageClass(tab);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-search-split-item': TabSearchSplitItemElement;
  }
}

customElements.define(TabSearchSplitItemElement.is, TabSearchSplitItemElement);
