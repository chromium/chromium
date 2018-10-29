// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'tts-subpage' is the collapsible section containing
 * text-to-speech settings.
 */

Polymer({
  is: 'settings-tts-subpage',

  behaviors: [WebUIListenerBehavior, I18nBehavior],

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
     * @type {Array<{language: string, code: string, preferred: boolean,
     *     voice: TtsHandlerVoice}>}
     */
    languagesToVoices: {
      type: Array,
      notify: true,
    },

    /**
     * All voices.
     * @type {Array<TtsHandlerVoice>}
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
  },

  /** @override */
  ready: function() {
    this.addWebUIListener(
        'all-voice-data-updated', this.populateVoiceList_.bind(this));
    chrome.send('getAllTtsVoiceData');
    this.addWebUIListener(
        'tts-extensions-updated', this.populateExtensionList_.bind(this));
    chrome.send('getTtsExtensions');
  },

  /**
   * Ticks for the Speech Rate slider. Non-linear as we expect people
   * to want more control near 1.0.
   * @return Array<cr_slider.SliderTick>
   * @private
   */
  speechRateTicks_: function() {
    return Array.from(Array(16).keys()).map(x => {
      return x <= 4 ?
          // Linear from rates 0.6 to 1.0
          this.initTick_(x / 10 + .6) :
          // Power function above 1.0 gives more control at lower values.
          this.initTick_(Math.pow(x - 3, 2) / 20 + 1);
    });
  },

  /**
   * Ticks for the Speech Pitch slider. Valid pitches are between 0 and 2,
   * exclusive of 0.
   * @return Array<cr_slider.SliderTick>
   * @private
   */
  speechPitchTicks_: function() {
    return Array.from(Array(10).keys()).map(x => {
      return this.initTick_(x * .2 + .2);
    });
  },

  /**
   * Ticks for the Speech Volume slider. Valid volumes are between 0 and
   * 1 (100%), but volumes lower than .2 are excluded as being too quiet.
   * The values are linear between .2 and 1.0.
   * @return Array<cr_slider.SliderTick>
   * @private
   */
  speechVolumeTicks_: function() {
    return Array.from(Array(9).keys()).map(x => {
      return this.initTick_(x * .1 + .2);
    });
  },

  /**
   * Initializes i18n labels for ticks arrays.
   * @param {number} tick The value to make a tick for.
   * @return {cr_slider.SliderTick}
   * @private
   */
  initTick_: function(tick) {
    let value = Math.round(100 * tick);
    let strValue = value.toFixed(0);
    let label = strValue === '100' ? this.i18n('defaultPercentage', strValue) :
                                     this.i18n('percentage', strValue);
    return {label: label, value: tick, ariaValue: value};
  },

  /**
   * Returns true if any voices are loaded.
   * @param {!Array<TtsHandlerVoice>} voices
   * @return {boolean}
   * @private
   */
  hasVoices_: function(voices) {
    return voices.length > 0;
  },

  /**
   * Populates the list of languages and voices for the UI to use in display.
   * @param {Array<TtsHandlerVoice>} voices
   * @private
   */
  populateVoiceList_: function(voices) {
    // Build a map of language code to human-readable language and voice.
    let result = {};
    let languageCodeMap = {};
    let pref = this.prefs.settings['language']['preferred_languages'];
    let preferredLangs = pref.value.split(',');
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
   * @param {{language: string, code: string, preferred: boolean,
   *     voice: TtsHandlerVoice}} language
   * @return {boolean} true if it's a primary language.
   */
  isPrimaryLanguage_: function(language) {
    return language.preferred;
  },

  /**
   * Returns true if the language is a secondary language and should be hidden
   * by default, true if it should be shown by default.
   * @param {{language: string, code: string, preferred: boolean,
   *     voice: TtsHandlerVoice}} language
   * @return {boolean} true if it's a secondary language.
   */
  isSecondaryLanguage_: function(language) {
    return !language.preferred;
  },

  /**
   * Sets the list of Text-to-Speech extensions for the UI.
   * @param {Array<TtsHandlerExtension>} extensions
   * @private
   */
  populateExtensionList_: function(extensions) {
    this.extensions = extensions;
  },

  /**
   * A function used for sorting languages alphabetically.
   * @param {Object} first A languageToVoices array item.
   * @param {Object} second A languageToVoices array item.
   * @return {number} The result of the comparison.
   * @private
   */
  alphabeticalSort_: function(first, second) {
    return first.language.localeCompare(second.language);
  },

  /**
   * Tests whether a language has just once voice.
   * @param {Object} lang A languageToVoices array item.
   * @return {boolean} True if the item has only one voice.
   * @private
   */
  hasOneLanguage_: function(lang) {
    return lang['voices'].length == 1;
  },

  /**
   * Returns a list of objects that can be used as drop-down menu options for a
   * language. This is a list of voices in that language.
   * @param {Object} lang A languageToVoices array item.
   * @return {Array<Object>} An array of menu options with a value and name.
   * @private
   */
  menuOptionsForLang_: function(lang) {
    return lang.voices.map(voice => {
      return {value: voice.id, name: voice.name};
    });
  },

  /**
   * Updates the preferences given the current list of voices.
   * @param {Object<string, {language: string, code: string, preferred: boolean,
   *     voices: Array<TtsHandlerVoice>}>} langToVoices
   * @private
   */
  updateLangToVoicePrefs_: function(langToVoices) {
    if (langToVoices.length == 0)
      return;
    let allCodes = new Set(
        Object.keys(this.prefs.settings['tts']['lang_to_voice_name'].value));
    for (let code in langToVoices) {
      // Remove from allCodes, to track what we've found a default for.
      allCodes.delete(code);
      let voices = langToVoices[code].voices;
      let defaultVoiceForLang =
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
      if (voices.some(voice => voice.id === defaultVoiceForLang))
        continue;
      // Change prefs that point to voices that no longer exist.
      this.set(
          'prefs.settings.tts.lang_to_voice_name.value.' + code,
          this.getBestVoiceForLocale_(voices));
    }
    // If there are any items left in allCodes, they are for languages that are
    // no longer covered by the UI. We could now delete them from the
    // lang_to_voice_name pref.
    for (let code of allCodes) {
      this.set('prefs.settings.tts.lang_to_voice_name.value.' + code, '');
    }
  },

  /**
   * Sets the voice to show in the preview drop-down as default, based on the
   * current locale and voice preferences.
   * @param {Array<TtsHandlerVoice>} allVoices
   * @param {Object<string, string>} languageCodeMap Mapping from language code
   *     to simple language code without locale.
   * @private
   */
  setDefaultPreviewVoiceForLocale_: function(allVoices, languageCodeMap) {
    if (!allVoices || allVoices.length == 0)
      return;

    // Force a synchronous render so that we can set the default.
    this.$.previewVoiceOptions.render();

    // Set something if nothing exists. This useful for new users where
    // sometimes browserProxy.getProspectiveUILanguage() does not complete the
    // callback.
    if (!this.defaultPreviewVoice)
      this.set('defaultPreviewVoice', this.getBestVoiceForLocale_(allVoices));

    let browserProxy = settings.LanguagesBrowserProxyImpl.getInstance();
    browserProxy.getProspectiveUILanguage().then(prospectiveUILanguage => {
      let result;
      if (prospectiveUILanguage && prospectiveUILanguage != '' &&
          languageCodeMap[prospectiveUILanguage]) {
        let code = languageCodeMap[prospectiveUILanguage];
        // First try the pref value.
        result = this.prefs.settings['tts']['lang_to_voice_name'].value[code];
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
   * @param {Array<TtsHandlerVoice>} voices Voices to search through.
   * @return {string} The ID of the best matching voice in the array.
   * @private
   */
  getBestVoiceForLocale_: function(voices) {
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
  onPreviewTtsClick_: function() {
    chrome.send(
        'previewTtsVoice',
        [this.$.previewInput.value, this.$.previewVoice.value]);
    chrome.metricsPrivate.recordSparseHashable(
        'TextToSpeech.Settings.PreviewVoiceClicked', this.$.previewVoice.value);
  },

  /** @private */
  onDefaultTtsVoicePicked_: function(event) {
    // Log the default voice the user selected. Each voice has at most one
    // language, so there's no need to log language as well.
    // The event target is the settings-dropdown-menu.
    let target = /** @type {{prefStringValue_: function():string}} */
        (event.target);
    let newDefault = target.prefStringValue_();
    chrome.metricsPrivate.recordSparseHashable(
        'TextToSpeech.Settings.DefaultVoicePicked', newDefault);
  },

  /**
   * @param {{model:Object}} event
   * @private
   */
  onEngineSettingsTap_: function(event) {
    chrome.send('wakeTtsEngine');
    window.open(event.model.extension.optionsPage);
  },
});
