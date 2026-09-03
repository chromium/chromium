// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'LanguageHelper' handles Chrome's language and input
 * method settings. The 'languages' property, which reflects the current
 * language settings, must not be changed directly. Instead, changes to
 * language settings should be made using the LanguageHelper APIs provided by
 * this class via the LanguageHelper singleton instance.
 */

import {assert} from '//resources/js/assert.js';
import {PromiseResolver} from '//resources/js/promise_resolver.js';
import {PrefService} from '/shared/settings/prefs2/pref_service.js';

import type {LanguagesBrowserProxy} from './languages_browser_proxy.js';
import {LanguagesBrowserProxyImpl} from './languages_browser_proxy.js';
import type {LanguageHelper, LanguagesModel, LanguageState, SpellCheckLanguageState} from './languages_types.js';
import {convertLanguageCodeForChrome, convertLanguageCodeForTranslate, getBaseLanguage} from './languages_util.js';

interface SpellCheckLanguages {
  on: SpellCheckLanguageState[];
  off: SpellCheckLanguageState[];
}

const MoveType = chrome.languageSettingsPrivate.MoveType;

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

let instance: LanguageHelperImpl|null = null;

export function getLanguageHelperInstance(): LanguageHelper {
  if (!instance) {
    instance = new LanguageHelperImpl();
  }
  return instance;
}

/**
 * Singleton class that generates the languages model on start-up and
 * updates it whenever Chrome's pref store and other settings change.
 */
export class LanguageHelperImpl extends EventTarget implements LanguageHelper {
  static resetInstanceForTesting(newInstance: LanguageHelperImpl|null = null) {
    if (instance) {
      instance.destroy();
    }
    instance = newInstance;
  }

  languages: LanguagesModel|undefined;

  private intlAcceptLanguagesPref_?: chrome.settingsPrivate.PrefObject<string>;
  private intlAppLocalePref_?: chrome.settingsPrivate.PrefObject<string>;
  private intlForcedLanguagesPref_?:
      chrome.settingsPrivate.PrefObject<string[]>;
  private spellcheckBlockedDictionariesPref_?:
      chrome.settingsPrivate.PrefObject<string[]>;
  private spellcheckDictionariesPref_?:
      chrome.settingsPrivate.PrefObject<string[]>;
  private spellcheckForcedDictionariesPref_?:
      chrome.settingsPrivate.PrefObject<string[]>;
  private translateAllowlistsPref_?:
      chrome.settingsPrivate.PrefObject<Record<string, string>>;
  private translateBlockedLanguagesPref_?:
      chrome.settingsPrivate.PrefObject<string[]>;
  private translateRecentTargetPref_?:
      chrome.settingsPrivate.PrefObject<string>;
  private translateSiteBlocklistPref_?:
      chrome.settingsPrivate.PrefObject<Record<string, string>>;

  private resolver_: PromiseResolver<void> = new PromiseResolver();
  private supportedLanguageMap_:
      Map<string, chrome.languageSettingsPrivate.Language> = new Map();
  private enabledLanguageSet_: Set<string> = new Set();
  private observerIds_: number[] = [];

  // <if expr="is_win">
  /** Prospective UI language when the page was loaded. */
  private originalProspectiveUILanguage_: string = '';
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

