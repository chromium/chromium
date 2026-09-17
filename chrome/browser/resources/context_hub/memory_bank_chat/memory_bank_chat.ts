// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/icons.html.js';
import '/strings.m.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory, ChatRole} from '../context_hub.mojom-webui.js';
import type {ChatMessage, MemoryBankEntry} from '../context_hub.mojom-webui.js';
import type {SearchSuggestion} from '../memory_banks/memory_banks_search.js';
import {computeSuggestions, hasEnumerableValues, matchesMemoryBankEntry, parseSearchQuery} from '../memory_banks/memory_banks_search.js';

import {getCss} from './memory_bank_chat.css.js';
import {getHtml} from './memory_bank_chat.html.js';

/**
 * Represents a lightweight chip for an attached memory displayed above the chat
 * input.
 */
export interface AttachedContextChip {
  id: bigint;
  title: string;
}

/**
 * Represents a single suggestion item in the '@' mention autocomplete dropdown.
 */
export interface MentionSuggestionItem {
  /**
   * Whether this suggestion completes a bare qualifier (attaching nothing), or
   * attaches the entries matching a filter, or attaches a single memory entry.
   */
  kind: 'qualifier-prefix'|'qualifier-filter'|'individual-entry';

  /** Text shown in the suggestion row (e.g. '@tag:', 'Page Title'). */
  label: string;

  /**
   * Replaces the active '@' mention when this item is chosen; always starts
   * with '@'. Items that complete the mention end with a space, while a bare
   * qualifier does not, so that its value can be typed straight after.
   */
  mentionText: string;

  /** Help text for a qualifier, shown on 'qualifier-prefix' rows. */
  description?: string;

  /** The chips a selection of this item would attach. */
  chips: AttachedContextChip[];
}

/**
 * Matches an active '@' mention at the end of the input string. A mention
 * normally ends at the first space, but a quoted phrase keeps it alive so that
 * multi-word values (e.g. '@title:"Hacker News"') can be typed. The closing
 * quote is optional because the mention is re-matched on every keystroke,
 * while the phrase is still being written.
 */
