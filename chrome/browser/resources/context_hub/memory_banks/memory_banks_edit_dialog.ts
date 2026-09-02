// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_textarea/cr_textarea.js';
import '//resources/cr_elements/icons.html.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory} from '../context_hub.mojom-webui.js';
import type {MemoryBankEntry} from '../context_hub.mojom-webui.js';

import {getCss} from './memory_banks_edit_dialog.css.js';
import {getHtml} from './memory_banks_edit_dialog.html.js';

export interface EntryAnnotationsUpdatedDetail {
  id: bigint;
  collection: string|null;
  note: string|null;
  tags: string[];
}

export interface MemoryBanksEditDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class MemoryBanksEditDialogElement extends CrLitElement {
  static get is() {
    return 'memory-banks-edit-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      entry: {type: Object},
      availableCollections: {type: Array},
      availableTags: {type: Array},
      editCollection_: {type: String, state: true},
      editNote_: {type: String, state: true},
      editTags_: {type: Array, state: true},
      highlightedTagIndex_: {type: Number, state: true},
      isAddingCollection_: {type: Boolean, state: true},
      newCollectionInput_: {type: String, state: true},
      isCreatingTag_: {type: Boolean, state: true},
      newTagInput_: {type: String, state: true},
    };
  }

  // Public Properties
  accessor entry: MemoryBankEntry|null = null;
  accessor availableCollections: string[] = [];
  accessor availableTags: string[] = [];

  // Internal Form State
  protected accessor editCollection_: string = '';
  protected accessor isAddingCollection_: boolean = false;
  protected accessor newCollectionInput_: string = '';
  protected accessor editTags_: string[] = [];
  protected accessor isCreatingTag_: boolean = false;
  protected accessor newTagInput_: string = '';
  protected accessor highlightedTagIndex_: number = -1;
  protected accessor editNote_: string = '';

  // Lifecycle
  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('entry') && this.entry) {
      this.editCollection_ = this.entry.collection || '';
      this.editNote_ = this.entry.note || '';
      this.editTags_ = this.entry.tags ? [...this.entry.tags] : [];
      this.isAddingCollection_ = false;
      this.newCollectionInput_ = '';
      this.isCreatingTag_ = false;
      this.newTagInput_ = '';
      this.highlightedTagIndex_ = -1;
    }
  }

  // Dialog Action Handlers
  protected onDialogClose_() {
    this.fire('close');
  }

  protected onCancelClick_() {
    this.$.dialog.cancel();
  }

  protected async onSaveClick_() {
    if (!this.entry) {
      return;
    }
    const id = this.entry.id;
    const collection =
        (this.isAddingCollection_ && this.newCollectionInput_.trim() ?
             this.newCollectionInput_ :
             this.editCollection_)
            .trim() ||
        null;
    const note = this.editNote_.trim() || null;
    if (this.isCreatingTag_ && this.newTagInput_.trim()) {
      this.selectTag_(this.newTagInput_);
    }
    const tags = [...this.editTags_];

    const {success} = await browserProxyFactory.getInstance()
                          .handler.updateMemoryBankEntryAnnotations(id, {
                            note,
                            collection,
                            tags: tags.length > 0 ? tags : null,
                          });

    if (success) {
      this.fire('entry-annotations-updated', {
        id,
        collection,
        note,
        tags,
      } as EntryAnnotationsUpdatedDetail);
      this.$.dialog.close();
    }
  }

  // Collection Section Handlers & Helpers
  // TODO(crbug.com/523377643): A lot of the tag and collection suggestion
  // and management logic here is similar to save_to_memory_bank.ts.
  // We should refactor both to share common components.
  protected getEditDialogCollections_(): string[] {
    const set = new Set<string>(this.availableCollections);
    if (this.editCollection_) {
      set.add(this.editCollection_);
    }
    return Array.from(set).sort((a, b) => a.localeCompare(b));
  }

  protected onEditCollectionChange_(e: Event) {
    this.editCollection_ = (e.target as HTMLSelectElement).value;
  }

  protected onAddCollectionClick_() {
    this.isAddingCollection_ = !this.isAddingCollection_;
    this.newCollectionInput_ = '';
  }

  protected onNewCollectionInputValueChanged_(e: CustomEvent<{value: string}>) {
    this.newCollectionInput_ = e.detail.value;
  }

  protected onNewCollectionInputKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter' && this.newCollectionInput_.trim()) {
      this.editCollection_ = this.newCollectionInput_.trim();
    }
    if (e.key === 'Enter' || e.key === 'Escape') {
      this.isAddingCollection_ = false;
      this.newCollectionInput_ = '';
    }
  }

  // Tag Section Handlers & Helpers
  protected getFilteredTags_(): string[] {
    const query = this.newTagInput_.trim().toLowerCase();
    if (!query) {
      return [];
    }
    const filtered = this.availableTags.filter(
        t => !this.editTags_.some(
                 tag => tag.toLowerCase() === t.toLowerCase()) &&
            t.toLowerCase().includes(query));
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

  private selectTag_(tag: string) {
    const trimmed = tag.trim();
    if (trimmed &&
        !this.editTags_.some(t => t.toLowerCase() === trimmed.toLowerCase())) {
      this.editTags_ = [...this.editTags_, trimmed];
    }
    this.newTagInput_ = '';
    this.isCreatingTag_ = false;
    this.highlightedTagIndex_ = -1;
  }

  protected async onAddTagClick_() {
    this.isCreatingTag_ = true;
    this.newTagInput_ = '';
    this.highlightedTagIndex_ = -1;
    await this.updateComplete;
    this.shadowRoot?.querySelector<HTMLElement>('.tag-input')?.focus();
  }

  protected onRemoveTagClick_(e: Event) {
    const tag = (e.currentTarget as HTMLElement).dataset['tag'];
    this.editTags_ = this.editTags_.filter(t => t !== tag);
  }

  protected onTagSuggestionMousedown_(e: MouseEvent) {
    e.preventDefault();
    const target = e.currentTarget as HTMLElement;
    const tag = target.dataset['tag'];
    if (tag) {
      this.selectTag_(tag);
    }
  }

  protected onNewTagInputValueChanged_(e: CustomEvent<{value: string}>) {
    this.newTagInput_ = e.detail.value;
    this.highlightedTagIndex_ = -1;
  }

  protected onNewTagInputKeydown_(e: KeyboardEvent) {
    const suggestions = this.getFilteredTags_();
    if (e.key === 'ArrowDown') {
      e.preventDefault();
      if (suggestions.length > 0) {
        this.highlightedTagIndex_ =
            (this.highlightedTagIndex_ + 1) % suggestions.length;
      }
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      if (suggestions.length > 0) {
        this.highlightedTagIndex_ =
            (this.highlightedTagIndex_ - 1 + suggestions.length) %
            suggestions.length;
      }
    } else if (e.key === 'Enter') {
      e.preventDefault();
      if (this.highlightedTagIndex_ >= 0 &&
          this.highlightedTagIndex_ < suggestions.length) {
        this.selectTag_(suggestions[this.highlightedTagIndex_] || '');
      } else {
        const val = this.newTagInput_.trim();
        if (val) {
          this.selectTag_(val);
        } else {
          this.isCreatingTag_ = false;
          this.newTagInput_ = '';
          this.highlightedTagIndex_ = -1;
        }
      }
    } else if (e.key === 'Escape') {
      e.preventDefault();
      this.isCreatingTag_ = false;
      this.newTagInput_ = '';
      this.highlightedTagIndex_ = -1;
    }
  }

  protected onNewTagBlur_() {
    if (this.isCreatingTag_) {
      const val = this.newTagInput_.trim();
      if (val) {
        this.selectTag_(val);
      } else {
        this.isCreatingTag_ = false;
        this.newTagInput_ = '';
        this.highlightedTagIndex_ = -1;
      }
    }
  }

  // Note Section Handlers
  protected onEditNoteValueChanged_(e: CustomEvent<{value: string}>) {
    this.editNote_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'memory-banks-edit-dialog': MemoryBanksEditDialogElement;
  }
}

customElements.define(
    MemoryBanksEditDialogElement.is, MemoryBanksEditDialogElement);

