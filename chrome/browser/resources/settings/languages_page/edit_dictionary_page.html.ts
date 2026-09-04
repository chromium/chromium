// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsEditDictionaryPageElement} from './edit_dictionary_page.js';

export function getHtml(this: SettingsEditDictionaryPageElement) {
  return html`<!--_html_template_start_-->
  <settings-subpage
      page-title="$i18n{editDictionaryPageTitle}"
      ?no-search="${!this.enableSpellcheckingPref_?.value}"
      route-path="${this.routePath}">
    <div class="cr-row first" @keydown="${this.onNewWordKeydown_}">
      <cr-input id="newWord" .value="${this.newWordValue_}"
          @value-changed="${this.onNewWordValueChanged_}"
          placeholder="$i18n{addDictionaryWordLabel}"
          ?invalid="${this.isWordInvalid_()}"
          error-message="${this.getErrorMessage_()}"
          spellcheck="false">
        <cr-button @click="${this.onAddWordClick_}" id="addWord" slot="suffix"
            ?disabled="${this.disableAddButton_()}">
          $i18n{addDictionaryWordButton}
        </cr-button>
      </cr-input>
    </div>
    <div class="cr-row continuation">
      <h2>$i18n{customDictionaryWords}</h2>
    </div>
    <div class="list-frame">
      ${this.hasWords_ ? html`
        <div id="list" role="listbox" focusgroup="listbox block">
          ${this.words_.map((item, index) => html`
            <div class="list-item">
              <div id="word${index}" class="word text-elide">${item}</div>
              <cr-icon-button class="icon-clear"
                  data-item="${item}"
                  @click="${this.onRemoveWordClick_}"
                  title="$i18n{deleteDictionaryWordButton}"
                  aria-describedby="word${index}">
              </cr-icon-button>
            </div>
          `)}
        </div>
      ` : ''}
      <div id="noWordsLabel" class="list-item" ?hidden="${this.hasWords_}">
        $i18n{noCustomDictionaryWordsFound}
      </div>
    </div>
  </settings-subpage>
<!--_html_template_end_-->`;
}
