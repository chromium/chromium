// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-languages' handles Chrome's language and input
 * method settings. The 'languages' property, which reflects the current
 * language settings, must not be changed directly. Instead, changes to
 * language settings should be made using the LanguageHelper APIs provided by
 * this class via languageHelper.
 */

import '/shared/settings/prefs/prefs.js';

import {assert} from '//resources/js/assert.js';
import {PromiseResolver} from '//resources/js/promise_resolver.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrSettingsPrefs} from '/shared/settings/prefs/prefs_types.js';

import type {LanguagesBrowserProxy} from './languages_browser_proxy.js';
import {LanguagesBrowserProxyImpl} from './languages_browser_proxy.js';
import type {LanguageHelper, LanguagesModel, LanguageState, SpellCheckLanguageState} from './languages_types.js';

interface SpellCheckLanguages {
  on: SpellCheckLanguageState[];
  off: SpellCheckLanguageState[];
}

const MoveType = chrome.languageSettingsPrivate.MoveType;

// For some codes translate uses a different version from Chrome.  Some are
// ISO 639 codes that have been renamed (e.g. "he" to "iw"). While others are
// languages that Translate considers similar (e.g. "nb" and "no").
// See also: components/language/core/common/language_util.cc.
const kChromeToTranslateCode: Map<string, string> = new Map([
  ['fil', 'tl'],
  ['he', 'iw'],
  ['jv', 'jw'],
  ['kok', 'gom'],
  ['nb', 'no'],
]);

// Reverse of the map above. Just the languages code that translate uses but
// Chrome has a different code for.
const kTranslateToChromeCode: Map<string, string> = new Map([
  ['gom', 'kok'],
  ['iw', 'he'],
  ['jw', 'jv'],
  ['no', 'nb'],
  ['tl', 'fil'],
]);

// The fake language name used for ARC IMEs. The value must be in sync with the
// one in ui/base/ime/ash/extension_ime_util.h.
const kArcImeLanguage: string = '_arc_ime_language_';

interface ModelArgs {
  supportedLanguages: chrome.languageSettingsPrivate.Language[];
  translateTarget: string;
  alwaysTranslateCodes: string[];
  neverTranslateCodes: string[];
  neverTranslateSites: string[];
  startingUILanguage: string;
  supportedInputMethods?: chrome.languageSettingsPrivate.InputMethod[];
  currentInputMethodId?: string;
}

/**
 * Singleton element that generates the languages model on start-up and
 * updates it whenever Chrome's pref store and other settings change.
 */

const SettingsLanguagesElementBase = PrefsMixin(PolymerElement);

