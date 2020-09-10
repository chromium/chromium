// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'tts-subpage' is the collapsible section containing
 * text-to-speech settings.
 */

Polymer({
  is: 'settings-tts-subpage',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    settings.RouteObserverBehavior,
    WebUIListenerBehavior,
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
     * Available languages.
     * @type {!Array<!{language: string, code: string, preferred: boolean,
     *     voice: TtsHandlerVoice}>}
     */
    languagesToVoices: {
      type: Array,
      notify: true,
    },

    /**
     * All voices.
     * @type {!Array<!TtsHandlerVoice>}
     */
    allVoices: {
      type: Array,
      value: [],
      notify: true,
    },

    /**
     * Default preview voice.
     */
    defaultPreviewVoice: {
      type: String,
      notify: true,
    },

    /**
     * Whether preview is currently speaking.
     * @private
     */
    isPreviewing_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    previewText_: {
      type: String,
      value: '',
    },

    /** Whether any voices are loaded. */
    hasVoices: {
      type: Boolean,
      computed: 'hasVoices_(allVoices)',
    },

    /** Whether the additional languages section has been opened. */
    languagesOpened: {
      type: Boolean,
      value: false,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kTextToSpeechRate,
        chromeos.settings.mojom.Setting.kTextToSpeechPitch,
        chromeos.settings.mojom.Setting.kTextToSpeechVolume,
        chromeos.settings.mojom.Setting.kTextToSpeechVoice,
        chromeos.settings.mojom.Setting.kTextToSpeechEngines,
      ]),
    },
  },

  /** @private {?TtsSubpageBrowserProxy} */
  ttsBrowserProxy_: null,

  /** @private {?settings.LanguagesBrowserProxy} */
  langBrowserProxy_: null,

  /** @override */
  created() {
    this.ttsBrowserProxy_ = TtsSubpageBrowserProxyImpl.getInstance();
    this.langBrowserProxy_ = settings.LanguagesBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    // Populate the preview text with textToSpeechPreviewInput. Users can change
    // this to their own value later.
    this.previewText_ = this.i18n('textToSpeechPreviewInput');
    this.addWebUIListener(
        'all-voice-data-updated', this.populateVoiceList_.bind(this));
    this.ttsBrowserProxy_.getAllTtsVoiceData();
    this.addWebUIListener(
        'tts-extensions-updated', this.populateExtensionList_.bind(this));
    this.addWebUIListener(
        'tts-preview-state-changed', this.onTtsPreviewStateChanged_.bind(this));
    this.ttsBrowserProxy_.getTtsExtensions();
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.MANAGE_TTS_SETTINGS) {
      return;
    }

    this.attemptDeepLink();
  },

  /*
   * Ticks for the Speech Rate slider. Valid rates are between 0.1 and 5.
   * @return {!Array<!cr_slider.SliderTick>}
   * @private
   */
  speechRateTicks_() {
    return this.buildLinearTicks_(0.1, 5);
  },

  /**
   * Ticks for the Speech Pitch slider. Valid pitches are between 0.2 and 2.
   * @return {!Array<!cr_slider.SliderTick>}
   * @private
   */
  speechPitchTicks_() {
    return this.buildLinearTicks_(0.2, 2);
  },

  /**
   * Ticks for the Speech Volume slider. Valid volumes are between 0.2 and
   * 1 (100%), but volumes lower than .2 are excluded as being too quiet.
   * @return {!Array<!cr_slider.SliderTick>}
   * @private
   */
  speechVolumeTicks_() {
    return this.buildLinearTicks_(0.2, 1);
  },

  /**
   * A helper to build a set of ticks between |min| and |max| (inclusive) spaced
   * evenly by 0.1.
   * @param {number} min
   * @param {number} max
   * @return {!Array<!cr_slider.SliderTick>}
   * @private
   */
  buildLinearTicks_(min, max) {
    const ticks = [];

    // Avoid floating point addition errors by scaling everything by 10.
    min *= 10;
    max *= 10;
    const step = 1;
    for (let tickValue = min; tickValue <= max; tickValue += step) {
      ticks.push(this.initTick_(tickValue / 10));
    }
    return ticks;
  },

  /**
   * Initializes i18n labels for ticks arrays.
   * @param {number} tick The value to make a tick for.
   * @return {!cr_slider.SliderTick}
   * @private
   */
  initTick_(tick) {
    const value = Math.round(100 * tick);
    const strValue = value.toFixed(0);
    const label = strValue === '100' ?
        this.i18n('defaultPercentage', strValue) :
        this.i18n('percentage', strValue);
    return {label: label, value: tick, ariaValue: value};
  },

  /**
   * Returns true if any voices are loaded.
   * @param {!Array<!TtsHandlerVoice>} voices
   * @return {boolean}
   * @private
   */
  hasVoices_(voices) {
    return voices.length > 0;
  },

  /**
   * Returns true if voices are loaded and preview is not currently speaking and
   * there is text to preview.
   * @param {!Array<!TtsHandlerVoice>} voices
   * @param {boolean} isPreviewing
   * @param {boolean} previewText
   * @return {boolean}
   * @private
   */
  enablePreviewButton_(voices, isPreviewing, previewText) {
    const nonWhitespaceRe = /\S+/;
    const hasPreviewText = nonWhitespaceRe.exec(previewText) != null;
    return this.hasVoices_(voices) && !isPreviewing && hasPreviewText;
  },

  /**
   * Populates the list of languages and voices for the UI to use in display.
   * @param {!Array<!TtsHandlerVoice>} voices
   * @private
   */
  populateVoiceList_(voices) {
    // Build a map of language code to human-readable language and voice.
    const result = {};
    const languageCodeMap = {};
    const pref = this.prefs.settings['language']['preferred_languages'];
    const preferredLangs = pref.value.split(',');
    voices.forEach(voice => {
      if (!result[voice.languageCode]) {
        result[voice.languageCode] = {
          language: voice.displayLanguage,
          code: voice.languageCode,
          preferred: false,
          voices: []
        };
      }
      // Each voice gets a unique ID from its name and extension.
      voice.id =
          JSON.stringify({name: voice.name, extension: voice.extensionId});
      // TODO(katie): Make voices a map rather than an array to enforce
      // uniqueness, then convert back to an array for polymer repeat.
      result[voice.languageCode].voices.push(voice);

      // A language is "preferred" if it has a voice that uses the default
      // locale of the device.
      result[voice.languageCode].preferred =
          result[voice.languageCode].preferred ||
          preferredLangs.indexOf(voice.fullLanguageCode) != -1;
      languageCodeMap[voice.fullLanguageCode] = voice.languageCode;
    });
    this.updateLangToVoicePrefs_(result);
    this.set('languagesToVoices', Object.values(result));
    this.set('allVoices', voices);
    this.setDefaultPreviewVoiceForLocale_(voices, languageCodeMap);
  },

  /**
   * Returns true if the language is a primary language and should be shown by
   * default, false if it should be hidden by default.
   * @param {!{language: string, code: string, preferred: boolean,
   *     voice: TtsHandlerVoice}} language
   * @return {boolean} true if it's a primary language.
   * @private
   */
  isPrimaryLanguage_(language) {
    return language.preferred;
  },

  /**
   * Returns true if the language is a secondary language and should be hidden
   * by default, true if it should be shown by default.
   * @param {!{language: string, code: string, preferred: boolean,
   *     voice: TtsHandlerVoice}} language
   * @return {boolean} true if it's a secondary language.
   * @private
   */
  isSecondaryLanguage_(language) {
    return !language.preferred;
  },

  /**
   * Sets the list of Text-to-Speech extensions for the UI.
   * @param {!Array<!TtsHandlerExtension>} extensions
   * @private
   */
  populateExtensionList_(extensions) {
    this.extensions = extensions;
  },

  /**
   * Called when the TTS voice preview state changes between speaking and not
   * speaking.
   * @param {boolean} isSpeaking
   * @private
   */
  onTtsPreviewStateChanged_(isSpeaking) {
    this.isPreviewing_ = isSpeaking;
  },

  /**
   * A function used for sorting languages alphabetically.
   * @param {!Object} first A languageToVoices array item.
   * @param {!Object} second A languageToVoices array item.
   * @return {number} The result of the comparison.
   * @private
   */
  alphabeticalSort_(first, second) {
    return first.language.localeCompare(second.language);
  },

  /**
   * Tests whether a language has just once voice.
   * @param {!Object} lang A languageToVoices array item.
   * @return {boolean} True if the item has only one voice.
   * @private
   */
  hasOneLanguage_(lang) {
    return lang['voices'].length == 1;
  },

  /**
   * Returns a list of objects that can be used as drop-down menu options for a
   * language. This is a list of voices in that language.
   * @param {!Object} lang A languageToVoices array item.
   * @return {!Array<!Object>} An array of menu options with a value and name.
   * @private
   */
  menuOptionsForLang_(lang) {
    return lang.voices.map(voice => {
      return {value: voice.id, name: voice.name};
    });
  },

  /**
   * Updates the preferences given the current list of voices.
   * @param {!Object<string, !{language: string,
   *                           code: string,
   *                           preferred: boolean,
   *                           voices: !Array<!TtsHandlerVoice>}>} langToVoices
   * @private
   */
  updateLangToVoicePrefs_(langToVoices) {
    if (langToVoices.length == 0) {
      return;
    }
    const allCodes = new Set(
        Object.keys(this.prefs.settings['tts']['lang_to_voice_name'].value));
    for (const code in langToVoices) {
      // Remove from allCodes, to track what we've found a default for.
      allCodes.delete(code);
      const voices = langToVoices[code].voices;
      const defaultVoiceForLang =
          this.prefs.settings['tts']['lang_to_voice_name'].value[code];
      if (!defaultVoiceForLang || defaultVoiceForLang === '') {
        // Initialize prefs that have no value
        this.set(
            'prefs.settings.tts.lang_to_voice_name.value.' + code,
            this.getBestVoiceForLocale_(voices));
        continue;
      }
      // See if the set voice ID is in the voices list, in which case we are
      // done checking this language.
      if (voices.some(voice => voice.id === defaultVoiceForLang)) {
        continue;
      }
      // Change prefs that point to voices that no longer exist.
      this.set(
          'prefs.settings.tts.lang_to_voice_name.value.' + code,
          this.getBestVoiceForLocale_(voices));
    }
    // If there are any items left in allCodes, they are for languages that are
    // no longer covered by the UI. We could now delete them from the
    // lang_to_voice_name pref.
    for (const code of allCodes) {
      this.set('prefs.settings.tts.lang_to_voice_name.value.' + code, '');
    }
  },

  /**
   * Sets the voice to show in the preview drop-down as default, based on the
   * current locale and voice preferences.
   * @param {!Array<!TtsHandlerVoice>} allVoices
   * @param {!Object<string, string>} languageCodeMap Mapping from language code
   *     to simple language code without locale.
   * @private
   */
  setDefaultPreviewVoiceForLocale_(allVoices, languageCodeMap) {
    if (!allVoices || allVoices.length == 0) {
      return;
    }

    // Force a synchronous render so that we can set the default.
    this.$.previewVoiceOptions.render();

    // Set something if nothing exists. This useful for new users where
    // sometimes browserProxy.getProspectiveUILanguage() does not complete the
    // callback.
    if (!this.defaultPreviewVoice) {
      this.set('defaultPreviewVoice', this.getBestVoiceForLocale_(allVoices));
    }

    this.langBrowserProxy_.getProspectiveUILanguage().then(
        prospectiveUILanguage => {
          let result;
          if (prospectiveUILanguage && prospectiveUILanguage != '' &&
              languageCodeMap[prospectiveUILanguage]) {
            const code = languageCodeMap[prospectiveUILanguage];
            // First try the pref value.
            result =
                this.prefs.settings['tts']['lang_to_voice_name'].value[code];
          }
          if (!result) {
            // If it's not a pref value yet, or the prospectiveUILanguage was
            // missing, try using the voice score.
            result = this.getBestVoiceForLocale_(allVoices);
          }
          this.set('defaultPreviewVoice', result);
        });
  },

  /**
   * Gets the best voice for the app locale.
   * @param {!Array<!TtsHandlerVoice>} voices Voices to search through.
   * @return {string} The ID of the best matching voice in the array.
   * @private
   */
  getBestVoiceForLocale_(voices) {
    let bestScore = -1;
    let bestVoice = '';
    voices.forEach((voice) => {
      if (voice.languageScore > bestScore) {
        bestScore = voice.languageScore;
        bestVoice = voice.id;
      }
    });
    return bestVoice;
  },

  /** @private */
  onPreviewTtsClick_() {
    this.ttsBrowserProxy_.previewTtsVoice(
        this.previewText_, this.$.previewVoice.value);
  },

  /**
   * @param {!{model:Object}} event
   * @private
   */
  onEngineSettingsTap_(event) {
    this.ttsBrowserProxy_.wakeTtsEngine();
    window.open(event.model.extension.optionsPage);
  },
});
