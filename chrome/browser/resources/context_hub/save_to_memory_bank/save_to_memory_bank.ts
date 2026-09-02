// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_textarea/cr_textarea.js';
import '//resources/cr_elements/icons.html.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory} from '../context_hub.mojom-webui.js';

import {getCss} from './save_to_memory_bank.css.js';
import {getHtml} from './save_to_memory_bank.html.js';

export class SaveToMemoryBankElement extends CrLitElement {
  static get is() {
    return 'save-to-memory-bank';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      title: {type: String},
      url: {type: String},
      snippet: {type: String},
      note: {type: String},
      collection: {type: String},
      collections: {type: Array},
      tags: {type: Array},
      availableTags: {type: Array},
      filteredTags_: {type: Array},
      isAddingCollection: {type: Boolean},
      newCollectionInput: {type: String},
      isCreatingCustomTag: {type: Boolean},
      newTagInput: {type: String},
      highlightedTagIndex: {type: Number},
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.fetchInitialData_();
  }

  private async fetchInitialData_() {
    const proxy = browserProxyFactory.getInstance().handler;

    const [{context}, {collections}, {tags}] = await Promise.all([
      proxy.getSaveToMemoryBankContext(),
      proxy.getAllMemoryBankCollections(),
      proxy.getAllMemoryBankTags(),
    ]);

    if (context) {
      this.title = context.tabTitle || '';
      this.url = context.url || '';
      this.snippet = context.selectedText || '';
    }

    this.collections = collections || [];
    this.availableTags = tags || [];
  }