class SettingsLanguagesElement extends SettingsLanguagesElementBase implements
    LanguageHelper {
  static get is() {
    return 'settings-languages';
  }

  static get properties() {
    return {
      languages: {
        type: Object,
        notify: true,
      },

      /**
       * This element, as a LanguageHelper instance for API usage.
       */
      languageHelper: {
        type: Object,
        notify: true,
        readOnly: true,
        value() {
          return this;
        },
      },

      /**
       * PromiseResolver to be resolved when the singleton has been initialized.
       */
      resolver_: {
        type: Object,
        value: () => new PromiseResolver(),
      },

      /**
       * Hash map of supported languages by language codes for fast lookup.
       */
      supportedLanguageMap_: {
        type: Object,
        value: () => new Map(),
      },

      /**
       * Hash set of enabled language codes for membership testing.
       */
      enabledLanguageSet_: {
        type: Object,
        value: () => new Set(),
      },

      // <if expr="is_win">
      /** Prospective UI language when the page was loaded. */
      originalProspectiveUILanguage_: String,
      // </if>
    };
  }

  static get observers() {
    return [
      // All observers wait for the model to be populated by including the
      // |languages| property.
      'alwaysTranslateLanguagesPrefChanged_(' +
          'prefs.translate_allowlists.value.*, languages)',
      'neverTranslateLanguagesPrefChanged_(' +
          'prefs.translate_blocked_languages.value.*, languages)',
      'neverTranslateSitesPrefChanged_(' +
          'prefs.translate_site_blocklist_with_time.value.*, languages)',
      // <if expr="is_win">
      'prospectiveUiLanguageChanged_(prefs.intl.app_locale.value, languages)',
      // </if>
      'preferredLanguagesPrefChanged_(' +
          'prefs.intl.accept_languages.value, languages)',
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
    ];
  }

  languages?: LanguagesModel|undefined;
  languageHelper: LanguageHelper;
  private resolver_: PromiseResolver<void>;
  private supportedLanguageMap_:
      Map<string, chrome.languageSettingsPrivate.Language>;
  private enabledLanguageSet_: Set<string>;

  // <if expr="is_win">
  private originalProspectiveUILanguage_: string;
  // </if>

  // <if expr="not is_macosx">
  private boundOnSpellcheckDictionariesChanged_:
      ((statuses: chrome.languageSettingsPrivate
            .SpellcheckDictionaryStatus[]) => void)|null = null;
  // </if>

  private browserProxy_: LanguagesBrowserProxy =
      LanguagesBrowserProxyImpl.getInstance();
  private languageSettingsPrivate_: typeof chrome.languageSettingsPrivate;

  constructor() {
    super();

    this.languageSettingsPrivate_ =
        this.browserProxy_.getLanguageSettingsPrivate();
  }

  override connectedCallback() {
    super.connectedCallback();

    const promises: Array<Promise<any>> = [];

    /**
     * An object passed into createModel to keep track of platform-specific
     * arguments, populated by the "promises" array.
     */
    const args: ModelArgs = {
      supportedLanguages: [],
      translateTarget: '',
      alwaysTranslateCodes: [],
      neverTranslateCodes: [],
      neverTranslateSites: [],
      startingUILanguage: '',

      // Only used by ChromeOS
      supportedInputMethods: [],
      currentInputMethodId: '',
    };

    // Wait until prefs are initialized before creating the model, so we can
    // include information about enabled languages.
    promises.push(CrSettingsPrefs.initialized);

    // Get the language list.
    promises.push(
        this.languageSettingsPrivate_.getLanguageList().then(result => {
          args.supportedLanguages = result;
        }));

    // Get the translate target language.
    promises.push(
        this.languageSettingsPrivate_.getTranslateTargetLanguage().then(
            result => {
              args.translateTarget = result;
            }));

    // Get the list of language-codes to always translate.
    promises.push(
        this.languageSettingsPrivate_.getAlwaysTranslateLanguages().then(
            result => {
              args.alwaysTranslateCodes = result;
            }));

    // Get the list of language-codes to never translate.
    promises.push(
        this.languageSettingsPrivate_.getNeverTranslateLanguages().then(
            result => {
              args.neverTranslateCodes = result;
            }));

    // <if expr="is_win">
    // Fetch the starting UI language, which affects which actions should be
    // enabled.
    promises.push(this.browserProxy_.getProspectiveUiLanguage().then(
        prospectiveUILanguage => {
          this.originalProspectiveUILanguage_ =
              prospectiveUILanguage || window.navigator.language;
        }));
    // </if>

    Promise.all(promises).then(() => {
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
      this.languageSettingsPrivate_.getSpellcheckDictionaryStatuses().then(
          this.boundOnSpellcheckDictionariesChanged_);
      // </if>

      this.resolver_.resolve();
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    // <if expr="not is_macosx">
    if (this.boundOnSpellcheckDictionariesChanged_) {
      this.languageSettingsPrivate_.onSpellcheckDictionariesChanged
          .removeListener(this.boundOnSpellcheckDictionariesChanged_);
      this.boundOnSpellcheckDictionariesChanged_ = null;
    }
    // </if>
  }

  // <if expr="is_win">
  /**
   * Updates the prospective UI language based on the new pref value.
   */
  private prospectiveUiLanguageChanged_(prospectiveUILanguage: string) {
    this.set(
        'languages.prospectiveUILanguage',
        prospectiveUILanguage || this.originalProspectiveUILanguage_);
  }
  // </if>

  /**
   * Updates the list of enabled languages from the preferred languages pref.
   */
  private preferredLanguagesPrefChanged_() {
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
      this.languageSettingsPrivate_.getSpellcheckDictionaryStatuses().then(
          this.boundOnSpellcheckDictionariesChanged_);
    }
    // </if>

    // Update translate target language.
    this.languageSettingsPrivate_.getTranslateTargetLanguage().then(result => {
      this.set('languages.translateTarget', result);
    });
  }

  /**
   * Updates the spellCheckEnabled state of each enabled language.
   */
  private spellCheckDictionariesPrefChanged_() {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }

    const spellCheckSet = this.makeSetFromArray_(
        this.getPref<string[]>('spellcheck.dictionaries').value);
    const spellCheckForcedSet = this.makeSetFromArray_(
        this.getPref<string[]>('spellcheck.forced_dictionaries').value);
    const spellCheckBlockedSet = this.makeSetFromArray_(
        this.getPref<string[]>('spellcheck.blocked_dictionaries').value);

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
  }

  /**
   * Returns two arrays of SpellCheckLanguageStates for spell check languages:
   * one for spell check on, one for spell check off.
   * @param supportedLanguages The list of supported languages, normally
   *     this.languages.supported.
   */
  private getSpellCheckLanguages_(
      supportedLanguages: chrome.languageSettingsPrivate.Language[]):
      SpellCheckLanguages {
    // The spell check preferences are prioritised in this order:
    // forced_dictionaries, blocked_dictionaries, dictionaries.

    // The set of all language codes seen thus far.
    const seenCodes = new Set<string>();

    /**
     * Gets the list of language codes indicated by the preference name, and
     * de-duplicates it with all other language codes.
     */
    const getPrefAndDedupe = (prefName: string): string[] => {
      const result =
          this.getPref<string[]>(prefName).value.filter(x => !seenCodes.has(x));
      result.forEach((code: string) => seenCodes.add(code));
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
          downloadDictionaryFailureCount: 0,
        });
      }
    }

    return {
      on,
      off,
    };
  }

  /**
   * Updates the list of always translate languages from translate prefs.
   */
  private alwaysTranslateLanguagesPrefChanged_() {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }
    const alwaysTranslateCodes =
        Object.keys(this.getPref('translate_allowlists').value);
    const alwaysTranslateLanguages =
        alwaysTranslateCodes.map((code: string) => this.getLanguage(code));
    this.set('languages.alwaysTranslate', alwaysTranslateLanguages);
  }

  /**
   * Updates the list of never translate languages from translate prefs.
   */
  private neverTranslateLanguagesPrefChanged_() {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }
    const neverTranslateCodes =
        this.getPref<string[]>('translate_blocked_languages').value;
    const neverTranslateLanguages =
        neverTranslateCodes.map(code => this.getLanguage(code));
    this.set('languages.neverTranslate', neverTranslateLanguages);
  }

  /**
   * Updates the list of never translate sites from translate prefs.
   */
  private neverTranslateSitesPrefChanged_() {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }
    const neverTranslateSites =
        Object.keys(this.getPref('translate_site_blocklist_with_time').value);
    this.set('languages.neverTranslateSites', neverTranslateSites);
  }

  private translateLanguagesPrefChanged_() {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }

    const translateBlockedPrefValue =
        this.getPref('translate_blocked_languages').value as string[];
    const translateBlockedSet =
        this.makeSetFromArray_(translateBlockedPrefValue);

    for (let i = 0; i < this.languages.enabled.length; i++) {
      const language = this.languages.enabled[i].language;
      const translateEnabled = this.isTranslateEnabled_(
          language.code, !!language.supportsTranslate, translateBlockedSet,
          this.languages.translateTarget, this.languages.prospectiveUILanguage);
      this.set(
          'languages.enabled.' + i + '.translateEnabled', translateEnabled);
    }
  }

  private translateTargetPrefChanged_() {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }
    this.set(
        'languages.translateTarget',
        this.getPref('translate_recent_target').value);
  }

  /**
   * Constructs the languages model.
   * @param args used to populate the model above.
   */
  private createModel_(args: ModelArgs) {
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
    // <if expr="is_win">
    // eslint-disable-next-line prefer-const
    prospectiveUILanguage = this.getPref<string>('intl.app_locale').value ||
        this.originalProspectiveUILanguage_;
    // </if>

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
        args.alwaysTranslateCodes.map(code => this.getLanguage(code)!);

    const neverTranslateLanguages =
        args.neverTranslateCodes.map(code => this.getLanguage(code)!);

    const model = {
      supported: args.supportedLanguages,
      enabled: enabledLanguageStates,
      translateTarget: args.translateTarget,
      alwaysTranslate: alwaysTranslateLanguages,
      neverTranslate: neverTranslateLanguages,
      neverTranslateSites: args.neverTranslateSites,
      spellCheckOnLanguages,
      spellCheckOffLanguages,
      // <if expr="is_win">
      prospectiveUILanguage: prospectiveUILanguage,
      // </if>
    };

    // Initialize the Polymer languages model.
    this.languages = model;
  }

  /**
   * Returns a list of LanguageStates for each enabled language in the supported
   * languages list.
   * @param translateTarget Language code of the default translate
   *     target language.
   * @param prospectiveUILanguage Prospective UI display language. Only defined
   *     on Windows and Chrome OS.
   */
  private getEnabledLanguageStates_(
      translateTarget: string,
      prospectiveUILanguage: string|undefined): LanguageState[] {
    assert(CrSettingsPrefs.isInitialized);

    const pref = this.getPref<string>('intl.accept_languages');
    const enabledLanguageCodes = pref.value.split(',');
    const languagesForcedPref = this.getPref<string[]>('intl.forced_languages');
    const spellCheckPref = this.getPref<string[]>('spellcheck.dictionaries');
    const spellCheckForcedPref =
        this.getPref<string[]>('spellcheck.forced_dictionaries');
    const spellCheckBlockedPref =
        this.getPref<string[]>('spellcheck.blocked_dictionaries');
    const languageForcedSet = this.makeSetFromArray_(languagesForcedPref.value);
    const spellCheckSet = this.makeSetFromArray_(
        spellCheckPref.value.concat(spellCheckForcedPref.value));
    const spellCheckForcedSet =
        this.makeSetFromArray_(spellCheckForcedPref.value);
    const spellCheckBlockedSet =
        this.makeSetFromArray_(spellCheckBlockedPref.value);

    const translateBlockedPrefValue =
        this.getPref<string[]>('translate_blocked_languages').value;
    const translateBlockedSet =
        this.makeSetFromArray_(translateBlockedPrefValue);

    const enabledLanguageStates: LanguageState[] = [];

    for (let i = 0; i < enabledLanguageCodes.length; i++) {
      const code = enabledLanguageCodes[i];
      const language = this.supportedLanguageMap_.get(code);
      // Skip unsupported languages.
      if (!language) {
        continue;
      }
      const languageState: LanguageState = {
        language: language,
        spellCheckEnabled:
            spellCheckSet.has(code) && !spellCheckBlockedSet.has(code) ||
            spellCheckForcedSet.has(code),
        translateEnabled: this.isTranslateEnabled_(
            code, !!language.supportsTranslate, translateBlockedSet,
            translateTarget, prospectiveUILanguage),
        isManaged:
            spellCheckForcedSet.has(code) || spellCheckBlockedSet.has(code),
        isForced: languageForcedSet.has(code),
        downloadDictionaryFailureCount: 0,
        removable: false,
        downloadDictionaryStatus: null,
      };
      enabledLanguageStates.push(languageState);
    }
    return enabledLanguageStates;
  }

  /**
   * True iff we translate pages that are in the given language.
   * @param code Language code.
   * @param supportsTranslate If translation supports the given language.
   * @param translateBlockedSet Set of languages for which translation is
   *     blocked.
   * @param translateTarget Language code of the default translate target
   *     language.
   * @param prospectiveUILanguage Prospective UI display language. Only define
   *     on Windows and Chrome OS.
   */
  private isTranslateEnabled_(
      code: string, supportsTranslate: boolean,
      translateBlockedSet: Set<string>, translateTarget: string,
      prospectiveUILanguage: string|undefined): boolean {
    const translateCode = this.convertLanguageCodeForTranslate(code);
    return supportsTranslate && !translateBlockedSet.has(translateCode) &&
        translateCode !== translateTarget &&
        (!prospectiveUILanguage || code !== prospectiveUILanguage);
  }

  // <if expr="not is_macosx">
  /**
   * Updates the dictionary download status for spell check languages in order
   * to track the number of times a spell check dictionary download has failed.
   */
  private onSpellcheckDictionariesChanged_(
      statuses: chrome.languageSettingsPrivate.SpellcheckDictionaryStatus[]) {
    const statusMap = new Map();
    statuses.forEach(status => {
      statusMap.set(status.languageCode, status);
    });

    const collectionNames =
        ['enabled', 'spellCheckOnLanguages', 'spellCheckOffLanguages'];
    const languages = this.languages as unknown as
        {[k: string]: Array<LanguageState|SpellCheckLanguageState>};
    collectionNames.forEach(collectionName => {
      languages[collectionName].forEach((languageState, index) => {
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
  }
  // </if>

  /**
   * Updates the |removable| property of the enabled language states based
   * on what other languages and input methods are enabled.
   */
  private updateRemovableLanguages_() {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }

    for (let i = 0; i < this.languages.enabled.length; i++) {
      const languageState = this.languages.enabled[i];
      this.set(
          'languages.enabled.' + i + '.removable',
          this.canDisableLanguage(languageState));
    }
  }

  /**
   * Creates a Set from the elements of the array.
   */
  private makeSetFromArray_<T>(list: T[]): Set<T> {
    return new Set(list);
  }

  // LanguageHelper implementation.
  whenReady(): Promise<void> {
    return this.resolver_.promise;
  }

  // <if expr="is_win">
  /**
   * Sets the prospective UI language to the chosen language. This won't affect
   * the actual UI language until a restart.
   */
  setProspectiveUiLanguage(languageCode: string) {
    this.browserProxy_.setProspectiveUiLanguage(languageCode);
  }

  /**
   * True if the prospective UI language was changed from its starting value.
   */
  requiresRestart(): boolean {
    return this.originalProspectiveUILanguage_ !==
        this.languages!.prospectiveUILanguage;
  }
  // </if>

  /**
   * @return The language code for ARC IMEs.
   */
  getArcImeLanguageCode(): string {
    return kArcImeLanguage;
  }

  /**
   * @param language
   * @return the [displayName] - [nativeDisplayName] if displayName and
   * nativeDisplayName are different.
   * If they're the same than only returns the displayName.
   */
  getFullName(language: chrome.languageSettingsPrivate.Language): string {
    let fullName = language.displayName;
    if (language.displayName !== language.nativeDisplayName) {
      fullName += ' - ' + language.nativeDisplayName;
    }
    return fullName;
  }

  /**
   * @return True if the language is for ARC IMEs.
   */
  isLanguageCodeForArcIme(languageCode: string): boolean {
    return languageCode === kArcImeLanguage;
  }

  /**
   *  @return True if the language is supported by Translate as a base and not
   * an extended sub-code (i.e. "it-CH" and "es-MX" are both marked as
   * supporting translation but only "it" and "es" are actually supported by the
   * Translate server.
   */
  isTranslateBaseLanguage(language: chrome.languageSettingsPrivate.Language):
      boolean {
    // The language must be marked as translatable.
    if (!language.supportsTranslate) {
      return false;
    }

    if (language.code === 'zh-CN' || language.code === 'zh-TW') {
      // In Translate, general Chinese is not used, and the sub code is
      // necessary as a language code for the Translate server.
      return true;
    }

    if (language.code === 'mni-Mtei') {
      // Translate uses the Meitei Mayek script for Manipuri
      return true;
    }

    const baseLanguage = this.getBaseLanguage(language.code);
    if (baseLanguage === 'nb') {
      // Norwegian Bokmål (nb) is listed as supporting translate but the
      // Translate server only supports Norwegian (no).
      return false;
    }
    // For all other languages only base languages are supported
    return language.code === baseLanguage;
  }

  /**
   * @return True if the language is enabled.
   */
  isLanguageEnabled(languageCode: string): boolean {
    return this.enabledLanguageSet_.has(languageCode);
  }

  /**
   * Enables the language, making it available for spell check and input.
   */
  enableLanguage(languageCode: string) {
    if (!CrSettingsPrefs.isInitialized) {
      return;
    }

    this.languageSettingsPrivate_.enableLanguage(languageCode);
  }

  /**
   * Disables the language.
   */
  disableLanguage(languageCode: string) {
    if (!CrSettingsPrefs.isInitialized) {
      return;
    }

    // Remove the language from spell check.
    this.deletePrefListItem('spellcheck.dictionaries', languageCode);

    // Remove the language from preferred languages.
    this.languageSettingsPrivate_.disableLanguage(languageCode);
  }

  canDisableLanguage(_languageState: LanguageState): boolean {
    // <if expr="is_win">
    // Cannot disable the prospective UI language.
    if (_languageState.language.code ===
        this.languages!.prospectiveUILanguage) {
      return false;
    }
    // </if>

    // Cannot disable the only enabled language.
    if (this.languages!.enabled.length === 1) {
      return false;
    }

    return true;
  }

  canEnableLanguage(language: chrome.languageSettingsPrivate.Language):
      boolean {
    return !(
        this.isLanguageEnabled(language.code) ||
        language.isProhibitedLanguage ||
        this.isLanguageCodeForArcIme(language.code) /* internal use only */);
  }

  /**
   * Sets whether a given language should always be automatically translated.
   */
  setLanguageAlwaysTranslateState(
      languageCode: string, alwaysTranslate: boolean) {
    this.languageSettingsPrivate_.setLanguageAlwaysTranslateState(
        languageCode, alwaysTranslate);
  }

  /**
   * Moves the language in the list of enabled languages either up (toward the
   * front of the list) or down (toward the back).
   * @param upDirection True if we need to move up, false if we need to move
   *     down
   */
  moveLanguage(languageCode: string, upDirection: boolean) {
    if (!CrSettingsPrefs.isInitialized) {
      return;
    }

    if (upDirection) {
      this.languageSettingsPrivate_.moveLanguage(languageCode, MoveType.UP);
    } else {
      this.languageSettingsPrivate_.moveLanguage(languageCode, MoveType.DOWN);
    }
  }

  /**
   * Moves the language directly to the front of the list of enabled languages.
   */
  moveLanguageToFront(languageCode: string) {
    if (!CrSettingsPrefs.isInitialized) {
      return;
    }

    this.languageSettingsPrivate_.moveLanguage(languageCode, MoveType.TOP);
  }

  /**
   * Enables translate for the given language by removing the translate
   * language from the blocked languages preference.
   */
  enableTranslateLanguage(languageCode: string) {
    this.languageSettingsPrivate_.setEnableTranslationForLanguage(
        languageCode, true);
  }

  /**
   * Disables translate for the given language by adding the translate
   * language to the blocked languages preference.
   */
  disableTranslateLanguage(languageCode: string) {
    this.languageSettingsPrivate_.setEnableTranslationForLanguage(
        languageCode, false);
  }

  /**
   * Sets the translate target language.
   */
  setTranslateTargetLanguage(languageCode: string) {
    this.languageSettingsPrivate_.setTranslateTargetLanguage(languageCode);
  }

  /**
   * Enables or disables spell check for the given language.
   */
  toggleSpellCheck(languageCode: string, enable: boolean) {
    if (!this.languages) {
      return;
    }

    if (enable) {
      this.getPref('spellcheck.dictionaries');
      this.appendPrefListItem('spellcheck.dictionaries', languageCode);
    } else {
      this.deletePrefListItem('spellcheck.dictionaries', languageCode);
    }
  }

  /**
   * Converts the language code to Translate server format where some deprecated
   * ISO 639 codes are used. The only sub-codes that Translate supports are for
   * "zh" where zh-HK is equivalent to zh-TW. For all other languages only
   * the base language is returned.
   */
  convertLanguageCodeForTranslate(languageCode: string): string {
    const base = this.getBaseLanguage(languageCode);
    if (base === 'zh') {
      return languageCode === 'zh-HK' ? 'zh-TW' : languageCode;
    }

    return kChromeToTranslateCode.get(base) || base;
  }

  /**
   * Converts deprecated ISO 639 language codes to Chrome format.
   */
  convertLanguageCodeForChrome(languageCode: string): string {
    return kTranslateToChromeCode.get(languageCode) || languageCode;
  }

  /**
   * Given a language code, returns just the base language without sub-codes.
   */
  getBaseLanguage(languageCode: string): string {
    return languageCode.split('-')[0];
  }

  getLanguage(languageCode: string): chrome.languageSettingsPrivate.Language
      |undefined {
    if (this.supportedLanguageMap_.has(languageCode)) {
      return this.supportedLanguageMap_.get(languageCode);
    }

    // If no languageCode is found, try the base Chrome format.
    const chromeLanguage =
        this.convertLanguageCodeForChrome(this.getBaseLanguage(languageCode));
    return this.supportedLanguageMap_.get(chromeLanguage);
  }

  /**
   * Retries downloading the dictionary for |languageCode|.
   */
  retryDownloadDictionary(languageCode: string) {
    this.languageSettingsPrivate_.retryDownloadDictionary(languageCode);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-languages': SettingsLanguagesElement;
  }
}

customElements.define(SettingsLanguagesElement.is, SettingsLanguagesElement);
