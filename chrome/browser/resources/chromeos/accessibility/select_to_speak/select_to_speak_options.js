// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrefsManager} from './prefs_manager.js';

const AccessibilityFeature = chrome.accessibilityPrivate.AccessibilityFeature;

class SelectToSpeakOptionsPage {
  constructor() {
    this.init_();
  }

  /**
   * Translate the page and sync all of the control values to the
   * values loaded from chrome.settingsPrivate.
   */
  init_() {
    this.addTranslatedMessagesToDom_();
    this.populateVoicesAndLanguages_();

    window.speechSynthesis.onvoiceschanged = (() => {
      this.populateVoicesAndLanguages_();
    });

    const select = document.getElementById('language');
    select.onchange = _ => {
      this.populateVoicesAndLanguages_();
    };

    this.syncSelectControlToPref_(
        'localVoices', PrefsManager.VOICE_NAME_KEY, 'voiceName');
    this.syncSelectControlToPref_(
        'naturalVoice', PrefsManager.ENHANCED_VOICE_NAME_KEY, 'voiceName');
    chrome.settingsPrivate.getPref(
        PrefsManager.ENHANCED_VOICES_POLICY_KEY, network_voices_allowed => {
          if (network_voices_allowed !== undefined &&
              !network_voices_allowed.value) {
            // If the feature is disallowed, sets the checkbox to false.
            const checkbox = document.getElementById('naturalVoices');
            checkbox.checked = false;
            checkbox.disabled = true;
            this.setVoiceSelectionAndPreviewVisibility_(
                /* isVisible = */ false);
          } else {
            // If the feature is allowed, syncs the checkbox with pref.
            this.syncCheckboxControlToPref_(
                'naturalVoices', PrefsManager.ENHANCED_NETWORK_VOICES_KEY,
                checked => {
                  this.setVoiceSelectionAndPreviewVisibility_(
                      /* isVisible = */ checked);
                });
          }
        });  // End of the chrome.settingsPrivate.getPref

    this.syncCheckboxControlToPref_(
        'wordHighlight', PrefsManager.WORD_HIGHLIGHT_KEY, checked => {
          const elem = document.getElementById('highlightSubOption');
          const select = document.getElementById('highlightColor');
          this.setElementVisible(elem, checked);
          select.disabled = !checked;
        });
    this.syncCheckboxControlToPref_(
        'backgroundShading', PrefsManager.BACKGROUND_SHADING_KEY, checked => {
          const elem = document.getElementById('backgroundPreviewContainer');
          this.setElementVisible(elem, checked);
        });
    this.syncCheckboxControlToPref_(
        'navigationControls', PrefsManager.NAVIGATION_CONTROLS_KEY);
    this.syncCheckboxControlToPref_(
        'voiceSwitching', PrefsManager.VOICE_SWITCHING_KEY,
        checked => this.voiceSwitchingToggleChanged(/* isEnabled = */ checked));

    this.setUpHighlightListener_();
    this.setUpNaturalVoicePreviewListener_();
    chrome.metricsPrivate.recordUserAction(
        'Accessibility.CrosSelectToSpeak.LoadSettings');
  }

  /**
   * Shows an element.
   * @private
   */
  showElement(element) {
    element.classList.remove('hidden');
    element.setAttribute('aria-hidden', false);
  }

  /**
   * Hides an element.
   * @private
   */
  hideElement(element) {
    element.classList.add('hidden');
    element.setAttribute('aria-hidden', true);
  }

  /**
   * Shows or hides an element.
   * @private
   */
  setElementVisible(element, isVisible) {
    if (isVisible) {
      this.showElement(element);
    } else {
      this.hideElement(element);
    }
  }

  /**
   * When voice switching toggle is on, disable natural voice toggle.
   * @param {boolean} isEnabled If voice switching toggle is enabled.
   * @private
   */
  voiceSwitchingToggleChanged(isEnabled) {
    const naturalVoiceToggle = document.getElementById('naturalVoices');
    naturalVoiceToggle.disabled = isEnabled;
  }

