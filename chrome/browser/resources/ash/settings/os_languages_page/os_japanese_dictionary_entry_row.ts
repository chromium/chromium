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
    };
  }

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

  private async saveEntryToDictionary_(): Promise<void> {
    UserDataServiceProvider.getRemote().editJapaneseDictionaryEntry(
        this.dictId, this.index, this.entry);
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
