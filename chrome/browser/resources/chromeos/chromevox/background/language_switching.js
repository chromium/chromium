// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides language switching services for ChromeVox, which
 * uses language detection information to automatically change the ChromeVox
 * output voice.
 */

goog.provide('LanguageSwitching');

/**
 * The UI language of the browser. This corresponds to the system language
 * set by the user. Behind the scenes, the getUIlanguage() API retrieves the
 * locale that was passed from the browser to the renderer via the --lang
 * command line flag.
 * @private {string}
 */
LanguageSwitching.browserUILanguage_ =
    chrome.i18n.getUILanguage().toLowerCase();

/**
 * The current output language. Initialize to the language of the browser or
 * empty string if the former is unavailable.
 * @private {string}
 */
LanguageSwitching.currentLanguage_ = LanguageSwitching.browserUILanguage_ || '';

/**
 * Confidence threshold to meet before assigning sub-node language.
 * @const
 * @private {number}
 */
LanguageSwitching.PROBABILITY_THRESHOLD_ = 0.9;

/**
 * Stores whether or not ChromeVox sub-node language switching is enabled.
 * Set to false as default, as sub-node language detection is still
 * experimental.
 * @private {boolean}
 */
LanguageSwitching.sub_node_switching_enabled_ = false;

/**
 * An array of all available TTS voices.
 * @type {!Array<!TtsVoice>}
 * @private
 */
LanguageSwitching.availableVoices_ = [];

/**
 * Initialization function for language switching.
 */
LanguageSwitching.init = function() {
  // Enable sub-node language switching if feature flag is enabled.
  chrome.commandLinePrivate.hasSwitch(
      'enable-experimental-accessibility-chromevox-sub-node-language-' +
          'switching',
      function(enabled) {
        LanguageSwitching.sub_node_switching_enabled_ = enabled;
      });

  // Ensure that availableVoices_ is set and stays updated.
  function setAvailableVoices() {
    chrome.tts.getVoices(function(voices) {
      LanguageSwitching.availableVoices_ = voices || [];
    });
  }
  setAvailableVoices();
  if (speechSynthesis) {
    speechSynthesis.addEventListener(
        'voiceschanged', setAvailableVoices, /* useCapture */ false);
  }
};

/**
 * Main language switching function.
 * Cut up string attribute value into multiple spans with different
 * languages. Ranges and associated language information are returned by the
 * languageAnnotationsForStringAttribute() function.
 * @param {AutomationNode} node
 * @param {string} stringAttribute The string attribute for which we want to
 * get a language annotation.
 * @param {function(string, string)} appendStringWithLanguage
 * A callback that appends outputString to the output buffer in newLanguage.
 */
LanguageSwitching.assignLanguagesForStringAttribute = function(
    node, stringAttribute, appendStringWithLanguage) {
  if (!node) {
    return;
  }

  var stringAttributeValue = node[stringAttribute];
  if (!stringAttributeValue) {
    return;
  }

  var languageAnnotation;
  // Quick note:
  // The decideNewLanguage function, which contains the core language switching
  // logic, is setup to prefer sub-node switching if the detected language's
  // probability exceeds the PROBABILITY_THRESHOLD_; otherwise node-level
  // switching will be used as a fallback.
  if (LanguageSwitching.sub_node_switching_enabled_) {
    languageAnnotation =
        node.languageAnnotationForStringAttribute(stringAttribute);
  } else {
    var nodeLevelLanguageData = {};
    // Ensure that we span the entire stringAttributeValue.
    nodeLevelLanguageData.startIndex = 0;
    nodeLevelLanguageData.endIndex = stringAttributeValue.length;
    nodeLevelLanguageData.language = '';
    nodeLevelLanguageData.probability = 0;
    languageAnnotation = [nodeLevelLanguageData];
  }

  // If no language annotation is found, append entire stringAttributeValue to
  // buffer and default to the browser UI language.
  if (!languageAnnotation || languageAnnotation.length === 0) {
    appendStringWithLanguage(
        stringAttributeValue, LanguageSwitching.browserUILanguage_);
    return;
  }

  // Split output based on language annotation.
  // Each object in languageAnnotation contains a language, probability,
  // and start/end indices that define a substring of stringAttributeValue.
  for (var i = 0; i < languageAnnotation.length; ++i) {
    var speechProps = new Output.SpeechProperties();
    var startIndex = languageAnnotation[i].startIndex;
    var endIndex = languageAnnotation[i].endIndex;
    var language = languageAnnotation[i].language.toLowerCase();
    var probability = languageAnnotation[i].probability;

    var outputString = LanguageSwitching.buildOutputString(
        stringAttributeValue, startIndex, endIndex);
    var newLanguage =
        LanguageSwitching.decideNewLanguage(node, language, probability);
    var displayLanguage = '';

    if (LanguageSwitching.didLanguageSwitch(newLanguage)) {
      LanguageSwitching.currentLanguage_ = newLanguage;
      // Get human-readable language in |newLanguage|.
      displayLanguage = chrome.accessibilityPrivate.getDisplayLanguage(
          newLanguage /* Language code to translate */,
          newLanguage /* Target language code */);
      // Prepend the human-readable language to outputString.
      outputString =
          Msgs.getMsg('language_switch', [displayLanguage, outputString]);
    }

    if (LanguageSwitching.hasVoiceForLanguage(newLanguage)) {
      appendStringWithLanguage(newLanguage, outputString);
    } else {
      // Translate |newLanguage| into human-readable string in the UI language.
      displayLanguage = chrome.accessibilityPrivate.getDisplayLanguage(
          newLanguage /* Language code to translate */,
          LanguageSwitching.browserUILanguage_ /* Target language code */);
      outputString =
          Msgs.getMsg('voice_unavailable_for_language', [displayLanguage]);
      // Alert the user that we have no available voice for the language.
      appendStringWithLanguage(
          LanguageSwitching.browserUILanguage_, outputString);
    }
  }
};