  /**
   * Sets the visibility for natural voices selection and preview.
   * Also disable voice switching toggle if natural voices enabled.
   * @param {boolean} isVisible The intended visibility of the elements.
   * @private
   */
  setVoiceSelectionAndPreviewVisibility_(isVisible) {
    const voice = document.getElementById('naturalVoiceSelection');
    const preview = document.getElementById('naturalVoicePreview');
    const select = document.getElementById('naturalVoice');
    const voiceSwitchingToggle = document.getElementById('voiceSwitching');
    this.setElementVisible(voice, isVisible);
    this.setElementVisible(preview, isVisible);
    select.disabled = !isVisible;
    voiceSwitchingToggle.disabled = isVisible;
  }

  /**
   * Processes an HTML DOM, replacing text content with translated text messages
   * on elements marked up for translation.  Elements whose class attributes
   * contain the 'i18n' class name are expected to also have an msgid
   * attribute. The value of the msgid attributes are looked up as message
   * IDs and the resulting text is used as the text content of the elements.
   * @private
   */
  addTranslatedMessagesToDom_() {
    var elts = document.querySelectorAll('.i18n');
    for (var i = 0; i < elts.length; i++) {
      var msgid = elts[i].getAttribute('msgid');
      if (!msgid) {
        throw new Error('Element has no msgid attribute: ' + elts[i]);
      }
      var translated = chrome.i18n.getMessage('select_to_speak_' + msgid);
      if (elts[i].tagName === 'INPUT') {
        elts[i].setAttribute('value', translated);
      } else {
        elts[i].textContent = translated;
      }
      elts[i].classList.add('i18n-processed');
    }
  }

  /**
   * Populate select element with a list of TTS voices for a given language.
   * Note that this is the legacy interface, for when the enhanced voices
   * feature flag is turned off.
   * @param {string} selectId The id of the select element.
   * @private
   */
  populateVoiceList_(selectId) {
    chrome.tts.getVoices(voices => {
      const select = document.getElementById(selectId);
      // Add the system voice.
      this.initializeSelectWithDefault_(
          select,
          this.getDefaultVoiceOption_(
              PrefsManager.SYSTEM_VOICE, 'select_to_speak_system_voice'));

      voices.forEach(function(voice) {
        voice.voiceName = voice.voiceName || '';
      });
      voices.sort(function(a, b) {
        return a.voiceName.localeCompare(b.voiceName || '');
      });
      voices.forEach(voice => {
        if (!this.isVoiceUsable_(voice) ||
            (voice.extensionId === PrefsManager.ENHANCED_TTS_EXTENSION_ID)) {
          // Don't show network voices for legacy interface.
          return;
        }
        const option = document.createElement('option');
        option.voiceName = voice.voiceName;
        option.innerText = option.voiceName;
        select.add(option);
      });
      if (select.updateFunction) {
        select.updateFunction();
      }
    });
  }

  /**
   * Populate select elements corresponding to local and network voices with a
   * list of corresponding TTS voices, and select element corresponding to
   * language with a list of languages covered by the available voices.
   * @private
   */
  populateVoicesAndLanguages_() {
    chrome.tts.getVoices(voices => {
      // Initialize language select.
      const languageSelect = document.getElementById('language');
      const originalLanguageValue =
          languageSelect.value || PrefsManager.USE_DEVICE_LANGUAGE;
      const currentLocale = chrome.i18n.getUILanguage().toLowerCase() || '';
      let lang = originalLanguageValue;
      if (lang === PrefsManager.USE_DEVICE_LANGUAGE) {
        lang = this.getLanguageShortCode_(currentLocale);
      }
      this.initializeSelectWithDefault_(
          languageSelect, this.getDefaultLanguageOption_());

      // Initialize local voices select.
      const localSelect = document.getElementById('localVoices');
      const originalLocalValue = localSelect.value || PrefsManager.SYSTEM_VOICE;
      this.initializeSelectWithDefault_(
          localSelect,
          this.getDefaultVoiceOption_(
              PrefsManager.SYSTEM_VOICE, 'select_to_speak_system_voice'));

      // Initialize network voices select.
      const networkSelect = document.getElementById('naturalVoice');
      const originalNetworkValue =
          networkSelect.value || PrefsManager.DEFAULT_NETWORK_VOICE;
      this.initializeSelectWithDefault_(
          networkSelect,
          this.getDefaultVoiceOption_(
              PrefsManager.DEFAULT_NETWORK_VOICE,
              'select_to_speak_default_network_voice'));

      // Group voices by language, and languages by language family.
      this.groupAndAddLanguagesAndVoices_(
          voices, lang, languageSelect, localSelect, networkSelect);

      // Restore original values for selects.
      languageSelect.value = originalLanguageValue;
      localSelect.value = originalLocalValue;
      networkSelect.value = originalNetworkValue;

      // Call update functions for selects.
      if (languageSelect.updateFunction) {
        languageSelect.updateFunction();
      }
      if (localSelect.updateFunction) {
        localSelect.updateFunction();
      }
      if (networkSelect.updateFunction) {
        networkSelect.updateFunction();
      }
    });
  }

