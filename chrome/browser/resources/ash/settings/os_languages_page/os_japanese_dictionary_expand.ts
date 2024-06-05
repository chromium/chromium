// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Foldable container for dictionary editor (for a single
 * dictionary).
 */
import './os_japanese_dictionary_entry_row.js';

import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {JapaneseDictionary, JpPosType} from '../mojom-webui/user_data_japanese_dictionary.mojom-webui.js';

import {getTemplate} from './os_japanese_dictionary_expand.html.js';
import {UserDataServiceProvider} from './user_data_service_provider.js';

class OsJapaneseDictionaryExpandElement extends PolymerElement {
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
    };
  }

  // The Japanese Dictionary that this component displays information on.
  dict: JapaneseDictionary;

  // Any entry beyond this index needs to be added to the dictionary rather than
  // "edited" since it does not exist in the file storage at the moment.
  syncedEntriesCount: number;

  // Whether or not this container UI is expanded or folded.
  private expanded_ = false;

  // Adds a new entry locally to create an entry-row component.
  private addEntry_(): void {
    // This changes the entries array from the parent component which it will
    // not be notified of. This is intentional.
    // We do not want to trigger a rerender in the parent component.
    this.push(
        'dict.entries',
        {key: '', value: '', pos: JpPosType.kNoPos, comment: ''});
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
      this.dispatchSavedEvent_();
    }
  }

  // Returns true if this entry is a locally added entry.
  private locallyAdded_(entryIndex: number): boolean {
    // This entry falls outside of the range of entries that were initially
    // synced, hence it must be added locally.
    return entryIndex > this.syncedEntriesCount;
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
  }
}
