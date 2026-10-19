// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsSearchEngineMoreEnginesDialogElement} from './search_engine_more_engines_dialog.js';

export function getHtml(this: SettingsSearchEngineMoreEnginesDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" class="fixed-height" show-on-attach>
  <div slot="header">
    <h2 class="title">$i18n{searchEnginesMoreEngines}</h2>
    <cr-search-field id="searchField" autofocus
        label="$i18n{searchEnginesSearchPlaceholder}"
        clear-label="$i18n{clearSearch}"
        @search-changed="${this.onSearchChanged_}">
    </cr-search-field>
  </div>
  <div id="dialogBody" slot="body" class="dialog-body">
    <cr-radio-group .selected="${this.selectedId_}"
        aria-label="$i18n{searchEnginesMoreEngines}"
        @selected-changed="${this.onRadioGroupSelectedChanged_}">
      ${this.filteredEngines_.map(item => html`
        <cr-radio-button name="${item.id}">${item.name}</cr-radio-button>
      `)}
    </cr-radio-group>
    <div role="status">
      <div class="no-search-results"
          ?hidden="${!this.showNoSearchResults_()}">
        $i18n{searchEnginesNoSearchResults}
      </div>
    </div>
  </div>
  <div slot="button-container">
    ${this.saveGuestChoice !== null ? html`
      <cr-checkbox id="saveGuestChoiceCheckbox"
          ?checked="${this.saveGuestChoice}"
          @checked-changed="${this.onSaveGuestChoiceCheckedChanged_}">
        $i18n{saveGuestChoiceText}
      </cr-checkbox>
    ` : ''}
    <cr-button id="cancelButton" @click="${this.onCancelClick_}">
      $i18n{searchEnginesCancelButton}
    </cr-button>
    <cr-button id="setAsDefaultButton" class="action-button"
        @click="${this.onSetAsDefaultClick_}"
        ?disabled="${!this.selectedId_}">
      $i18n{searchEnginesSetAsDefaultButton}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