  /**
   * Group and sort available voices by language, and add languages, local
   * voices, and network voices to their respective select elements.
   * TODO(crbug.com/1234115): Add unit tests for this method.
   * @param {Array<!chrome.tts.TtsVoice>} voices Array of supported voices.
   * @param {string} preferredLang User's choice of preferred language.
   * @param {Element} languageSelect Select element for language options.
   * @param {Element} localSelect Select element for local voices.
   * @param {Element} networkSelect Select element for network voices.
   */
  groupAndAddLanguagesAndVoices_(
      voices, preferredLang, languageSelect, localSelect, networkSelect) {
    // Group voices by language.
    const languageDisplayNames = new Map();
    const localVoices = new Map();
    const networkVoices = new Map();
    const currentLocale = chrome.i18n.getUILanguage().toLowerCase() || '';

    voices.forEach(voice => {
      if (!this.isVoiceUsable_(voice)) {
        return;
      }
      // Only show language names based on base language code.
      const languageCode = this.getLanguageShortCode_(voice.lang || '');
      const displayName = chrome.accessibilityPrivate.getDisplayNameForLocale(
          languageCode, currentLocale);
      if (!displayName) {
        return;
      }
      languageDisplayNames.set(languageCode, displayName);
      if (voice.extensionId === PrefsManager.ENHANCED_TTS_EXTENSION_ID) {
        // Compute display name from locale for enhanced voices, since the
        // supplied voiceName is not human-readable (e.g. enc-wavenet).
        voice.displayName = chrome.accessibilityPrivate.getDisplayNameForLocale(
            voice.lang || '', currentLocale);
        this.addVoiceToMapForLanguage_(voice, networkVoices, languageCode);
      } else {
        voice.displayName = voice.voiceName;
        this.addVoiceToMapForLanguage_(voice, localVoices, languageCode);
      }
    });

    this.populateLanguages_(languageDisplayNames, languageSelect);

    // Sort voices by language, with the preferred language on top.
    const voiceLanguagesList = Array.from(languageDisplayNames.keys());
    voiceLanguagesList.sort(function(lang1, lang2) {
      return ((lang2 === preferredLang) - (lang1 === preferredLang)) ||
          lang1.localeCompare(lang2);
    });

    // Populate local and network selects.
    voiceLanguagesList.forEach(voiceLang => {
      this.appendVoicesToSelect_(
          localSelect, localVoices.get(voiceLang), /*numberVoices=*/ false);
      this.appendVoicesToSelect_(
          networkSelect, networkVoices.get(voiceLang),
          /*numberVoices=*/ true);
    });
  }

  /**
   * Populate language select element with language display names.
   * @param {Map<string, string>} languageDisplayNames Map of language code
   *     (e.g. en) to display name (e.g. English).
   * @param {Element} languageSelect Select element for language options.
   */
  populateLanguages_(languageDisplayNames, languageSelect) {
    const supportedLanguagesList = Array.from(languageDisplayNames.keys());
    supportedLanguagesList.sort(function(lang1, lang2) {
      return languageDisplayNames.get(lang1).localeCompare(
          languageDisplayNames.get(lang2));
    });
    supportedLanguagesList.forEach(function(language) {
      const option = document.createElement('option');
      option.value = language;
      option.innerText = languageDisplayNames.get(language);
      languageSelect.add(option);
    });
  }

