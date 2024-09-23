// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'app-language-selection-dialog' is a dialog to select
 * per-app language. The component supports suggested items, and filtering
 * by search query.
 * Note: `App` object must be present before showing this dialog.
 */
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_search_field/cr_search_field.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import './app_language_selection_item.js';
import '../../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrSearchFieldElement} from 'chrome://resources/ash/common/cr_elements/cr_search_field/cr_search_field.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {App, Locale} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {getAppIcon} from 'chrome://resources/cr_components/app_management/util.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppManagementBrowserProxy} from '../app_management/browser_proxy.js';

import {getTemplate} from './app_language_selection_dialog.html.js';

// Keep this in sync with tools/metrics/histograms/metadata/arc/histograms.xml
// Arc.AppLanguageSwitch.{SettingsPage}.TargetLanguage.
export enum AppLanguageSelectionDialogEntryPoint {
  APPS_MANAGEMENT_PAGE = 'AppsManagementPage',
  LANGUAGES_PAGE = 'LanguagesPage',
}

export interface AppLanguageSelectionDialogElement {
  $: {
    dialog: CrDialogElement,
    search: CrSearchFieldElement,
  };
}

const AppLanguageSelectionDialogElementBase =
    PrefsMixin(I18nMixin(PolymerElement));

export class AppLanguageSelectionDialogElement extends
    AppLanguageSelectionDialogElementBase {
  static get is() {
    return 'app-language-selection-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: Object,
      filteredLanguages_: {
        type: Array,
        computed:
            'getFilteredLanguages_(searchQuery_.length, suggestedLanguages_)',
      },
      entryPoint: String,
    };
  }

  // Public API: Bidirectional data flow.
  // prefs is provided by PrefsMixin.

  // App must be present when this dialog is shown.
  app: App;
  entryPoint: AppLanguageSelectionDialogEntryPoint;
  private suggestedLanguages_: Locale[] = [];
  private filteredLanguages_: Locale[] = [];
  private selectedLanguage_?: Locale;
  private searchQuery_ = '';

  override ready(): void {
    super.ready();
    this.initializeSuggestedLanguages_();
  }

  private onCancelButtonClick_(): void {
    this.$.dialog.close();
  }

  private onActionButtonClick_(): void {
    AppManagementBrowserProxy.getInstance().handler.setAppLocale(
        this.app.id,
        this.selectedLanguage_!.localeTag,
    );
    chrome.metricsPrivate.recordSparseValueWithHashMetricName(
        `Arc.AppLanguageSwitch.${this.entryPoint}.TargetLanguage`,
        this.selectedLanguage_!.localeTag);
    this.$.dialog.close();
  }

  private shouldDisableActionButton_(): boolean {
    return !this.selectedLanguage_;
  }

  // 'on-search-changed' event listener on a <cr-search-field>.
  private onSearchChanged_(e: CustomEvent<string>): void {
    this.searchQuery_ = e.detail.toLocaleLowerCase();
  }

  // Used to display "search result is empty" text.
  private isSearchResultEmpty_(): boolean {
    return this.searchQuery_.length > 0 && this.filteredLanguages_.length === 0;
  }

  // 'on-keydown' event listener on a <cr-search-field>.
  private onKeydown_(e: KeyboardEvent): void {
    // Close dialog if 'esc' is pressed and the search box is already empty.
    if (e.key === 'Escape' && !this.$.search.getValue().trim()) {
      this.$.dialog.close();
    } else if (e.key !== 'PageDown' && e.key !== 'PageUp') {
      this.$.search.scrollIntoView();
    }
  }

  // 'on-click' event listener on a <app-language-selection-item>.
  private toggleSelection_(e: DomRepeatEvent<Locale>): void {
    if (e.model.item === this.selectedLanguage_) {
      this.selectedLanguage_ = undefined;
    } else {
      this.selectedLanguage_ = e.model.item;
    }
  }

  // Defines whether a selected-icon should be added to the item.
  private isItemSelected_(item: Locale): boolean {
    if (!this.selectedLanguage_) {
      return false;
    }
    return this.selectedLanguage_.localeTag === item.localeTag;
  }

  // Hide suggestedLanguages along with "Device Language", and "All languages"
  // label if searchQuery exists.
  private isSearchQueryPresent(): boolean {
    return this.searchQuery_.length > 0;
  }

  // The only case where `filteredLanguages` is completely hidden is when
  // there's only 1 app-language available, and it's already selected. In this
  // case, we'll only display suggestedLanguages (deviceLanguage,
  // selectedLanguage).
  private showFilteredLanguages(): boolean {
    return this.filteredLanguages_.length > 0;
  }

  // `suggestedLanguages_` is immutable throughout the dialog lifecycle, hence
  // it's better to pre-populate once here.
  private initializeSuggestedLanguages_(): void {
    const defaultDeviceLanguage: Locale = {
      localeTag: '',
      displayName: this.i18n('appLanguageDeviceLanguageLabel'),
      nativeDisplayName: '',
    };
    // Possibly add selected locale to suggested list and init
    // currently-selected language.
    const suggestedLanguages = [defaultDeviceLanguage];
    if (this.app.selectedLocale && this.app.selectedLocale.localeTag !== '') {
      suggestedLanguages.push(this.app.selectedLocale);
      this.selectedLanguage_ = this.app.selectedLocale;
    } else {
      // Set device language as selected if no other locale selected.
      this.selectedLanguage_ = defaultDeviceLanguage;
    }
    const lastSetAppLocaleTag = this.getPref('arc.last_set_app_locale').value;
    // Add last-set-app-locale into suggestions if it's supported by app and
    // hasn't been included yet.
    const lastSetAppLocale = this.app.supportedLocales.find(
        locale => locale.localeTag === lastSetAppLocaleTag);
    if (lastSetAppLocale &&
        !suggestedLanguages.find(
            locale => locale.localeTag === lastSetAppLocale.localeTag)) {
      suggestedLanguages.push(lastSetAppLocale);
    }

    this.suggestedLanguages_ = suggestedLanguages;
  }

  // Display list of all supported languages, minus suggestedLanguages, and
  // filtered by search query.
  private getFilteredLanguages_(): Locale[] {
    let filteredItems;
    if (this.searchQuery_.length === 0) {
      // Filter out elements in suggestedList
      filteredItems = this.app.supportedLocales.filter(
          locale => !this.suggestedLanguages_.some(
              suggestedLanguage =>
                  suggestedLanguage.localeTag === locale.localeTag));
    } else {
      // Filter out based on search query.
      filteredItems = this.app.supportedLocales.filter(
          locale => locale.displayName.toLocaleLowerCase().includes(
                        this.searchQuery_) ||
              locale.nativeDisplayName.toLocaleLowerCase().includes(
                  this.searchQuery_));
    }
    return filteredItems.sort(
        (a, b) => a.displayName.localeCompare(b.displayName));
  }

  private iconUrlFromApp_(): string {
    return getAppIcon(this.app);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AppLanguageSelectionDialogElement.is]: AppLanguageSelectionDialogElement;
  }
}

customElements.define(
    AppLanguageSelectionDialogElement.is, AppLanguageSelectionDialogElement);
