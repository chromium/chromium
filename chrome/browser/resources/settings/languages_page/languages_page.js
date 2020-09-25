// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-languages-page' is the settings page
 * for language and input method settings.
 */
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './add_languages_dialog.js';
import './languages.m.js';
import '../controls/controlled_radio_button.m.js';
import '../controls/settings_radio_group.m.js';
import '../controls/settings_toggle_button.m.js';
import '../icons.m.js';
import '../settings_page/settings_animated_pages.m.js';
import '../settings_page/settings_subpage.m.js';
import '../settings_shared_css.m.js';
import '../settings_vars_css.m.js';

// <if expr="not is_macosx">
import './edit_dictionary_page.js';
// </if>

import {assert} from 'chrome://resources/js/assert.m.js';
import {isChromeOS, isWindows} from 'chrome://resources/js/cr.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {flush, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {LifetimeBrowserProxyImpl} from '../lifetime_browser_proxy.m.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.m.js';
import {routes} from '../route.js';
import {Route, Router} from '../router.m.js';

// <if expr="chromeos">
import {LanguagesMetricsProxy, LanguagesMetricsProxyImpl, LanguagesPageInteraction} from './languages_metrics_proxy.js';
// </if>

/**
 * @type {number} Millisecond delay that can be used when closing an action
 *      menu to keep it briefly on-screen.
 */
export const kMenuCloseDelay = 100;

/**
 * Name of the language setting is shown uma histogram.
 * @type {string}
 */
const LANGUAGE_SETTING_IS_SHOWN_UMA_NAME = 'Translate.LanguageSettingsIsShown';