  /**
   * Create the default option for language selection.
   * @returns Option element with device language option.
   */
  getDefaultLanguageOption_() {
    const option = document.createElement('option');
    option.value = PrefsManager.USE_DEVICE_LANGUAGE;
    option.innerText =
        chrome.i18n.getMessage('select_to_speak_options_device_language');
    return option;
  }

  /**
   * Create the default option for language selection.
   * @param {string} voiceName Name of the voice to pass to TTS engine.
   * @param {string} displayMessageName Name of the string for the display name
   *     of the default option.
   * @returns Option element with device language option.
   */
  getDefaultVoiceOption_(voiceName, displayMessageName) {
    const option = document.createElement('option');
    option.voiceName = voiceName;
    option.value = voiceName;
    option.innerText = chrome.i18n.getMessage(displayMessageName);
    return option;
  }

  /**
   * Checks if a voice has the properties and events needed for Select-to-speak.
   * @param {!chrome.tts.TtsVoice} voice
   * @returns whether the voice is usable by Select-to-speak.
   */
  isVoiceUsable_(voice) {
    if (!voice.voiceName || !voice.lang) {
      return false;
    }
    if (!voice.eventTypes.includes(chrome.tts.EventType.START) ||
        !voice.eventTypes.includes(chrome.tts.EventType.END) ||
        !voice.eventTypes.includes(chrome.tts.EventType.WORD) ||
        !voice.eventTypes.includes(chrome.tts.EventType.CANCELLED)) {
      // Required event types for Select-to-Speak.
      return false;
    }
    return true;
  }

  /**
   * Returns the ISO 639 code (e.g. en) for the given language code (e.g.
   * en-us).
   * @param {string} lang Language
   * @returns ISO 639 code (e.g. en or yue)
   */
  getLanguageShortCode_(lang) {
    return lang.trim().split(/-|_/)[0];
  }

  /**
   * Initializes a select element with a single, default option.
   * @param {Element} select
   * @param {Element} defaultOption
   */
  initializeSelectWithDefault_(select, defaultOption) {
    select.innerHTML = '';
    select.add(defaultOption);
  }

  /**
   * Groups voices by display name (e.g. English (Australia)) and if there is
   * more than one voice per display name, adds a numerical index to them (e.g.
   * English (Australia) 1) for disambiguation.
   * @param {Array<!chrome.tts.TtsVoice>} voiceList
   */
  addIndexToVoiceDisplayNames_(voiceList) {
    const displayNameCounts = new Map();
    voiceList.forEach(function(voice) {
      if (!displayNameCounts.has(voice.displayName)) {
        displayNameCounts.set(voice.displayName, [voice]);
      } else {
        displayNameCounts.get(voice.displayName).push(voice);
      }
    });
    for (const voiceGroup of displayNameCounts.values()) {
      if (voiceGroup.length > 1) {
        let index = 1;
        voiceGroup.forEach(function(voice) {
          voice.displayName = chrome.i18n.getMessage(
              'select_to_speak_natural_voice_name', [voice.displayName, index]);
          // voice.displayName += ' ' + index;
          index += 1;
        });
      }
    }
  }

  /**
   * Add options corresponding to the given list of voices to a select element.
   * @param {Element} select
   * @param {Array<!chrome.tts.TtsVoice>} voiceList
   * @param {boolean} numberVoices if true, add numbers to disambiguate voices
   *     with identical display names.
   */
  appendVoicesToSelect_(select, voiceList, numberVoices) {
    if (!voiceList) {
      return;
    }
    if (voiceList.length > 1) {
      voiceList.sort(function(a, b) {
        return a.displayName.localeCompare(b.displayName);
      });
      if (numberVoices) {
        this.addIndexToVoiceDisplayNames_(voiceList);
      }
    }

    voiceList.forEach(function(voice) {
      const option = document.createElement('option');
      option.voiceName = voice.voiceName;
      option.value = voice.voiceName;
      option.innerText = voice.displayName;
      select.add(option);
    });
  }

