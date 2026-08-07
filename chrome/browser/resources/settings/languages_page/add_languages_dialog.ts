// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-add-languages-dialog' is a dialog for enabling
 * languages.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';

import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrSearchFieldElement} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import {FindShortcutMixinLit} from 'chrome://resources/cr_elements/find_shortcut_mixin_lit.js';
import type {I18nMixinLitInterface} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './add_languages_dialog.css.js';
import {getHtml} from './add_languages_dialog.html.js';
import {getFullName} from './languages_util.js';

export interface SettingsAddLanguagesDialogElement extends
    I18nMixinLitInterface {
  $: {
    dialog: CrDialogElement,
    list: HTMLElement,
    search: CrSearchFieldElement,
  };
}

const SettingsAddLanguagesDialogElementBase =
    FindShortcutMixinLit(I18nMixinLit(CrLitElement));

export class SettingsAddLanguagesDialogElement extends
    SettingsAddLanguagesDialogElementBase {
  static get is() {
    return 'settings-add-languages-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      languages: {type: Array},
      languagesToAdd_: {type: Object},
      disableActionButton_: {type: Boolean},
      filterValue_: {type: String},
    };
  }

  accessor languages: chrome.languageSettingsPrivate.Language[] = [];
  protected accessor languagesToAdd_: Set<string> = new Set();
  protected accessor disableActionButton_: boolean = true;
  protected accessor filterValue_: string = '';

  override connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  // Override FindShortcutMixin methods.
  override handleFindShortcut(_modalContextOpen: boolean) {
    // Assumes this is the only open modal.
    const searchInput = this.$.search.getSearchInput();
    searchInput.scrollIntoViewIfNeeded();
    if (!this.searchInputHasFocus()) {
      searchInput.focus();
    }
    return true;
  }

  // Override FindShortcutMixin methods.
  override searchInputHasFocus() {
    return this.$.search.getSearchInput() ===
        this.$.search.shadowRoot.activeElement;
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.filterValue_ = e.detail;
  }

  /** @return A list of languages to be displayed. */
  protected getLanguages_(): chrome.languageSettingsPrivate.Language[] {
    if (!this.filterValue_) {
      return this.languages;
    }

    const filterValue = this.filterValue_.toLowerCase();

    return this.languages.filter(language => {
      return language.displayName.toLowerCase().includes(filterValue) ||
          language.nativeDisplayName.toLowerCase().includes(filterValue);
    });
  }

  /** @return The number of languages to be displayed. */
  protected getLanguagesCount_(): number {
    return this.getLanguages_().length;
  }

  protected getDisplayText_(language: chrome.languageSettingsPrivate.Language):
      string {
    return getFullName(language);
  }

  /**
   * @return Whether the user has chosen to add this language (checked its
   *     checkbox).
   */
  protected willAdd_(languageCode: string): boolean {
    return this.languagesToAdd_.has(languageCode);
  }

  /** Handler for checking or unchecking a language item. */
  protected onLanguageCheckboxChange_(e: Event) {
    const checkbox = e.currentTarget as CrCheckboxElement;
    const languageCode = checkbox.dataset['code']!;
    if (checkbox.checked) {
      this.languagesToAdd_.add(languageCode);
    } else {
      this.languagesToAdd_.delete(languageCode);
    }

    this.disableActionButton_ = !this.languagesToAdd_.size;
  }

  protected onCancelButtonClick_() {
    this.$.dialog.close();
  }

  /** Enables the checked languages. */
  protected onActionButtonClick_() {
    this.fire('languages-added', Array.from(this.languagesToAdd_));
    this.$.dialog.close();
  }

  protected onKeydown_(e: KeyboardEvent) {
    // Close dialog if 'esc' is pressed and the search box is already empty.
    if (e.key === 'Escape' && !this.$.search.getValue().trim()) {
      this.$.dialog.close();
    } else if (e.key !== 'PageDown' && e.key !== 'PageUp') {
      this.$.search.scrollIntoViewIfNeeded();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-add-languages-dialog': SettingsAddLanguagesDialogElement;
  }
}

customElements.define(
    SettingsAddLanguagesDialogElement.is, SettingsAddLanguagesDialogElement);
