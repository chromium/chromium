// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_textarea/cr_textarea.js';
import '//resources/cr_elements/icons.html.js';
import '/strings.m.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
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

// Shown when generating tab group suggestions fails.
export const GROUPING_FAILED_MESSAGE = 'Couldn\'t group your tabs. Try again.';
export const NOT_ENOUGH_TABS_MESSAGE =
    'Open at least 2 tabs to get suggestions.';
// Shown when creating/confirming the proposed tab groups fails.
export const CREATE_GROUPS_FAILED_MESSAGE = 'Couldn\'t create groups.';

interface TabGroup {
  label: string;
  tabs: TabInfo[];
}

// The suggestion state restored by Undo.
interface SuggestionSnapshot {
  groups: TabGroup[];
  ungroupedTabs: TabInfo[];
  responseText: string;
}

// Index of the "Not grouped" list when used as a move source or target.
export const UNGROUPED_INDEX = -1;

// What is currently being dragged, or which tab the "Move to" menu is for.
type MoveSource = {
  kind: 'tab',
  groupIndex: number,
  tabId: bigint,
}|{
  kind: 'group',
  groupIndex: number,
};

export interface TabGroupsElement {
  $: {
    moveMenu: CrActionMenuElement,
  };
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
      groups_: {type: Array},
      ungroupedTabs_: {type: Array},
      confirmedGroupSummaries_: {type: Array},
      tabs_: {type: Array},
      expandedConfirmedGroups_: {type: Object},
      isGrouped_: {type: Boolean},
      isGrouping_: {type: Boolean},
      isRegrouping_: {type: Boolean},
      inputValue_: {type: String},
      statusMessage_: {type: String},
      responseText_: {type: String},
      canUndo_: {type: Boolean},
      editingGroupIndex_: {type: Number},
      editingLabel_: {type: String},
      dropTargetIndex_: {type: Number},
      moveMenuSource_: {type: Object},
      ungroupedExpanded_: {type: Boolean},
      confirmedExpanded_: {type: Boolean},
      canvasFeedbackLiked_: {type: Boolean},
      feedbackDialogOpen_: {type: Boolean},
      feedbackRating_: {type: Number},
      feedbackDefect_: {type: String},
      feedbackComments_: {type: String},
      feedbackLiked_: {type: Boolean},
      raterName_: {type: String},
      copiedRowSuccess_: {type: Boolean},
    };
  }

  protected accessor groups_: TabGroup[] = [];
  protected accessor ungroupedTabs_: TabInfo[] = [];
  protected chatHistory_: ChatMessage[] = [];
  protected accessor confirmedGroupSummaries_: TabGroupMojom[] = [];
  // Open ungrouped tabs, listed on the landing card.
  protected accessor tabs_: TabInfo[] = [];
  protected accessor expandedConfirmedGroups_: Set<string> = new Set();
  protected accessor isGrouped_: boolean = false;
  protected accessor isGrouping_: boolean = false;
  protected accessor isRegrouping_: boolean = false;
  protected autoTabGroupsEnabled_: boolean =
      loadTimeData.getBoolean('kAutoTabGroups');
  protected maxTabGroupChatHistoryTurns_: number = this.autoTabGroupsEnabled_ ?
      loadTimeData.getInteger('kMaxTabGroupChatHistoryTurns') :
      10;
  protected accessor inputValue_: string = '';
  protected accessor statusMessage_: string = '';
  // The model's note about the current suggestions, if any.
  protected accessor responseText_: string = '';
  protected accessor canUndo_: boolean = false;
  protected accessor editingGroupIndex_: number = -1;
  protected accessor editingLabel_: string = '';
  protected accessor dropTargetIndex_: number|null = null;
  protected accessor moveMenuSource_: MoveSource|null = null;
  protected accessor ungroupedExpanded_: boolean = false;
  protected accessor confirmedExpanded_: boolean = false;
  protected accessor canvasFeedbackLiked_: boolean|null = null;
  protected accessor feedbackDialogOpen_: boolean = false;
  protected accessor feedbackRating_: number|null = null;
  protected accessor feedbackDefect_: string = '';
  protected accessor feedbackComments_: string = '';
  protected accessor feedbackLiked_: boolean = true;
  protected accessor raterName_: string = '';
  protected accessor copiedRowSuccess_: boolean = false;
  protected lastGroupPrompt_: string = 'Default';
  private undoStack_: SuggestionSnapshot[] = [];
  private dragSource_: MoveSource|null = null;

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

  // Refreshes the confirmed groups and the open ungrouped tabs shown on the
  // landing card. Both change whenever groups are created or ungrouped.
  private async fetchConfirmedTabGroupSummaries_() {
    if (!this.autoTabGroupsEnabled_) {
      return;
    }
    try {
      const handler = browserProxyFactory.getInstance().handler;
      const [{groups}, {tabs}] = await Promise.all(
          [handler.getConfirmedTabGroups(), handler.getTabs()]);
      this.confirmedGroupSummaries_ = groups;
      this.tabs_ = tabs;
    } catch (e) {
      console.error('Failed to fetch confirmed tab groups:', e);
    }
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
                                  }));
        this.ungroupedTabs_ = ungroupedTabs;
        this.isGrouped_ = true;
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
      }
    } catch (e) {
      console.error('Failed to fetch existing tab groups:', e);
    }
  }

  // Returns the view to the landing state.
  private resetSuggestions_() {
    this.groups_ = [];
    this.ungroupedTabs_ = [];
    this.isGrouped_ = false;
    this.statusMessage_ = '';
    this.responseText_ = '';
    this.editingGroupIndex_ = -1;
    this.canvasFeedbackLiked_ = null;
  }

  protected hasPendingPrompt_(): boolean {
    return this.inputValue_.trim().length > 0;
  }

  protected isBusy_(): boolean {
    return !this.autoTabGroupsEnabled_ || this.isGrouping_;
  }

  private pushUndoSnapshot_() {
    this.undoStack_.push({
      groups: this.groups_,
      ungroupedTabs: this.ungroupedTabs_,
      responseText: this.responseText_,
    });
    this.canUndo_ = true;
  }

  private popUndoSnapshot_(): SuggestionSnapshot|undefined {
    const snapshot = this.undoStack_.pop();
    this.canUndo_ = this.undoStack_.length > 0;
    return snapshot;
  }

  private clearUndoStack_() {
    this.undoStack_ = [];
    this.canUndo_ = false;
  }

  // Shows `groups` / `ungroupedTabs` and stores them in the browser so that
  // "Create groups" applies what is on screen.
  private async applySuggestions_(
      groups: TabGroup[], ungroupedTabs: TabInfo[]) {
    this.groups_ = groups;
    this.ungroupedTabs_ = ungroupedTabs;
    await browserProxyFactory.getInstance().handler.replaceTabGroups(
        groups.map(g => ({label: g.label, tabs: g.tabs, savedGuid: null})));
  }

  // Sends a grouping request: the default request from the landing card, or
  // the typed prompt as a follow-up to the current suggestions.
  protected async onGroupTabsClick_() {
    if (!this.autoTabGroupsEnabled_ || this.isGrouping_) {
      return;
    }

    const command = this.inputValue_.trim();
    const hadSuggestions = this.isGrouped_;
    if (hadSuggestions) {
      this.pushUndoSnapshot_();
    } else {
      this.clearUndoStack_();
    }
    this.isRegrouping_ = hadSuggestions;
    this.isGrouping_ = true;
    this.statusMessage_ = '';
    this.editingGroupIndex_ = -1;
    this.lastGroupPrompt_ = command || 'Default';
    this.inputValue_ = '';
    this.canvasFeedbackLiked_ = null;

    if (command) {
      this.chatHistory_ = this.trimChatHistory_([
        ...this.chatHistory_,
        {role: ChatRole.kUser, content: command},
      ]);
    }

    try {
      const {groups, ungroupedTabs, llmResponse} =
          await browserProxyFactory.getInstance().handler.retrieveAndGroupTabs(
              command);

      const suggestions = groups
                              .map(group => ({
                                     label: group.label,
                                     tabs: group.tabs,
                                   }))
                              .filter(group => group.tabs.length > 0);
      const responseText = llmResponse?.content.trim() || '';

      this.chatHistory_ = this.trimChatHistory_([
        ...this.chatHistory_,
        {
          role: ChatRole.kAssistant,
          content: responseText ||
              (suggestions.length > 0 ?
                   `Grouped tabs into ${suggestions.length} group(s).` :
                   'Failed to group tabs.'),
        },
      ]);

      if (suggestions.length > 0) {
        this.groups_ = suggestions;
        this.ungroupedTabs_ = ungroupedTabs;
        this.responseText_ = responseText;
        this.isGrouped_ = true;
      } else if (hadSuggestions) {
        // Keep the previous suggestions on screen.
        this.popUndoSnapshot_();
        this.statusMessage_ = responseText || GROUPING_FAILED_MESSAGE;
      } else if (responseText) {
        this.statusMessage_ = responseText;
      } else {
        this.statusMessage_ = ungroupedTabs.length < 2 ?
            NOT_ENOUGH_TABS_MESSAGE :
            GROUPING_FAILED_MESSAGE;
      }
    } catch (e) {
      console.error('Failed to retrieve and group tabs:', e);
      this.chatHistory_ = this.trimChatHistory_([
        ...this.chatHistory_,
        {role: ChatRole.kAssistant, content: 'Failed to group tabs.'},
      ]);
      if (hadSuggestions) {
        this.popUndoSnapshot_();
      }
      this.statusMessage_ = GROUPING_FAILED_MESSAGE;
    } finally {
      this.isGrouping_ = false;
    }
  }

  protected async onConfirmAllGroupsClick_() {
    if (!this.autoTabGroupsEnabled_ || this.isGrouping_) {
      return;
    }
    try {
      const {success} =
          await browserProxyFactory.getInstance().handler.confirmAllTabGroups();
      if (success) {
        this.resetSuggestions_();
        this.clearUndoStack_();
        await this.fetchConfirmedTabGroupSummaries_();
      } else {
        this.statusMessage_ = CREATE_GROUPS_FAILED_MESSAGE;
      }
    } catch (e) {
      console.error('Failed to confirm tab groups:', e);
      this.statusMessage_ = CREATE_GROUPS_FAILED_MESSAGE;
    }
  }

  protected onPrimaryActionClick_() {
    if (this.hasPendingPrompt_()) {
      this.onGroupTabsClick_();
    } else {
      this.onConfirmAllGroupsClick_();
    }
  }

  // Discards the suggestions and the chat context. Confirmed groups are not
  // affected.
  protected async onClearClick_() {
    if (!this.autoTabGroupsEnabled_ || this.isGrouping_) {
      return;
    }
    const handler = browserProxyFactory.getInstance().handler;
    await handler.clearTabGroups();
    await handler.clearTabGroupChatHistory();
    this.chatHistory_ = [];
    this.inputValue_ = '';
    this.clearUndoStack_();
    this.resetSuggestions_();
  }

  protected async onUndoClick_() {
    if (this.isGrouping_) {
      return;
    }
    const snapshot = this.popUndoSnapshot_();
    if (!snapshot) {
      return;
    }
    this.editingGroupIndex_ = -1;
    this.responseText_ = snapshot.responseText;
    await this.applySuggestions_(snapshot.groups, snapshot.ungroupedTabs);
  }

  protected async onEditGroupClick_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const group = this.groups_[index];
    if (!group || this.isGrouping_) {
      return;
    }
    this.editingLabel_ = group.label;
    this.editingGroupIndex_ = index;
    await this.updateComplete;
    this.shadowRoot.querySelector<CrInputElement>('#rename-input')?.focus();
  }

  protected onRenameValueChanged_(e: CustomEvent<{value: string}>) {
    this.editingLabel_ = e.detail.value;
  }

  protected onRenameKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      e.preventDefault();
      this.commitRename_();
    } else if (e.key === 'Escape') {
      this.editingGroupIndex_ = -1;
    }
  }

  protected onRenameBlur_() {
    this.commitRename_();
  }

  private async commitRename_() {
    const index = this.editingGroupIndex_;
    const label = this.editingLabel_.trim();
    this.editingGroupIndex_ = -1;
    const group = this.groups_[index];
    if (!group || !label || label === group.label) {
      return;
    }
    this.pushUndoSnapshot_();
    await this.applySuggestions_(
        this.groups_.map((g, i) => i === index ? {...g, label} : g),
        this.ungroupedTabs_);
  }

  protected async onDismissGroupClick_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const group = this.groups_[index];
    if (!group || this.isGrouping_) {
      return;
    }
    this.editingGroupIndex_ = -1;
    this.pushUndoSnapshot_();
    await this.applySuggestions_(
        this.groups_.filter((_, i) => i !== index),
        [...this.ungroupedTabs_, ...group.tabs]);
  }

  // Moves `tabId` from group `fromGroup` to group `toGroup`, where
  // UNGROUPED_INDEX stands for the "Not grouped" list. A group left without
  // tabs is removed.
  private async moveTab_(tabId: bigint, fromGroup: number, toGroup: number) {
    if (fromGroup === toGroup) {
      return;
    }
    const source = fromGroup === UNGROUPED_INDEX ?
        this.ungroupedTabs_ :
        this.groups_[fromGroup]?.tabs;
    const tab = source?.find(t => t.id === tabId);
    if (!tab || (toGroup !== UNGROUPED_INDEX && !this.groups_[toGroup])) {
      return;
    }
    this.pushUndoSnapshot_();
    const groups = this.groups_.map((g, i) => {
      if (i === fromGroup) {
        return {...g, tabs: g.tabs.filter(t => t.id !== tabId)};
      }
      if (i === toGroup) {
        return {...g, tabs: [...g.tabs, tab]};
      }
      return g;
    });
    let ungrouped = this.ungroupedTabs_.filter(t => t.id !== tabId);
    if (toGroup === UNGROUPED_INDEX) {
      ungrouped = [...ungrouped, tab];
    }
    await this.applySuggestions_(
        groups.filter(g => g.tabs.length > 0), ungrouped);
  }

  private async moveGroup_(fromGroup: number, toGroup: number) {
    if (fromGroup === toGroup || !this.groups_[fromGroup] ||
        !this.groups_[toGroup]) {
      return;
    }
    this.pushUndoSnapshot_();
    const groups = [...this.groups_];
    const [moved] = groups.splice(fromGroup, 1);
    groups.splice(toGroup, 0, moved!);
    await this.applySuggestions_(groups, this.ungroupedTabs_);
  }

  private getGroupIndex_(e: Event): number {
    return Number((e.currentTarget as HTMLElement).dataset['group']);
  }

  protected onTabDragstart_(e: DragEvent) {
    const target = e.currentTarget as HTMLElement;
    this.dragSource_ = {
      kind: 'tab',
      groupIndex: this.getGroupIndex_(e),
      tabId: BigInt(target.dataset['id'] || '0'),
    };
    e.dataTransfer!.effectAllowed = 'move';
  }

  protected onGroupDragstart_(e: DragEvent) {
    this.dragSource_ = {kind: 'group', groupIndex: this.getGroupIndex_(e)};
    e.dataTransfer!.effectAllowed = 'move';
  }

  protected onDragend_() {
    this.dragSource_ = null;
    this.dropTargetIndex_ = null;
  }

  protected onCardDragover_(e: DragEvent) {
    if (!this.dragSource_) {
      return;
    }
    const target = this.getGroupIndex_(e);
    if (this.dragSource_.kind === 'group' && target === UNGROUPED_INDEX) {
      return;
    }
    e.preventDefault();
    e.dataTransfer!.dropEffect = 'move';
    this.dropTargetIndex_ = target;
  }

  protected onCardDragleave_(e: DragEvent) {
    const card = e.currentTarget as HTMLElement;
    if (e.relatedTarget instanceof Node && card.contains(e.relatedTarget)) {
      return;
    }
    this.dropTargetIndex_ = null;
  }

  protected onCardDrop_(e: DragEvent) {
    e.preventDefault();
    const source = this.dragSource_;
    const target = this.getGroupIndex_(e);
    this.onDragend_();
    if (!source) {
      return;
    }
    if (source.kind === 'tab') {
      this.moveTab_(source.tabId, source.groupIndex, target);
    } else {
      this.moveGroup_(source.groupIndex, target);
    }
  }

  // Keyboard-accessible equivalent of dragging a tab.
  protected async onMoveTabClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    this.moveMenuSource_ = {
      kind: 'tab',
      groupIndex: this.getGroupIndex_(e),
      tabId: BigInt(target.dataset['id'] || '0'),
    };
    if (this.getMoveTargets_().length === 0) {
      return;
    }
    await this.updateComplete;
    this.$.moveMenu.showAt(target);
  }

  // Destinations offered by the "Move to" menu: every other group, then
  // "Not grouped".
  protected getMoveTargets_(): Array<{index: number, label: string}> {
    const from = this.moveMenuSource_?.groupIndex;
    const targets = this.groups_.map((g, index) => ({index, label: g.label}))
                        .filter(t => t.index !== from);
    if (from !== UNGROUPED_INDEX) {
      targets.push({index: UNGROUPED_INDEX, label: 'Not grouped'});
    }
    return targets;
  }

  protected onMoveToClick_(e: Event) {
    const source = this.moveMenuSource_;
    this.$.moveMenu.close();
    if (source?.kind === 'tab') {
      this.moveTab_(source.tabId, source.groupIndex, this.getGroupIndex_(e));
    }
  }

  // Fires however the menu closes (destination chosen, Escape, click
  // outside), so the source never outlives the menu.
  protected onMoveMenuClose_() {
    this.moveMenuSource_ = null;
  }

  protected onTabClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const tabId = BigInt(target.dataset['id'] || '0');
    if (tabId !== 0n) {
      browserProxyFactory.getInstance().handler.switchToTab(tabId);
    }
  }

  protected onInputValueChanged_(e: CustomEvent<{value: string}>) {
    this.inputValue_ = e.detail.value;
  }

  protected onInputKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter' && !e.shiftKey && this.hasPendingPrompt_()) {
      e.preventDefault();
      this.onGroupTabsClick_();
    }
  }

  protected onUngroupedExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.ungroupedExpanded_ = e.detail.value;
  }

  protected onConfirmedExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.confirmedExpanded_ = e.detail.value;
  }

  protected isConfirmedGroupExpanded_(group: TabGroupMojom): boolean {
    return !!group.savedGuid &&
        this.expandedConfirmedGroups_.has(group.savedGuid.value);
  }

  protected onGroupExpandedChanged_(e: CustomEvent<{value: boolean}>) {
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
  }

  protected getCanvasThumbsUpIcon_(): string {
    return this.canvasFeedbackLiked_ === true ? 'cr:thumb-up-filled' :
                                                'cr:thumb-up';
  }

  protected getCanvasThumbsDownIcon_(): string {
    return this.canvasFeedbackLiked_ === false ? 'cr:thumb-down-filled' :
                                                 'cr:thumb-down';
  }

  protected cleanUrl_(urlStr: string): string {
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
    this.openFeedbackDialog_(true);
  }

  protected onCanvasThumbsDownClick_() {
    this.openFeedbackDialog_(false);
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
        rating >= 7    ? 'Good (7-8)' :
        rating >= 4    ? 'Bad (4-6)' :
                         'Very Bad (1-3)';
  }

  protected getRatingTierClass_(rating: number): string {
    return rating >= 9 ? 'tier-very-good' :
        rating >= 7    ? 'tier-good' :
        rating >= 4    ? 'tier-bad' :
                         'tier-very-bad';
  }

  protected getRatingTierLabel_(rating: number): string {
    return rating >= 9 ? 'Very Good' :
        rating >= 7    ? 'Good' :
        rating >= 4    ? 'Bad' :
                         'Very Bad';
  }

  protected onRatingChipClick_(e: Event) {
    const btn = e.currentTarget as HTMLElement;
    this.feedbackRating_ = Number(btn.dataset['score']);
  }

  protected onDefectChange_(e: Event) {
    const select = e.target as HTMLSelectElement;
    this.feedbackDefect_ = select.value;
  }

  protected onFeedbackCommentsValueChanged_(e: CustomEvent<{value: string}>) {
    this.feedbackComments_ = e.detail.value;
  }

  protected openFeedbackDialog_(liked: boolean) {
    this.feedbackLiked_ = liked;
    this.feedbackComments_ = '';
    this.copiedRowSuccess_ = false;
    this.canvasFeedbackLiked_ = liked;

    if (liked) {
      this.feedbackRating_ = 10;
      this.feedbackDefect_ = 'All good';
    } else {
      this.feedbackRating_ = 3;
      this.feedbackDefect_ = '';
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
    const userPrompt = this.lastGroupPrompt_ || 'Default';
    const rating = this.feedbackRating_ ?? 10;
    const ratingBucket = this.getRatingBucket_(rating);
    const sentiment = this.feedbackLiked_ ? 'Liked' : 'Disliked';
    const defect =
        this.feedbackDefect_ || (sentiment === 'Liked' ? 'All good' : '');
    const comments = (this.feedbackComments_ || '').trim();

    const activeGroups: TabGroup[] = this.groups_.length > 0 ?
        this.groups_ :
        this.confirmedGroupSummaries_.map(
            g => ({label: g.label, tabs: g.tabs}));

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
      groups: activeGroups.map(
          g => ({
            label: g.label,
            tabs:
                g.tabs.map(t => ({title: t.title, url: this.cleanUrl_(t.url)})),
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
      'Canvas',
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
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-groups': TabGroupsElement;
  }
}

customElements.define(TabGroupsElement.is, TabGroupsElement);
