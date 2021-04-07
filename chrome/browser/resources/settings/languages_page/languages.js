// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-languages' handles Chrome's language and input
 * method settings. The 'languages' property, which reflects the current
 * language settings, must not be changed directly. Instead, changes to
 * language settings should be made using the LanguageHelper APIs provided by
 * this class via languageHelper.
 */

import '../prefs/prefs.js';

import {assert} from '//resources/js/assert.m.js';
import {isChromeOS, isWindows} from '//resources/js/cr.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {PromiseResolver} from '//resources/js/promise_resolver.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsBehavior} from '../prefs/prefs_behavior.js';
import {CrSettingsPrefs} from '../prefs/prefs_types.js';

import {LanguagesBrowserProxy, LanguagesBrowserProxyImpl} from './languages_browser_proxy.js';


const MoveType = chrome.languageSettingsPrivate.MoveType;

// Translate server treats some language codes the same.
// See also: components/translate/core/common/translate_util.cc.
const kLanguageCodeToTranslateCode = {
  'nb': 'no',
  'fil': 'tl',
  'zh-HK': 'zh-TW',
  'zh-MO': 'zh-TW',
  'zh-SG': 'zh-CN',
};

// Some ISO 639 language codes have been renamed, e.g. "he" to "iw", but
// Translate still uses the old versions. TODO(michaelpg): Chrome does too.
// Follow up with Translate owners to understand the right thing to do.
const kTranslateLanguageSynonyms = {
  'he': 'iw',
  'jv': 'jw',
};

// The fake language name used for ARC IMEs. The value must be in sync with the
// one in ui/base/ime/chromeos/extension_ime_util.h.
const kArcImeLanguage = '_arc_ime_language_';

const preferredLanguagesPrefName = isChromeOS ?
    'settings.language.preferred_languages' :
    'intl.accept_languages';

/**
 * @typedef {{
 *   initialized: boolean,
 *   supportedLanguages: !Array<!chrome.languageSettingsPrivate.Language>,
 *   translateTarget: string,
 *   alwaysTranslateCodes: !Array<string>,
 *   neverTranslateCodes: !Array<string>,
 *   startingUILanguage: string,
 *   supportedInputMethods:
 * (!Array<!chrome.languageSettingsPrivate.InputMethod>|undefined),
 *   currentInputMethodId: (string|undefined)
 * }}
 */
let ModelArgs;

/**
 * Singleton element that generates the languages model on start-up and
 * updates it whenever Chrome's pref store and other settings change.
 * @implements {LanguageHelper}
 */
