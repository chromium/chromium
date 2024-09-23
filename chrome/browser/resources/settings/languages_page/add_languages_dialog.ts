// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-add-languages-dialog' is a dialog for enabling
 * languages.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../controls/settings_checkbox_list_entry.js';
import '../settings_shared.css.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrSearchFieldElement} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import {FindShortcutMixin} from 'chrome://resources/cr_elements/find_shortcut_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsCheckboxListEntryElement} from '../controls/settings_checkbox_list_entry.js';
import {ScrollableMixin} from '../scrollable_mixin.js';

import {getTemplate} from './add_languages_dialog.html.js';
import type {LanguageHelper} from './languages_types.js';

export interface SettingsAddLanguagesDialogElement {
  $: {
    dialog: CrDialogElement,
    search: CrSearchFieldElement,
  };
}

interface Repeaterevent extends Event {
  target: SettingsCheckboxListEntryElement;
  model: {
    item: chrome.languageSettingsPrivate.Language,
  };
}

const SettingsAddLanguagesDialogElementBase =
    ScrollableMixin(FindShortcutMixin(I18nMixin(PolymerElement)));

export class SettingsAddLanguagesDialogElement extends
    SettingsAddLanguagesDialogElementBase {
  static get is() {
    return 'settings-add-languages-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      languages: {
        type: Array,
        notify: true,
      },

      languageHelper: Object,

      languagesToAdd_: {
        type: Object,
        value() {
          return new Set();
        },
      },

      disableActionButton_: {
        type: Boolean,
        value: true,
      },

      filterValue_: {
        type: String,
        value: '',
      },
    };
  }

  languages: chrome.languageSettingsPrivate.Language[];
  languageHelper: LanguageHelper;
  private languagesToAdd_: Set<string>;
  private disableActionButton_: boolean;
  private filterValue_: string;

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
        this.$.search.shadowRoot!.activeElement;
  }

  private onSearchChanged_(e: CustomEvent<string>) {
    this.filterValue_ = e.detail;
  }

  /** @return A list of languages to be displayed. */
  private getLanguages_(): chrome.languageSettingsPrivate.Language[] {
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
  private getLanguagesCount_(): number {
    return this.getLanguages_().length;
  }

  /** @return A 1-based index for aria-posinset. */
  private getAriaPosinset_(index: number): number {
    return index + 1;
  }

  private getDisplayText_(language: chrome.languageSettingsPrivate.Language):
      string {
    return this.languageHelper.getFullName(language);
  }

  /**
   * @return Whether the user has chosen to add this language (checked its
   *     checkbox).
   */
  private willAdd_(languageCode: string): boolean {
    return this.languagesToAdd_.has(languageCode);
  }

  /** Handler for checking or unchecking a language item. */
  private onLanguageCheckboxChange_(e: Repeaterevent) {
    // Add or remove the item to the Set. No need to worry about data binding:
    // willAdd_ is called to initialize the checkbox state (in case the
    // iron-list re-uses a previous checkbox), and the checkbox can only be
    // changed after that by user action.
    const language = e.model.item;
    if (e.target.checked) {
      this.languagesToAdd_.add(language.code);
    } else {
      this.languagesToAdd_.delete(language.code);
    }

    this.disableActionButton_ = !this.languagesToAdd_.size;
  }

  private onCancelButtonClick_() {
    this.$.dialog.close();
  }

  /** Enables the checked languages. */
  private onActionButtonClick_() {
    this.dispatchEvent(new CustomEvent('languages-added', {
      bubbles: true,
      composed: true,
      detail: Array.from(this.languagesToAdd_),
    }));
    this.$.dialog.close();
  }

  private onKeydown_(e: KeyboardEvent) {
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
