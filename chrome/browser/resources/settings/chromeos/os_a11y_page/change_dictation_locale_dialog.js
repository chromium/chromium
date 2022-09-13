// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog holds a Dictation locale selection pane that
 * allows a user to pick their locale for Dictation's speech recognition.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_scrollable_behavior.js';
import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * A locale option for Dictation, including the human-readable name, the
 * locale value (like en-US), whether it works offline, whether the language
 * pack for the locale is installed, and whether it should be highlighted as
 * recommended to the user.
 * @typedef {{
 *   name: string,
 *   value: string,
 *   worksOffline: boolean,
 *   installed: boolean,
 *   recommended: boolean,
 * }}
 */
let DictationLocaleOption;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ChangeDictationLocaleDialogBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class ChangeDictationLocaleDialog extends
    ChangeDictationLocaleDialogBase {
  static get is() {
    return 'os-settings-change-dictation-locale-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Set by the manage OS a11y page, this is the full list of locales
       * available.
       * @type {!Array<!DictationLocaleOption>}
       */
      options: Array,

      /**
       * Preference associated with Dictation locales.
       * @type {!chrome.settingsPrivate.PrefObject}
       */
      pref: Object,

      /** @private {!Array<!DictationLocaleOption>} */
      displayedLocales_: {
        type: Array,
        computed: `getAllDictationLocales_(options, lowercaseQueryString_)`,
      },

      /** @private {!Array<!DictationLocaleOption>} */
      recommendedLocales_: {
        type: Array,
        computed:
            `getRecommendedDictationLocales_(options, lowercaseQueryString_)`,
      },

      /**
       * Whether any locales are displayed.
       * @private
       */
      displayedLocalesEmpty_: {
        type: Boolean,
        computed: 'isZero_(displayedLocales_.length)',
      },

      /**
       * Whether any locales are displayed.
       * @private
       */
      recommendedLocalesEmpty_: {
        type: Boolean,
        computed: 'isZero_(recommendedLocales_.length)',
      },

      /**
       * Whether to enable the button to update the locale pref.
       * @private
       */
      disableUpdateButton_: {
        type: Boolean,
        computed: 'shouldDisableActionButton_(selectedLocale_)',
      },

      /** @private */
      lowercaseQueryString_: {
        type: String,
        value: '',
      },

      /**
       * The currently selected locale from the recommended locales list.
       * @private {?DictationLocaleOption}
       */
      selectedRecommendedLocale_: {
        type: Object,
        value: null,
      },

      /**
       * The currently selected locale from the full locales list.
       * @private {?DictationLocaleOption}
       */
      selectedLocale_: {
        type: Object,
        value: null,
      },
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.addEventListener('exit-pane', () => this.onPaneExit_());
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    // Sets offset in iron-list that uses the body as a scrollTarget.
    afterNextRender(this, function() {
      this.$.allLocalesList.scrollOffset = this.$.allLocalesList.offsetTop;
    });
  }

  /**
   * Gets the list of all recommended Dictation locales based on the current
   * search.
   * @return {!Array<!DictationLocaleOption>}
   * @private
   */
  getRecommendedDictationLocales_() {
    return this.getPossibleDictationLocales_(/*recommendedOnly=*/ true);
  }

  /**
   * Gets the list of all possible Dictation locales based on the current
   * search.
   * @return {!Array<!DictationLocaleOption>}
   */
  getAllDictationLocales_() {
    return this.getPossibleDictationLocales_(/*recommendedOnly=*/ false);
  }

  /**
   * @param {boolean} recommendedOnly Whether to filter only to recommended
   *     locales.
   * @return {!Array<!DictationLocaleOption>} A list of possible dictation
   *     locales based on the current search.
   * @private
   */
  getPossibleDictationLocales_(recommendedOnly) {
    return this.options
        .filter(option => {
          // Filter recommended options. The currently selected option is also
          // recommended.
          if (recommendedOnly &&
              !(this.pref.value === option.value || option.recommended)) {
            return false;
          }
          return !this.lowercaseQueryString_ ||
              option.name.toLowerCase().includes(this.lowercaseQueryString_) ||
              option.value.toLowerCase().includes(this.lowercaseQueryString_);
        })
        .sort((first, second) => {
          return first.name.localeCompare(second.name);
        });
  }

  /**
   * |selectedRecommendedLocale_| is not changed by the time this is called. The
   * value that |selectedRecommendedLocale_| will be assigned to is stored in
   * |this.$.recommendedLocalesList.selectedItem|.
   * @private
   */
  selectedRecommendedLocaleChanged_() {
    const allLocalesSelected = this.$.allLocalesList.selectedItem;
    const recommendedLocalesSelected =
        this.$.recommendedLocalesList.selectedItem;

    // Check for equality before updating to avoid an infinite loop with
    // selectedLocaleChanged_().
    if (allLocalesSelected === recommendedLocalesSelected) {
      return;
    }
    if (recommendedLocalesSelected) {
      this.$.allLocalesList.selectItem(recommendedLocalesSelected);
    } else {
      this.$.allLocalesList.deselectItem(allLocalesSelected);
    }
  }

  /**
   * |selectedLocale_| is not changed by the time this is called. The value that
   * |selectedLocale_| will be assigned to is stored in
   * |this.$.allLocalesList.selectedItem|.
   * @private
   */
  selectedLocaleChanged_() {
    const allLocalesSelected = this.$.allLocalesList.selectedItem;
    const recommendedLocalesSelected =
        this.$.recommendedLocalesList.selectedItem;

    if (allLocalesSelected === recommendedLocalesSelected) {
      return;
    }
    // Check if the locale is also in the recommended list.
    if (allLocalesSelected && allLocalesSelected.recommended) {
      this.$.recommendedLocalesList.selectItem(allLocalesSelected);
    } else if (recommendedLocalesSelected) {
      this.$.recommendedLocalesList.deselectItem(recommendedLocalesSelected);
    }
  }

  /**
   * @param {number} num
   * @return {boolean} Whether num is equal to 0.
   * @private
   */
  isZero_(num) {
    return num === 0;
  }

  /**
   * Disable the action button unless a new locale has been selected.
   * @return {boolean} Whether the "update" action button should be disabled.
   * @private
   */
  shouldDisableActionButton_() {
    return this.selectedLocale_ === null ||
        this.selectedLocale_.value === this.pref.value;
  }

  /**
   * Gets the ARIA label for an item given the online/offline state and selected
   * state, which are also portrayed via icons in the HTML.
   * @param {!DictationLocaleOption} item
   * @param {boolean} selected
   * @return {string} The ARIA label for the item.
   * @private
   */
  getAriaLabelForItem_(item, selected) {
    const longName = item.worksOffline ?
        this.i18n(
            'dictationChangeLanguageDialogOfflineDescription', item.name) :
        item.name;
    const description = selected ?
        'dictationChangeLanguageDialogSelectedDescription' :
        'dictationChangeLanguageDialogNotSelectedDescription';
    return this.i18n(description, longName);
  }

  /**
   * @param {boolean} selected
   * @return {string}
   * @private
   */
  getItemClass_(selected) {
    return selected ? 'selected' : '';
  }

  /**
   * @param {!DictationLocaleOption} item
   * @param {boolean} selected
   * @return {string}
   * @private
   */
  getIconClass_(item, selected) {
    if (this.pref.value === item.value) {
      return 'previous';
    }
    return selected ? 'active' : 'hidden';
  }

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onSearchChanged_(e) {
    this.lowercaseQueryString_ = e.detail.toLowerCase();
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeydown_(e) {
    // Close dialog if 'esc' is pressed and the search box is already empty.
    if (e.key === 'Escape' && !this.$.search.getValue().trim()) {
      this.$.changeDictationLocaleDialog.close();
    } else if (e.key !== 'PageDown' && e.key !== 'PageUp') {
      this.$.search.scrollIntoViewIfNeeded();
    }
  }

  /** @private */
  onPaneExit_() {
    this.$.changeDictationLocaleDialog.close();
  }

  /** @private */
  onCancelClick_() {
    this.$.changeDictationLocaleDialog.close();
  }

  /** @private */
  onUpdateClick_() {
    if (this.selectedLocale_) {
      this.set('pref.value', this.selectedLocale_.value);
    }
    this.$.changeDictationLocaleDialog.close();
  }
}

customElements.define(
    ChangeDictationLocaleDialog.is, ChangeDictationLocaleDialog);
