// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_textarea/cr_textarea.js';
import '//resources/cr_elements/icons.html.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory} from '../context_hub.mojom-webui.js';

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
    this.initializeContext_();
  }

  private async initializeContext_() {
    const {context} = await browserProxyFactory.getInstance()
                          .handler.getSaveToMemoryBankContext();
    if (context) {
      this.url = context.url || '';
      this.title = context.tabTitle || '';
      this.snippet = context.selectedText || '';
    }
  }

  override accessor title: string = '';
  accessor url: string = '';
  accessor snippet: string = '';
  accessor note: string = '';
  accessor collection: string = DEFAULT_COLLECTIONS[0] ?? '';
  accessor collections: string[] = [...DEFAULT_COLLECTIONS];
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

  private commitNewCollection_() {
    const trimmed = this.newCollectionInput.trim();
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

  protected onNewCollectionBlur() {
    if (this.isAddingCollection) {
      this.commitNewCollection_();
    }
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
    await this.updateComplete;
    this.shadowRoot?.querySelector<CrInputElement>('.tag-input')?.focus();
  }

  protected onNewTagValueChanged(e: CustomEvent<{value: string}>) {
    this.newTagInput = e.detail.value;
  }

  private commitNewTag_() {
    const trimmed = this.newTagInput.trim();
    if (trimmed &&
        !this.tags.some(t => t.toLowerCase() === trimmed.toLowerCase())) {
      this.tags = [...this.tags, trimmed];
    }
    this.newTagInput = '';
    this.isCreatingCustomTag = false;
  }

  protected onNewTagBlur() {
    if (this.isCreatingCustomTag) {
      this.commitNewTag_();
    }
  }

  protected onNewTagKeydown(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      e.preventDefault();
      this.commitNewTag_();
    } else if (e.key === 'Escape') {
      e.preventDefault();
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

  protected async onSaveClick() {
    if (this.isAddingCollection && this.newCollectionInput.trim()) {
      this.commitNewCollection_();
    }
    if (this.isCreatingCustomTag && this.newTagInput.trim()) {
      this.commitNewTag_();
    }
    const {success} =
        await browserProxyFactory.getInstance().handler.saveMemoryBankEntry({
          note: this.note || null,
          collection: this.collection || null,
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
