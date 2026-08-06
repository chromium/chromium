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
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';

import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrSearchFieldElement} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import {FindShortcutMixin} from 'chrome://resources/cr_elements/find_shortcut_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ScrollableMixin} from '../scrollable_mixin.js';

import {getTemplate} from './add_languages_dialog.html.js';
import {getFullName} from './languages_util.js';

export interface SettingsAddLanguagesDialogElement {
  $: {
    dialog: CrDialogElement,
    list: HTMLElement,
    search: CrSearchFieldElement,
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
      languages: Object,

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

  declare languages: chrome.languageSettingsPrivate.Language[];
  declare private languagesToAdd_: Set<string>;
  declare private disableActionButton_: boolean;
  declare private filterValue_: string;

  override connectedCallback() {
    super.connectedCallback();

    // TODO(crbug.com/540914692): Workaround for Blink bug, by resetting
    // focusgroup attribute restores FocusgroupData that was wiped out during
    // detachment/attachment. Can probably remove this after migrating to Lit,
    // since detachment/attachment happens due to the parent chain using dom-if.
    this.$.list.setAttribute(
        'focusgroup', this.$.list.getAttribute('focusgroup')!);

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

  private getDisplayText_(language: chrome.languageSettingsPrivate.Language):
      string {
    return getFullName(language);
  }

  /**
   * @return Whether the user has chosen to add this language (checked its
   *     checkbox).
   */
  private willAdd_(languageCode: string): boolean {
    return this.languagesToAdd_.has(languageCode);
  }

  /** Handler for checking or unchecking a language item. */
  private onLanguageCheckboxChange_(
      e: DomRepeatEvent<chrome.languageSettingsPrivate.Language>) {
    const checkbox = e.target as CrCheckboxElement;
    const language = e.model.item;
    if (checkbox.checked) {
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
