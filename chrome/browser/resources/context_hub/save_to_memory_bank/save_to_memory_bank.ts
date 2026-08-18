// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_textarea/cr_textarea.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './save_to_memory_bank.css.js';
import {getHtml} from './save_to_memory_bank.html.js';

const DEFAULT_COLLECTIONS: string[] =
    ['General', 'Research', 'Work', 'Personal'];

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
      isAddingCollection: {type: Boolean},
      newCollectionInput: {type: String},
      isCreatingCustomTag: {type: Boolean},
      newTagInput: {type: String},
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    if (this.collections.length === 0) {
      this.collections = [...DEFAULT_COLLECTIONS];
    }
    if (!this.collection && this.collections.length > 0) {
      this.collection = this.collections[0] || '';
    }
  }

  override accessor title: string = '';
  accessor url: string = '';
  accessor snippet: string = '';
  accessor note: string = '';
  accessor collection: string = '';
  accessor collections: string[] = [];
  accessor tags: string[] = [];
  accessor isAddingCollection: boolean = false;
  accessor newCollectionInput: string = '';
  accessor isCreatingCustomTag: boolean = false;
  accessor newTagInput: string = '';

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

  protected onCollectionChange(e: Event) {
    this.collection = (e.target as HTMLSelectElement).value;
  }

  protected onAddCollectionClick() {
    this.isAddingCollection = !this.isAddingCollection;
    this.newCollectionInput = '';
  }

  protected onNewCollectionValueChanged(e: CustomEvent<{value: string}>) {
    this.newCollectionInput = e.detail.value;
  }

  protected onNewCollectionKeydown(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      const trimmed = this.newCollectionInput.trim();
      if (trimmed) {
        if (!this.collections.includes(trimmed)) {
          this.collections = [...this.collections, trimmed];
        }
        this.collection = trimmed;
        this.newCollectionInput = '';
        this.isAddingCollection = false;
      }
    } else if (e.key === 'Escape') {
      this.isAddingCollection = false;
      this.newCollectionInput = '';
    }
  }

  protected onAddTagClick() {
    this.isCreatingCustomTag = !this.isCreatingCustomTag;
    this.newTagInput = '';
  }

  protected onNewTagValueChanged(e: CustomEvent<{value: string}>) {
    this.newTagInput = e.detail.value;
  }

  protected onNewTagKeydown(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      const trimmed = this.newTagInput.trim();
      if (trimmed && !this.tags.includes(trimmed)) {
        this.tags = [...this.tags, trimmed];
        this.newTagInput = '';
        this.isCreatingCustomTag = false;
      }
    } else if (e.key === 'Escape') {
      this.isCreatingCustomTag = false;
      this.newTagInput = '';
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

  protected onSaveClick() {
    window.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'save-to-memory-bank': SaveToMemoryBankElement;
  }
}

customElements.define(SaveToMemoryBankElement.is, SaveToMemoryBankElement);
