// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Foldable container for dictionary editor (for a single
 * dictionary).
 */
import './os_japanese_dictionary_entry_row.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';

import type {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import type {BigBuffer} from 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {BigString} from 'chrome://resources/mojo/mojo/public/mojom/base/big_string.mojom-webui.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {JapaneseDictionary} from '../mojom-webui/user_data_japanese_dictionary.mojom-webui.js';
import {JpPosType} from '../mojom-webui/user_data_japanese_dictionary.mojom-webui.js';

import type {EntryDeletedCustomEvent} from './os_japanese_dictionary_entry_row.js';
import {getTemplate} from './os_japanese_dictionary_expand.html.js';
import {UserDataServiceProvider} from './user_data_service_provider.js';

export type DictionaryDeletedCustomEvent = CustomEvent<{dictIndex: number}>;

interface OsJapaneseDictionaryExpandElement {
  $: {
    selectFileDialog: HTMLElement,
  };
}

class OsJapaneseDictionaryExpandElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'os-japanese-dictionary-expand' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      dict: {
        type: Object,
      },
      syncedEntriesCount: {
        type: Number,
      },
      showingDeleteDialog_: {
        type: Boolean,
      },
      statusMessage_: {
        type: String,
      },
      dictIndex: {
        type: Number,
      },
    };
  }

  override ready(): void {
    super.ready();
    this.addEventListener('dictionary-entry-deleted', this.onEntryDelete_);
  }

  // The Japanese Dictionary that this component displays information on.
  dict: JapaneseDictionary;

  // Any entry beyond this index needs to be added to the dictionary rather than
  // "edited" since it does not exist in the file storage at the moment.
  syncedEntriesCount: number;

  // Whether or not this container UI is expanded or folded.
  private expanded_ = false;

  private showingDeleteDialog_ = false;

  // Used for chromevox announcements.
  private statusMessage_ = '';

  private dictIndex = 0;

  private onEntryDelete_(event: EntryDeletedCustomEvent): void {
    this.statusMessage_ = '';
    afterNextRender(this, () => {
      this.statusMessage_ = this.i18n('japaneseDictionaryEntryDeleted');
      if (event.detail.isLastEntry) {
        const newEntryButton =
            this.shadowRoot!.querySelector<HTMLElement>('#newEntryButton');

        if (newEntryButton) {
          // TODO(crbug.com/419677565): Find a way to queue this focus only
          // after the hidden value has changed by itself. This is a hacky way
          // to resolve a race condition where the hidden value has not changed
          // yet before the focus is moved.
          newEntryButton.removeAttribute('hidden');
          newEntryButton.focus();
        }
      }
    });
  }

  // Adds a new entry locally to create an entry-row component.
  private addEntry_(): void {
    // This changes the entries array from the parent component which it will
    // not be notified of. This is intentional.
    // We do not want to trigger a rerender in the parent component.
    this.push(
        'dict.entries',
        {key: '', value: '', pos: JpPosType.kNoPos, comment: ''});
    afterNextRender(this, () => {
      this.shadowRoot!
          .querySelector<HTMLElement>(
              'os-japanese-dictionary-entry-row:last-of-type')!.shadowRoot!
          .querySelector<HTMLElement>('cr-input')!.focus();
    });
  }

  // Renames the dictionary.
  private async saveName_(e: Event): Promise<void> {
    this.dict.name = (e.target as CrInputElement).value;
    const dictionarySaved =
        (await UserDataServiceProvider.getRemote().renameJapaneseDictionary(
             this.dict.id, this.dict.name))
            .status.success;
    if (dictionarySaved) {
      this.dispatchSavedEvent_();
    }
  }

  // Renames the dictionary.
  private async deleteDictionary_(): Promise<void> {
    const dictionarySaved =
        (await UserDataServiceProvider.getRemote().deleteJapaneseDictionary(
             this.dict.id))
            .status.success;
    if (dictionarySaved) {
      this.dispatchDictionaryDeletedEvent_();
      this.dispatchSavedEvent_();
    }
    this.showingDeleteDialog_ = false;
  }

  private hideDeleteDialog_() {
    this.showingDeleteDialog_ = false;
  }

  private showDeleteDialog_() {
    this.showingDeleteDialog_ = true;
  }

  // Export dictionary.
  private exportDictionary_(): void {
    const a = document.createElement('a');
    a.href = `jp-export-dictionary/${this.dict.id}`;
    // In case there is no name, use a placeholder name.
    const fileName = this.dict.name || 'unnamed-dictionary';
    a.download = `${fileName}.txt`;
    a.click();
  }

  // Imports dictionary.
  private importDictionary_(): void {
    this.$.selectFileDialog.dispatchEvent(new MouseEvent('click'));
  }

  private async handleFileSelectChange_(e: Event): Promise<void> {
    const fileInput = e.target as HTMLInputElement;
    const fileData = fileInput.files![0];
    // Use bytes for now rather than shared memory for simplicity.
    // TODO(b/366101658): Use shared memory when file is too big.
    // The limit below is the max size that a mojo BigBuffer can handle via
    // directly using the bytes rather than shared memory.
    if (fileData.size >= 128 * 1048576) {
      // Clear value so that file select change will retrigger for the same
      // file.
      fileInput.value = '';
      return;
    }
    const fileDataView = new Uint8Array(await fileData.arrayBuffer());
    const fileMojomBigBuffer: BigBuffer = {
      bytes: Array.from(fileDataView),
    };
    const fileMojomBigString: BigString = {data: fileMojomBigBuffer};

    const {status} =
        await UserDataServiceProvider.getRemote().importJapaneseDictionary(
            this.dict.id, fileMojomBigString);
    if (status.success) {
      this.dispatchSavedEvent_();
    }
    // Clear value so that file select change will retrigger for the same file.
    fileInput.value = '';
  }

  // Returns true if this entry is a locally added entry.
  private locallyAdded_(entryIndex: number): boolean {
    // This entry falls outside of the range of entries that were initially
    // synced, hence it must be added locally.
    return entryIndex > this.syncedEntriesCount;
  }

  // Returns true if this entry is the last one.
  private isLastEntry_(entryIndex: number): boolean {
    return entryIndex === this.dict.entries.length - 1;
  }

  // If there is currently an unsynced entry then hide the add button.
  // This is to prevent two "unadded" entries to cause issues with ordering when
  // synced. Users should only be able to add one entry at a time before a sync
  // occurs.
  private shouldShowAddButton_(entriesLength: number): boolean {
    return entriesLength - 1 <= this.syncedEntriesCount;
  }

  private dispatchSavedEvent_(): void {
    this.dispatchEvent(
        new CustomEvent('dictionary-saved', {bubbles: true, composed: true}));
  }

  private dispatchDictionaryDeletedEvent_(): void {
    this.dispatchEvent(new CustomEvent('dictionary-deleted', {
      bubbles: true,
      composed: true,
      detail: {dictIndex: this.dictIndex},
    }));
  }


  private i18nDialogString_(dictName: string): string {
    return this.i18n('japaneseDeleteDictionaryDetail', dictName);
  }
}


customElements.define(
    OsJapaneseDictionaryExpandElement.is, OsJapaneseDictionaryExpandElement);

declare global {
  interface HTMLElementTagNameMap {
    [OsJapaneseDictionaryExpandElement.is]: OsJapaneseDictionaryExpandElement;
  }
}

declare global {
  interface HTMLElementEventMap {
    ['dictionary-saved']: CustomEvent;
    ['dictionary-deleted']: DictionaryDeletedCustomEvent;
  }
}
