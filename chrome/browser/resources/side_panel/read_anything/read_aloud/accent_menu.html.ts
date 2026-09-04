// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AccentMenuElement} from './accent_menu.js';

export function getHtml(this: AccentMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <cr-dialog id="accentMenu" @close="${this.onClose_}"
      @keydown="${this.onKeydown_}"
      close-text="$i18n{accentMenuClose}"
      show-close-button show-on-attach ignore-popstate>
  <div slot="title" class="accent-menu-title-bar">
    <div class="accent-menu-title">$i18n{accentMenuLabel}</div>
  </div>
  <div slot="header">
    <cr-input autofocus id="searchField" class="search-field" type="search"
        placeholder="$i18n{readingModeLanguageMenuSearchLabel}"
        @value-changed="${this.onLanguageSearchValueChanged_}"
        .value="${this.languageSearchValue_}">
      <cr-icon slot="inline-prefix" alt="" icon="cr:search"></cr-icon>
      ${this.languageSearchValue_ ? html`
        <cr-icon-button id="clearLanguageSearch"
          iron-icon="cr:cancel-filled"
          slot="inline-suffix"
          @click="${this.onClearSearchClick_}"
          title="$i18n{readingModeLanguageMenuSearchClear}">
        </cr-icon-button>` : ''}
    </cr-input>
  </div>
  <div slot="body" class="accent-menu-body"
      role="radiogroup"
      aria-label="$i18n{accentMenuLabel}">
    <span id="noResultsMessage" ?hidden="${this.searchHasLanguages()}"
      aria-live="polite">
      $i18n{languageMenuNoResults}
    </span>
    ${this.availableLanguages_.map((item, index) => html`
      <button class="dropdown-item"
          data-index="${index}"
          role="radio"
          aria-checked="${this.getItemAriaChecked_(item)}"
          @click="${this.onLanguageSelectClick_}"
          aria-labelledby="accent-name-${index}">
        <cr-icon id="check-mark-${index}"
            class="check-mark check-mark-showing-${item.selected}"
            icon="cr:check">
        </cr-icon>
        <span id="accent-name-${index}" class="language-name">
          ${item.readableLanguage}
        </span>
      </button>
      <span id="notificationText-${index}"
          class="notification-text notification-error-${
              item.notification.isError}"
          aria-live="polite">
        ${item.notification.text ? this.i18n(item.notification.text) : ''}
      </span>
    `)}
    <language-toast .numAvailableVoices="${this.availableVoices.length}">
    </language-toast>
  </div>
  <div slot="footer" class="accent-menu-footer">
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