Polymer({
  is: 'settings-languages-page',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Read-only reference to the languages model provided by the
     * 'settings-languages' instance.
     * @type {!LanguagesModel|undefined}
     */
    languages: {
      type: Object,
      notify: true,
    },

    /** @type {!LanguageHelper} */
    languageHelper: Object,

    // <if expr="not is_macosx">
    /** @private */
    spellCheckLanguages_: {
      type: Array,
      value() {
        return [];
      },
    },
    // </if>

    /**
     * The language to display the details for.
     * @type {!LanguageState|undefined}
     * @private
     */
    detailLanguage_: Object,

    /** @private */
    hideSpellCheckLanguages_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the language settings list is opened.
     * @private
     */
    languagesOpened_: {
      type: Boolean,
      observer: 'onLanguagesOpenedChanged_',
    },

    /** @private */
    showAddLanguagesDialog_: Boolean,

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        // <if expr="not is_macosx">
        if (routes.EDIT_DICTIONARY) {
          map.set(routes.EDIT_DICTIONARY.path, '#spellCheckSubpageTrigger');
        }
        // </if>
        return map;
      },
    },

    // <if expr="chromeos">
    /** @private */
    isGuest_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isGuest');
      },
    },

    /** @private */
    isChromeOSLanguagesSettingsUpdate_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isChromeOSLanguagesSettingsUpdate');
      },
    },
    // </if>
  },

  // <if expr="chromeos">
  /** @private {?LanguagesMetricsProxy} */
  languagesMetricsProxy_: null,

  /** @override */
  created() {
    this.languagesMetricsProxy_ = LanguagesMetricsProxyImpl.getInstance();
  },

  /** @private */
  onOpenChromeOSLanguagesSettingsClick_() {
    const chromeOSLanguagesSettingsPath =
        loadTimeData.getString('chromeOSLanguagesSettingsPath');
    window.location.href =
        `chrome://os-settings/${chromeOSLanguagesSettingsPath}`;
  },
  // </if>

  // <if expr="not is_macosx">
  observers: [
    'updateSpellcheckLanguages_(languages.enabled.*, ' +
        'languages.forcedSpellCheckLanguages.*)',
    'updateSpellcheckEnabled_(prefs.browser.enable_spellchecking.*)',
  ],
  // </if>

  // <if expr="chromeos or is_win">
  /** @private {boolean} */
  isChangeInProgress_: false,
  // </if>

  // <if expr="not is_macosx">
  /**
   * Checks if there are any errors downloading the spell check dictionary.
   * This is used for showing/hiding error messages, spell check toggle and
   * retry. button.
   * @param {number} downloadDictionaryFailureCount
   * @param {number} threshold
   * @return {boolean}
   * @private
   */
  errorsGreaterThan_(downloadDictionaryFailureCount, threshold) {
    return downloadDictionaryFailureCount > threshold;
  },
  // </if>

  /**
   * Stamps and opens the Add Languages dialog, registering a listener to
   * disable the dialog's dom-if again on close.
   * @param {!Event} e
   * @private
   */
  onAddLanguagesTap_(e) {
    e.preventDefault();
    // <if expr="chromeos">
    this.languagesMetricsProxy_.recordAddLanguages();
    // </if>
    this.showAddLanguagesDialog_ = true;
  },

  /** @private */
  onAddLanguagesDialogClose_() {
    this.showAddLanguagesDialog_ = false;
    focusWithoutInk(assert(this.$$('#addLanguages')));
  },

  /**
   * Checks if there are supported languages that are not enabled but can be
   * enabled.
   * @param {LanguagesModel|undefined} languages
   * @return {boolean} True if there is at least one available language.
   * @private
   */
  canEnableSomeSupportedLanguage_(languages) {
    return languages === undefined || languages.supported.some(language => {
      return this.languageHelper.canEnableLanguage(language);
    });
  },

  /**
   * Used to determine whether to show the separator between checkbox settings
   * and move buttons in the dialog menu.
   * @return {boolean} True if there is currently more than one selected
   *     language.
   * @private
   */
  shouldShowDialogSeparator_() {
    // <if expr="chromeos">
    if (this.isGuest_) {
      return false;
    }
    // </if>
    return this.languages !== undefined && this.languages.enabled.length > 1;
  },

  /**
   * Used to determine which "Move" buttons to show for ordering enabled
   * languages.
   * @param {number} n
   * @return {boolean} True if |language| is at the |n|th index in the list of
   *     enabled languages.
   * @private
   */
  isNthLanguage_(n) {
    if (this.languages === undefined || this.detailLanguage_ === undefined) {
      return false;
    }

    if (n >= this.languages.enabled.length) {
      return false;
    }

    const compareLanguage = assert(this.languages.enabled[n]);
    return this.detailLanguage_.language === compareLanguage.language;
  },

  /**
   * @return {boolean} True if the "Move to top" option for |language| should
   *     be visible.
   * @private
   */
  showMoveUp_() {
    // "Move up" is a no-op for the top language, and redundant with
    // "Move to top" for the 2nd language.
    return !this.isNthLanguage_(0) && !this.isNthLanguage_(1);
  },

  /**
   * @return {boolean} True if the "Move down" option for |language| should be
   *     visible.
   * @private
   */
  showMoveDown_() {
    return this.languages !== undefined &&
        !this.isNthLanguage_(this.languages.enabled.length - 1);
  },

  /**
   * @param {!Object} change Polymer change object for languages.enabled.*.
   * @return {boolean} True if there are less than 2 languages.
   */
  isHelpTextHidden_(change) {
    return this.languages !== undefined && this.languages.enabled.length <= 1;
  },

  /**
   * @param {string} languageCode The language code identifying a language.
   * @param {string} translateTarget The target language.
   * @return {string} 'target' if |languageCode| matches the target language,
   'non-target' otherwise.
   */
  isTranslationTarget_(languageCode, translateTarget) {
    if (this.languageHelper.convertLanguageCodeForTranslate(languageCode) ===
        translateTarget) {
      return 'target';
    } else {
      return 'non-target';
    }
  },

  // <if expr="chromeos">
  /**
   * Applies Chrome OS session tweaks to the menu.
   * @param {!CrActionMenuElement} menu
   * @private
   */
  tweakMenuForCrOS_(menu) {
    // In a CrOS multi-user session, the primary user controls the UI
    // language.
    // TODO(michaelpg): The language selection should not be hidden, but
    // should show a policy indicator. crbug.com/648498
    if (this.isSecondaryUser_()) {
      menu.querySelector('#uiLanguageItem').hidden = true;
    }

    // The UI language choice doesn't persist for guests.
    if (this.isGuest_) {
      menu.querySelector('#uiLanguageItem').hidden = true;
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  onTranslateToggleChange_(e) {
    this.languagesMetricsProxy_.recordToggleTranslate(e.target.checked);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onSpellcheckToggleChange_(e) {
    this.languagesMetricsProxy_.recordToggleSpellCheck(e.target.checked);
  },
  // </if>

  // <if expr="chromeos or is_win">
  /**
   * @return {boolean} True for a secondary user in a multi-profile session.
   * @private
   */
  isSecondaryUser_() {
    return isChromeOS && loadTimeData.getBoolean('isSecondaryUser');
  },

  /**
   * @param {string} languageCode The language code identifying a language.
   * @param {string} prospectiveUILanguage The prospective UI language.
   * @return {boolean} True if the prospective UI language is set to
   *     |languageCode| but requires a restart to take effect.
   * @private
   */
  isRestartRequired_(languageCode, prospectiveUILanguage) {
    return prospectiveUILanguage === languageCode &&
        this.languageHelper.requiresRestart();
  },

  /** @private */
  onCloseMenu_() {
    if (!this.isChangeInProgress_) {
      return;
    }
    flush();
    this.isChangeInProgress_ = false;
    const restartButton = this.$$('#restartButton');
    if (!restartButton) {
      return;
    }
    focusWithoutInk(restartButton);
  },

  /**
   * @param {!LanguageState} languageState
   * @param {string} prospectiveUILanguage The chosen UI language.
   * @return {boolean} True if the given language cannot be set as the
   *     prospective UI language by the user.
   * @private
   */
  disableUILanguageCheckbox_(languageState, prospectiveUILanguage) {
    if (this.detailLanguage_ === undefined) {
      return true;
    }

    // UI language setting belongs to the primary user.
    if (this.isSecondaryUser_()) {
      return true;
    }

    // If the language cannot be a UI language, we can't set it as the
    // prospective UI language.
    if (!languageState.language.supportsUI) {
      return true;
    }

    // Unchecking the currently chosen language doesn't make much sense.
    if (languageState.language.code === prospectiveUILanguage) {
      return true;
    }

    // Check if the language is prohibited by the current "AllowedLanguages"
    // policy.
    if (languageState.language.isProhibitedLanguage) {
      return true;
    }

    // Otherwise, the prospective language can be changed to this language.
    return false;
  },

  /**
   * Handler for changes to the UI language checkbox.
   * @param {!{target: !Element}} e
   * @private
   */
  onUILanguageChange_(e) {
    // We don't support unchecking this checkbox. TODO(michaelpg): Ask for a
    // simpler widget.
    assert(e.target.checked);
    // <if expr="chromeos">
    this.languagesMetricsProxy_.recordInteraction(
        LanguagesPageInteraction.SWITCH_SYSTEM_LANGUAGE);
    // </if>
    this.isChangeInProgress_ = true;
    this.languageHelper.setProspectiveUILanguage(
        this.detailLanguage_.language.code);
    this.languageHelper.moveLanguageToFront(this.detailLanguage_.language.code);

    this.closeMenuSoon_();
  },

  /**
   * Checks whether the prospective UI language (the pref that indicates what
   * language to use in Chrome) matches the current language. This pref is
   * used only on Chrome OS and Windows; we don't control the UI language
   * elsewhere.
   * @param {string} languageCode The language code identifying a language.
   * @param {string} prospectiveUILanguage The prospective UI language.
   * @return {boolean} True if the given language matches the prospective UI
   *     pref (which may be different from the actual UI language).
   * @private
   */
  isProspectiveUILanguage_(languageCode, prospectiveUILanguage) {
    return languageCode === prospectiveUILanguage;
  },

  /**
   * @param {string} prospectiveUILanguage
   * @return {string}
   * @private
   */
  getProspectiveUILanguageName_(prospectiveUILanguage) {
    return this.languageHelper.getLanguage(prospectiveUILanguage).displayName;
  },

  /**
   * Handler for the restart button.
   * @private
   */
  onRestartTap_() {
    // <if expr="chromeos">
    this.languagesMetricsProxy_.recordInteraction(
        LanguagesPageInteraction.RESTART);
    LifetimeBrowserProxyImpl.getInstance().signOutAndRestart();
    // </if>
    // <if expr="is_win">
    LifetimeBrowserProxyImpl.getInstance().restart();
    // </if>
  },
  // </if>

  /**
   * @param {!LanguageState|undefined} languageState
   * @param {string} targetLanguageCode The default translate target language.
   * @return {boolean} True if the translate checkbox should be disabled.
   * @private
   */
  disableTranslateCheckbox_(languageState, targetLanguageCode) {
    if (languageState === undefined || languageState.language === undefined ||
        !languageState.language.supportsTranslate) {
      return true;
    }

    if (this.languageHelper.isOnlyTranslateBlockedLanguage(languageState)) {
      return true;
    }

    return this.languageHelper.convertLanguageCodeForTranslate(
               languageState.language.code) === targetLanguageCode;
  },

  /**
   * Handler for changes to the translate checkbox.
   * @param {!{target: !Element}} e
   * @private
   */
  onTranslateCheckboxChange_(e) {
    if (e.target.checked) {
      this.languageHelper.enableTranslateLanguage(
          this.detailLanguage_.language.code);
    } else {
      this.languageHelper.disableTranslateLanguage(
          this.detailLanguage_.language.code);
    }
    // <if expr="chromeos">
    this.languagesMetricsProxy_.recordTranslateCheckboxChanged(
        e.target.checked);
    // </if>
    this.closeMenuSoon_();
  },

  /**
   * Returns "complex" if the menu includes checkboxes, which should change
   * the spacing of items and show a separator in the menu.
   * @param {boolean} translateEnabled
   * @return {string}
   */
  getMenuClass_(translateEnabled) {
    if (translateEnabled || isChromeOS || isWindows) {
      return 'complex';
    }
    return '';
  },

  /**
   * Moves the language to the top of the list.
   * @private
   */
  onMoveToTopTap_() {
    /** @type {!CrActionMenuElement} */ (this.$$('#menu').get()).close();
    this.languageHelper.moveLanguageToFront(this.detailLanguage_.language.code);
  },

  /**
   * Moves the language up in the list.
   * @private
   */
  onMoveUpTap_() {
    /** @type {!CrActionMenuElement} */ (this.$$('#menu').get()).close();
    this.languageHelper.moveLanguage(
        this.detailLanguage_.language.code, true /* upDirection */);
  },

  /**
   * Moves the language down in the list.
   * @private
   */
  onMoveDownTap_() {
    /** @type {!CrActionMenuElement} */ (this.$$('#menu').get()).close();
    this.languageHelper.moveLanguage(
        this.detailLanguage_.language.code, false /* upDirection */);
  },

  /**
   * Disables the language.
   * @private
   */
  onRemoveLanguageTap_() {
    /** @type {!CrActionMenuElement} */ (this.$$('#menu').get()).close();
    this.languageHelper.disableLanguage(this.detailLanguage_.language.code);
  },

  // <if expr="not is_macosx">
  /**
   * Returns the value to use as the |pref| attribute for the policy indicator
   * of spellcheck languages, based on whether or not the language is enabled.
   * @param {boolean} isEnabled Whether the language is enabled or not.
   */
  getIndicatorPrefForManagedSpellcheckLanguage_(isEnabled) {
    return isEnabled ?
        this.get('spellcheck.forced_dictionaries', this.prefs) :
        this.get('spellcheck.blacklisted_dictionaries', this.prefs);
  },

  /**
   * Returns an array of enabled languages, plus spellcheck languages that are
   * force-enabled by policy.
   * @return {!Array<!LanguageState|!ForcedLanguageState>}
   * @private
   */
  getSpellCheckLanguages_() {
    const supportedSpellcheckLanguages =
        /** @type {!Array<!LanguageState|!ForcedLanguageState>} */ (
            this.languages.enabled.filter(
                (item) => item.language.supportsSpellcheck));
    const supportedSpellcheckLanguagesSet =
        new Set(supportedSpellcheckLanguages.map(x => x.language.code));

    this.languages.forcedSpellCheckLanguages.forEach(forcedLanguage => {
      if (!supportedSpellcheckLanguagesSet.has(forcedLanguage.language.code)) {
        supportedSpellcheckLanguages.push(forcedLanguage);
      }
    });

    return supportedSpellcheckLanguages;
  },

  /** @private */
  updateSpellcheckLanguages_() {
    if (this.languages === undefined) {
      return;
    }

    this.set('spellCheckLanguages_', this.getSpellCheckLanguages_());

    // Notify Polymer of subproperties that might have changed on the items in
    // the spellCheckLanguages_ array, to make sure the UI updates. Polymer
    // would otherwise not notice the changes in the subproperties, as some of
    // them are references to those from |this.languages.enabled|. It would be
    // possible to |this.linkPaths()| objects from |this.languages.enabled| to
    // |this.spellCheckLanguages_|, but that would require complex
    // housekeeping to |this.unlinkPaths()| as |this.languages.enabled|
    // changes.
    for (let i = 0; i < this.spellCheckLanguages_.length; i++) {
      this.notifyPath(`spellCheckLanguages_.${i}.isManaged`);
      this.notifyPath(`spellCheckLanguages_.${i}.spellCheckEnabled`);
      this.notifyPath(
          `spellCheckLanguages_.${i}.downloadDictionaryFailureCount`);
    }

    if (this.spellCheckLanguages_.length === 0) {
      // If there are no supported spell check languages, automatically turn
      // off spell check to indicate no spell check will happen.
      this.setPrefValue('browser.enable_spellchecking', false);
    }

    if (this.spellCheckLanguages_.length === 1) {
      const singleLanguage = this.spellCheckLanguages_[0];

      // Hide list of spell check languages if there is only 1 language
      // and we don't need to display any errors for that language

      // TODO(crbug/1124888): Make hideSpellCheckLanugages_ a computed property
      this.hideSpellCheckLanguages_ = !singleLanguage.isManaged &&
          singleLanguage.downloadDictionaryFailureCount === 0;
    } else {
      this.hideSpellCheckLanguages_ = false;
    }
  },

  /** @private */
  updateSpellcheckEnabled_() {
    if (this.prefs === undefined) {
      return;
    }

    // If there is only 1 language, we hide the list of languages so users
    // are unable to toggle on/off spell check specifically for the 1
    // language. Therefore, we need to treat the toggle for
    // `browser.enable_spellchecking` as the toggle for the 1 language as
    // well.
    if (this.spellCheckLanguages_.length === 1) {
      this.languageHelper.toggleSpellCheck(
          this.spellCheckLanguages_[0].language.code,
          !!this.getPref('browser.enable_spellchecking').value);
    }
  },

  /**
   * Opens the Custom Dictionary page.
   * @private
   */
  onEditDictionaryTap_() {
    // <if expr="chromeos">
    this.languagesMetricsProxy_.recordInteraction(
        LanguagesPageInteraction.OPEN_CUSTOM_SPELL_CHECK);
    // </if>
    Router.getInstance().navigateTo(
        /** @type {!Route} */ (routes.EDIT_DICTIONARY));
  },

  /**
   * Handler for enabling or disabling spell check for a specific language.
   * @param {!{target: Element, model: !{item: !LanguageState}}} e
   */
  onSpellCheckLanguageChange_(e) {
    const item = e.model.item;
    if (!item.language.supportsSpellcheck) {
      return;
    }

    this.languageHelper.toggleSpellCheck(
        item.language.code, !item.spellCheckEnabled);
  },

  /**
   * Handler to initiate another attempt at downloading the spell check
   * dictionary for a specified language.
   * @param {!{target: Element, model: !{item: !LanguageState}}} e
   */
  onRetryDictionaryDownloadClick_(e) {
    assert(this.errorsGreaterThan_(
        e.model.item.downloadDictionaryFailureCount, 0));
    this.languageHelper.retryDownloadDictionary(e.model.item.language.code);
  },

  /**
   * Handler for clicking on the name of the language. The action taken must
   * match the control that is available.
   * @param {!{target: Element, model: !{item: !LanguageState}}} e
   */
  onSpellCheckNameClick_(e) {
    assert(!this.isSpellCheckNameClickDisabled_(e.model.item));
    this.onSpellCheckLanguageChange_(e);
  },

  /**
   * Name only supports clicking when language is not managed, supports
   * spellcheck, and the dictionary has been downloaded with no errors.
   * @param {!LanguageState|!ForcedLanguageState} item
   * @return {boolean}
   * @private
   */
  isSpellCheckNameClickDisabled_(item) {
    return item.isManaged || !item.language.supportsSpellcheck ||
        item.downloadDictionaryFailureCount > 0;
  },
  // </if>

  /**
   * Returns either the "selected" class, if the language matches the
   * prospective UI language, or an empty string. Languages can only be
   * selected on Chrome OS and Windows.
   * @param {string} languageCode The language code identifying a language.
   * @param {string} prospectiveUILanguage The prospective UI language.
   * @return {string} The class name for the language item.
   * @private
   */
  getLanguageItemClass_(languageCode, prospectiveUILanguage) {
    if ((isChromeOS || isWindows) && languageCode === prospectiveUILanguage) {
      return 'selected';
    }
    return '';
  },

  /**
   * @return {string|undefined}
   * @private
   */
  getSpellCheckSubLabel_() {
    // <if expr="not is_macosx">
    if (this.spellCheckLanguages_.length === 0) {
      return this.i18n('spellCheckDisabledReason');
    }
    // </if>

    return undefined;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onDotsTap_(e) {
    // Set a copy of the LanguageState object since it is not data-bound to
    // the languages model directly.
    this.detailLanguage_ = /** @type {!LanguageState} */ (Object.assign(
        {},
        /** @type {!{model: !{item: !LanguageState}}} */ (e).model.item));

    // Ensure the template has been stamped.
    let menu =
        /** @type {?CrActionMenuElement} */ (this.$$('#menu').getIfExists());
    if (!menu) {
      menu = /** @type {!CrActionMenuElement} */ (this.$$('#menu').get());
      // <if expr="chromeos">
      this.tweakMenuForCrOS_(menu);
      // </if>
    }

    menu.showAt(/** @type {!Element} */ (e.target));
  },

  /**
   * @param {boolean} newVal The new value of languagesOpened_.
   * @param {boolean} oldVal The old value of languagesOpened_.
   * @private
   */
  onLanguagesOpenedChanged_(newVal, oldVal) {
    if (!oldVal && newVal) {
      chrome.send(
          'metricsHandler:recordBooleanHistogram',
          [LANGUAGE_SETTING_IS_SHOWN_UMA_NAME, true]);
    }
  },

  /**
   * Closes the shared action menu after a short delay, so when a checkbox is
   * clicked it can be seen to change state before disappearing.
   * @private
   */
  closeMenuSoon_() {
    const menu = /** @type {!CrActionMenuElement} */ (this.$$('#menu').get());
    setTimeout(function() {
      if (menu.open) {
        menu.close();
      }
    }, kMenuCloseDelay);
  },

  /**
   * Toggles the expand button within the element being listened to.
   * @param {!Event} e
   * @private
   */
  toggleExpandButton_(e) {
    // The expand button handles toggling itself.
    const expandButtonTag = 'CR-EXPAND-BUTTON';
    if (e.target.tagName === expandButtonTag) {
      return;
    }

    if (!e.currentTarget.hasAttribute('actionable')) {
      return;
    }

    /** @type {!CrExpandButtonElement} */
    const expandButton = e.currentTarget.querySelector(expandButtonTag);
    assert(expandButton);
    expandButton.expanded = !expandButton.expanded;
    focusWithoutInk(expandButton);
  },
});