  override accessor title: string = '';
  accessor url: string = '';
  accessor snippet: string = '';
  accessor note: string = '';
  accessor collection: string = '';
  accessor collections: string[] = [];
  accessor tags: string[] = [];
  accessor availableTags: string[] = [];
  accessor filteredTags_: string[] = [];
  accessor isAddingCollection: boolean = false;
  accessor newCollectionInput: string = '';
  accessor isCreatingCustomTag: boolean = false;
  accessor newTagInput: string = '';
  accessor highlightedTagIndex: number = -1;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('newTagInput') ||
        changedProperties.has('tags') ||
        changedProperties.has('availableTags')) {
      this.filteredTags_ = this.computeFilteredTags_();
    }
  }

  getDisplayUrl(): string {
    if (!this.url) {
      return '';
    }
    try {
      const parsed = new URL(this.url);
      if (['http:', 'https:', 'chrome:', 'chrome-extension:'].includes(
              parsed.protocol)) {
        return parsed.href;
      }
      return parsed.origin || parsed.protocol;
    } catch {
      return this.url;
    }
  }

  private computeFilteredTags_(): string[] {
    const unselected = this.availableTags.filter(
        t => !this.tags.some(tag => tag.toLowerCase() === t.toLowerCase()));
    const query = this.newTagInput.trim().toLowerCase();
    if (!query) {
      return unselected.sort((a, b) => a.localeCompare(b));
    }
    const filtered = unselected.filter(t => t.toLowerCase().includes(query));
    return filtered.sort((a, b) => {
      const aStarts = a.toLowerCase().startsWith(query);
      const bStarts = b.toLowerCase().startsWith(query);
      if (aStarts && !bStarts) {
        return -1;
      }
      if (!aStarts && bStarts) {
        return 1;
      }
      return a.localeCompare(b);
    });
  }

  private selectCollection_(col: string) {
    const trimmed = col.trim();
    if (trimmed) {
      const existing =
          this.collections.find(c => c.toLowerCase() === trimmed.toLowerCase());
      if (existing) {
        this.collection = existing;
      } else {
        this.collections = [...this.collections, trimmed];
        this.collection = trimmed;
      }
    }
    this.newCollectionInput = '';
    this.isAddingCollection = false;
  }

  private commitNewCollection_() {
    this.selectCollection_(this.newCollectionInput);
  }

  protected onNewCollectionBlur() {
    if (this.isAddingCollection) {
      this.commitNewCollection_();
    }
  }

  private selectTag_(tag: string) {
    const trimmed = tag.trim();
    if (trimmed &&
        !this.tags.some(t => t.toLowerCase() === trimmed.toLowerCase())) {
      this.tags = [...this.tags, trimmed];
    }
    this.newTagInput = '';
    this.isCreatingCustomTag = false;
    this.highlightedTagIndex = -1;
  }

  private commitNewTag_() {
    this.selectTag_(this.newTagInput);
  }

  protected onNewTagBlur() {
    if (this.isCreatingCustomTag) {
      this.commitNewTag_();
    }
  }

  protected onTagSuggestionMousedown(e: MouseEvent) {
    e.preventDefault();
    const target = e.currentTarget as HTMLElement;
    const tag = target.dataset['tag'];
    if (tag) {
      this.selectTag_(tag);
    }
  }

  protected onCollectionChange(e: Event) {
    this.collection = (e.target as HTMLSelectElement).value;
  }

  protected async onAddCollectionClick() {
    this.isAddingCollection = true;
    this.newCollectionInput = '';
    await this.updateComplete;
    this.shadowRoot?.querySelector<CrInputElement>('.collection-input')
        ?.focus();
  }

  protected onNewCollectionValueChanged(e: CustomEvent<{value: string}>) {
    this.newCollectionInput = e.detail.value;
  }

  protected onNewCollectionKeydown(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      e.preventDefault();
      this.commitNewCollection_();
    } else if (e.key === 'Escape') {
      e.preventDefault();
      this.isAddingCollection = false;
      this.newCollectionInput = '';
    }
  }

  protected async onAddTagClick() {
    this.isCreatingCustomTag = true;
    this.newTagInput = '';
    this.highlightedTagIndex = -1;
    await this.updateComplete;
    this.shadowRoot?.querySelector<CrInputElement>('.tag-input')?.focus();
  }

  protected onNewTagValueChanged(e: CustomEvent<{value: string}>) {
    this.newTagInput = e.detail.value;
    this.highlightedTagIndex = -1;
  }

  protected async onNewTagKeydown(e: KeyboardEvent) {
    const suggestions = this.filteredTags_;
    if (e.key === 'ArrowDown') {
      e.preventDefault();
      if (suggestions.length > 0) {
        this.highlightedTagIndex =
            (this.highlightedTagIndex + 1) % suggestions.length;
        await this.updateComplete;
        this.shadowRoot?.querySelector('.suggestion-item.highlighted')
            ?.scrollIntoView({block: 'nearest'});
      }
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      if (suggestions.length > 0) {
        this.highlightedTagIndex = this.highlightedTagIndex <= 0 ?
            suggestions.length - 1 :
            this.highlightedTagIndex - 1;
        await this.updateComplete;
        this.shadowRoot?.querySelector('.suggestion-item.highlighted')
            ?.scrollIntoView({block: 'nearest'});
      }
    } else if (e.key === 'Enter') {
      e.preventDefault();
      if (this.highlightedTagIndex >= 0 &&
          this.highlightedTagIndex < suggestions.length) {
        this.selectTag_(suggestions[this.highlightedTagIndex] || '');
      } else {
        this.commitNewTag_();
      }
    } else if (e.key === 'Escape') {
      e.preventDefault();
      this.isCreatingCustomTag = false;
      this.newTagInput = '';
      this.highlightedTagIndex = -1;
    }
  }

  protected onRemoveTagClick(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const tagToRemove = target.dataset['tag'];
    if (tagToRemove) {
      this.tags = this.tags.filter(t => t !== tagToRemove);
    }
  }

  protected onNoteValueChanged(e: CustomEvent<{value: string}>) {
    this.note = e.detail.value;
  }

  protected onCancelClick() {
    window.close();
  }

  protected async onSaveClick() {
    if (this.isAddingCollection && this.newCollectionInput.trim()) {
      this.commitNewCollection_();
    }
    if (this.isCreatingCustomTag && this.newTagInput.trim()) {
      this.commitNewTag_();
    }
    const {success} =
        await browserProxyFactory.getInstance().handler.saveMemoryBankEntry({
          collection: this.collection || null,
          note: this.note || null,
          tags: this.tags.length > 0 ? this.tags : null,
        });
    if (success) {
      window.close();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'save-to-memory-bank': SaveToMemoryBankElement;
  }
}

customElements.define(SaveToMemoryBankElement.is, SaveToMemoryBankElement);