Polymer({
  is: 'settings-languages',

  _template: null,

  behaviors: [PrefsBehavior],

  properties: {
    /**
     * @type {!LanguagesModel|undefined}
     */
    languages: {
      type: Object,
      notify: true,
      readOnly: true,
    },

    /**
     * This element, as a LanguageHelper instance for API usage.
     * @type {!LanguageHelper}
     */
    languageHelper: {
      type: Object,
      notify: true,
      readOnly: true,
      value() {
        return /** @type {!LanguageHelper} */ (this);
      },
    },

    /**
     * PromiseResolver to be resolved when the singleton has been initialized.
     * @private {!PromiseResolver}
     */
    resolver_: {
      type: Object,
      value() {
        return new PromiseResolver();
      },
    },

    /**
     * Hash map of supported languages by language codes for fast lookup.
     * @private {!Map<string, !chrome.languageSettingsPrivate.Language>}
     */
    supportedLanguageMap_: {
      type: Object,
      value() {
        return new Map();
      },
    },

    /**
     * Hash set of enabled language codes for membership testing.
     * @private {!Set<string>}
     */
    enabledLanguageSet_: {
      type: Object,
      value() {
        return new Set();
      },
    },

    // <if expr="chromeos">
    /**
     * Hash map of supported input methods by ID for fast lookup.
     * @private {!Map<string, chrome.languageSettingsPrivate.InputMethod>}
     */
    supportedInputMethodMap_: {
      type: Object,
      value() {
        return new Map();
      },
    },

    /**
     * Hash map of input methods supported for each language.
     * @type {!Map<string,
     *             !Array<!chrome.languageSettingsPrivate.InputMethod>>}
     * @private
     */
    languageInputMethods_: {
      type: Object,
      value() {
        return new Map();
      },
    },

    /**
     * Hash set of enabled input methods id for mebership testings
     * @private {!Set<string>}
     */
    enabledInputMethodSet_: {
      type: Object,
      value() {
        return new Set();
      }
    },
    // </if>

    /** @private Prospective UI language when the page was loaded. */
    originalProspectiveUILanguage_: String,
  },

  observers: [
    // All observers wait for the model to be populated by including the
    // |languages| property.
    'prospectiveUILanguageChanged_(prefs.intl.app_locale.value, languages)',
    'preferredLanguagesPrefChanged_(' +
        'prefs.' + preferredLanguagesPrefName + '.value, languages)',
    'preferredLanguagesPrefChanged_(' +
        'prefs.intl.forced_languages.value.*, languages)',
    'spellCheckDictionariesPrefChanged_(' +
        'prefs.spellcheck.dictionaries.value.*, ' +
        'prefs.spellcheck.forced_dictionaries.value.*, ' +
        'prefs.spellcheck.blocked_dictionaries.value.*, languages)',
    'translateLanguagesPrefChanged_(' +
        'prefs.translate_blocked_languages.value.*, languages)',
    'translateTargetPrefChanged_(' +
        'prefs.translate_recent_target.value, languages)',
    'updateRemovableLanguages_(' +
        'prefs.intl.app_locale.value, languages.enabled)',
    'updateRemovableLanguages_(' +
        'prefs.translate_blocked_languages.value.*)',
    // Observe Chrome OS prefs (ignored for non-Chrome OS).
    'updateRemovableLanguages_(' +
        'prefs.settings.language.preload_engines.value, ' +
        'prefs.settings.language.enabled_extension_imes.value, ' +
        'languages)',
  ],

  /** @private {?Function} */
  boundOnInputMethodChanged_: null,

  // <if expr="not is_macosx">
  /** @private {?Function} */
  boundOnSpellcheckDictionariesChanged_: null,
  // </if>

  /** @private {?LanguagesBrowserProxy} */
  browserProxy_: null,

  /** @private {?LanguageSettingsPrivate} */
  languageSettingsPrivate_: null,

  // <if expr="chromeos">
  /** @private {?InputMethodPrivate} */
  inputMethodPrivate_: null,

  /** @private {?Function} */
  boundOnInputMethodAdded_: null,

  /** @private {?Function} */
  boundOnInputMethodRemoved_: null,
  // </if>

  /** @override */
  attached() {
    this.browserProxy_ = LanguagesBrowserProxyImpl.getInstance();
    this.languageSettingsPrivate_ =
        this.browserProxy_.getLanguageSettingsPrivate();
    // <if expr="chromeos">
    this.inputMethodPrivate_ = this.browserProxy_.getInputMethodPrivate();
    // </if>

    const promises = [];

    /**
     * An object passed into createModel to keep track of platform-specific
     * arguments, populated by the "promises" array.
     * @type {!ModelArgs}
     */
    const args = {
      initialized: false,
      supportedLanguages: [],
      translateTarget: '',
      alwaysTranslateCodes: [],
      neverTranslateCodes: [],
      startingUILanguage: '',

      // Only used by ChromeOS
      supportedInputMethods: [],
      currentInputMethodId: '',
    };

    // Wait until prefs are initialized before creating the model, so we can
    // include information about enabled languages.
    promises.push(
        CrSettingsPrefs.initialized.then(result => args.initialized = result));

    // Get the language list.
    promises.push(new Promise(resolve => {
                    this.languageSettingsPrivate_.getLanguageList(resolve);
                  }).then(result => args.supportedLanguages = result));

    // Get the translate target language.
    promises.push(new Promise(resolve => {
                    this.languageSettingsPrivate_.getTranslateTargetLanguage(
                        resolve);
                  }).then(result => args.translateTarget = result));

    if (isChromeOS) {
      promises.push(
          new Promise(resolve => {
            this.languageSettingsPrivate_.getInputMethodLists(function(lists) {
              resolve(lists.componentExtensionImes.concat(
                  lists.thirdPartyExtensionImes));
            });
          }).then(result => args.supportedInputMethods = result));

      promises.push(new Promise(resolve => {
                      this.inputMethodPrivate_.getCurrentInputMethod(resolve);
                    }).then(result => args.currentInputMethodId = result));
    }

    // Get the list of language-codes to always translate.
    promises.push(new Promise(resolve => {
                    this.languageSettingsPrivate_.getAlwaysTranslateLanguages(
                        resolve);
                  }).then(result => args.alwaysTranslateCodes = result));

    // Get the list of language-codes to never translate.
    promises.push(new Promise(resolve => {
                    this.languageSettingsPrivate_.getNeverTranslateLanguages(
                        resolve);
                  }).then(result => args.neverTranslateCodes = result));

    if (isWindows || isChromeOS) {
      // Fetch the starting UI language, which affects which actions should be
      // enabled.
      promises.push(this.browserProxy_.getProspectiveUILanguage().then(
          prospectiveUILanguage => {
            this.originalProspectiveUILanguage_ =
                prospectiveUILanguage || window.navigator.language;
          }));
    }

    Promise.all(promises).then(results => {
      if (!this.isConnected) {
        // Return early if this element was detached from the DOM before
        // this async callback executes (can happen during testing).
        return;
      }

      this.createModel_(args);

      // <if expr="not is_macosx">
      this.boundOnSpellcheckDictionariesChanged_ =
          this.onSpellcheckDictionariesChanged_.bind(this);
      this.languageSettingsPrivate_.onSpellcheckDictionariesChanged.addListener(
          this.boundOnSpellcheckDictionariesChanged_);
      this.languageSettingsPrivate_.getSpellcheckDictionaryStatuses(
          this.boundOnSpellcheckDictionariesChanged_);
      // </if>

      this.resolver_.resolve();
    });

    if (isChromeOS) {
      this.boundOnInputMethodChanged_ = this.onInputMethodChanged_.bind(this);
      this.inputMethodPrivate_.onChanged.addListener(
          assert(this.boundOnInputMethodChanged_));
      this.boundOnInputMethodAdded_ = this.onInputMethodAdded_.bind(this);
      this.languageSettingsPrivate_.onInputMethodAdded.addListener(
          this.boundOnInputMethodAdded_);
      this.boundOnInputMethodRemoved_ = this.onInputMethodRemoved_.bind(this);
      this.languageSettingsPrivate_.onInputMethodRemoved.addListener(
          this.boundOnInputMethodRemoved_);
    }
  },

  /** @override */
  detached() {
    if (isChromeOS) {
      this.inputMethodPrivate_.onChanged.removeListener(
          assert(this.boundOnInputMethodChanged_));
      this.boundOnInputMethodChanged_ = null;
      this.languageSettingsPrivate_.onInputMethodAdded.removeListener(
          assert(this.boundOnInputMethodAdded_));
      this.boundOnInputMethodAdded_ = null;
      this.languageSettingsPrivate_.onInputMethodRemoved.removeListener(
          assert(this.boundOnInputMethodRemoved_));
      this.boundOnInputMethodRemoved_ = null;
    }

    // <if expr="not is_macosx">
    if (this.boundOnSpellcheckDictionariesChanged_) {
      this.languageSettingsPrivate_.onSpellcheckDictionariesChanged
          .removeListener(this.boundOnSpellcheckDictionariesChanged_);
      this.boundOnSpellcheckDictionariesChanged_ = null;
    }
    // </if>
  },

  /**
   * Updates the prospective UI language based on the new pref value.
   * @param {string} prospectiveUILanguage
   * @private
   */
  prospectiveUILanguageChanged_(prospectiveUILanguage) {
    this.set(
        'languages.prospectiveUILanguage',
        prospectiveUILanguage || this.originalProspectiveUILanguage_);
  },

  /**
   * Updates the list of enabled languages from the preferred languages pref.
   * @private
   */
  preferredLanguagesPrefChanged_() {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }

    const enabledLanguageStates = this.getEnabledLanguageStates_(
        this.languages.translateTarget, this.languages.prospectiveUILanguage);

    // Recreate the enabled language set before updating languages.enabled.
    this.enabledLanguageSet_.clear();
    for (let i = 0; i < enabledLanguageStates.length; i++) {
      this.enabledLanguageSet_.add(enabledLanguageStates[i].language.code);
    }

    this.set('languages.enabled', enabledLanguageStates);

    // <if expr="not is_macosx">
    if (this.boundOnSpellcheckDictionariesChanged_) {
      this.languageSettingsPrivate_.getSpellcheckDictionaryStatuses(
          this.boundOnSpellcheckDictionariesChanged_);
    }
    // </if>

    // Update translate target language.
    new Promise(resolve => {
      this.languageSettingsPrivate_.getTranslateTargetLanguage(resolve);
    }).then(result => {
      this.set('languages.translateTarget', result);
    });
  },

  /**
   * Updates the spellCheckEnabled state of each enabled language.
   * @private
   */
  spellCheckDictionariesPrefChanged_() {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }

    const spellCheckSet = this.makeSetFromArray_(/** @type {!Array<string>} */ (
        this.getPref('spellcheck.dictionaries').value));
    const spellCheckForcedSet =
        this.makeSetFromArray_(/** @type {!Array<string>} */ (
            this.getPref('spellcheck.forced_dictionaries').value));
    const spellCheckBlockedSet =
        this.makeSetFromArray_(/** @type {!Array<string>} */ (
            this.getPref('spellcheck.blocked_dictionaries').value));

    for (let i = 0; i < this.languages.enabled.length; i++) {
      const languageState = this.languages.enabled[i];
      const isUser = spellCheckSet.has(languageState.language.code);
      const isForced = spellCheckForcedSet.has(languageState.language.code);
      const isBlocked = spellCheckBlockedSet.has(languageState.language.code);
      this.set(
          `languages.enabled.${i}.spellCheckEnabled`,
          (isUser && !isBlocked) || isForced);
      this.set(`languages.enabled.${i}.isManaged`, isForced || isBlocked);
    }

    const {on: spellCheckOnLanguages, off: spellCheckOffLanguages} =
        this.getSpellCheckLanguages_(this.languages.supported);
    this.set('languages.spellCheckOnLanguages', spellCheckOnLanguages);
    this.set('languages.spellCheckOffLanguages', spellCheckOffLanguages);
  },

  /**
   * Returns two arrays of SpellCheckLanguageStates for spell check languages:
   * one for spell check on, one for spell check off.
   * @param {!Array<!chrome.languageSettingsPrivate.Language>}
   *     supportedLanguages The list of supported languages, normally
   *     this.languages.supported.
   * @return {{on: !Array<SpellCheckLanguageState>, off:
   *     !Array<SpellCheckLanguageState>}}
   * @private
   */
  getSpellCheckLanguages_(supportedLanguages) {
    // The spell check preferences are prioritised in this order:
    // forced_dictionaries, blocked_dictionaries, dictionaries.

    // The set of all language codes seen thus far.
    const /** !Set<string> */ seenCodes = new Set();

    /**
     * Gets the list of language codes indicated by the preference name, and
     * de-duplicates it with all other language codes.
     * @param {string} prefName
     * @return {!Array<string>}
     */
    const getPrefAndDedupe = prefName => {
      const /** !Array<string> */ result =
          this.getPref(prefName).value.filter(x => !seenCodes.has(x));
      result.forEach(code => seenCodes.add(code));
      return result;
    };

    const forcedCodes = getPrefAndDedupe('spellcheck.forced_dictionaries');
    const forcedCodesSet = new Set(forcedCodes);
    const blockedCodes = getPrefAndDedupe('spellcheck.blocked_dictionaries');
    const blockedCodesSet = new Set(blockedCodes);
    const enabledCodes = getPrefAndDedupe('spellcheck.dictionaries');

    const /** !Array<SpellCheckLanguageState> */ on = [];
    // We want to add newly enabled languages to the end of the "on" list, so we
    // should explicitly move the forced languages to the front of the list.
    for (const code of [...forcedCodes, ...enabledCodes]) {
      const language = this.supportedLanguageMap_.get(code);
      // language could be undefined if code is not in supportedLanguageMap_.
      // This should be rare, but could happen if supportedLanguageMap_ is
      // missing languages or the prefs are manually modified. We want to fail
      // gracefully if this happens - throwing an error here would cause
      // language settings to not load.
      if (language) {
        on.push({
          language,
          isManaged: forcedCodesSet.has(code),
          spellCheckEnabled: true,
          downloadDictionaryStatus: null,
          downloadDictionaryFailureCount: 0,
        });
      }
    }

    // Because the list of "spell check supported" languages is only exposed
    // through "supported languages", we need to filter that list along with
    // whether we've seen the language before.
    // We don't want to split this list in "forced" / "not-forced" like the
    // spell check on list above, as we don't want to explicitly surface / hide
    // blocked languages to the user.
    const /** !Array<SpellCheckLanguageState> */ off = [];

    for (const language of supportedLanguages) {
      // If spell check is off for this language, it must either not be in any
      // spell check pref, or be in the blocked dictionaries pref.
      if (language.supportsSpellcheck &&
          (!seenCodes.has(language.code) ||
           blockedCodesSet.has(language.code))) {
        off.push({
          language,
          isManaged: blockedCodesSet.has(language.code),
          spellCheckEnabled: false,
          downloadDictionaryStatus: null,
          downloadDictionaryFailureCount: 0
        });
      }
    }

    return {
      on,
      off,
    };
  },

  /** @private */
  translateLanguagesPrefChanged_() {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }

    const translateBlockedPref = this.getPref('translate_blocked_languages');
    const translateBlockedSet = this.makeSetFromArray_(
        /** @type {!Array<string>} */ (translateBlockedPref.value));

    for (let i = 0; i < this.languages.enabled.length; i++) {
      const language = this.languages.enabled[i].language;
      const translateEnabled = this.isTranslateEnabled_(
          language.code, !!language.supportsTranslate, translateBlockedSet,
          this.languages.translateTarget, this.languages.prospectiveUILanguage);
      this.set(
          'languages.enabled.' + i + '.translateEnabled', translateEnabled);
    }
  },

  /** @private */
  translateTargetPrefChanged_() {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }
    this.set(
        'languages.translateTarget',
        this.getPref('translate_recent_target').value);
  },

  /**
   * Constructs the languages model.
   * @param {!ModelArgs} args used to populate the model
   *     above.
   * @private
   */
  createModel_(args) {
    // Populate the hash map of supported languages.
    for (let i = 0; i < args.supportedLanguages.length; i++) {
      const language = args.supportedLanguages[i];
      language.supportsUI = !!language.supportsUI;
      language.supportsTranslate = !!language.supportsTranslate;
      language.supportsSpellcheck = !!language.supportsSpellcheck;
      language.isProhibitedLanguage = !!language.isProhibitedLanguage;
      this.supportedLanguageMap_.set(language.code, language);
    }

    let prospectiveUILanguage;
    if (isChromeOS || isWindows) {
      prospectiveUILanguage =
          /** @type {string} */ (this.getPref('intl.app_locale').value) ||
          this.originalProspectiveUILanguage_;
    }

    // Create a list of enabled languages from the supported languages.
    const enabledLanguageStates = this.getEnabledLanguageStates_(
        args.translateTarget, prospectiveUILanguage);
    // Populate the hash set of enabled languages.
    for (let l = 0; l < enabledLanguageStates.length; l++) {
      this.enabledLanguageSet_.add(enabledLanguageStates[l].language.code);
    }

    const {on: spellCheckOnLanguages, off: spellCheckOffLanguages} =
        this.getSpellCheckLanguages_(args.supportedLanguages);

    const alwaysTranslateLanguages =
        args.alwaysTranslateCodes.map(code => this.getLanguage(code));

    const neverTranslateLangauges =
        args.neverTranslateCodes.map(code => this.getLanguage(code));

    const model = /** @type {!LanguagesModel} */ ({
      supported: args.supportedLanguages,
      enabled: enabledLanguageStates,
      translateTarget: args.translateTarget,
      alwaysTranslate: alwaysTranslateLanguages,
      neverTranslate: neverTranslateLangauges,
      spellCheckOnLanguages,
      spellCheckOffLanguages,
    });

    if (isChromeOS || isWindows) {
      model.prospectiveUILanguage = prospectiveUILanguage;
    }

    if (isChromeOS) {
      if (args.supportedInputMethods) {
        this.createInputMethodModel_(args.supportedInputMethods);
      }
      model.inputMethods = /** @type {!InputMethodsModel} */ ({
        supported: args.supportedInputMethods,
        enabled: this.getEnabledInputMethods_(),
        currentId: args.currentInputMethodId,
      });
    }

    // Initialize the Polymer languages model.
    this._setLanguages(model);
  },

  /**
   * Returns a list of LanguageStates for each enabled language in the supported
   * languages list.
   * @param {string} translateTarget Language code of the default translate
   *     target language.
   * @param {(string|undefined)} prospectiveUILanguage Prospective UI display
   *     language. Only defined on Windows and Chrome OS.
   * @return {!Array<!LanguageState>}
   * @private
   */
  getEnabledLanguageStates_(translateTarget, prospectiveUILanguage) {
    assert(CrSettingsPrefs.isInitialized);

    const pref = this.getPref(preferredLanguagesPrefName);
    const enabledLanguageCodes = pref.value.split(',');
    const languagesForcedPref = this.getPref('intl.forced_languages');
    const spellCheckPref = this.getPref('spellcheck.dictionaries');
    const spellCheckForcedPref = this.getPref('spellcheck.forced_dictionaries');
    const spellCheckBlockedPref =
        this.getPref('spellcheck.blocked_dictionaries');
    const languageForcedSet = this.makeSetFromArray_(
        /** @type {!Array<string>} */ (languagesForcedPref.value));
    const spellCheckSet = this.makeSetFromArray_(
        /** @type {!Array<string>} */ (
            spellCheckPref.value.concat(spellCheckForcedPref.value)));
    const spellCheckForcedSet = this.makeSetFromArray_(
        /** @type {!Array<string>} */ (spellCheckForcedPref.value));
    const spellCheckBlockedSet = this.makeSetFromArray_(
        /** @type {!Array<string>} */ (spellCheckBlockedPref.value));

    const translateBlockedPref = this.getPref('translate_blocked_languages');
    const translateBlockedSet = this.makeSetFromArray_(
        /** @type {!Array<string>} */ (translateBlockedPref.value));

    const enabledLanguageStates = [];

    for (let i = 0; i < enabledLanguageCodes.length; i++) {
      const code = enabledLanguageCodes[i];
      const language = this.supportedLanguageMap_.get(code);
      // Skip unsupported languages.
      if (!language) {
        continue;
      }
      const languageState = /** @type {LanguageState} */ ({});
      languageState.language = language;
      languageState.spellCheckEnabled =
          spellCheckSet.has(code) && !spellCheckBlockedSet.has(code) ||
          spellCheckForcedSet.has(code);
      languageState.translateEnabled = this.isTranslateEnabled_(
          code, !!language.supportsTranslate, translateBlockedSet,
          translateTarget, prospectiveUILanguage);
      languageState.isManaged =
          spellCheckForcedSet.has(code) || spellCheckBlockedSet.has(code);
      languageState.isForced = languageForcedSet.has(code);
      languageState.downloadDictionaryFailureCount = 0;
      enabledLanguageStates.push(languageState);
    }
    return enabledLanguageStates;
  },

  /**
   * True iff we translate pages that are in the given language.
   * @param {string} code Language code.
   * @param {boolean} supportsTranslate If translation supports the given
   *     language.
   * @param {!Set<string>} translateBlockedSet Set of languages for which
   *     translation is blocked.
   * @param {string} translateTarget Language code of the default translate
   *     target language.
   * @param {(string|undefined)} prospectiveUILanguage Prospective UI display
   *     language. Only defined on Windows and Chrome OS.
   * @return {boolean}
   * @private
   */
  isTranslateEnabled_(
      code, supportsTranslate, translateBlockedSet, translateTarget,
      prospectiveUILanguage) {
    const translateCode = this.convertLanguageCodeForTranslate(code);
    return supportsTranslate && !translateBlockedSet.has(translateCode) &&
        translateCode !== translateTarget &&
        (!prospectiveUILanguage || code !== prospectiveUILanguage);
  },

  // <if expr="not is_macosx">
  /**
   * Updates the dictionary download status for spell check languages in order
   * to track the number of times a spell check dictionary download has failed.
   * @param {!Array<!chrome.languageSettingsPrivate.SpellcheckDictionaryStatus>}
   *     statuses
   * @private
   */
  onSpellcheckDictionariesChanged_(statuses) {
    const statusMap = new Map();
    statuses.forEach(status => {
      statusMap.set(status.languageCode, status);
    });

    const collectionNames =
        ['enabled', 'spellCheckOnLanguages', 'spellCheckOffLanguages'];
    collectionNames.forEach(collectionName => {
      this.languages[collectionName].forEach((languageState, index) => {
        const status = statusMap.get(languageState.language.code);
        if (!status) {
          return;
        }

        const previousStatus = languageState.downloadDictionaryStatus;
        const keyPrefix = `languages.${collectionName}.${index}`;
        this.set(`${keyPrefix}.downloadDictionaryStatus`, status);

        const failureCountKey = `${keyPrefix}.downloadDictionaryFailureCount`;
        if (status.downloadFailed &&
            !(previousStatus && previousStatus.downloadFailed)) {
          const failureCount = languageState.downloadDictionaryFailureCount + 1;
          this.set(failureCountKey, failureCount);
        } else if (
            status.isReady && !(previousStatus && previousStatus.isReady)) {
          this.set(failureCountKey, 0);
        }
      });
    });
  },
  // </if>

  /**
   * Updates the |removable| property of the enabled language states based
   * on what other languages and input methods are enabled.
   * @private
   */
  updateRemovableLanguages_() {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }

    // TODO(michaelpg): Enabled input methods can affect which languages are
    // removable, so run updateEnabledInputMethods_ first (if it has been
    // scheduled).
    if (isChromeOS) {
      this.updateEnabledInputMethods_();
    }

    for (let i = 0; i < this.languages.enabled.length; i++) {
      const languageState = this.languages.enabled[i];
      this.set(
          'languages.enabled.' + i + '.removable',
          this.canDisableLanguage(languageState));
    }
  },

  /**
   * Creates a Set from the elements of the array.
   * @param {!Array<T>} list
   * @return {!Set<T>}
   * @template T
   * @private
   */
  makeSetFromArray_(list) {
    return new Set(list);
  },

  // LanguageHelper implementation.
  // TODO(michaelpg): replace duplicate docs with @override once b/24294625
  // is fixed.

  /** @return {!Promise} */
  whenReady() {
    return this.resolver_.promise;
  },

  // <if expr="chromeos or is_win">
  /**
   * Sets the prospective UI language to the chosen language. This won't affect
   * the actual UI language until a restart.
   * @param {string} languageCode
   */
  setProspectiveUILanguage(languageCode) {
    this.browserProxy_.setProspectiveUILanguage(languageCode);
  },

  /**
   * True if the prospective UI language was changed from its starting value.
   * @return {boolean}
   */
  requiresRestart() {
    return this.originalProspectiveUILanguage_ !==
        this.languages.prospectiveUILanguage;
  },
  // </if>

  /**
   * @return {string} The language code for ARC IMEs.
   */
  getArcImeLanguageCode() {
    return kArcImeLanguage;
  },

  /**
   * @param {string} languageCode
   * @return {boolean} True if the language is for ARC IMEs.
   */
  isLanguageCodeForArcIme(languageCode) {
    return languageCode === kArcImeLanguage;
  },

  /**
   * @param {string} languageCode
   * @return {boolean} True if the language is enabled.
   */
  isLanguageEnabled(languageCode) {
    return this.enabledLanguageSet_.has(languageCode);
  },

  /**
   * Enables the language, making it available for spell check and input.
   * @param {string} languageCode
   */
  enableLanguage(languageCode) {
    if (!CrSettingsPrefs.isInitialized) {
      return;
    }

    this.languageSettingsPrivate_.enableLanguage(languageCode);
  },

  /**
   * Disables the language.
   * @param {string} languageCode
   */
  disableLanguage(languageCode) {
    if (!CrSettingsPrefs.isInitialized) {
      return;
    }

    // Remove the language from spell check.
    // <if expr="not chromeos">
    this.deletePrefListItem('spellcheck.dictionaries', languageCode);
    // </if>

    // <if expr="chromeos">
    // For CrOS language settings V2 update 2, languages and spell check are
    // decoupled so there's no need to remove the language from spell check.
    if (!this.isChromeOSLanguageSettingsV2Update2_()) {
      this.deletePrefListItem('spellcheck.dictionaries', languageCode);
    }

    // For language settings V2, languages and input methods are decoupled
    // so there's no need to remove related input methods.
    // TODO(crbug.com/1097328): Remove this as LSV2 has launched to 100%.
    if (!this.isChromeOSLanguageSettingsV2_()) {
      // Remove input methods that don't support any other enabled language.
      const inputMethods = this.languageInputMethods_.get(languageCode) || [];
      for (const inputMethod of inputMethods) {
        const supportsOtherEnabledLanguages = inputMethod.languageCodes.some(
            otherLanguageCode => otherLanguageCode !== languageCode &&
                this.isLanguageEnabled(otherLanguageCode));
        if (!supportsOtherEnabledLanguages) {
          this.removeInputMethod(inputMethod.id);
        }
      }
    }
    // </if>

    // Remove the language from preferred languages.
    this.languageSettingsPrivate_.disableLanguage(languageCode);
  },

  /**
   * @return {boolean}
   * @private
   */
  isChromeOSLanguageSettingsV2_() {
    if (!isChromeOS) {
      return false;
    }
    return loadTimeData.valueExists('enableLanguageSettingsV2') &&
        loadTimeData.getBoolean('enableLanguageSettingsV2');
  },

  // <if expr="chromeos">
  /**
   * @return {boolean}
   * @private
   */
  isChromeOSLanguageSettingsV2Update2_() {
    return this.isChromeOSLanguageSettingsV2_() &&
        loadTimeData.valueExists('enableLanguageSettingsV2Update2') &&
        loadTimeData.getBoolean('enableLanguageSettingsV2Update2');
  },
  // </if>

  /**
   * @param {!LanguageState} languageState
   * @return {boolean}
   */
  isOnlyTranslateBlockedLanguage(languageState) {
    return !languageState.translateEnabled &&
        this.languages.enabled.filter(lang => !lang.translateEnabled).length ===
        1;
  },

  /**
   * @param {!LanguageState} languageState
   * @return {boolean}
   */
  canDisableLanguage(languageState) {
    // Cannot disable the prospective UI language.
    // Exception for Chrome OS language settings V2 as we are decoupling
    // language preference from UI language.
    if (languageState.language.code === this.languages.prospectiveUILanguage &&
        !this.isChromeOSLanguageSettingsV2_()) {
      return false;
    }

    // Cannot disable the only enabled language.
    if (this.languages.enabled.length === 1) {
      return false;
    }

    // Cannot disable the last translate blocked language.
    if (this.isOnlyTranslateBlockedLanguage(languageState)) {
      return false;
    }

    if (!isChromeOS) {
      return true;
    }

    // ChromeOS language settings V2 does not remove input methods when removing
    // languages, so there's no need to check for other enabled input methods
    // below.
    if (this.isChromeOSLanguageSettingsV2_()) {
      return true;
    }

    // If this is the only enabled language that is supported by all enabled
    // component IMEs, it cannot be disabled because we need those IMEs.
    const otherInputMethodsEnabled =
        this.languages.enabled.some(function(otherLanguageState) {
          const otherLanguageCode = otherLanguageState.language.code;
          if (otherLanguageCode === languageState.language.code) {
            return false;
          }
          const inputMethods =
              this.languageInputMethods_.get(otherLanguageCode);
          return inputMethods && inputMethods.some(function(inputMethod) {
            return this.isComponentIme(inputMethod) &&
                this.supportedInputMethodMap_.get(inputMethod.id).enabled;
          }, this);
        }, this);
    return otherInputMethodsEnabled;
  },

  /**
   * @param {!chrome.languageSettingsPrivate.Language} language
   * @return {boolean} true if the given language can be enabled
   */
  canEnableLanguage(language) {
    return !(
        this.isLanguageEnabled(language.code) ||
        language.isProhibitedLanguage ||
        this.isLanguageCodeForArcIme(language.code) /* internal use only */);
  },

  /**
   * Sets whether a given language should always be automatically translated.
   * @param {string} languageCode
   * @param {boolean} alwaysTranslate
   */
  setLanguageAlwaysTranslateState(languageCode, alwaysTranslate) {
    this.languageSettingsPrivate_.setLanguageAlwaysTranslateState(
        languageCode, alwaysTranslate);
  },

  /**
   * Moves the language in the list of enabled languages either up (toward the
   * front of the list) or down (toward the back).
   * @param {string} languageCode
   * @param {boolean} upDirection True if we need to move up, false if we
   *     need to move down
   */
  moveLanguage(languageCode, upDirection) {
    if (!CrSettingsPrefs.isInitialized) {
      return;
    }

    if (upDirection) {
      this.languageSettingsPrivate_.moveLanguage(languageCode, MoveType.UP);
    } else {
      this.languageSettingsPrivate_.moveLanguage(languageCode, MoveType.DOWN);
    }
  },

  /**
   * Moves the language directly to the front of the list of enabled languages.
   * @param {string} languageCode
   */
  moveLanguageToFront(languageCode) {
    if (!CrSettingsPrefs.isInitialized) {
      return;
    }

    this.languageSettingsPrivate_.moveLanguage(languageCode, MoveType.TOP);
  },

  /**
   * Enables translate for the given language by removing the translate
   * language from the blocked languages preference.
   * @param {string} languageCode
   */
  enableTranslateLanguage(languageCode) {
    this.languageSettingsPrivate_.setEnableTranslationForLanguage(
        languageCode, true);
  },

  /**
   * Disables translate for the given language by adding the translate
   * language to the blocked languages preference.
   * @param {string} languageCode
   */
  disableTranslateLanguage(languageCode) {
    this.languageSettingsPrivate_.setEnableTranslationForLanguage(
        languageCode, false);
  },

  /**
   * Sets the translate target language and adds it to the content languages if
   * not already there.
   * @param {string} languageCode
   */
  setTranslateTargetLanguage(languageCode) {
    this.languageSettingsPrivate_.setTranslateTargetLanguage(languageCode);
  },

  /**
   * Enables or disables spell check for the given language.
   * @param {string} languageCode
   * @param {boolean} enable
   */
  toggleSpellCheck(languageCode, enable) {
    if (!this.languages) {
      return;
    }

    if (enable) {
      const spellCheckPref = this.getPref('spellcheck.dictionaries');
      this.appendPrefListItem('spellcheck.dictionaries', languageCode);
    } else {
      this.deletePrefListItem('spellcheck.dictionaries', languageCode);
    }
  },

  /**
   * Converts the language code for translate. There are some differences
   * between the language set the Translate server uses and that for
   * Accept-Language.
   * @param {string} languageCode
   * @return {string} The converted language code.
   */
  convertLanguageCodeForTranslate(languageCode) {
    if (languageCode in kLanguageCodeToTranslateCode) {
      return kLanguageCodeToTranslateCode[languageCode];
    }

    const main = languageCode.split('-')[0];
    if (main === 'zh') {
      // In Translate, general Chinese is not used, and the sub code is
      // necessary as a language code for the Translate server.
      return languageCode;
    }
    if (main in kTranslateLanguageSynonyms) {
      return kTranslateLanguageSynonyms[main];
    }

    return main;
  },

  /**
   * Given a language code, returns just the base language. E.g., converts
   * 'en-GB' to 'en'.
   * @param {string} languageCode
   * @return {string}
   */
  getLanguageCodeWithoutRegion(languageCode) {
    // The Norwegian languages fall under the 'no' macrolanguage.
    if (languageCode === 'nb' || languageCode === 'nn') {
      return 'no';
    }

    // The installer still uses the old language code "iw", instead of "he",
    // for Hebrew. It needs to be converted to "he", otherwise it will not be
    // found in supportedLanguageMap_.
    //
    // Note that this value is saved in the user's local state. Even
    // if the installer is changed to use "he", because the installer does not
    // overwrite this value, the conversion is still needed for old users.
    if (languageCode === 'iw') {
      return 'he';
    }

    // Match the characters before the hyphen.
    const result = languageCode.match(/^([^-]+)-?/);
    assert(result.length === 2);
    return result[1];
  },

  /**
   * @param {string} languageCode
   * @return {!chrome.languageSettingsPrivate.Language|undefined}
   */
  getLanguage(languageCode) {
    // If a languageCode is not found, try language without location.
    return this.supportedLanguageMap_.get(languageCode) ||
        this.supportedLanguageMap_.get(
            this.getLanguageCodeWithoutRegion(languageCode));
  },

  /**
   * Retries downloading the dictionary for |languageCode|.
   * @param {string} languageCode
   */
  retryDownloadDictionary(languageCode) {
    this.languageSettingsPrivate_.retryDownloadDictionary(languageCode);
  },

  // TODO(crbug/1126259): Once migration is over, use separate languages.js for
  // browser and chromeos

  // <if expr="chromeos">
  /**
   * Constructs the input method part of the languages model.
   * @param {!Array<!chrome.languageSettingsPrivate.InputMethod>}
   *     supportedInputMethods Input methods.
   * @private
   */
  createInputMethodModel_(supportedInputMethods) {
    // Populate the hash map of supported input methods.
    this.supportedInputMethodMap_.clear();
    this.languageInputMethods_.clear();
    for (let j = 0; j < supportedInputMethods.length; j++) {
      const inputMethod = supportedInputMethods[j];
      inputMethod.enabled = !!inputMethod.enabled;
      inputMethod.isProhibitedByPolicy = !!inputMethod.isProhibitedByPolicy;
      // Add the input method to the map of IDs.
      this.supportedInputMethodMap_.set(inputMethod.id, inputMethod);
      // Add the input method to the list of input methods for each language
      // it supports.
      for (let k = 0; k < inputMethod.languageCodes.length; k++) {
        const languageCode = inputMethod.languageCodes[k];
        if (!this.supportedLanguageMap_.has(languageCode)) {
          continue;
        }
        if (!this.languageInputMethods_.has(languageCode)) {
          this.languageInputMethods_.set(languageCode, [inputMethod]);
        } else {
          this.languageInputMethods_.get(languageCode).push(inputMethod);
        }
      }
    }
  },

  /**
   * Returns a list of enabled input methods.
   * @return {!Array<!chrome.languageSettingsPrivate.InputMethod>}
   * @private
   */
  getEnabledInputMethods_() {
    assert(CrSettingsPrefs.isInitialized);

    let enabledInputMethodIds =
        this.getPref('settings.language.preload_engines').value.split(',');
    enabledInputMethodIds = enabledInputMethodIds.concat(
        this.getPref('settings.language.enabled_extension_imes')
            .value.split(','));
    this.enabledInputMethodSet_ = new Set(enabledInputMethodIds);

    // Return only supported input methods.
    return enabledInputMethodIds
        .map(id => this.supportedInputMethodMap_.get(id))
        .filter(function(inputMethod) {
          return !!inputMethod;
        });
  },

  /** @private */
  updateSupportedInputMethods_() {
    const promise = new Promise(resolve => {
      this.languageSettingsPrivate_.getInputMethodLists(function(lists) {
        resolve(
            lists.componentExtensionImes.concat(lists.thirdPartyExtensionImes));
      });
    });
    promise.then(result => {
      const supportedInputMethods = result;
      this.createInputMethodModel_(supportedInputMethods);
      this.set('languages.inputMethods.supported', supportedInputMethods);
      this.updateEnabledInputMethods_();
    });
  },

  /** @private */
  updateEnabledInputMethods_() {
    const enabledInputMethods = this.getEnabledInputMethods_();
    const enabledInputMethodSet = this.makeSetFromArray_(enabledInputMethods);

    for (let i = 0; i < this.languages.inputMethods.supported.length; i++) {
      this.set(
          'languages.inputMethods.supported.' + i + '.enabled',
          enabledInputMethodSet.has(this.languages.inputMethods.supported[i]));
    }
    this.set('languages.inputMethods.enabled', enabledInputMethods);
  },

  /** @param {string} id */
  addInputMethod(id) {
    if (!this.supportedInputMethodMap_.has(id)) {
      return;
    }
    this.languageSettingsPrivate_.addInputMethod(id);
  },

  /** @param {string} id */
  removeInputMethod(id) {
    if (!this.supportedInputMethodMap_.has(id)) {
      return;
    }
    this.languageSettingsPrivate_.removeInputMethod(id);
  },

  /** @param {string} id */
  setCurrentInputMethod(id) {
    this.inputMethodPrivate_.setCurrentInputMethod(id);
  },

  /**
   * @param {string} languageCode
   * @return {!Array<!chrome.languageSettingsPrivate.InputMethod>}
   */
  getInputMethodsForLanguage(languageCode) {
    return this.languageInputMethods_.get(languageCode) || [];
  },

  /**
   * Returns the input methods that support any of the given languages.
   * @param {!Array<string>} languageCodes
   * @return {!Array<!chrome.languageSettingsPrivate.InputMethod>}
   */
  getInputMethodsForLanguages(languageCodes) {
    // Input methods that have already been listed for this language.
    const /** !Set<string> */ usedInputMethods = new Set();
    /** @type {!Array<chrome.languageSettingsPrivate.InputMethod>} */
    const combinedInputMethods = [];
    for (const languageCode of languageCodes) {
      const inputMethods = this.getInputMethodsForLanguage(languageCode);
      // Get the language's unused input methods and mark them as used.
      const newInputMethods = inputMethods.filter(
          inputMethod => !usedInputMethods.has(inputMethod.id));
      newInputMethods.forEach(
          inputMethod => usedInputMethods.add(inputMethod.id));
      combinedInputMethods.push(...newInputMethods);
    }
    return combinedInputMethods;
  },

  /**
   * @return {!Set<string>} list of enabled language code.
   */
  getEnabledLanguageCodes() {
    return this.enabledLanguageSet_;
  },

  /**
   * @param {string} id the input method id
   * @return {boolean} True if the input method is enabled
   */
  isInputMethodEnabled(id) {
    return this.enabledInputMethodSet_.has(id);
  },

  /**
   * @param {!chrome.languageSettingsPrivate.InputMethod} inputMethod
   * @return {boolean}
   */
  isComponentIme(inputMethod) {
    return inputMethod.id.startsWith('_comp_');
  },

  /** @param {string} id Input method ID. */
  openInputMethodOptions(id) {
    this.inputMethodPrivate_.openOptionsPage(id);
  },

  /** @param {string} id New current input method ID. */
  onInputMethodChanged_(id) {
    this.set('languages.inputMethods.currentId', id);
  },

  /** @param {string} id Added input method ID. */
  onInputMethodAdded_(id) {
    this.updateSupportedInputMethods_();
  },

  /** @param {string} id Removed input method ID. */
  onInputMethodRemoved_(id) {
    this.updateSupportedInputMethods_();
  },

  /**
   * @param {string} id Input method ID.
   * @return {string}
   */
  getInputMethodDisplayName(id) {
    const inputMethod = this.supportedInputMethodMap_.get(id);
    assert(inputMethod);
    return inputMethod.displayName;
  },
  // </if>
});
