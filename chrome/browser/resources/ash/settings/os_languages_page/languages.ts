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

// TODO(b/263828712): Upstream and downstream changes from browser settings, and
// consider merging the two.

import '/shared/settings/prefs/prefs.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrSettingsPrefs} from '/shared/settings/prefs/prefs_types.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';

import {LanguagesBrowserProxy, LanguagesBrowserProxyImpl} from './languages_browser_proxy.js';
import {LanguageHelper, LanguagesModel, LanguageState, SpellCheckLanguageState} from './languages_types.js';

const MoveType = chrome.languageSettingsPrivate.MoveType;

// Translate server treats some language codes the same.
// See also: components/translate/core/common/translate_util.cc.
const kLanguageCodeToTranslateCode = {
  'nb': 'no',
  'fil': 'tl',
  'zh-HK': 'zh-TW',
  'zh-MO': 'zh-TW',
  'zh-SG': 'zh-CN',
} as const;

// Some ISO 639 language codes have been renamed, e.g. "he" to "iw", but
// Translate still uses the old versions. TODO(michaelpg): Chrome does too.
// Follow up with Translate owners to understand the right thing to do.
const kTranslateLanguageSynonyms = {
  he: 'iw',
  jv: 'jw',
} as const;

// The fake language name used for ARC IMEs. The value must be in sync with the
// one in ui/base/ime/ash/extension_ime_util.h.
const kArcImeLanguage = '_arc_ime_language_';

// The IME ID for the Accessibility Common extension used by Dictation.
export const ACCESSIBILITY_COMMON_IME_ID =
    '_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation';

interface ModelArgs {
  // Unused.
  supportedLanguages: chrome.languageSettingsPrivate.Language[];
  translateTarget: string;
  alwaysTranslateCodes: string[];
  neverTranslateCodes: string[];
  startingUILanguage: string;
  // TODO(b/263824661): Remove undefined from these definitions if we do not
  // share this file with Chrome browser.
  /** Always defined on CrOS. */
  supportedInputMethods: (chrome.languageSettingsPrivate.InputMethod[]|
                          undefined);
  /** Always defined on CrOS. */
  currentInputMethodId: (string|undefined);
}

/**
 * Singleton element that generates the languages model on start-up and
 * updates it whenever Chrome's pref store and other settings change.
 */
const SettingsLanguagesElementBase = PrefsMixin(PolymerElement);