/**
 * Run error checks on language data and decide new output language.
 * @param {!AutomationNode} node
 * @param {string} subNodeLanguage
 * @param {number} probability
 * @return {string}
 */
LanguageSwitching.decideNewLanguage = function(
    node, subNodeLanguage, probability) {
  // Use the following priority rankings when deciding language.
  // 1. Sub-node language. If we can detect sub-node language with a high
  // enough probability of accuracy, then we should use it.
  // 2. Node-level detected language.
  // 3. Author-provided language. This language is also assigned at the node
  // level.
  // 4. UI language of the browser. This is the language the user has chosen to
  // display their content in.

  // Use subNodeLanguage if probability exceeds threshold.
  if (probability > LanguageSwitching.PROBABILITY_THRESHOLD_) {
    return subNodeLanguage;
  }

  var nodeLevelLanguage = node.detectedLanguage || node.language;
  // We do not have enough information to make a confident language assignment,
  // so we fall back on the UI language of the browser.
  if (!nodeLevelLanguage) {
    return LanguageSwitching.browserUILanguage_;
  }

  nodeLevelLanguage = nodeLevelLanguage.toLowerCase();

  if (LanguageSwitching.isValidLanguageCode(nodeLevelLanguage)) {
    return nodeLevelLanguage;
  }

  return LanguageSwitching.browserUILanguage_;
};

/**
 * Returns a unicode-aware substring of |text| from startIndex to endIndex.
 * @param {string} text
 * @param {number} startIndex
 * @param {number} endIndex
 * @return {string}
 */
LanguageSwitching.buildOutputString = function(text, startIndex, endIndex) {
  var result = '';
  var textSymbolArray = [...text];
  for (var i = startIndex; i < endIndex; ++i) {
    result += textSymbolArray[i];
  }
  return result;
};

// TODO(akihiroota): Some languages may have the same language code, but be
// distinctly different. For example, there are some dialects of Chinese that
// are very different from each other. For these cases, comparing just the
// language components is not enough to differentiate the languages.
/**
 * Returns true if newLanguage is different than current language.
 * Only compares the language components of the language code.
 * Note: Language code validation is the responsibility of the caller. This
 * function assumes valid language codes.
 * Ex: 'fr-fr' and 'fr-ca' have the same language component, but different
 * locales. We would return false in the above case. Ex: 'fr-ca' and 'en-ca' are
 * different language components, but same locales. We would return true in the
 * above case.
 * @param {string} newLanguage The language for current output.
 * @return {boolean}
 */
LanguageSwitching.didLanguageSwitch = function(newLanguage) {
  // Compare language components of current and new language codes.
  var newLanguageComponents = newLanguage.split('-');
  var currentLanguageComponents = LanguageSwitching.currentLanguage_.split('-');
  if (newLanguageComponents[0] !== currentLanguageComponents[0]) {
    return true;
  }
  return false;
};

/**
 * Runs validation on language code and returns true if it's properly formatted.
 * @param {string} languageCode
 * @return {boolean}
 */
LanguageSwitching.isValidLanguageCode = function(languageCode) {
  // There are five possible components of a language code. See link for more
  // details: http://userguide.icu-project.org/locale
  // The TTS Engine handles parsing language codes, but it needs to have a
  // valid language component for the engine not to crash.
  // For example, given the language code 'en-US', 'en' is the language
  // component.
  var langComponentArray = languageCode.split('-');
  if (!langComponentArray || (langComponentArray.length === 0)) {
    return false;
  }

  // The language component should have length of either two or three.
  if (langComponentArray[0].length !== 2 &&
      langComponentArray[0].length !== 3) {
    return false;
  }

  // Use the accessibilityPrivate.getDisplayLanguage() API to validate language
  // code. If the language code is invalid, then this API returns an empty
  // string.
  if (chrome.accessibilityPrivate.getDisplayLanguage(
          languageCode, languageCode) === '') {
    return false;
  }

  return true;
};

/**
 * Returns true if there is a tts voice that supports the given languageCode.
 * This function is not responsible for deciding the proper output voice, it
 * simply tells us if output in |languageCode| is possible.
 * @param {string} languageCode
 * @return {boolean}
 */
LanguageSwitching.hasVoiceForLanguage = function(languageCode) {
  // Extract language from languageCode.
  var languageCodeComponents = languageCode.split('-');
  if (!languageCodeComponents || (languageCodeComponents.length === 0)) {
    return false;
  }
  var language = languageCodeComponents[0];
  for (var i = 0; i < LanguageSwitching.availableVoices_.length; ++i) {
    // Note: availableVoices_[i].lang is always in the form of
    // 'language-region'. See link for documentation on chrome.tts api:
    // https://developer.chrome.com/apps/tts#type-TtsVoice
    var candidateLanguage =
        LanguageSwitching.availableVoices_[i].lang.toLowerCase().split('-')[0];
    if (language === candidateLanguage) {
      return true;
    }
  }
  return false;
};
