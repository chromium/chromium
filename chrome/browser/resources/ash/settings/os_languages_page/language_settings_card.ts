// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'language-settings-card' is the card element containing language settings.
 */

import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {PrefsState} from '../common/types.js';
import {Router, routes} from '../router.js';

import {getTemplate} from './language_settings_card.html.js';
import {ACCESSIBILITY_COMMON_IME_ID} from './languages.js';
import {LanguageHelper, LanguagesModel} from './languages_types.js';

const LanguageSettingsCardElementBase =
    RouteOriginMixin(I18nMixin(PolymerElement));

export class LanguageSettingsCardElement extends
    LanguageSettingsCardElementBase {
  static get is() {
    return 'language-settings-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      languages: Object,

      languageHelper: Object,

      isRevampWayfindingEnabled_: Boolean,

      rowIcons_: {
        type: Object,
        value() {
          if (isRevampWayfindingEnabled()) {
            return {
              languages: 'os-settings:language-revamp',
            };
          }
          return {
            languages: '',
          };
        },
      },
    };
  }

  // Public API: Bidirectional data flow.
  /** Passed down to children. Do not access without using PrefsMixin. */
  prefs: PrefsState;

  // Public API: Downwards data flow.
  languages: LanguagesModel|undefined;
  languageHelper: LanguageHelper|undefined;

  // Internal state.
  private isRevampWayfindingEnabled_ = isRevampWayfindingEnabled();
  private rowIcons_: Record<string, string>;

  // Internal properties for mixins.
  // From RouteOriginMixin. This needs to be defined after
  // `isRevampWayfindingEnabled_`.
  override route = this.isRevampWayfindingEnabled_ ? routes.SYSTEM_PREFERENCES :
                                                     routes.OS_LANGUAGES;

  override ready(): void {
    super.ready();

    this.addFocusConfig(routes.OS_LANGUAGES_LANGUAGES, '#languagesRow');
    this.addFocusConfig(routes.OS_LANGUAGES_INPUT, '#inputRow');
  }

  private getHeaderText_(): string {
    if (this.isRevampWayfindingEnabled_) {
      return this.i18n('languagesPageTitle');
    }
    return this.i18n('osLanguagesPageTitle');
  }

  private onLanguagesV2Click_(): void {
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_LANGUAGES);
  }

  private onInputClick_(): void {
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_INPUT);
  }

  /**
   * @param code The language code of the language.
   * @return The display name of the language specified.
   */
  private getLanguageDisplayName_(code: string|undefined): string {
    if (!code || !this.languageHelper) {
      return '';
    }
    const language = this.languageHelper.getLanguage(code);
    if (!language) {
      return '';
    }
    return language.displayName;
  }

  /**
   * @param id The input method ID.
   * @return The display name of the input method.
   */
  private getInputMethodDisplayName_(id: string|undefined): string {
    if (!id || !this.languageHelper) {
      return '';
    }
    if (id === ACCESSIBILITY_COMMON_IME_ID) {
      return '';
    }
    return this.languageHelper.getInputMethodDisplayName(id);
  }
}

customElements.define(
    LanguageSettingsCardElement.is, LanguageSettingsCardElement);

declare global {
  interface HTMLElementTagNameMap {
    [LanguageSettingsCardElement.is]: LanguageSettingsCardElement;
  }
}