export class SettingsLanguagesElement extends SettingsLanguagesElementBase
    implements LanguageHelper {
  static get is() {
    return 'settings-languages' as const;
  }

  static get properties() {
    return {
      languages: {
        type: Object,
        notify: true,
        // TODO(b/238031866): Remove readOnly here and set `this.languages` with
        // an assignment instead of a `this._setProperty`. See
        // https://crrev.com/c/3176181/comment/63b644b9_ee7ad7df/ for more
        // details.
        readOnly: true,
      },

      /**
       * This element, as a LanguageHelper instance for API usage.
       */
      languageHelper: {
        type: Object,
        notify: true,
        readOnly: true,
        value(this: SettingsLanguagesElement): LanguageHelper {
          return this;
        },
      },

      /**
       * PromiseResolver to be resolved when the singleton has been initialized.
       */
      resolver_: {
        type: Object,
        value() {
          return new PromiseResolver();
        },
      },

      /**
       * Hash map of supported languages by language codes for fast lookup.
       */
      supportedLanguageMap_: {
        type: Object,
        value() {
          return new Map();
        },
      },

      /**
       * Hash set of enabled language codes for membership testing.
       */
      enabledLanguageSet_: {
        type: Object,
        value() {
          return new Set();
        },
      },

      /**
       * Hash map of supported input methods by ID for fast lookup.
       */
      supportedInputMethodMap_: {
        type: Object,
        value() {
          return new Map();
        },
      },

      /**
       * Hash map of input methods supported for each language.
       */
      languageInputMethods_: {
        type: Object,
        value() {
          return new Map();
        },
      },

      /**
       * Hash set of enabled input methods id for mebership testings
       */
      enabledInputMethodSet_: {
        type: Object,
        value() {
          return new Set();
        },
      },

      /** Prospective UI language when the page was loaded. */
      originalProspectiveUILanguage_: String,
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
      'prospectiveUiLanguageChanged_(prefs.intl.app_locale.value, languages)',
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
      // Observe Chrome OS prefs (ignored for non-Chrome OS).
      'updateRemovableLanguages_(' +
          'prefs.settings.language.preload_engines.value, ' +
          'prefs.settings.language.enabled_extension_imes.value, ' +
          'languages)',
    ];
  }

  // Public API: Bidirectional data flow.
  // override prefs: any;  // From PrefsMixin.

  // Public API: Upwards data flow.
  languages?: LanguagesModel;
  languageHelper: LanguageHelper;

  // API proxies.
  private browserProxy_: LanguagesBrowserProxy =
      LanguagesBrowserProxyImpl.getInstance();
  private languageSettingsPrivate_: typeof chrome.languageSettingsPrivate =
      this.browserProxy_.getLanguageSettingsPrivate();
  private inputMethodPrivate_: typeof chrome.inputMethodPrivate =
      this.browserProxy_.getInputMethodPrivate();

  // Internal state.
  private resolver_: PromiseResolver<undefined>;
  private supportedLanguageMap_:
      Map<string, chrome.languageSettingsPrivate.Language>;
  private enabledLanguageSet_: Set<string>;
  private supportedInputMethodMap_:
      Map<string, chrome.languageSettingsPrivate.InputMethod>;
  private languageInputMethods_:
      Map<string, chrome.languageSettingsPrivate.InputMethod[]>;
  private enabledInputMethodSet_: Set<string>;
  private originalProspectiveUILanguage_?: string;

  // Bound methods.
  // Instances of SettingsLanguagesElement below should be replaced with
  // (typeof this) due to possible subclasses of SettingsLanguagesElement
  // replacing these methods with a Liskov substitution principle-compatible
  // method. However, that type is too complicated for TypeScript to check (it
  // results in incorrect type errors), and we don't expect there to be any
  // subclasses.
  private boundOnSpellcheckDictionariesChanged_: OmitThisParameter<
      SettingsLanguagesElement['onSpellcheckDictionariesChanged_']>|null = null;
  private boundOnInputMethodAdded_:
      OmitThisParameter<SettingsLanguagesElement['onInputMethodAdded_']>|null =
          null;
  private boundOnInputMethodRemoved_:
      OmitThisParameter<SettingsLanguagesElement['onInputMethodRemoved_']>|
      null = null;
  private boundOnInputMethodChanged_:
      OmitThisParameter<SettingsLanguagesElement['onInputMethodChanged_']>|
      null = null;
  private boundOnLanguagePackStatusChanged_: OmitThisParameter<
      SettingsLanguagesElement['onLanguagePackStatusChanged_']>|null = null;

  // loadTimeData flags.
  // We do not expect this to change over the lifetime of this element, so this
  // is not included in `properties()` above.
  private languagePacksInSettingsEnabled_ =
      loadTimeData.getBoolean('languagePacksInSettingsEnabled');

  override connectedCallback(): void {
    super.connectedCallback();

    const promises = [];

    /**
     * An object passed into createModel to keep track of platform-specific
     * arguments, populated by the "promises" array.
     */
    const args: ModelArgs = {
      supportedLanguages: [],
      translateTarget: '',
      alwaysTranslateCodes: [],
      neverTranslateCodes: [],
      startingUILanguage: '',

      supportedInputMethods: [],
      currentInputMethodId: '',
    };

    // Wait until prefs are initialized before creating the model, so we can
    // include information about enabled languages.
    promises.push(CrSettingsPrefs.initialized);

    // Get the language list.
    promises.push(this.languageSettingsPrivate_.getLanguageList().then(
        result => args.supportedLanguages = result));

    // Get the translate target language.
    promises.push(
        this.languageSettingsPrivate_.getTranslateTargetLanguage().then(
            result => args.translateTarget = result));

    promises.push(this.languageSettingsPrivate_.getInputMethodLists().then(
        lists => args.supportedInputMethods =
            lists.componentExtensionImes.concat(
                lists.thirdPartyExtensionImes)));

    promises.push(this.inputMethodPrivate_.getCurrentInputMethod().then(
        result => args.currentInputMethodId = result));

    // Get the list of language-codes to always translate.
    promises.push(
        this.languageSettingsPrivate_.getAlwaysTranslateLanguages().then(
            result => args.alwaysTranslateCodes = result));

    // Get the list of language-codes to never translate.
    promises.push(
        this.languageSettingsPrivate_.getNeverTranslateLanguages().then(
            result => args.neverTranslateCodes = result));

    // Fetch the starting UI language, which affects which actions should be
    // enabled.
    promises.push(this.browserProxy_.getProspectiveUiLanguage().then(
        prospectiveUILanguage => {
          this.originalProspectiveUILanguage_ =
              prospectiveUILanguage || window.navigator.language;
        }));

    Promise.all(promises).then(() => {
      if (!this.isConnected) {
        // Return early if this element was detached from the DOM before
        // this async callback executes (can happen during testing).
        return;
      }

      this.createModel_(args);

      this.boundOnSpellcheckDictionariesChanged_ =
          this.onSpellcheckDictionariesChanged_.bind(this);
      this.languageSettingsPrivate_.onSpellcheckDictionariesChanged.addListener(
          this.boundOnSpellcheckDictionariesChanged_);
      this.languageSettingsPrivate_.getSpellcheckDictionaryStatuses().then(
          this.boundOnSpellcheckDictionariesChanged_);

      if (this.languagePacksInSettingsEnabled_) {
        // Get the initial state of language pack statuses.
        // Do so in the next microtask to prevent `connectedCallback()` from
        // failing and stalling tests.
        Promise.resolve().then(() => this.fetchMissingLanguagePackStatuses_());
        this.boundOnLanguagePackStatusChanged_ =
            this.onLanguagePackStatusChanged_.bind(this);
        this.inputMethodPrivate_.onLanguagePackStatusChanged.addListener(
            this.boundOnLanguagePackStatusChanged_);
      }

      this.resolver_.resolve(undefined);
    });

    this.boundOnInputMethodChanged_ = this.onInputMethodChanged_.bind(this);
    this.inputMethodPrivate_.onChanged.addListener(
        this.boundOnInputMethodChanged_);
    this.boundOnInputMethodAdded_ = this.onInputMethodAdded_.bind(this);
    this.languageSettingsPrivate_.onInputMethodAdded.addListener(
        this.boundOnInputMethodAdded_);
    this.boundOnInputMethodRemoved_ = this.onInputMethodRemoved_.bind(this);
    this.languageSettingsPrivate_.onInputMethodRemoved.addListener(
        this.boundOnInputMethodRemoved_);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    // Safety: All bound methods here were set in `connectedCallback`,
    // which is guaranteed to be run before `disconnectedCallback`.
    this.inputMethodPrivate_.onChanged.removeListener(
        castExists(this.boundOnInputMethodChanged_));
    this.boundOnInputMethodChanged_ = null;
    this.languageSettingsPrivate_.onInputMethodAdded.removeListener(
        castExists(this.boundOnInputMethodAdded_));
    this.boundOnInputMethodAdded_ = null;
    this.languageSettingsPrivate_.onInputMethodRemoved.removeListener(
        castExists(this.boundOnInputMethodRemoved_));
    this.boundOnInputMethodRemoved_ = null;

    if (this.boundOnSpellcheckDictionariesChanged_) {
      this.languageSettingsPrivate_.onSpellcheckDictionariesChanged
          .removeListener(this.boundOnSpellcheckDictionariesChanged_);
      this.boundOnSpellcheckDictionariesChanged_ = null;
    }
    if (this.boundOnLanguagePackStatusChanged_) {
      this.inputMethodPrivate_.onLanguagePackStatusChanged.removeListener(
          this.boundOnLanguagePackStatusChanged_);
      this.boundOnLanguagePackStatusChanged_ = null;
    }
  }

  /**
   * Updates the prospective UI language based on the new pref value.
   */
  private prospectiveUiLanguageChanged_(prospectiveUILanguage: string): void {
    this.set(
        'languages.prospectiveUILanguage',
        prospectiveUILanguage || this.originalProspectiveUILanguage_);
  }

  /**
   * Updates the list of enabled languages from the preferred languages pref.
   */
  private preferredLanguagesPrefChanged_(): void {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }

    const enabledLanguageStates = this.getEnabledLanguageStates_(
        this.languages.translateTarget, this.languages.prospectiveUILanguage);

    // Recreate the enabled language set before updating languages.enabled.
    this.enabledLanguageSet_.clear();
    for (const enabledLanguageState of enabledLanguageStates) {
      this.enabledLanguageSet_.add(enabledLanguageState.language.code);
    }

    this.set('languages.enabled', enabledLanguageStates);

    if (this.boundOnSpellcheckDictionariesChanged_) {
      this.languageSettingsPrivate_.getSpellcheckDictionaryStatuses().then(
          this.boundOnSpellcheckDictionariesChanged_);
    }

    // Update translate target language.
    this.languageSettingsPrivate_.getTranslateTargetLanguage().then(result => {
      this.set('languages.translateTarget', result);
    });
  }

  /**
   * Updates the spellCheckEnabled state of each enabled language.
   */
  private spellCheckDictionariesPrefChanged_(): void {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }

    const spellCheckSet = this.makeSetFromArray_(
        this.getPref<string[]>('spellcheck.dictionaries').value);
    const spellCheckForcedSet = this.makeSetFromArray_(
        this.getPref<string[]>('spellcheck.forced_dictionaries').value);
    const spellCheckBlockedSet = this.makeSetFromArray_(
        this.getPref<string[]>('spellcheck.blocked_dictionaries').value);

    for (const [i, languageState] of this.languages.enabled.entries()) {
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
      {on: SpellCheckLanguageState[], off: SpellCheckLanguageState[]} {
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
      result.forEach(code => seenCodes.add(code));
      return result;
    };

    const forcedCodes = getPrefAndDedupe('spellcheck.forced_dictionaries');
    const forcedCodesSet = new Set(forcedCodes);
    const blockedCodes = getPrefAndDedupe('spellcheck.blocked_dictionaries');
    const blockedCodesSet = new Set(blockedCodes);
    const enabledCodes = getPrefAndDedupe('spellcheck.dictionaries');

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
  private alwaysTranslateLanguagesPrefChanged_(): void {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }
    const alwaysTranslateCodes = Object.keys(
        this.getPref<Record<string, string>>('translate_allowlists').value);
    const alwaysTranslateLanguages =
        // This `getLanguage` assertion is potentially unsafe and could fail.
        // TODO(b/265554088): Prove that this assertion is safe, or rewrite this
        // to avoid this assertion.
        alwaysTranslateCodes.map(code => this.getLanguage(code)!);
    this.set('languages.alwaysTranslate', alwaysTranslateLanguages);
  }

  /**
   * Updates the list of never translate languages from translate prefs.
   */
  private neverTranslateLanguagesPrefChanged_(): void {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }
    const neverTranslateCodes =
        this.getPref<string[]>('translate_blocked_languages').value;
    const neverTranslateLanguages =
        // This `getLanguage` assertion is potentially unsafe and could fail.
        // TODO(b/265554088): Prove that this assertion is safe, or rewrite this
        // to avoid this assertion.
        neverTranslateCodes.map(code => this.getLanguage(code)!);
    this.set('languages.neverTranslate', neverTranslateLanguages);
  }

  private translateLanguagesPrefChanged_(): void {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }

    const translateBlockedPref =
        this.getPref<string[]>('translate_blocked_languages');
    const translateBlockedSet =
        this.makeSetFromArray_(translateBlockedPref.value);

    for (const [i, languageState] of this.languages.enabled.entries()) {
      const language = languageState.language;
      const translateEnabled = this.isTranslateEnabled_(
          language.code, !!language.supportsTranslate, translateBlockedSet,
          this.languages.translateTarget, this.languages.prospectiveUILanguage);
      this.set(
          'languages.enabled.' + i + '.translateEnabled', translateEnabled);
    }
  }

  private translateTargetPrefChanged_(): void {
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
  private createModel_(args: ModelArgs): void {
    // Populate the hash map of supported languages.
    for (const language of args.supportedLanguages) {
      language.supportsUI = !!language.supportsUI;
      language.supportsTranslate = !!language.supportsTranslate;
      language.supportsSpellcheck = !!language.supportsSpellcheck;
      language.isProhibitedLanguage = !!language.isProhibitedLanguage;
      this.supportedLanguageMap_.set(language.code, language);
    }

    // The below getPref call should always be defined, so the
    // `this.originalProspectiveUILanguage_` part of this expression is
    // redundant.
    // TODO(b/238031866): Investigate why we have two ways of getting the
    // prospective UI language, and simplify this expression if necessary.
    const prospectiveUILanguage =
        this.getPref<string>('intl.app_locale').value ||
        // Safety: This method is only called after all the promises
        // in `connectedCallback()` have resolved, which includes a promise
        // which sets `this.originalProspectiveUILanguage_`.
        // TODO(b/238031866): Move this variable to `ModelArgs` to avoid this
        // assertion.
        this.originalProspectiveUILanguage_!;

    // Create a list of enabled languages from the supported languages.
    const enabledLanguageStates = this.getEnabledLanguageStates_(
        args.translateTarget, prospectiveUILanguage);
    // Populate the hash set of enabled languages.
    for (const enabledLanguageState of enabledLanguageStates) {
      this.enabledLanguageSet_.add(enabledLanguageState.language.code);
    }

    const {on: spellCheckOnLanguages, off: spellCheckOffLanguages} =
        this.getSpellCheckLanguages_(args.supportedLanguages);

    const alwaysTranslateLanguages =
        // This `getLanguage` assertion is potentially unsafe and could fail.
        // TODO(b/265554088): Prove that this assertion is safe, or rewrite this
        // to avoid this assertion.
        args.alwaysTranslateCodes.map(code => this.getLanguage(code)!);

    const neverTranslateLangauges =
        // This `getLanguage` assertion is potentially unsafe and could fail.
        // TODO(b/265554088): Prove that this assertion is safe, or rewrite this
        // to avoid this assertion.
        args.neverTranslateCodes.map(code => this.getLanguage(code)!);

    // TODO(b/238031866): Remove the use of Partial here.
    const model: Partial<LanguagesModel> = {
      supported: args.supportedLanguages,
      enabled: enabledLanguageStates,
      translateTarget: args.translateTarget,
      alwaysTranslate: alwaysTranslateLanguages,
      neverTranslate: neverTranslateLangauges,
      spellCheckOnLanguages,
      spellCheckOffLanguages,
    };

    model.prospectiveUILanguage = prospectiveUILanguage;

    if (args.supportedInputMethods) {
      this.createInputMethodModel_(args.supportedInputMethods);
    }
    model.inputMethods = {
      // Safety: `ModelArgs.supportedInputMethods` is always defined on CrOS.
      supported: args.supportedInputMethods!,
      enabled: this.getEnabledInputMethods_(),
      // Safety: `ModelArgs.currentInputMethodId` is always defined on CrOS.
      currentId: args.currentInputMethodId!,
      imeLanguagePackStatus: {},
    };

    // Initialize the Polymer languages model.
    // Safety: All properties of `LanguagesModel` were set above.
    this._setProperty('languages', model as LanguagesModel);
  }

  /**
   * Returns a list of LanguageStates for each enabled language in the supported
   * languages list.
   * This must be called after `whenReady()` is resolved.
   * @param translateTarget Language code of the default translate
   *     target language.
   * @param prospectiveUILanguage Prospective UI display
   *     language. Only defined on Windows and Chrome OS.
   */
  private getEnabledLanguageStates_(
      translateTarget: string,
      prospectiveUILanguage: (string|undefined)): LanguageState[] {
    // Safety: Enforced in documentation.
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
        (spellCheckPref.value.concat(spellCheckForcedPref.value)));
    const spellCheckForcedSet =
        this.makeSetFromArray_(spellCheckForcedPref.value);
    const spellCheckBlockedSet =
        this.makeSetFromArray_(spellCheckBlockedPref.value);

    const translateBlockedPref =
        this.getPref<string[]>('translate_blocked_languages');
    const translateBlockedSet =
        this.makeSetFromArray_(translateBlockedPref.value);

    const enabledLanguageStates: LanguageState[] = [];

    for (const code of enabledLanguageCodes) {
      const language = this.supportedLanguageMap_.get(code);
      // Skip unsupported languages.
      if (!language) {
        continue;
      }
      // TODO(b/238031866): Remove the use of Partial here.
      const languageState: Partial<LanguageState> = {};
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
      // This cast is very unsafe as `downloadDictionaryStatus` and `removable`
      // have not been set.
      // TODO(b/265554105): Investigate and remove this cast if possible.
      enabledLanguageStates.push(languageState as LanguageState);
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
   * @param prospectiveUILanguage Prospective UI display language. Only defined
   *     on Windows and Chrome OS.
   */
  private isTranslateEnabled_(
      code: string, supportsTranslate: boolean,
      translateBlockedSet: Set<string>, translateTarget: string,
      prospectiveUILanguage: (string|undefined)): boolean {
    const translateCode = this.convertLanguageCodeForTranslate(code);
    return supportsTranslate && !translateBlockedSet.has(translateCode) &&
        translateCode !== translateTarget &&
        (!prospectiveUILanguage || code !== prospectiveUILanguage);
  }

  /**
   * Updates the dictionary download status for spell check languages in order
   * to track the number of times a spell check dictionary download has failed.
   */
  private onSpellcheckDictionariesChanged_(
      statuses: chrome.languageSettingsPrivate.SpellcheckDictionaryStatus[]):
      void {
    const statusMap = new Map<
        string, chrome.languageSettingsPrivate.SpellcheckDictionaryStatus>();
    statuses.forEach(status => {
      statusMap.set(status.languageCode, status);
    });

    const collectionNames =
        ['enabled', 'spellCheckOnLanguages', 'spellCheckOffLanguages'] as const;
    for (const collectionName of collectionNames) {
      // This assertion of `this.languages` is potentially unsafe and could
      // fail.
      // TODO(b/265553377): Prove that this assertion is safe, or rewrite this
      // to avoid this assertion.
      this.languages![collectionName].forEach((languageState, index) => {
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
    }
  }

  /**
   * Updates the |removable| property of the enabled language states based
   * on what other languages and input methods are enabled.
   */
  private updateRemovableLanguages_(): void {
    if (this.prefs === undefined || this.languages === undefined) {
      return;
    }

    // TODO(michaelpg): Enabled input methods can affect which languages are
    // removable, so run updateEnabledInputMethods_ first (if it has been
    // scheduled).
    this.updateEnabledInputMethods_();

    for (const [i, languageState] of this.languages.enabled.entries()) {
      this.set(
          'languages.enabled.' + i + '.removable',
          this.canDisableLanguage(languageState));
    }
  }

  /**
   * Creates a Set from the elements of the array.
   */
  private makeSetFromArray_<T>(list: T[]): Set<T> {
    // TODO(b/238031866): Inline these calls.
    return new Set(list);
  }

  // LanguageHelper implementation.
  // TODO(michaelpg): replace duplicate docs with @override once b/24294625
  // is fixed.
  whenReady(): Promise<void> {
    return this.resolver_.promise;
  }

  /**
   * Sets the prospective UI language to the chosen language. This won't affect
   * the actual UI language until a restart.
   */
  setProspectiveUiLanguage(languageCode: string): void {
    this.browserProxy_.setProspectiveUiLanguage(languageCode);
  }

  /**
   * True if the prospective UI language was changed from its starting value.
   */
  // TODO(b/263824661): Remove this unused method if we do not share this file
  // with browser settings.
  requiresRestart(): boolean {
    return this.originalProspectiveUILanguage_ !==
        // This assertion of `this.languages` is potentially unsafe and could
        // fail.
        // TODO(b/265553377): Prove that this assertion is safe, or rewrite this
        // to avoid this assertion.
        this.languages!.prospectiveUILanguage;
  }

  /**
   * @return The language code for ARC IMEs.
   */
  getArcImeLanguageCode(): string {
    return kArcImeLanguage;
  }

  /**
   * @return True if the language is for ARC IMEs.
   */
  isLanguageCodeForArcIme(languageCode: string): boolean {
    return languageCode === kArcImeLanguage;
  }

  /**
   * @return True if the language can be translated by Chrome.
   */
  isLanguageTranslatable(language: chrome.languageSettingsPrivate.Language):
      boolean {
    if (language.code === 'zh-CN' || language.code === 'zh-TW') {
      // In Translate, general Chinese is not used, and the sub code is
      // necessary as a language code for the Translate server.
      return true;
    }
    if (language.code === this.getLanguageCodeWithoutRegion(language.code) &&
        language.supportsTranslate) {
      return true;
    }
    return false;
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
  enableLanguage(languageCode: string): void {
    if (!CrSettingsPrefs.isInitialized) {
      return;
    }

    this.languageSettingsPrivate_.enableLanguage(languageCode);
  }

  /**
   * Disables the language.
   */
  disableLanguage(languageCode: string): void {
    if (!CrSettingsPrefs.isInitialized) {
      return;
    }

    // Chrome Browser removes the web language from spell check, as web
    // languages and spell check languages are coupled.
    // On ChromeOS, we decouple web languages and spell check languages, so
    // we intentionally omit this behaviour.

    // Remove the language from preferred languages.
    this.languageSettingsPrivate_.disableLanguage(languageCode);
  }

  isOnlyTranslateBlockedLanguage(languageState: LanguageState): boolean {
    return !languageState.translateEnabled &&
        // This assertion of `this.languages` is potentially unsafe and could
        // fail.
        // TODO(b/265553377): Prove that this assertion is safe, or rewrite this
        // to avoid this assertion.
        this.languages!.enabled.filter(lang => !lang.translateEnabled)
            .length === 1;
  }

  canDisableLanguage(languageState: LanguageState): boolean {
    // Cannot disable the only enabled language.
    // This assertion of `this.languages` is potentially unsafe and could fail.
    // TODO(b/265553377): Prove that this assertion is safe, or rewrite this to
    // avoid this assertion.
    if (this.languages!.enabled.length === 1) {
      return false;
    }

    // Cannot disable the last translate blocked language.
    if (this.isOnlyTranslateBlockedLanguage(languageState)) {
      return false;
    }

    return true;
  }

  /**
   * @return true if the given language can be enabled
   */
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
      languageCode: string, alwaysTranslate: boolean): void {
    this.languageSettingsPrivate_.setLanguageAlwaysTranslateState(
        languageCode, alwaysTranslate);
  }

  /**
   * Moves the language in the list of enabled languages either up (toward the
   * front of the list) or down (toward the back).
   * @param upDirection True if we need to move up, false if we
   *     need to move down
   */
  moveLanguage(languageCode: string, upDirection: boolean): void {
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
  moveLanguageToFront(languageCode: string): void {
    if (!CrSettingsPrefs.isInitialized) {
      return;
    }

    this.languageSettingsPrivate_.moveLanguage(languageCode, MoveType.TOP);
  }

  /**
   * Enables translate for the given language by removing the translate
   * language from the blocked languages preference.
   */
  enableTranslateLanguage(languageCode: string): void {
    this.languageSettingsPrivate_.setEnableTranslationForLanguage(
        languageCode, true);
  }

  /**
   * Disables translate for the given language by adding the translate
   * language to the blocked languages preference.
   */
  disableTranslateLanguage(languageCode: string): void {
    this.languageSettingsPrivate_.setEnableTranslationForLanguage(
        languageCode, false);
  }

  /**
   * Sets the translate target language and adds it to the content languages if
   * not already there.
   */
  setTranslateTargetLanguage(languageCode: string): void {
    this.languageSettingsPrivate_.setTranslateTargetLanguage(languageCode);
  }

  /**
   * Enables or disables spell check for the given language.
   */
  toggleSpellCheck(languageCode: string, enable: boolean): void {
    if (!this.languages) {
      return;
    }

    if (enable) {
      this.appendPrefListItem('spellcheck.dictionaries', languageCode);
    } else {
      this.deletePrefListItem('spellcheck.dictionaries', languageCode);
    }
  }

  /**
   * Converts the language code for translate. There are some differences
   * between the language set the Translate server uses and that for
   * Accept-Language.
   * @return The converted language code.
   */
  convertLanguageCodeForTranslate(languageCode: string): string {
    if (languageCode in kLanguageCodeToTranslateCode) {
      // Work around https://github.com/microsoft/TypeScript/issues/21732.
      // As of writing, it is marked as fixed by
      // https://github.com/microsoft/TypeScript/pull/50666, but that PR does
      // not address this specific issue of narrowing a `string` down to keys of
      // an object.
      type LanguageCode = keyof typeof kLanguageCodeToTranslateCode;
      // Safety: We checked that languageCode is a key above.
      return kLanguageCodeToTranslateCode[languageCode as LanguageCode];
    }

    const main = languageCode.split('-')[0];
    if (main === undefined) {
      // The only time a split could return 0 items is if the string is empty.
      throw new Error('languageCode cannot be empty');
    }
    if (main === 'zh') {
      // In Translate, general Chinese is not used, and the sub code is
      // necessary as a language code for the Translate server.
      return languageCode;
    }
    if (main in kTranslateLanguageSynonyms) {
      type TranslateSynonymKey = keyof typeof kTranslateLanguageSynonyms;
      // Safety: We checked that languageCode is a key above.
      return kTranslateLanguageSynonyms[main as TranslateSynonymKey];
    }

    return main;
  }

  /**
   * Given a language code, returns just the base language. E.g., converts
   * 'en-GB' to 'en'.
   */
  getLanguageCodeWithoutRegion(languageCode: string): string {
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
    // This assertion is unsafe if `languageCode` is an empty string, or starts
    // with a hyphen.
    // TODO(b/265554105): Gracefully handle this case.
    const result = languageCode.match(/^([^-]+)-?/)!;
    // Safety: The regex above has one non-optional capturing group.
    assert(result.length === 2);
    return result[1]!;
  }

  getLanguage(languageCode: string): chrome.languageSettingsPrivate.Language
      |undefined {
    // If a languageCode is not found, try language without location.
    return this.supportedLanguageMap_.get(languageCode) ||
        this.supportedLanguageMap_.get(
            this.getLanguageCodeWithoutRegion(languageCode));
  }

  /**
   * Retries downloading the dictionary for |languageCode|.
   */
  retryDownloadDictionary(languageCode: string): void {
    this.languageSettingsPrivate_.retryDownloadDictionary(languageCode);
  }

  /**
   * Constructs the input method part of the languages model.
   */
  private createInputMethodModel_(
      supportedInputMethods: chrome.languageSettingsPrivate.InputMethod[]):
      void {
    // Populate the hash map of supported input methods.
    this.supportedInputMethodMap_.clear();
    this.languageInputMethods_.clear();
    for (const inputMethod of supportedInputMethods) {
      inputMethod.enabled = !!inputMethod.enabled;
      inputMethod.isProhibitedByPolicy = !!inputMethod.isProhibitedByPolicy;
      // Add the input method to the map of IDs.
      this.supportedInputMethodMap_.set(inputMethod.id, inputMethod);
      // Add the input method to the list of input methods for each language
      // it supports.
      for (const languageCode of inputMethod.languageCodes) {
        if (!this.supportedLanguageMap_.has(languageCode)) {
          continue;
        }
        const inputMethods = this.languageInputMethods_.get(languageCode);
        if (inputMethods === undefined) {
          this.languageInputMethods_.set(languageCode, [inputMethod]);
        } else {
          inputMethods.push(inputMethod);
        }
      }
    }
  }

  /**
   * Returns a list of enabled input methods.
   *
   * This must be called after `whenReady()` is resolved.
   */
  private getEnabledInputMethods_():
      chrome.languageSettingsPrivate.InputMethod[] {
    // Safety: Enforced in documentation.
    assert(CrSettingsPrefs.isInitialized);

    let enabledInputMethodIds =
        this.getPref<string>('settings.language.preload_engines')
            .value.split(',');
    enabledInputMethodIds = enabledInputMethodIds.concat(
        this.getPref<string>('settings.language.enabled_extension_imes')
            .value.split(','));
    this.enabledInputMethodSet_ = new Set(enabledInputMethodIds);

    // Return only supported input methods. Don't include the Dictation
    // (Accessibility Common) input method.
    return enabledInputMethodIds
        .map(id => this.supportedInputMethodMap_.get(id))
        .filter(
            <T>(inputMethod: T): inputMethod is NonNullable<T> => !!inputMethod)
        .filter(inputMethod => inputMethod.id !== ACCESSIBILITY_COMMON_IME_ID);
  }

  private async updateSupportedInputMethods_(): Promise<void> {
    const lists = await this.languageSettingsPrivate_.getInputMethodLists();
    const supportedInputMethods =
        lists.componentExtensionImes.concat(lists.thirdPartyExtensionImes);
    this.createInputMethodModel_(supportedInputMethods);
    // The two lines below are potentially unsafe and could fail, as they assume
    // that `this.languages` is defined.
    // TODO(b/265553377): Prove that this assertion is safe, or rewrite this to
    // avoid this assertion.
    this.set('languages.inputMethods.supported', supportedInputMethods);
    this.updateEnabledInputMethods_();
  }

  private updateEnabledInputMethods_(): void {
    const enabledInputMethods = this.getEnabledInputMethods_();
    const enabledInputMethodSet = this.makeSetFromArray_(enabledInputMethods);

    // This assertion of `this.languages` is potentially unsafe and could fail.
    // TODO(b/265553377): Prove that this assertion is safe, or rewrite this to
    // avoid this assertion.
    // Safety: `LanguagesModel.inputMethods` is always defined on CrOS.
    for (const [i, inputMethod] of this.languages!.inputMethods!.supported
             .entries()) {
      this.set(
          'languages.inputMethods.supported.' + i + '.enabled',
          enabledInputMethodSet.has(inputMethod));
    }
    this.set('languages.inputMethods.enabled', enabledInputMethods);
    if (this.languagePacksInSettingsEnabled_) {
      this.fetchMissingLanguagePackStatuses_();
    }
  }

  addInputMethod(id: string): void {
    if (!this.supportedInputMethodMap_.has(id)) {
      return;
    }
    this.languageSettingsPrivate_.addInputMethod(id);
  }

  removeInputMethod(id: string): void {
    if (!this.supportedInputMethodMap_.has(id)) {
      return;
    }
    this.languageSettingsPrivate_.removeInputMethod(id);
  }

  setCurrentInputMethod(id: string): void {
    this.inputMethodPrivate_.setCurrentInputMethod(id);
  }

  getCurrentInputMethod(): Promise<string> {
    return this.inputMethodPrivate_.getCurrentInputMethod();
  }

  getInputMethodsForLanguage(languageCode: string):
      chrome.languageSettingsPrivate.InputMethod[] {
    return this.languageInputMethods_.get(languageCode) || [];
  }

  /**
   * Returns the input methods that support any of the given languages.
   */
  getInputMethodsForLanguages(languageCodes: string[]):
      chrome.languageSettingsPrivate.InputMethod[] {
    // Input methods that have already been listed for this language.
    const usedInputMethods = new Set<string>();
    const combinedInputMethods: chrome.languageSettingsPrivate.InputMethod[] =
        [];
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
  }

  /**
   * @return list of enabled language code.
   */
  getEnabledLanguageCodes(): Set<string> {
    return this.enabledLanguageSet_;
  }

  /**
   * @param id the input method id
   * @return True if the input method is enabled
   */
  isInputMethodEnabled(id: string): boolean {
    return this.enabledInputMethodSet_.has(id);
  }

  isComponentIme(inputMethod: chrome.languageSettingsPrivate.InputMethod):
      boolean {
    return inputMethod.id.startsWith('_comp_');
  }

  /** @param id Input method ID. */
  openInputMethodOptions(id: string): void {
    this.inputMethodPrivate_.openOptionsPage(id);
  }

  /** @param id New current input method ID. */
  private onInputMethodChanged_(id: string): void {
    this.set('languages.inputMethods.currentId', id);
  }

  /** @param _id Added input method ID. */
  private onInputMethodAdded_(_id: string): void {
    this.updateSupportedInputMethods_();
  }

  /** @param id Removed input method ID. */
  private onInputMethodRemoved_(_id: string): void {
    this.updateSupportedInputMethods_();
  }

  /**
   * @param id Input method ID.
   */
  getInputMethodDisplayName(id: string): string {
    const inputMethod = this.supportedInputMethodMap_.get(id);
    if (inputMethod === undefined) {
      return '';
    }
    return inputMethod.displayName;
  }

  private setLanguagePackStatus_(
      id: string, status: chrome.inputMethodPrivate.LanguagePackStatus): void {
    this.set(
        ['languages', 'inputMethods', 'imeLanguagePackStatus', id], status);
  }

  /**
   * Fetch the language pack status of enabled input methods which we do not
   * have a status for.
   */
  private fetchMissingLanguagePackStatuses_(): void {
    if (!this.languages) {
      return;
    }
    // Safety: `LanguagesModel.inputMethods` is always defined on CrOS.
    for (const inputMethod of this.languages.inputMethods!.enabled) {
      if (this.languages.inputMethods!.imeLanguagePackStatus[inputMethod.id] ===
          undefined) {
        // Explicitly set this input method status to unknown to prevent future
        // calls of this method from fetching this again.
        this.languages.inputMethods!.imeLanguagePackStatus[inputMethod.id] =
            chrome.inputMethodPrivate.LanguagePackStatus.UNKNOWN;

        void this.inputMethodPrivate_.getLanguagePackStatus(inputMethod.id)
            .then((status) => {
              this.setLanguagePackStatus_(inputMethod.id, status);
            });
      }
    }
  }

  private onLanguagePackStatusChanged_(
      change: chrome.inputMethodPrivate.LanguagePackStatusChange): void {
    for (const engineId of change.engineIds) {
      this.setLanguagePackStatus_(engineId, change.status);
    }
  }

  getImeLanguagePackStatus(id: string):
      chrome.inputMethodPrivate.LanguagePackStatus {
    // Safety: `LanguagesModel.inputMethods` is always defined on CrOS.
    return this.languages?.inputMethods!.imeLanguagePackStatus[id] ??
        chrome.inputMethodPrivate.LanguagePackStatus.UNKNOWN;
  }
}

customElements.define(SettingsLanguagesElement.is, SettingsLanguagesElement);

declare global {
  interface HTMLElementTagNameMap {
    [SettingsLanguagesElement.is]: SettingsLanguagesElement;
  }
}
