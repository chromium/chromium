// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-change-device-language-dialog' is a dialog for
 * changing device language.
 */
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_search_field/cr_search_field.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import './shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import './languages.js';
import '../settings_shared.css.js';

import {LifetimeBrowserProxyImpl} from '/shared/settings/lifetime_browser_proxy.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrScrollableMixin} from 'chrome://resources/ash/common/cr_elements/cr_scrollable_mixin.js';
import {CrSearchFieldElement} from 'chrome://resources/ash/common/cr_elements/cr_search_field/cr_search_field.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';

import {getTemplate} from './change_device_language_dialog.html.js';
import {LanguagesMetricsProxyImpl, LanguagesPageInteraction} from './languages_metrics_proxy.js';
import {LanguageHelper, LanguagesModel} from './languages_types.js';

export interface OsSettingsChangeDeviceLanguageDialogElement {
  $: {
    dialog: CrDialogElement,
    search: CrSearchFieldElement,
  };
}

const OsSettingsChangeDeviceLanguageDialogElementBase =
    I18nMixin(CrScrollableMixin(PolymerElement));

export class OsSettingsChangeDeviceLanguageDialogElement extends
    OsSettingsChangeDeviceLanguageDialogElementBase {
  static get is() {
    return 'os-settings-change-device-language-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      languages: Object,

      displayedLanguages_: {
        type: Array,
        computed: `getPossibleDeviceLanguages_(languages.supported,
            languages.enabled.*, lowercaseQueryString_)`,
      },

      displayedLanguagesEmpty_: {
        type: Boolean,
        computed: 'isZero_(displayedLanguages_.length)',
      },

      languageHelper: Object,

      selectedLanguage_: {
        type: Object,
        value: null,
      },

      disableActionButton_: {
        type: Boolean,
        computed: 'shouldDisableActionButton_(selectedLanguage_)',
      },

      lowercaseQueryString_: {
        type: String,
        value: '',
      },
    };
  }

  // Public API: Downwards data flow.
  languages: LanguagesModel|undefined;
  languageHelper: LanguageHelper;

  // Internal state.
  private lowercaseQueryString_: string;
  private selectedLanguage_: chrome.languageSettingsPrivate.Language|null;

  // Computed properties.
  private disableActionButton_: boolean;
  private displayedLanguages_: chrome.languageSettingsPrivate.Language[];
  private displayedLanguagesEmpty_: boolean;

  private onSearchChanged_(e: CustomEvent<string>): void {
    this.lowercaseQueryString_ = e.detail.toLowerCase();
  }

  private getPossibleDeviceLanguages_():
      chrome.languageSettingsPrivate.Language[] {
    // This assertion of `this.languages` is potentially unsafe and could fail.
    // TODO(b/265553377): Prove that this assertion is safe, or rewrite this to
    // avoid this assertion.
    return this.languages!.supported
        .filter(language => {
          if (!language.supportsUI || language.isProhibitedLanguage ||
              // Safety: We checked that `this.languages` is defined above, and
              // `prospectiveUILanguage` is always define on CrOS.
              language.code === this.languages!.prospectiveUILanguage!) {
            return false;
          }

          return !this.lowercaseQueryString_ ||
              language.displayName.toLowerCase().includes(
                  this.lowercaseQueryString_) ||
              language.nativeDisplayName.toLowerCase().includes(
                  this.lowercaseQueryString_);
        })
        .sort((a, b) => {
          // Sort by native display name so the order of languages is
          // deterministic in case the user selects the wrong language.
          // We need to manually specify a locale in localeCompare for
          // determinism (as changing language may change sort order if a locale
          // is not manually specified).
          return a.nativeDisplayName.localeCompare(b.nativeDisplayName, 'en');
        });
  }

  private getItemClass_(selected: boolean): 'selected'|'' {
    return selected ? 'selected' : '';
  }

  private getAriaLabelForItem_(
      item: chrome.languageSettingsPrivate.Language,
      selected: boolean): string {
    const instruction = selected ? 'selectedDeviceLanguageInstruction' :
                                   'notSelectedDeviceLanguageInstruction';
    return this.i18n(instruction, this.getDisplayText_(item));
  }

  private getDisplayText_(language: chrome.languageSettingsPrivate.Language):
      string {
    let displayText = language.nativeDisplayName;
    // If the local name is different, add it.
    if (language.displayName !== language.nativeDisplayName) {
      displayText += ' - ' + language.displayName;
    }
    return displayText;
  }

  private shouldDisableActionButton_(): boolean {
    return this.selectedLanguage_ === null;
  }

  private onCancelButtonClick_(): void {
    this.$.dialog.close();
  }

  /**
   * Sets device language and restarts device.
   */
  private onActionButtonClick_(): void {
    // Safety: This method is only called as an event listener on the action
    // button, which is only enabled if `disableActionButton_` is false - i.e.
    // `this.selectedLanguage_ !== null`.
    assert(this.selectedLanguage_);
    const languageCode = this.selectedLanguage_.code;
    this.languageHelper.setProspectiveUiLanguage(languageCode);
    // If the language isn't enabled yet, it should be added.
    if (!this.languageHelper.isLanguageEnabled(languageCode)) {
      this.languageHelper.enableLanguage(languageCode);
    }
    // The new language should always be moved to the top, as users get confused
    // that websites are displaying in a different language:
    // https://crbug.com/1330209
    this.languageHelper.moveLanguageToFront(languageCode);
    recordSettingChange(Setting.kChangeDeviceLanguage);
    LanguagesMetricsProxyImpl.getInstance().recordInteraction(
        LanguagesPageInteraction.RESTART);
    LifetimeBrowserProxyImpl.getInstance().signOutAndRestart();
  }

  private onKeydown_(e: KeyboardEvent): void {
    // Close dialog if 'esc' is pressed and the search box is already empty.
    if (e.key === 'Escape' && !this.$.search.getValue().trim()) {
      this.$.dialog.close();
    } else if (e.key !== 'PageDown' && e.key !== 'PageUp') {
      this.$.search.scrollIntoViewIfNeeded();
    }
  }

  private isZero_(num: number): boolean {
    return num === 0;
  }
}

customElements.define(
    OsSettingsChangeDeviceLanguageDialogElement.is,
    OsSettingsChangeDeviceLanguageDialogElement);

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsChangeDeviceLanguageDialogElement.is]:
        OsSettingsChangeDeviceLanguageDialogElement;
  }
}
