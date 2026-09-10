// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_slider/cr_slider.js';
import type {CrSliderElement} from
    '//resources/cr_elements/cr_slider/cr_slider.js';
import '//resources/cr_elements/cr_textarea/cr_textarea.js';
import '//resources/cr_elements/icons.html.js';
import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory, ChatRole} from '../context_hub.mojom-webui.js';
import type {ChatMessage, TabGroup as TabGroupMojom, TabInfo} from '../context_hub.mojom-webui.js';

import {getCss} from './tab_groups.css.js';
import {getHtml} from './tab_groups.html.js';

// Standardized 8 Defect Categories
export const DEFECT_CATEGORIES = [
  'All good',
  'Few tabs left ungrouped',
  'Bad group name',
  'Group(s) too broad / coarse',
  'Group(s) too fragmented / fine',
  'Tabs placed in wrong group',
  'Unnecessary grouping',
  'Completely wrong',
] as const;

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
      feedbackDialogOpen_: {type: Boolean},
      feedbackRating_: {type: Number},
      feedbackDefect_: {type: String},
      feedbackComments_: {type: String},
      feedbackLiked_: {type: Boolean},
      feedbackSource_: {type: String},
      raterName_: {type: String},
      copiedRowSuccess_: {type: Boolean},
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
  protected accessor feedbackDialogOpen_: boolean = false;
  protected accessor feedbackRating_: number|null = null;
  protected accessor feedbackDefect_: string = '';
  protected accessor feedbackComments_: string = '';
  protected accessor feedbackLiked_: boolean = true;
  protected accessor feedbackSource_: 'canvas'|'chat' = 'canvas';
  protected accessor raterName_: string = '';
  protected accessor copiedRowSuccess_: boolean = false;
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
    try {
      this.raterName_ = localStorage.getItem('autotab_eval_rater') || '';
    } catch {
      // Ignore local storage errors.
    }
    this.fetchExistingTabGroupsAndChats_();
    this.fetchConfirmedTabGroupSummaries_();
  }

  protected onRaterNameValueChanged_(e: CustomEvent<{value: string}>) {
    this.raterName_ = e.detail.value;
    try {
      localStorage.setItem('autotab_eval_rater', e.detail.value);
    } catch {
      // Ignore local storage errors.
    }
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

  protected cleanUrl_(rawUrl: string|{url: string}|null|undefined): string {
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

  protected onCanvasThumbsUpClick_() {
    this.openFeedbackDialog_('canvas', true);
  }

  protected onCanvasThumbsDownClick_() {
    this.openFeedbackDialog_('canvas', false);
  }

  protected onChatThumbsUpClick_() {
    this.openFeedbackDialog_('chat', true);
  }

  protected onChatThumbsDownClick_() {
    this.openFeedbackDialog_('chat', false);
  }

  protected canSubmitFeedback_(): boolean {
    return this.feedbackLiked_ ||
        (!!this.feedbackDefect_ && this.feedbackDefect_ !== 'All good');
  }

  protected escapeTsvField_(value: string|number): string {
    const str = String(value ?? '');
    const normalized = str.replace(/\r\n/g, '\n').replace(/\r/g, '\n');
    if (normalized.includes('\t') || normalized.includes('\n') ||
        normalized.includes('"')) {
      return `"${normalized.replace(/"/g, '""')}"`;
    }
    return normalized;
  }

  protected formatTimestamp_(date: Date = new Date()): string {
    const pad = (n: number) => String(n).padStart(2, '0');
    return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${
        pad(date.getDate())} ${pad(date.getHours())}:${
        pad(date.getMinutes())}:${pad(date.getSeconds())}`;
  }

  protected getRatingBucket_(rating: number): string {
    return rating >= 9 ? 'Very Good (9-10)' :
        rating >= 6 ? 'Good (6-8)' :
        rating >= 4 ? 'Bad (4-5)' : 'Very Bad (1-3)';
  }

  protected onRatingCrSliderValueChanged_(e: Event) {
    this.feedbackRating_ = (e.target as CrSliderElement).value;
  }

  protected onDefectChange_(e: Event) {
    const select = e.target as HTMLSelectElement;
    this.feedbackDefect_ = select.value;
  }

  protected onFeedbackCommentsValueChanged_(e: CustomEvent<{value: string}>) {
    this.feedbackComments_ = e.detail.value;
  }

  protected openFeedbackDialog_(source: 'canvas'|'chat', liked: boolean) {
    this.feedbackSource_ = source;
    this.feedbackLiked_ = liked;
    this.feedbackComments_ = '';
    this.copiedRowSuccess_ = false;

    if (liked) {
      this.feedbackRating_ = 10;
      this.feedbackDefect_ = 'All good';
      if (source === 'canvas') {
        this.canvasFeedbackLiked_ = true;
      } else {
        this.chatFeedbackLiked_ = true;
      }
    } else {
      this.feedbackRating_ = 3;
      this.feedbackDefect_ = '';
      if (source === 'canvas') {
        this.canvasFeedbackLiked_ = false;
      } else {
        this.chatFeedbackLiked_ = false;
      }
    }

    this.feedbackDialogOpen_ = true;
  }

  protected onCloseFeedbackDialogClick_() {
    this.feedbackDialogOpen_ = false;
    this.copiedRowSuccess_ = false;
  }

  protected onFeedbackDialogClose_() {
    this.feedbackDialogOpen_ = false;
    this.copiedRowSuccess_ = false;
  }

  protected async onCopyForSheetClick_() {
    if (!this.canSubmitFeedback_()) {
      return;
    }

    const totalTurns = Math.ceil(this.chatHistory_.length / 2);
    const sourceLabel = this.feedbackSource_ === 'canvas' ? 'Canvas' : 'Chat';
    const userPrompt = this.lastGroupPrompt_ || 'Default';
    const rating = this.feedbackRating_ ?? 10;
    const ratingBucket = this.getRatingBucket_(rating);
    const sentiment = this.feedbackLiked_ ? 'Liked' : 'Disliked';
    const defect =
        this.feedbackDefect_ || (sentiment === 'Liked' ? 'All good' : '');
    const comments = (this.feedbackComments_ || '').trim();

    const activeGroups = this.groups_.length > 0 ?
        this.groups_ :
        this.confirmedGroupSummaries_.map(
            g => ({label: g.label, tabs: g.tabs, expanded: false}));

    const groupCount = activeGroups.length;
    const tabCount = activeGroups.reduce((acc, g) => acc + g.tabs.length, 0);

    let lastAssistantMessage = '';
    for (let i = this.chatHistory_.length - 1; i >= 0; i--) {
      const msg = this.chatHistory_[i];
      if (msg && msg.role === ChatRole.kAssistant) {
        lastAssistantMessage = msg.content;
        break;
      }
    }

    const exportedData = JSON.stringify({
      groups: activeGroups.map(g => ({
        label: g.label,
        tabs: g.tabs.map(t => ({title: t.title, url: this.cleanUrl_(t.url)})),
      })),
      ungrouped: this.ungroupedTabs_.map(
          t => ({title: t.title, url: this.cleanUrl_(t.url)})),
    });

    // 14 columns precisely aligned with 'Wave 1 Feedback' Google Sheet:
    // 1: Timestamp, 2: Rater, 3: Source, 4: Turn, 5: Prompt, 6: Rating (1-10),
    // 7: Rating Bucket, 8: Sentiment, 9: Defect Category, 10: Comments,
    // 11: Groups, 12: Tabs, 13: Assistant Response,
    // 14: Exported Tab Data (JSON)
    const columns: Array<string|number> = [
      this.formatTimestamp_(),
      this.raterName_.trim(),
      sourceLabel,
      totalTurns > 0 ? `Turn ${totalTurns}` : 'Turn 1/1',
      userPrompt,
      rating,
      ratingBucket,
      sentiment,
      defect,
      comments,
      groupCount,
      tabCount,
      lastAssistantMessage,
      exportedData,
    ];

    const tsvRow = columns.map(c => this.escapeTsvField_(c)).join('\t');

    try {
      await navigator.clipboard.writeText(tsvRow);
      this.copiedRowSuccess_ = true;
    } catch (err) {
      console.error('Failed to copy TSV row to clipboard:', err);
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
