// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Foldable container for dictionary editor (for a single
 * dictionary).
 */
import './os_japanese_dictionary_entry_row.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {JapaneseDictionary} from '../mojom-webui/user_data_japanese_dictionary.mojom-webui.js';

import {getTemplate} from './os_japanese_dictionary_expand.html.js';

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
    };
  }

  // The Japanese Dictionary that this component displays information on.
  dict: JapaneseDictionary;

  // Whether or not this container UI is expanded or folded.
  private expanded_ = false;
}

customElements.define(
    OsJapaneseDictionaryExpandElement.is, OsJapaneseDictionaryExpandElement);

declare global {
  interface HTMLElementTagNameMap {
    [OsJapaneseDictionaryExpandElement.is]: OsJapaneseDictionaryExpandElement;
  }
}
