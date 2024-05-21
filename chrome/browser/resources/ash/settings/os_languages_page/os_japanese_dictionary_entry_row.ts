// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A row that represents an editable Japanese dictionary entry.
 */

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {JapaneseDictionaryEntry} from '../mojom-webui/user_data_japanese_dictionary.mojom-webui.js';

import {getTemplate} from './os_japanese_dictionary_entry_row.html.js';

class OsJapaneseDictionaryEntryRowElement extends PolymerElement {
  static get is() {
    return 'os-japanese-dictionary-entry-row' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      entry: {
        type: Object,
      },
    };
  }

  // The JapaneseDictionary entry that represents a key value pair and
  // attributes.
  entry: JapaneseDictionaryEntry;
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
