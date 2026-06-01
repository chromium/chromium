// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MouseHoverableMixinLit} from 'chrome://resources/cr_elements/mouse_hoverable_mixin_lit.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getDisplayHostnameForUrl, normalizeURL, SplitViewData} from './tab_data.js';
import {colorName} from './tab_group_color_helper.js';
import {SplitTabLayout} from './tab_search.mojom-webui.js';
import {getCss} from './tab_search_split_item.css.js';
import {getHtml} from './tab_search_split_item.html.js';

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
  protected accessor tabGroupColorRefresh_: boolean =
      loadTimeData.getBoolean('useTabGroupColorRefresh');

  protected getGroupColor_(): string {
    if (!this.data.tabGroup) {
      return '';
    }
    return this.tabGroupColorRefresh_ ?
        `var(--tab-group-refresh-color-${
            colorName(this.data.tabGroup.color)})` :
        `var(--tab-group-color-${colorName(this.data.tabGroup.color)})`;
  }

  protected groupSvgDisplay_(): string {
    return this.data.tabGroup ? 'block' : 'none';
  }

  protected getFaviconUrl_(url: string): string {
    return getFaviconForPageURL(url, false);
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
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-search-split-item': TabSearchSplitItemElement;
  }
}

customElements.define(TabSearchSplitItemElement.is, TabSearchSplitItemElement);
