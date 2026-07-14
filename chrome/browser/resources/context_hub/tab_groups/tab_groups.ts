// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
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
      autoTabGroupsEnabled_: {type: Boolean},
      inputValue_: {type: String},
    };
  }

  protected accessor tabs_: TabInfo[] = [];
  protected accessor groups_: TabGroup[] = [];
  protected accessor ungroupedTabs_: TabInfo[] = [];
  protected accessor isGrouped_: boolean = false;
  protected accessor isGrouping_: boolean = false;
  protected accessor autoTabGroupsEnabled_: boolean =
      loadTimeData.getBoolean('kAutoTabGroups');
  protected accessor inputValue_: string = '';

  override connectedCallback() {
    super.connectedCallback();
    this.fetchTabs_();
  }

  private async fetchTabs_() {
    if (!this.autoTabGroupsEnabled_) {
      return;
    }
    const {tabs} = await BrowserProxyImpl.getInstance().handler.getTabs();
    this.tabs_ = tabs;
    this.groups_ = [];
    this.isGrouped_ = false;
    this.ungroupedTabs_ = [];
    this.isGrouping_ = false;
  }

  protected async onGroupTabsClick_() {
    if (!this.autoTabGroupsEnabled_) {
      return;
    }
    if (this.isGrouped_) {
      this.fetchTabs_();
      return;
    }

    this.isGrouping_ = true;

    try {
      const {groups, ungroupedTabs} =
          await BrowserProxyImpl.getInstance().handler.retrieveAndGroupTabs();

      this.groups_ = groups
                         .map(group => ({
                                label: group.label,
                                tabs: group.tabs,
                                expanded: false,
                              }))
                         .filter(group => group.tabs.length > 0);

      this.ungroupedTabs_ = ungroupedTabs;
      this.isGrouped_ = true;
    } catch (e) {
      console.error('Failed to retrieve and group tabs:', e);
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

  protected onInputValueChanged_(e: CustomEvent<{value: string}>) {
    this.inputValue_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-groups': TabGroupsElement;
  }
}

customElements.define(TabGroupsElement.is, TabGroupsElement);
