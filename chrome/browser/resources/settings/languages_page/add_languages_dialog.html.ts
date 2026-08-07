// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsAddLanguagesDialogElement} from './add_languages_dialog.js';

export function getHtml(this: SettingsAddLanguagesDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}">
  <div id="dialog-title" slot="title">
    <span>$i18n{addLanguagesDialogTitle}</span>
    <cr-search-field label="$i18n{searchLanguages}" id="search"
        clear-label="$i18n{clearSearch}"
        @search-changed="${this.onSearchChanged_}"
        @keydown="${this.onKeydown_}" autofocus>
    </cr-search-field>
  </div>
  <div id="dialog-body" slot="body" scrollable>
    <div id="list" role="listbox" focusgroup="listbox block"
        ?hidden="${!this.getLanguagesCount_()}">
      ${this.getLanguages_().map(item => html`
        <cr-checkbox ?checked="${this.willAdd_(item.code)}"
            data-code="${item.code}"
            aria-label="${this.i18n('addLanguageAriaLabel', item.displayName)}"
            @change="${this.onLanguageCheckboxChange_}">
          <div class="text-elide">${this.getDisplayText_(item)}</div>
        </cr-checkbox>
      `)}
    </div>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelButtonClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" @click="${this.onActionButtonClick_}"
        ?disabled="${this.disableActionButton_}">
      $i18n{add}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