const MENTION_REGEX = /(^|\s)@([^\s@"]*(?:"[^"]*"?)?)$/;

const MAX_SUGGESTED_ENTRIES = 5;

/** Projects a memory entry down to the fields the chat UI displays. */
function toChip(entry: MemoryBankEntry): AttachedContextChip {
  return {
    id: entry.id,
    title: entry.tabTitle?.trim() || 'Untitled memory',
  };
}

/** Returns the qualifier of a 'qualifier:value' string, or null if absent. */
function qualifierOf(text: string): string|null {
  const colonIndex = text.indexOf(':');
  return colonIndex > 0 ? text.slice(0, colonIndex).toLowerCase().trim() : null;
}

/**
 * Returns whether a mention has entered a qualifier whose values are not drawn
 * from any list, e.g. '@title:'. There is nothing to complete, so its matching
 * entries are offered instead.
 */
function usesFreeTextQualifier(query: string): boolean {
  const qualifier = qualifierOf(query);
  return qualifier !== null && !hasEnumerableValues(qualifier);
}

/**
 * Returns whether `query` is matched against entries directly, rather than
 * against the values a qualifier can be completed with. That covers freeform
 * text, which has no qualifier, and the free-text qualifiers.
 */
function searchesEntries(query: string): boolean {
  return qualifierOf(query) === null || usesFreeTextQualifier(query);
}

/**
 * Balances a phrase whose closing quote has not been typed yet. The search
 * tokenizer drops an unterminated quote and splits the phrase into separate
 * terms, which would leave '@title:"Hacker News' matching nothing until the
 * final quote arrives.
 */
function closeDanglingQuote(query: string): string {
  const quoteCount = (query.match(/"/g) || []).length;
  return quoteCount % 2 === 0 ? query : `${query}"`;
}

export class MemoryBankChatElement extends CrLitElement {
  static get is() {
    return 'memory-bank-chat';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      attachedChips_: {type: Object, state: true},
      availableEntries_: {type: Array, state: true},
      chatHistory_: {type: Array, state: true},
      highlightedMentionIndex_: {type: Number, state: true},
      inputValue_: {type: String, state: true},
      isLoading_: {type: Boolean, state: true},
      mentionQuery_: {type: String, state: true},
      mentionSuggestions_: {type: Array, state: true},
      showMentionMenu_: {type: Boolean, state: true},
    };
  }

  protected accessor attachedChips_: Map<bigint, AttachedContextChip> =
      new Map();
  protected accessor chatHistory_: ChatMessage[] = [];
  protected accessor highlightedMentionIndex_: number = 0;
  protected accessor inputValue_: string = '';
  protected accessor isLoading_: boolean = false;
  protected accessor mentionSuggestions_: MentionSuggestionItem[] = [];
  protected accessor showMentionMenu_: boolean = false;
  private accessor availableEntries_: MemoryBankEntry[] = [];
  private accessor mentionQuery_: string = '';
  private maxChatHistoryTurns_: number =
      loadTimeData.getInteger('kMaxMemoryBankChatHistoryTurns');

  // Derived from availableEntries_; cached because autocomplete reads them on
  // every keystroke.
  private allTags_: string[] = [];
  private allCollections_: string[] = [];

  override connectedCallback() {
    super.connectedCallback();
    this.fetchChatHistory_();
    this.fetchAvailableEntries_();
    document.addEventListener('pointerdown', this.onDocumentPointerDown_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener('pointerdown', this.onDocumentPointerDown_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    // These depend only on the entry list, not on the query.
    if (changedPrivateProperties.has('availableEntries_')) {
      this.allTags_ = [
        ...new Set(this.availableEntries_.flatMap(e => e.tags || [])),
      ].sort();
      this.allCollections_ = [
        ...new Set(this.availableEntries_.map(e => e.collection)
                       .filter((c): c is string => !!c)),
      ].sort();
    }

    if (changedPrivateProperties.has('mentionQuery_') ||
        changedPrivateProperties.has('availableEntries_') ||
        changedPrivateProperties.has('attachedChips_') ||
        changedPrivateProperties.has('showMentionMenu_')) {
      this.mentionSuggestions_ =
          this.showMentionMenu_ ? this.computeMentionSuggestions_() : [];
      // The list can shrink without the query changing, e.g. on attach.
      this.highlightedMentionIndex_ = Math.min(
          this.highlightedMentionIndex_,
          Math.max(0, this.mentionSuggestions_.length - 1));
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('highlightedMentionIndex_') &&
        this.showMentionMenu_) {
      const highlightedEl = this.shadowRoot?.querySelector<HTMLElement>(
          '.mention-item.highlighted');
      highlightedEl?.scrollIntoView({block: 'nearest'});
    }
  }

  private trimChatHistory_(history: ChatMessage[]): ChatMessage[] {
    if (this.maxChatHistoryTurns_ > 0 &&
        history.length > this.maxChatHistoryTurns_) {
      return history.slice(-this.maxChatHistoryTurns_);
    }
    return history;
  }

  private appendChatTurn_(role: ChatRole, content: string) {
    this.chatHistory_ = this.trimChatHistory_([
      ...this.chatHistory_,
      {role, content},
    ]);
    this.scrollToBottom_();
  }

  private async fetchChatHistory_() {
    try {
      const {history} = await browserProxyFactory.getInstance()
                            .handler.getMemoryBankChatHistory();
      if (history && history.length > 0) {
        this.chatHistory_ = this.trimChatHistory_(history);
        this.scrollToBottom_();
      }
    } catch (e) {
      console.error('Failed to load memory bank chat history:', e);
    }
  }

  // TODO(crbug.com/545323764): Improve fetching logic to avoid loading all
  // entries at once.
  private async fetchAvailableEntries_() {
    try {
      const {entries} = await browserProxyFactory.getInstance()
                            .handler.getAllMemoryBankEntries();
      this.availableEntries_ = entries || [];
    } catch (e) {
      console.error('Failed to load available memory bank entries:', e);
    }
  }

  protected onRemoveAttachedChipClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const id = target.dataset['id'];
    if (!id) {
      return;
    }
    const updated = new Map(this.attachedChips_);
    updated.delete(BigInt(id));
    this.attachedChips_ = updated;
  }

  protected onClearAttachedChipsClick_() {
    this.attachedChips_ = new Map();
  }

  protected getMentionHeader_(): string {
    const query = this.mentionQuery_.trim();
    return query ? `Suggestions for "${query}"` : 'Search qualifiers';
  }

  protected getEntryCountLabel_(count: number): string {
    return `${count} ${count === 1 ? 'entry' : 'entries'}`;
  }

  private computeMentionSuggestions_(): MentionSuggestionItem[] {
    const rawQuery = this.mentionQuery_.trim();
    return [
      ...this.getQualifierSuggestions_(rawQuery),
      ...this.getMemoryBankEntrySuggestions_(rawQuery),
    ];
  }

  private getQualifierSuggestions_(query: string): MentionSuggestionItem[] {
    // computeSuggestions() answers a typed free-text qualifier with a format
    // reminder, which the search box needs but the menu does not: it lists the
    // entries that qualifier would search instead.
    if (usesFreeTextQualifier(query)) {
      return [];
    }

    const suggestions =
        computeSuggestions(query, this.allTags_, this.allCollections_);
    const items: MentionSuggestionItem[] = [];
    for (const suggestion of suggestions) {
      // computeSuggestions() emits both bare prefixes (e.g. 'tag:') and
      // concrete values (e.g. 'tag:recipes'); only the latter can match
      // entries.
      const item = suggestion.label.endsWith(':') ?
          this.createQualifierPrefixItem_(suggestion) :
          this.createQualifierFilterItem_(suggestion);
      if (item) {
        items.push(item);
      }
    }
    return items;
  }

  /**
   * Returns chips for the entries matching `query` that are not attached yet,
   * so that suggestions only ever offer (and count) memories a selection would
   * add. The query is matched as typed, mid-phrase included.
   */
  private findUnattachedChips_(query: string, limit?: number):
      AttachedContextChip[] {
    const parsed = parseSearchQuery(closeDanglingQuote(query));
    const matches = this.availableEntries_.filter(
        entry => !this.attachedChips_.has(entry.id) &&
            matchesMemoryBankEntry(entry, parsed));
    const limited = limit === undefined ? matches : matches.slice(0, limit);
    return limited.map(toChip);
  }

  /** Builds a row that completes a bare qualifier, e.g. '@tag:'. */
  private createQualifierPrefixItem_(suggestion: SearchSuggestion):
      MentionSuggestionItem {
    return {
      kind: 'qualifier-prefix',
      label: `@${suggestion.label}`,
      mentionText: `@${suggestion.query}`,
      description: suggestion.description,
      chips: [],
    };
  }

  /**
   * Builds a row that attaches every unattached entry matching a concrete
   * filter, e.g. '@tag:recipes'. Returns null when nothing matches, so that
   * filters that would attach nothing are not offered.
   */
  private createQualifierFilterItem_(suggestion: SearchSuggestion):
      MentionSuggestionItem|null {
    const chips = this.findUnattachedChips_(suggestion.query);
    if (chips.length === 0) {
      return null;
    }

    return {
      kind: 'qualifier-filter',
      label: `@${suggestion.label}`,
      mentionText: `@${suggestion.query}`,
      chips,
    };
  }

  /**
   * Suggests individual entries matching `query`, which is either freeform
   * text or a free-text filter. A freeform term matches any field of an entry,
   * while a qualifier such as 'title:' narrows it to that field.
   */
  private getMemoryBankEntrySuggestions_(query: string):
      MentionSuggestionItem[] {
    if (query.length === 0 || !searchesEntries(query)) {
      return [];
    }

    // A qualifier with no value yet filters nothing, so offer the entries it
    // would search. Passing it through as-is would instead look for the
    // literal text 'title:'.
    const filter = query.endsWith(':') ? '' : query;
    return this.findUnattachedChips_(filter, MAX_SUGGESTED_ENTRIES)
        .map(chip => ({
               kind: 'individual-entry',
               label: chip.title,
               mentionText: `@${chip.title} `,
               chips: [chip],
             }));
  }

  /** Replaces the '@' mention being typed at the end of the input. */
  private replaceActiveMention_(replacement: string) {
    const match = this.inputValue_.match(MENTION_REGEX);
    if (!match || match.index === undefined) {
      return;
    }
    // match[1] is the whitespace before the '@', which must be preserved.
    const mentionStart = match.index + (match[1]?.length ?? 0);
    this.inputValue_ =
        `${this.inputValue_.substring(0, mentionStart)}${replacement}`;
  }

  private focusChatInput_() {
    this.shadowRoot?.querySelector<CrInputElement>('#chat-input')?.focusInput();
  }

  /** Attaches the given context chips, ignoring duplicates. */
  private attachChips_(chips: AttachedContextChip[]) {
    const updated = new Map(this.attachedChips_);
    for (const chip of chips) {
      updated.set(chip.id, chip);
    }
    this.attachedChips_ = updated;
  }

  protected selectMentionSuggestion_(item: MentionSuggestionItem) {
    this.replaceActiveMention_(item.mentionText);

    // A bare qualifier completes the prefix only and keeps the menu open so a
    // value can be picked; anything else attaches its chips and closes it.
    const isPrefix = item.kind === 'qualifier-prefix';
    if (isPrefix) {
      this.mentionQuery_ = item.mentionText.slice(1);
    } else {
      this.attachChips_(item.chips);
    }

    this.showMentionMenu_ = isPrefix;
    this.highlightedMentionIndex_ = 0;
    this.focusChatInput_();
  }

  /** Returns the suggestion index carried by a mention row's dataset. */
  private getMentionItemIndex_(e: Event): number {
    const target = e.currentTarget as HTMLElement;
    return Number(target.dataset['index']);
  }

  protected onMentionItemMouseenter_(e: MouseEvent) {
    const index = this.getMentionItemIndex_(e);
    if (!isNaN(index)) {
      this.highlightedMentionIndex_ = index;
    }
  }

  protected onMentionItemClick_(e: MouseEvent) {
    const item = this.mentionSuggestions_[this.getMentionItemIndex_(e)];
    if (item) {
      this.selectMentionSuggestion_(item);
    }
  }

  private onDocumentPointerDown_ = (e: PointerEvent) => {
    const path = e.composedPath();
    const mentionMenu = this.shadowRoot?.querySelector('#mention-menu');
    const chatInput = this.shadowRoot?.querySelector('#chat-input');

    if (this.showMentionMenu_ && mentionMenu && !path.includes(mentionMenu) &&
        chatInput && !path.includes(chatInput)) {
      this.showMentionMenu_ = false;
    }
  };

  protected onInputValueChanged_(e: CustomEvent<{value: string}>) {
    this.inputValue_ = e.detail.value;
    const match = this.inputValue_.match(MENTION_REGEX);
    this.showMentionMenu_ = match !== null;
    if (match) {
      // Group 2 is the partial query typed after the '@'.
      this.mentionQuery_ = match[2] ?? '';
      this.highlightedMentionIndex_ = 0;
    } else {
      this.mentionQuery_ = '';
    }
  }

  protected onInputKeydown_(e: KeyboardEvent) {
    if (this.showMentionMenu_ && this.handleMentionMenuKeydown_(e)) {
      return;
    }

    if (e.key === 'Enter' && this.inputValue_.trim().length > 0) {
      this.onSendMessageClick_();
    }
  }

  /**
   * Handles navigation keys while the mention menu is open. Returns whether
   * the key was consumed by the menu; if not, it falls through to the input
   * (e.g. Enter with nothing to select still sends the message).
   */
  private handleMentionMenuKeydown_(e: KeyboardEvent): boolean {
    const suggestions = this.mentionSuggestions_;
    switch (e.key) {
      case 'ArrowDown':
      case 'ArrowUp':
        if (suggestions.length === 0) {
          return false;
        }
        e.preventDefault();
        this.highlightedMentionIndex_ =
            (this.highlightedMentionIndex_ + (e.key === 'ArrowDown' ? 1 : -1) +
             suggestions.length) %
            suggestions.length;
        return true;
      case 'Enter': {
        const selected = suggestions[this.highlightedMentionIndex_];
        if (!selected) {
          // Nothing to complete: fall through so Enter sends the message,
          // which closes the menu.
          return false;
        }
        e.preventDefault();
        this.selectMentionSuggestion_(selected);
        return true;
      }
      case 'Escape':
        e.preventDefault();
        this.showMentionMenu_ = false;
        return true;
      default:
        return false;
    }
  }

  protected async onSendMessageClick_() {
    if (this.isLoading_) {
      return;
    }

    const command = this.inputValue_.trim();
    if (!command) {
      return;
    }

    const attachedEntryIds = Array.from(this.attachedChips_.keys());
    this.isLoading_ = true;
    this.inputValue_ = '';
    this.showMentionMenu_ = false;
    this.appendChatTurn_(ChatRole.kUser, command);

    try {
      const {response} =
          await browserProxyFactory.getInstance().handler.askGeminiWithContext(
              command, attachedEntryIds);

      const assistantContent =
          response?.content.trim() || 'No response from Gemini.';
      this.appendChatTurn_(ChatRole.kAssistant, assistantContent);
    } catch (e) {
      console.error('Failed to process request with Gemini:', e);
      this.appendChatTurn_(
          ChatRole.kAssistant, 'Failed to process request with Gemini.');
      if (!this.inputValue_) {
        this.inputValue_ = command;
      }
    } finally {
      this.isLoading_ = false;
    }
  }

  protected async onClearChatHistoryClick_() {
    if (this.isLoading_) {
      return;
    }
    await browserProxyFactory.getInstance()
        .handler.clearMemoryBankChatHistory();
    this.chatHistory_ = [];
    this.attachedChips_ = new Map();
    this.inputValue_ = '';
  }

  private async scrollToBottom_() {
    await this.updateComplete;
    const chatMessages =
        this.shadowRoot?.querySelector<HTMLElement>('#chat-messages');
    if (chatMessages) {
      chatMessages.scrollTop = chatMessages.scrollHeight;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'memory-bank-chat': MemoryBankChatElement;
  }
}

customElements.define(MemoryBankChatElement.is, MemoryBankChatElement);
