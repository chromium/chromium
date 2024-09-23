// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A row that represents an editable Japanese dictionary entry.
 */

import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {JapaneseDictionaryEntry} from '../mojom-webui/user_data_japanese_dictionary.mojom-webui.js';

import {getTemplate} from './os_japanese_dictionary_entry_row.html.js';
import {UserDataServiceProvider} from './user_data_service_provider.js';

class OsJapaneseDictionaryEntryRowElement extends PolymerElement {
  static get is() {
    return 'os-japanese-dictionary-entry-row' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // TODO(b/340393256): Handle deserialization.
      dictId: {
        type: Number,
      },
      index: {
        type: Number,
      },
      entry: {
        type: Object,
      },
      locallyAdded: {
        type: Boolean,
      },
    };
  }

  // Whether the entry needs to be added to the storage.
  locallyAdded = false;

  // The ID of the Japanese User Dictionary that the entry is part of.
  dictId: bigint;

  // Index of the entry within the dictionary.
  index: number;

  // The JapaneseDictionary entry that represents a key value pair and
  // attributes.
  entry: JapaneseDictionaryEntry;

  private saveReading_(e: Event): void {
    this.entry.key = (e.target as CrInputElement).value;
    this.saveEntryToDictionary_();
  }

  private saveWord_(e: Event): void {
    this.entry.value = (e.target as CrInputElement).value;
    this.saveEntryToDictionary_();
  }

  private saveComment_(e: Event): void {
    this.entry.comment = (e.target as CrInputElement).value;
    this.saveEntryToDictionary_();
  }

  private async deleteEntry_(): Promise<void> {
    const dictionarySaved =
        (await UserDataServiceProvider.getRemote()
             .deleteJapaneseDictionaryEntry(this.dictId, this.index))
            .status.success;
    if (dictionarySaved) {
      this.dispatchSavedEvent_();
    }
  }

  private async saveEntryToDictionary_(): Promise<void> {
    if (this.entry.key === '' || this.entry.value === '') {
      return;
    }

    let dictionarySaved = false;
    if (this.locallyAdded) {
      // Entry does not exist inside the storage, hence we need to use the "add"
      // function to add this entry.
      // TODO(b/340393256): Handle possible race condition when two add..Entry
      // requests are in flight at the same time.
      const resp =
          (await UserDataServiceProvider.getRemote().addJapaneseDictionaryEntry(
               this.dictId, this.entry))
              .status;

      if (resp.success) {
        // If successful, then the entry is no longer "locally added", since it
        // also exists inside the storage. Future edits need to be done via the
        // "edit" api call.
        this.locallyAdded = false;
        dictionarySaved = true;
      }
    } else {
      dictionarySaved = (await UserDataServiceProvider.getRemote()
                             .editJapaneseDictionaryEntry(
                                 this.dictId, this.index, this.entry))
                            .status.success;
    }

    if (dictionarySaved) {
      this.dispatchSavedEvent_();
    }
  }

  private dispatchSavedEvent_(): void {
    this.dispatchEvent(
        new CustomEvent('dictionary-saved', {bubbles: true, composed: true}));
  }
}

customElements.define(
    OsJapaneseDictionaryEntryRowElement.is,
    OsJapaneseDictionaryEntryRowElement);

declare global {
  interface HTMLElementTagNameMap {
    [OsJapaneseDictionaryEntryRowElement.is]:
        OsJapaneseDictionaryEntryRowElement;
  }
}

declare global {
  interface HTMLElementEventMap {
    ['dictionary-saved']: CustomEvent;
  }
}
