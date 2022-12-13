// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-change-device-language-dialog' is a dialog for
 * changing device language.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import './shared_style.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import './languages.js';
import '../../settings_shared.css.js';

import {CrScrollableBehavior, CrScrollableBehaviorInterface} from 'chrome://resources/ash/common/cr_scrollable_behavior.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LifetimeBrowserProxyImpl} from '../../lifetime_browser_proxy.js';
import {recordSettingChange} from '../metrics_recorder.js';

import {getTemplate} from './change_device_language_dialog.html.js';
import {LanguagesMetricsProxy, LanguagesMetricsProxyImpl, LanguagesPageInteraction} from './languages_metrics_proxy.js';
import {LanguageHelper, LanguagesModel} from './languages_types.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrScrollableBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 */
const OsSettingsChangeDeviceLanguageDialogElementBase =
    mixinBehaviors([CrScrollableBehavior, I18nBehavior], PolymerElement);

/** @polymer */
class OsSettingsChangeDeviceLanguageDialogElement extends
    OsSettingsChangeDeviceLanguageDialogElementBase {
  static get is() {
    return 'os-settings-change-device-language-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {!LanguagesModel|undefined} */
      languages: Object,

      /** @private {!Array<!chrome.languageSettingsPrivate.Language>} */
      displayedLanguages_: {
        type: Array,
        computed: `getPossibleDeviceLanguages_(languages.supported,
            languages.enabled.*, lowercaseQueryString_)`,
      },

      /** @private {boolean} */
      displayedLanguagesEmpty_: {
        type: Boolean,
        computed: 'isZero_(displayedLanguages_.length)',
      },

      /** @type {!LanguageHelper} */
      languageHelper: Object,

      /** @private {?chrome.languageSettingsPrivate.Language} */
      selectedLanguage_: {
        type: Object,
        value: null,
      },

      /** @private */
      disableActionButton_: {
        type: Boolean,
        computed: 'shouldDisableActionButton_(selectedLanguage_)',
      },

      /** @private */
      lowercaseQueryString_: {
        type: String,
        value: '',
      },
    };
  }

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onSearchChanged_(e) {
    this.lowercaseQueryString_ = e.detail.toLowerCase();
  }

  /**
   * @return {!Array<!chrome.languageSettingsPrivate.Language>} A list of
   *     possible device language.
   * @private
   */
  getPossibleDeviceLanguages_() {
    return this.languages.supported
        .filter(language => {
          if (!language.supportsUI || language.isProhibitedLanguage ||
              language.code === this.languages.prospectiveUILanguage) {
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

  /**
   * @param {boolean} selected
   * @private
   */
  getItemClass_(selected) {
    return selected ? 'selected' : '';
  }

  /**
   * @param {!chrome.languageSettingsPrivate.Language} item
   * @param {boolean} selected
   * @return {!string}
   * @private
   */
  getAriaLabelForItem_(item, selected) {
    const instruction = selected ? 'selectedDeviceLanguageInstruction' :
                                   'notSelectedDeviceLanguageInstruction';
    return this.i18n(instruction, this.getDisplayText_(item));
  }

  /**
   * @param {!chrome.languageSettingsPrivate.Language} language
   * @return {string} The text to be displayed.
   * @private
   */
  getDisplayText_(language) {
    let displayText = language.nativeDisplayName;
    // If the local name is different, add it.
    if (language.displayName !== language.nativeDisplayName) {
      displayText += ' - ' + language.displayName;
    }
    return displayText;
  }

  /** @private */
  shouldDisableActionButton_() {
    return this.selectedLanguage_ === null;
  }

  /** @private */
  onCancelButtonTap_() {
    this.$.dialog.close();
  }

  /**
   * Sets device language and restarts device.
   * @private
   */
  onActionButtonTap_() {
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
    recordSettingChange();
    LanguagesMetricsProxyImpl.getInstance().recordInteraction(
        LanguagesPageInteraction.RESTART);
    LifetimeBrowserProxyImpl.getInstance().signOutAndRestart();
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeydown_(e) {
    // Close dialog if 'esc' is pressed and the search box is already empty.
    if (e.key === 'Escape' && !this.$.search.getValue().trim()) {
      this.$.dialog.close();
    } else if (e.key !== 'PageDown' && e.key !== 'PageUp') {
      this.$.search.scrollIntoViewIfNeeded();
    }
  }

  /**
   * @param {number} num
   * @return {boolean}
   * @private
   */
  isZero_(num) {
    return num === 0;
  }
}

customElements.define(
    OsSettingsChangeDeviceLanguageDialogElement.is,
    OsSettingsChangeDeviceLanguageDialogElement);