    this.init_();
  }

  private init_() {
    this.observePrefs_();

    const promises: Array<Promise<void>> = [];

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
    promises.push(PrefService.getInstance().whenInitialized());

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

  private observePref_<T>(
      key: string,
      setPref: (pref: chrome.settingsPrivate.PrefObject<T>) => void,
      onChange: () => void) {
    this.observerIds_.push(
        PrefService.getInstance().addObserver<T>(key, pref => {
          setPref(pref as chrome.settingsPrivate.PrefObject<T>);
          if (!this.languages) {
            return;
          }
          onChange();
          this.dispatchLanguagesChanged_();
        }));
  }

  private observePrefs_() {
    this.observePref_<string>(
        'intl.accept_languages', pref => this.intlAcceptLanguagesPref_ = pref,
        () => this.preferredLanguagesPrefChanged_());
    this.observePref_<string>(
        'intl.app_locale', pref => this.intlAppLocalePref_ = pref, () => {
          // <if expr="is_win">
          this.prospectiveUiLanguageChanged_();
          // </if>
          this.updateRemovableLanguages_();
        });
    this.observePref_<string[]>(
        'intl.forced_languages', pref => this.intlForcedLanguagesPref_ = pref,
        () => this.preferredLanguagesPrefChanged_());
    this.observePref_<string[]>(
        'spellcheck.blocked_dictionaries',
        pref => this.spellcheckBlockedDictionariesPref_ = pref,
        () => this.spellCheckDictionariesPrefChanged_());
    this.observePref_<string[]>(
        'spellcheck.dictionaries',
        pref => this.spellcheckDictionariesPref_ = pref,
        () => this.spellCheckDictionariesPrefChanged_());
    this.observePref_<string[]>(
        'spellcheck.forced_dictionaries',
        pref => this.spellcheckForcedDictionariesPref_ = pref,
        () => this.spellCheckDictionariesPrefChanged_());
    this.observePref_<Record<string, string>>(
        'translate_allowlists', pref => this.translateAllowlistsPref_ = pref,
        () => this.alwaysTranslateLanguagesPrefChanged_());
    this.observePref_<string[]>(
        'translate_blocked_languages',
        pref => this.translateBlockedLanguagesPref_ = pref, () => {
          this.neverTranslateLanguagesPrefChanged_();
          this.translateLanguagesPrefChanged_();
          this.updateRemovableLanguages_();
        });
    this.observePref_<string>(
        'translate_recent_target',
        pref => this.translateRecentTargetPref_ = pref,
        () => this.translateTargetPrefChanged_());
    this.observePref_<Record<string, string>>(
        'translate_site_blocklist_with_time',
        pref => this.translateSiteBlocklistPref_ = pref,
        () => this.neverTranslateSitesPrefChanged_());
  }

  destroy() {
    const prefService = PrefService.getInstance();
    for (const id of this.observerIds_) {
      prefService.removeObserver(id);
    }
    this.observerIds_ = [];

    // <if expr="not is_macosx">
    if (this.boundOnSpellcheckDictionariesChanged_) {
      this.languageSettingsPrivate_.onSpellcheckDictionariesChanged
          .removeListener(this.boundOnSpellcheckDictionariesChanged_);
      this.boundOnSpellcheckDictionariesChanged_ = null;
    }
    // </if>
  }

  private dispatchLanguagesChanged_() {
    assert(this.languages);
    this.languages = Object.assign({}, this.languages);
    this.dispatchEvent(new CustomEvent('languages-changed', {
      detail: this.languages,
    }));
  }

  // <if expr="is_win">
  /**
   * Updates the prospective UI language based on the new pref value.
   */
  private prospectiveUiLanguageChanged_() {
    if (this.intlAppLocalePref_ === undefined || this.languages === undefined) {
      return;
    }
    this.languages.prospectiveUILanguage =
        this.intlAppLocalePref_.value || this.originalProspectiveUILanguage_;
  }
  // </if>

  /**
   * Updates the list of enabled languages from the preferred languages pref.
   */
  private preferredLanguagesPrefChanged_() {
    if (this.intlAcceptLanguagesPref_ === undefined ||
        this.intlForcedLanguagesPref_ === undefined ||
        this.languages === undefined) {
      return;
    }

    const enabledLanguageStates = this.getEnabledLanguageStates_(
        this.languages.translateTarget, this.languages.prospectiveUILanguage);

    // Recreate the enabled language set before updating languages.enabled.
    this.enabledLanguageSet_.clear();
    for (let i = 0; i < enabledLanguageStates.length; i++) {
      this.enabledLanguageSet_.add(enabledLanguageStates[i].language.code);
    }

    this.languages.enabled = enabledLanguageStates;
    this.updateRemovableLanguages_();

    // <if expr="not is_macosx">
    if (this.boundOnSpellcheckDictionariesChanged_) {
      this.languageSettingsPrivate_.getSpellcheckDictionaryStatuses().then(
          this.boundOnSpellcheckDictionariesChanged_);
    }
    // </if>

    // Update translate target language.
    this.languageSettingsPrivate_.getTranslateTargetLanguage().then(result => {
      this.languages!.translateTarget = result;
      this.dispatchLanguagesChanged_();
    });
  }

  /**
   * Updates the spellCheckEnabled state of each enabled language.
   */
  private spellCheckDictionariesPrefChanged_() {
    if (this.spellcheckDictionariesPref_ === undefined ||
        this.spellcheckForcedDictionariesPref_ === undefined ||
        this.spellcheckBlockedDictionariesPref_ === undefined ||
        this.languages === undefined) {
      return;
    }

    const spellCheckSet =
        this.makeSetFromArray_(this.spellcheckDictionariesPref_.value);
    const spellCheckForcedSet =
        this.makeSetFromArray_(this.spellcheckForcedDictionariesPref_.value);
    const spellCheckBlockedSet =
        this.makeSetFromArray_(this.spellcheckBlockedDictionariesPref_.value);

    this.languages.enabled = this.languages.enabled.map(languageState => {
      const isUser = spellCheckSet.has(languageState.language.code);
      const isForced = spellCheckForcedSet.has(languageState.language.code);
      const isBlocked = spellCheckBlockedSet.has(languageState.language.code);
      return {
        ...languageState,
        spellCheckEnabled: (isUser && !isBlocked) || isForced,
        isManaged: isForced || isBlocked,
      };
    });

    const {on: spellCheckOnLanguages, off: spellCheckOffLanguages} =
        this.getSpellCheckLanguages_(this.languages.supported);
    this.languages.spellCheckOnLanguages = spellCheckOnLanguages;
    this.languages.spellCheckOffLanguages = spellCheckOffLanguages;
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
    const getPrefAndDedupe =
        (pref: chrome.settingsPrivate.PrefObject<string[]>): string[] => {
          const result = pref.value.filter(x => !seenCodes.has(x));
          result.forEach((code: string) => seenCodes.add(code));
          return result;
        };

    assert(this.spellcheckForcedDictionariesPref_);
    const forcedCodes =
        getPrefAndDedupe(this.spellcheckForcedDictionariesPref_);
    const forcedCodesSet = new Set(forcedCodes);
    assert(this.spellcheckBlockedDictionariesPref_);
    const blockedCodes =
        getPrefAndDedupe(this.spellcheckBlockedDictionariesPref_);
    const blockedCodesSet = new Set(blockedCodes);
    assert(this.spellcheckDictionariesPref_);
    const enabledCodes = getPrefAndDedupe(this.spellcheckDictionariesPref_);

    const on: SpellCheckLanguageState[] = [];
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
    const off: SpellCheckLanguageState[] = [];

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
    if (this.translateAllowlistsPref_ === undefined ||
        this.languages === undefined) {
      return;
    }
    const alwaysTranslateCodes =
        Object.keys(this.translateAllowlistsPref_.value);
    const alwaysTranslateLanguages =
        alwaysTranslateCodes.map((code: string) => this.getLanguage(code)!);
    this.languages.alwaysTranslate = alwaysTranslateLanguages;
  }

  /**
   * Updates the list of never translate languages from translate prefs.
   */
  private neverTranslateLanguagesPrefChanged_() {
    if (this.translateBlockedLanguagesPref_ === undefined ||
        this.languages === undefined) {
      return;
    }
    const neverTranslateCodes = this.translateBlockedLanguagesPref_.value;
    const neverTranslateLanguages =
        neverTranslateCodes.map(code => this.getLanguage(code)!);
    this.languages.neverTranslate = neverTranslateLanguages;
  }

  /**
   * Updates the list of never translate sites from translate prefs.
   */
  private neverTranslateSitesPrefChanged_() {
    if (this.translateSiteBlocklistPref_ === undefined ||
        this.languages === undefined) {
      return;
    }
    const neverTranslateSites =
        Object.keys(this.translateSiteBlocklistPref_.value);
    this.languages.neverTranslateSites = neverTranslateSites;
  }

  private translateLanguagesPrefChanged_() {
    if (this.translateBlockedLanguagesPref_ === undefined ||
        this.languages === undefined) {
      return;
    }

    const translateBlockedPrefValue = this.translateBlockedLanguagesPref_.value;
    const translateBlockedSet =
        this.makeSetFromArray_(translateBlockedPrefValue);

    this.languages.enabled = this.languages.enabled.map(languageState => {
      const language = languageState.language;
      const translateEnabled = this.isTranslateEnabled_(
          language.code, !!language.supportsTranslate, translateBlockedSet,
          this.languages!.translateTarget,
          this.languages!.prospectiveUILanguage);
      return {...languageState, translateEnabled};
    });
  }

  private translateTargetPrefChanged_() {
    if (this.translateRecentTargetPref_ === undefined ||
        this.languages === undefined) {
      return;
    }
    this.languages.translateTarget = this.translateRecentTargetPref_.value;
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
    prospectiveUILanguage =
        this.intlAppLocalePref_?.value || this.originalProspectiveUILanguage_;
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

    // Initialize the languages model.
    this.languages = model;
    this.updateRemovableLanguages_();
    this.dispatchLanguagesChanged_();
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
    const enabledLanguageCodes =
        (this.intlAcceptLanguagesPref_?.value || '').split(',');
    const languageForcedSet =
        this.makeSetFromArray_(this.intlForcedLanguagesPref_?.value || []);
    const spellCheckSet = this.makeSetFromArray_(
        (this.spellcheckDictionariesPref_?.value ||
         []).concat(this.spellcheckForcedDictionariesPref_?.value || []));
    const spellCheckForcedSet = this.makeSetFromArray_(
        this.spellcheckForcedDictionariesPref_?.value || []);
    const spellCheckBlockedSet = this.makeSetFromArray_(
        this.spellcheckBlockedDictionariesPref_?.value || []);

    const translateBlockedPrefValue =
        this.translateBlockedLanguagesPref_?.value || [];
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
    const translateCode = convertLanguageCodeForTranslate(code);
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
    assert(this.languages);
    const statusMap = new Map();
    statuses.forEach(status => {
      statusMap.set(status.languageCode, status);
    });

    const updateStates = <T extends LanguageState|SpellCheckLanguageState>(
        states: T[]): T[] => {
      return states.map(languageState => {
        const status = statusMap.get(languageState.language.code);
        if (!status) {
          return languageState;
        }

        const previousStatus = languageState.downloadDictionaryStatus;
        let downloadDictionaryFailureCount =
            languageState.downloadDictionaryFailureCount;
        if (status.downloadFailed &&
            !(previousStatus && previousStatus.downloadFailed)) {
          downloadDictionaryFailureCount++;
        } else if (
            status.isReady && !(previousStatus && previousStatus.isReady)) {
          downloadDictionaryFailureCount = 0;
        }

        return {
          ...languageState,
          downloadDictionaryStatus: status,
          downloadDictionaryFailureCount,
        };
      });
    };

    this.languages.enabled = updateStates(this.languages.enabled);
    this.languages.spellCheckOnLanguages =
        updateStates(this.languages.spellCheckOnLanguages);
    this.languages.spellCheckOffLanguages =
        updateStates(this.languages.spellCheckOffLanguages);
    this.dispatchLanguagesChanged_();
  }
  // </if>

  /**
   * Updates the |removable| property of the enabled language states based
   * on what other languages and input methods are enabled.
   */
  private updateRemovableLanguages_() {
    if (this.intlAppLocalePref_ === undefined ||
        this.translateBlockedLanguagesPref_ === undefined ||
        this.languages === undefined) {
      return;
    }

    this.languages.enabled = this.languages.enabled.map(languageState => {
      return {
        ...languageState,
        removable: this.canDisableLanguage(languageState),
      };
    });
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
   * @return True if the language is for ARC IMEs.
   */
  private isLanguageCodeForArcIme_(languageCode: string): boolean {
    return languageCode === kArcImeLanguage;
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
    this.languageSettingsPrivate_.enableLanguage(languageCode);
  }

  /**
   * Disables the language.
   */
  disableLanguage(languageCode: string) {
    // Remove the language from spell check.
    const pref = this.spellcheckDictionariesPref_!;
    const index = pref.value.indexOf(languageCode);
    if (index !== -1) {
      const updated = [...pref.value];
      updated.splice(index, 1);
      PrefService.getInstance().setPrefValue<string[]>(
          'spellcheck.dictionaries', updated);
    }

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
        (this.isLanguageEnabled(language.code) ||
         language.isProhibitedLanguage ||
         this.isLanguageCodeForArcIme_(language.code)) /* internal use only */);
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

    const pref = this.spellcheckDictionariesPref_!;
    if (enable) {
      PrefService.getInstance().appendPrefListItem<string>(
          'spellcheck.dictionaries', languageCode);
    } else {
      const index = pref.value.indexOf(languageCode);
      if (index !== -1) {
        const updated = [...pref.value];
        updated.splice(index, 1);
        PrefService.getInstance().setPrefValue<string[]>(
            'spellcheck.dictionaries', updated);
      }
    }
  }

  getLanguage(languageCode: string): chrome.languageSettingsPrivate.Language
      |undefined {
    if (this.supportedLanguageMap_.has(languageCode)) {
      return this.supportedLanguageMap_.get(languageCode);
    }

    // If no languageCode is found, try the base Chrome format.
    const chromeLanguage =
        convertLanguageCodeForChrome(getBaseLanguage(languageCode));
    return this.supportedLanguageMap_.get(chromeLanguage);
  }

  /**
   * Retries downloading the dictionary for |languageCode|.
   */
  retryDownloadDictionary(languageCode: string) {
    this.languageSettingsPrivate_.retryDownloadDictionary(languageCode);
  }
}
