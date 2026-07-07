// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from '../browser_proxy.js';
import type {TabInfo} from '../context_hub.mojom-webui.js';

import {getCss} from './tab_groups.css.js';
import {getHtml} from './tab_groups.html.js';

interface TabGroup {
  label: string;
  tabs: TabInfo[];
  expanded: boolean;
}

export class TabGroupsElement extends CrLitElement {
  static get is() {
    return 'tab-groups';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      tabs_: {type: Array},
      groups_: {type: Array},
      ungroupedTabs_: {type: Array},
      isGrouped_: {type: Boolean},
      isGrouping_: {type: Boolean},
    };
  }

  protected accessor tabs_: TabInfo[] = [];
  protected accessor groups_: TabGroup[] = [];
  protected accessor ungroupedTabs_: TabInfo[] = [];
  protected accessor isGrouped_: boolean = false;
  protected accessor isGrouping_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.fetchTabs_();
  }

  private async fetchTabs_() {
    const {tabs} = await BrowserProxyImpl.getInstance().handler.getTabs();
    this.tabs_ = tabs;
    this.isGrouped_ = false;
    this.ungroupedTabs_ = [];
    this.isGrouping_ = false;
  }

  protected async onGroupTabsClick_() {
    if (this.isGrouped_) {
      this.isGrouped_ = false;
      this.groups_ = [];
      this.ungroupedTabs_ = [];
      return;
    }

    this.isGrouping_ = true;

    try {
      const {clusters, ungroupedTabIds} =
          await BrowserProxyImpl.getInstance().handler.clusterTabs();

      const tabMap = new Map(this.tabs_.map(tab => [tab.id, tab]));

      this.groups_ =
          clusters
              .map(cluster => ({
                     label: cluster.label,
                     tabs: cluster.tabIds.map(id => tabMap.get(id))
                               .filter((t): t is TabInfo => t !== undefined),
                     expanded: false,
                   }))
              // Filter out any groups that ended up empty
              .filter(group => group.tabs.length > 0);

      this.ungroupedTabs_ = ungroupedTabIds.map(id => tabMap.get(id))
                                .filter((t): t is TabInfo => t !== undefined);

      this.isGrouped_ = true;
    } catch (e) {
      console.error('Failed to cluster tabs:', e);
    } finally {
      this.isGrouping_ = false;
    }
  }

  protected onTabClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const tabId = parseInt(target.dataset['id'] || '0', 10);
    if (tabId) {
      BrowserProxyImpl.getInstance().handler.switchToTab(tabId);
    }
  }

  protected onGroupExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    const target = e.currentTarget as HTMLElement;
    const indexStr = target.dataset['index'];
    if (indexStr === undefined) {
      return;
    }
    const index = parseInt(indexStr, 10);

    this.groups_ = this.groups_.map((g, i) => {
      if (i === index) {
        return {...g, expanded: e.detail.value};
      }
      return g;
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-groups': TabGroupsElement;
  }
}

customElements.define(TabGroupsElement.is, TabGroupsElement);
