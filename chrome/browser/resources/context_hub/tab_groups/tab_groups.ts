// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/icons.html.js';
import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory, ChatRole} from '../context_hub.mojom-webui.js';
import type {ChatMessage, TabInfo} from '../context_hub.mojom-webui.js';

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
      chatHistory_: {type: Array},
      isGrouped_: {type: Boolean},
      isGrouping_: {type: Boolean},
      autoTabGroupsEnabled_: {type: Boolean},
      inputValue_: {type: String},
    };
  }

  protected accessor tabs_: TabInfo[] = [];
  protected accessor groups_: TabGroup[] = [];
  protected accessor ungroupedTabs_: TabInfo[] = [];
  protected accessor chatHistory_: ChatMessage[] = [];
  protected accessor isGrouped_: boolean = false;
  protected accessor isGrouping_: boolean = false;
  protected accessor autoTabGroupsEnabled_: boolean =
      loadTimeData.getBoolean('kAutoTabGroups');
  protected maxTabGroupChatHistoryTurns_: number = this.autoTabGroupsEnabled_ ?
      loadTimeData.getInteger('kMaxTabGroupChatHistoryTurns') :
      10;
  protected accessor inputValue_: string = '';

  private trimChatHistory_(history: ChatMessage[]): ChatMessage[] {
    if (this.maxTabGroupChatHistoryTurns_ > 0 &&
        history.length > this.maxTabGroupChatHistoryTurns_) {
      return history.slice(history.length - this.maxTabGroupChatHistoryTurns_);
    }
    return history;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.fetchExistingTabGroupsAndChats_();
  }

  private async fetchExistingTabGroupsAndChats_() {
    if (!this.autoTabGroupsEnabled_) {
      return;
    }
    try {
      const {groups, ungroupedTabs, history} =
          await browserProxyFactory.getInstance()
              .handler.getExistingTabGroupsAndChats();

      if (groups && groups.length > 0) {
        this.groups_ = groups.map(group => ({
                                    label: group.label,
                                    tabs: group.tabs,
                                    expanded: false,
                                  }));
        this.ungroupedTabs_ = ungroupedTabs;
        this.isGrouped_ = true;
      } else {
        await this.fetchTabs_();
      }

      if (history && history.length > 0) {
        let processedHistory = history;
        const lastMsg = processedHistory[processedHistory.length - 1];
        if (lastMsg &&
            (lastMsg.role === ChatRole.kUser || !lastMsg.content.trim())) {
          processedHistory = [
            ...processedHistory,
            {role: ChatRole.kAssistant, content: 'Grouped tabs.'},
          ];
        }
        this.chatHistory_ = this.trimChatHistory_(processedHistory);
        this.scrollToBottom_();
      }
    } catch (e) {
      await this.fetchTabs_();
    }
  }

  private async fetchTabs_() {
    if (!this.autoTabGroupsEnabled_) {
      return;
    }
    const {tabs} = await browserProxyFactory.getInstance().handler.getTabs();
    this.tabs_ = tabs;
    this.groups_ = [];
    this.isGrouped_ = false;
    this.ungroupedTabs_ = [];
    this.isGrouping_ = false;
  }

  protected async onGroupTabsClick_() {
    if (!this.autoTabGroupsEnabled_ || this.isGrouping_) {
      return;
    }

    const command = this.inputValue_.trim();
    this.isGrouping_ = true;
    this.inputValue_ = '';

    if (command) {
      this.chatHistory_ = this.trimChatHistory_([
        ...this.chatHistory_,
        {role: ChatRole.kUser, content: command},
      ]);
      this.scrollToBottom_();
    }

    try {
      const {groups, ungroupedTabs, llmResponse} =
          await browserProxyFactory.getInstance().handler.retrieveAndGroupTabs(
              command);

      this.groups_ = groups
                         .map(group => ({
                                label: group.label,
                                tabs: group.tabs,
                                expanded: false,
                              }))
                         .filter(group => group.tabs.length > 0);

      this.ungroupedTabs_ = ungroupedTabs;

      const assistantContent = llmResponse?.content.trim() ||
          (this.groups_.length > 0 ?
               `Grouped tabs into ${this.groups_.length} group(s).` :
               'Failed to group tabs.');

      this.chatHistory_ = this.trimChatHistory_([
        ...this.chatHistory_,
        {role: ChatRole.kAssistant, content: assistantContent},
      ]);
      this.isGrouped_ = true;
    } catch (e) {
      console.error('Failed to retrieve and group tabs:', e);
      this.chatHistory_ = this.trimChatHistory_([
        ...this.chatHistory_,
        {role: ChatRole.kAssistant, content: 'Failed to group tabs.'},
      ]);
    } finally {
      this.isGrouping_ = false;
      this.scrollToBottom_();
    }
  }

  protected async onUngroupTabsClick_() {
    if (!this.autoTabGroupsEnabled_ || this.isGrouping_) {
      return;
    }
    await browserProxyFactory.getInstance().handler.clearTabGroups();
    this.inputValue_ = '';
    await this.fetchTabs_();
  }

  protected async onClearChatHistoryClick_() {
    if (!this.autoTabGroupsEnabled_ || this.isGrouping_) {
      return;
    }
    await browserProxyFactory.getInstance().handler.clearTabGroupChatHistory();
    this.chatHistory_ = [];
  }

  protected async onDefaultGroupingClick_() {
    if (!this.autoTabGroupsEnabled_ || this.isGrouping_) {
      return;
    }
    this.inputValue_ = '';
    await this.onGroupTabsClick_();
  }

  protected onTabClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const tabId = BigInt(target.dataset['id'] || '0');
    if (tabId !== 0n) {
      browserProxyFactory.getInstance().handler.switchToTab(tabId);
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

  protected onInputKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter' && this.inputValue_.trim().length > 0) {
      this.onGroupTabsClick_();
    }
  }

  private scrollToBottom_() {
    this.updateComplete.then(() => {
      const chatMessages =
          this.shadowRoot?.querySelector<HTMLElement>('#chat-messages');
      if (chatMessages) {
        chatMessages.scrollTop = chatMessages.scrollHeight;
      }
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-groups': TabGroupsElement;
  }
}

customElements.define(TabGroupsElement.is, TabGroupsElement);