  /**
   * Adds a voice to the map entry corresponding to the given language.
   * @param {!chrome.tts.TtsVoice} voice
   * @param {Map<string, Array<!chrome.tts.TtsVoice>>} map Map with
   *     language-code as key and an array of voices for that language code as
   *     value.
   * @param {string} lang Language corresponding to the voice
   */
  addVoiceToMapForLanguage_(voice, map, lang) {
    voice.languageCode = lang;
    if (map.has(lang)) {
      map.get(lang).push(voice);
    } else {
      map.set(lang, [voice]);
    }
  }

  /**
   * Populate a checkbox with its current setting.
   * @param {string} checkboxId The id of the checkbox element.
   * @param {string} pref The name for a chrome.settingsPrivate pref.
   * @param {?function(boolean): undefined=} opt_onChange A function
   * to be called every time the checkbox state is changed.
   * @private
   */
  syncCheckboxControlToPref_(checkboxId, pref, opt_onChange) {
    const checkbox = document.getElementById(checkboxId);

    function updateFromPref() {
      chrome.settingsPrivate.getPref(pref, ({value}) => {
        checkbox.checked = value;
        if (opt_onChange) {
          opt_onChange(Boolean(checkbox.checked));
        }
      });
    }

    checkbox.addEventListener('keypress', function(e) {
      if (e.code === 'Enter') {
        e.stopPropagation();
        checkbox.click();
      }
    });

    checkbox.addEventListener('change', function() {
      chrome.settingsPrivate.setPref(pref, checkbox.checked);
    });

    updateFromPref();
    chrome.settingsPrivate.onPrefsChanged.addListener(updateFromPref);
  }

  /**
   * Given the id of an HTML select element and the name of a
   * chrome.settingsPrivate pref, sync them both ways.
   * @param {string} selectId The id of the select element.
   * @param {string} pref The name of a chrome.settingsPrivate pref.
   * @param {string} valueKey The key of the option to use as value.
   * @param {?function(string): undefined=} opt_onChange Optional change
   *     listener to call when the setting has been changed.
   */
  syncSelectControlToPref_(selectId, pref, valueKey, opt_onChange) {
    var element = document.getElementById(selectId);

    function updateFromPref() {
      chrome.settingsPrivate.getPref(pref, ({value}) => {
        element.selectedIndex = -1;
        for (var i = 0; i < element.options.length; ++i) {
          if (element.options[i][valueKey] === value) {
            element.selectedIndex = i;
            break;
          }
        }
        if (opt_onChange) {
          opt_onChange(String(value));
        }
      });
    }

    element.addEventListener('change', function() {
      const newValue = element.options[element.selectedIndex][valueKey];
      chrome.settingsPrivate.setPref(pref, newValue);
    });

    element.updateFunction = updateFromPref;
    updateFromPref();
    chrome.settingsPrivate.onPrefsChanged.addListener(updateFromPref);
  }

  /**
   * Sets up the highlight listeners and preferences.
   * @private
   */
  setUpHighlightListener_() {
    const onChange = function(value) {
      const examples = document.getElementsByClassName('highlight');
      for (let i = 0; i < examples.length; i++) {
        examples[i].style.background = value;
      }
    };

    this.syncSelectControlToPref_(
        'highlightColor', PrefsManager.HIGHLIGHT_COLOR_KEY, 'value', onChange);

    document.getElementById('wordHighlightOption')
        .addEventListener('click', function(e) {
          e.stopPropagation();
          const checkbox = document.getElementById('wordHighlight');
          // Make sure it isn't the auto-generated click itself.
          if (e.srcElement !== checkbox) {
            checkbox.click();
          }
        });
  }

  /**
   * Sets up a listener on the Play button to preview natural voices.
   * @private
   */
  setUpNaturalVoicePreviewListener_() {
    const button = document.getElementById('naturalVoicesPlay');
    button.addEventListener('click', () => {
      const select = document.getElementById('naturalVoice');
      const voiceName = select.options[select.selectedIndex].voiceName;
      const options = /** @type {!chrome.tts.TtsOptions} */ ({});
      const preview = document.getElementById('naturalVoicesExample');
      const previewText = preview.value;
      options['voiceName'] = voiceName;
      chrome.tts.speak(previewText, options);
    });
  }
}

new SelectToSpeakOptionsPage();
