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
import type {ChatMessage, TabGroup as TabGroupMojom, TabInfo} from '../context_hub.mojom-webui.js';

import {getCss} from './tab_groups.css.js';
import {getHtml} from './tab_groups.html.js';

const CANVAS_FEEDBACK_FORM_URL =
    'https://docs.google.com/forms/d/e/1FAIpQLSfseE-j9tXWU7oSbcUAY37K2pGlkkCPGzjxe9V9ZigGasSB3Q/viewform';
const ENTRY_USER_PROMPT = 'entry.372998523';
const ENTRY_EXPORTED_DATA = 'entry.1489365180';
const ENTRY_LIKED_DISLIKED = 'entry.1865051344';
const ENTRY_GROUPING_DESCRIPTION = 'entry.532400426';
const ENTRY_OVERALL_RATING = 'entry.647161720';

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
      confirmedGroupSummaries_: {type: Array},
      expandedConfirmedGroups_: {type: Object},
      isGrouped_: {type: Boolean},
      isGrouping_: {type: Boolean},
      autoTabGroupsEnabled_: {type: Boolean},
      inputValue_: {type: String},
      canvasFeedbackLiked_: {type: Boolean},
      chatFeedbackLiked_: {type: Boolean},
    };
  }

  protected accessor tabs_: TabInfo[] = [];
  protected accessor groups_: TabGroup[] = [];
  protected accessor ungroupedTabs_: TabInfo[] = [];
  protected accessor chatHistory_: ChatMessage[] = [];
  protected accessor confirmedGroupSummaries_: TabGroupMojom[] = [];
  protected accessor expandedConfirmedGroups_: Set<string> = new Set();
  protected accessor isGrouped_: boolean = false;
  protected accessor isGrouping_: boolean = false;
  protected accessor autoTabGroupsEnabled_: boolean =
      loadTimeData.getBoolean('kAutoTabGroups');
  protected maxTabGroupChatHistoryTurns_: number = this.autoTabGroupsEnabled_ ?
      loadTimeData.getInteger('kMaxTabGroupChatHistoryTurns') :
      10;
  protected accessor inputValue_: string = '';
  protected accessor canvasFeedbackLiked_: boolean|null = null;
  protected accessor chatFeedbackLiked_: boolean|null = null;
  protected lastGroupPrompt_: string = 'Default';

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
    this.fetchConfirmedTabGroupSummaries_();
  }

  private async fetchConfirmedTabGroupSummaries_() {
    if (!this.autoTabGroupsEnabled_) {
      return;
    }
    const {groups} =
        await browserProxyFactory.getInstance().handler.getConfirmedTabGroups();
    this.confirmedGroupSummaries_ = groups;
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
    this.canvasFeedbackLiked_ = null;
    this.chatFeedbackLiked_ = null;
  }

  protected async onGroupTabsClick_() {
    if (!this.autoTabGroupsEnabled_ || this.isGrouping_) {
      return;
    }

    const command = this.inputValue_.trim();
    this.isGrouping_ = true;
    this.lastGroupPrompt_ = command || 'Default';
    this.inputValue_ = '';
    this.canvasFeedbackLiked_ = null;
    this.chatFeedbackLiked_ = null;

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

  protected async onConfirmAllGroupsClick_() {
    if (!this.autoTabGroupsEnabled_ || this.isGrouping_) {
      return;
    }
    const {success} =
        await browserProxyFactory.getInstance().handler.confirmAllTabGroups();
    if (success) {
      this.groups_ = [];
      this.isGrouped_ = false;
      await this.fetchTabs_();
      await this.fetchConfirmedTabGroupSummaries_();
    }
  }

  protected async onUngroupTabsClick_() {
    if (!this.autoTabGroupsEnabled_ || this.isGrouping_) {
      return;
    }
    await browserProxyFactory.getInstance().handler.clearTabGroups();
    this.inputValue_ = '';
    await this.fetchTabs_();
    await this.fetchConfirmedTabGroupSummaries_();
  }

  protected async onClearChatHistoryClick_() {
    if (!this.autoTabGroupsEnabled_ || this.isGrouping_) {
      return;
    }
    await browserProxyFactory.getInstance().handler.clearTabGroupChatHistory();
    this.chatHistory_ = [];
    this.chatFeedbackLiked_ = null;
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

  protected onConfirmedGroupExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    const target = e.currentTarget as HTMLElement;
    const indexStr = target.dataset['index'];
    if (indexStr === undefined) {
      return;
    }
    const index = parseInt(indexStr, 10);
    const summary = this.confirmedGroupSummaries_[index];
    if (!summary || !summary.savedGuid) {
      return;
    }
    const newSet = new Set(this.expandedConfirmedGroups_);
    if (e.detail.value) {
      newSet.add(summary.savedGuid.value);
    } else {
      newSet.delete(summary.savedGuid.value);
    }
    this.expandedConfirmedGroups_ = newSet;
  }

  protected async onUngroupAllConfirmedGroupsClick_() {
    if (!this.autoTabGroupsEnabled_ || this.isGrouping_) {
      return;
    }
    await browserProxyFactory.getInstance()
        .handler.removeAllConfirmedTabGroups();
    await this.fetchConfirmedTabGroupSummaries_();
    await this.fetchTabs_();
  }

  protected async onUngroupConfirmedGroupClick_(e: Event) {
    if (!this.autoTabGroupsEnabled_ || this.isGrouping_) {
      return;
    }
    const target = e.currentTarget as HTMLElement;
    const savedGuid = target.dataset['guid'];
    if (!savedGuid) {
      return;
    }
    await browserProxyFactory.getInstance().handler.removeConfirmedTabGroup(
        {value: savedGuid});
    await this.fetchConfirmedTabGroupSummaries_();
    await this.fetchTabs_();
  }

  protected async onCloseConfirmedGroupClick_(e: Event) {
    if (!this.autoTabGroupsEnabled_ || this.isGrouping_) {
      return;
    }
    const target = e.currentTarget as HTMLElement;
    const savedGuid = target.dataset['guid'];
    if (!savedGuid) {
      return;
    }
    await browserProxyFactory.getInstance().handler.closeConfirmedTabGroup(
        {value: savedGuid});
    await this.fetchConfirmedTabGroupSummaries_();
    await this.fetchTabs_();
  }

  protected getCanvasThumbsUpIcon_(): string {
    return this.canvasFeedbackLiked_ === true ? 'cr:thumb-up-filled' :
                                                'cr:thumb-up';
  }

  protected getCanvasThumbsDownIcon_(): string {
    return this.canvasFeedbackLiked_ === false ? 'cr:thumb-down-filled' :
                                                 'cr:thumb-down';
  }

  protected getChatThumbsUpIcon_(): string {
    return this.chatFeedbackLiked_ === true ? 'cr:thumb-up-filled' :
                                              'cr:thumb-up';
  }

  protected getChatThumbsDownIcon_(): string {
    return this.chatFeedbackLiked_ === false ? 'cr:thumb-down-filled' :
                                               'cr:thumb-down';
  }

  private sanitizeText_(str: string): string {
    if (!str) {
      return '';
    }
    return str.toWellFormed()
        .replace(/[\x00-\x08\x0B\x0C\x0E-\x1F]/g, '')
        .trim();
  }

  private cleanUrl_(rawUrl: string|{url: string}|null|undefined): string {
    const urlStr = typeof rawUrl === 'string' ? rawUrl : (rawUrl?.url || '');
    if (!urlStr) {
      return '';
    }
    try {
      const u = new URL(urlStr);
      return `${u.origin}${u.pathname}`;
    } catch {
      return urlStr.substring(0, 80);
    }
  }

  protected exportGroupDataMarkdown_(): string {
    const lines: string[] = [];

    if (this.chatHistory_.length > 0) {
      lines.push('## Chat History');
      for (const msg of this.chatHistory_) {
        const role = msg.role === ChatRole.kUser ? 'User' : 'Assistant';
        lines.push(`- ${role}: ${this.sanitizeText_(msg.content)}`);
      }
      lines.push('');
    }

    const activeGroups = this.groups_.length > 0 ?
        this.groups_ :
        this.confirmedGroupSummaries_.map(
            g => ({label: g.label, tabs: g.tabs, expanded: false}));

    if (activeGroups.length > 0) {
      lines.push('## Tab Groups');
      for (const group of activeGroups) {
        lines.push(`### ${this.sanitizeText_(group.label)} (${
            group.tabs.length} tabs)`);
        for (const tab of group.tabs) {
          const title = this.sanitizeText_(tab.title);
          const url = this.cleanUrl_(tab.url);
          lines.push(`* ${title} | ${url}`);
        }
        lines.push('');
      }
    }

    if (this.ungroupedTabs_.length > 0) {
      lines.push(`## Ungrouped Tabs (${this.ungroupedTabs_.length} tabs)`);
      for (const tab of this.ungroupedTabs_) {
        const title = this.sanitizeText_(tab.title);
        const url = this.cleanUrl_(tab.url);
        lines.push(`* ${title} | ${url}`);
      }
      lines.push('');
    }

    return lines.join('\n');
  }

  protected onCanvasThumbsUpClick_() {
    this.sendFeedback_('canvas', true);
  }

  protected onCanvasThumbsDownClick_() {
    this.sendFeedback_('canvas', false);
  }

  protected onChatThumbsUpClick_() {
    this.sendFeedback_('chat', true);
  }

  protected onChatThumbsDownClick_() {
    this.sendFeedback_('chat', false);
  }

  protected sendFeedback_(source: 'canvas'|'chat', liked: boolean) {
    if (source === 'canvas') {
      if (this.canvasFeedbackLiked_ === liked) {
        this.canvasFeedbackLiked_ = null;
        return;
      }
      this.canvasFeedbackLiked_ = liked;
    } else {
      if (this.chatFeedbackLiked_ === liked) {
        this.chatFeedbackLiked_ = null;
        return;
      }
      this.chatFeedbackLiked_ = liked;
    }

    const totalTurns = Math.ceil(this.chatHistory_.length / 2);
    const sourceLabel = source === 'canvas' ? 'Canvas' : 'Chat';
    const userPrompt = this.lastGroupPrompt_ || 'Default';
    const promptHeader = totalTurns > 0 ?
        `[${sourceLabel} - Turn ${totalTurns}/${totalTurns}] ${userPrompt}` :
        `[${sourceLabel}] ${userPrompt}`;

    const exportedData = this.exportGroupDataMarkdown_();

    const params = new URLSearchParams({
      'usp': 'pp_url',
      [ENTRY_USER_PROMPT]: promptHeader,
      [ENTRY_EXPORTED_DATA]: exportedData,
      [ENTRY_LIKED_DISLIKED]: liked ? 'Liked 👍' : 'Disliked 👎',
    });

    if (liked) {
      params.set(ENTRY_GROUPING_DESCRIPTION, 'All good');
      params.set(ENTRY_OVERALL_RATING, '10');
    }

    window.open(`${CANVAS_FEEDBACK_FORM_URL}?${params.toString()}`, '_blank');
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
