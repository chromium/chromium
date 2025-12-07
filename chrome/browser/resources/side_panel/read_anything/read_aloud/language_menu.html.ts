// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LanguageMenuElement} from './language_menu.js';

export function getHtml(this: LanguageMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-dialog id="languageMenu"
    @close="${this.closeLanguageMenu_}"
    @keydown="${this.onKeyDown_}"
    close-text="$i18n{readingModeLanguageMenuClose}"
    show-close-button show-on-attach ignore-popstate>
  <div slot="title" class="language-menu-title-bar">
    <div class="language-menu-title">$i18n{readingModeLanguageMenuTitle}</div>
  </div>
  <div slot="header">
    <cr-input autofocus id="searchField" class="search-field" type="search"
        placeholder="$i18n{readingModeLanguageMenuSearchLabel}"
        @value-changed="${this.onLanguageSearchValueChanged_}"
        .value="${this.languageSearchValue_}">
      <cr-icon slot="inline-prefix" alt="" icon="cr:search"></cr-icon>
      ${this.languageSearchValue_ ? html`
        <cr-icon-button id="clearLanguageSearch"
          iron-icon="cr:cancel"
          slot="inline-suffix"
          @click="${this.onClearSearchClick_}"
          title="$i18n{readingModeLanguageMenuSearchClear}">
        </cr-icon-button>` : ''}
    </cr-input>
  </div>
  <div slot="body" class="language-menu-body">
    <span id="noResultsMessage" ?hidden="${this.searchHasLanguages()}"
      aria-live="polite">
      $i18n{languageMenuNoResults}
    </span>
    ${this.availableLanguages_.map((item, index) => html`
      <div class="language-line dropdown-line">
        <span id="language-name-${index}" class="language-name">
          ${item.readableLanguage}
        </span>
        <cr-toggle ?checked="${item.checked}" @change="${this.onToggleChange_}"
          data-index="${index}"
          ?disabled="${item.disabled}"
          aria-labelledby="language-name-${index}"
          lang="${item.languageCode}">
        </cr-toggle>
      </div>
      <span id="notificationText"
          class="notification-error-${item.notification.isError}"
          aria-live="polite">
        ${item.notification.text ? this.i18n(item.notification.text) : ''}
      </span>
    `)}
    <language-toast .numAvailableVoices="${this.availableVoices.length}">
    </language-toast>
  </div>
  <div slot="footer" class="language-menu-footer">
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
