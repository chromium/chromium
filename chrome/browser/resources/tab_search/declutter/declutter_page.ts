// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../tab_search_item.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {TabItemType} from '../tab_data.js';
import type {TabData} from '../tab_data.js';
import type {Tab} from '../tab_search.mojom-webui.js';

import {getCss} from './declutter_page.css.js';
import {getHtml} from './declutter_page.html.js';

export class DeclutterPageElement extends CrLitElement {
  static get is() {
    return 'declutter-page';
  }

  static override get properties() {
    return {
      staleTabDatas_: {type: Array},
    };
  }

  protected staleTabDatas_: TabData[] = this.getDummyStaleTabDatas_();

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  // TODO(358383553): Replace with actual data.
  private getDummyStaleTabDatas_(): TabData[] {
    return [
      this.createTabData_(this.createTab_({title: 'Tab 1'})),
      this.createTabData_(this.createTab_({title: 'Tab 2'})),
      this.createTabData_(this.createTab_({title: 'Tab 3'})),
    ];
  }

  private createTabData_(tab: Tab): TabData {
    return {
      tab: tab,
      hostname: '',
      inActiveWindow: false,
      type: TabItemType.OPEN_TAB,
      a11yTypeText: '',
      highlightRanges: {},
    };
  }

  private createTab_(override: Partial<Tab> = {}): Tab {
    return Object.assign(
        {
          active: false,
          alertStates: [],
          index: -1,
          faviconUrl: null,
          tabId: -1,
          groupId: -1,
          pinned: false,
          title: '',
          url: {url: 'https://www.google.com'},
          isDefaultFavicon: false,
          showIcon: false,
          lastActiveTimeTicks: -1,
          lastActiveElapsedText: '',
        },
        override);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'declutter-page': DeclutterPageElement;
  }
}

customElements.define(DeclutterPageElement.is, DeclutterPageElement);
