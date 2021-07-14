// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrefsManager} from './prefs_manager.js';

class SelectToSpeakOptionsPage {
  constructor() {
    this.init_();
  }

  /**
   * Translate the page and sync all of the control values to the
   * values loaded from chrome.storage.
   */
  init_() {
    this.addTranslatedMessagesToDom_();
    // Depending on whether the enhanced TTS voices are enabled, show either the
    // enhanced voices settings or the legacy settings.
    const AccessibilityFeature =
        chrome.accessibilityPrivate.AccessibilityFeature;
    chrome.accessibilityPrivate.isFeatureEnabled(
        AccessibilityFeature.ENHANCED_NETWORK_VOICES, (result) => {
          const newElem = document.getElementById('naturalVoicesOptions');
          const legacyElem = document.getElementById('noNaturalVoicesOptions');
          if (!result) {
            // Show UI without natural voices
            this.hideElement(newElem);
            this.showElement(legacyElem);
            this.populateVoiceList_('voice');
            window.speechSynthesis.onvoiceschanged = (function() {
              this.populateVoiceList_('voice');
            }.bind(this));
            this.syncSelectControlToPref_('voice', 'voice', 'voiceName');
          } else {
            // Show UI with natural voices
            this.hideElement(legacyElem);
            this.showElement(newElem);
            this.populateVoiceList_('localVoices');
            window.speechSynthesis.onvoiceschanged = (function() {
              this.populateVoiceList_('localVoices');
            }.bind(this));

            this.syncSelectControlToPref_('localVoices', 'voice', 'voiceName');
            this.syncCheckboxControlToPref_(
                'naturalVoices', 'enhancedNetworkVoices', (checked) => {
                  const voice =
                      document.getElementById('naturalVoiceSelection');
                  const preview =
                      document.getElementById('naturalVoicePreview');
                  const select = document.getElementById('naturalVoice');
                  this.setElementVisible(voice, checked);
                  this.setElementVisible(preview, checked);
                  select.disabled = !checked;
                });
            // TODO(crbug.com/1227589): add enhanced voice and language
            // selection to prefs and sync the controls for those settings to
            // preferences.
          }
        });

    this.syncCheckboxControlToPref_(
        'wordHighlight', 'wordHighlight', (checked) => {
          const elem = document.getElementById('highlightSubOption');
          const select = document.getElementById('highlightColor');
          this.setElementVisible(elem, checked);
          select.disabled = !checked;
        });
    this.syncCheckboxControlToPref_(
        'backgroundShading', 'backgroundShading', (checked) => {
          const elem = document.getElementById('backgroundPreviewContainer');
          this.setElementVisible(elem, checked);
        });
    this.syncCheckboxControlToPref_('navigationControls', 'navigationControls');
    // Hide navigation control setting if feature is not enabled
    chrome.accessibilityPrivate.isFeatureEnabled(
        AccessibilityFeature.SELECT_TO_SPEAK_NAVIGATION_CONTROL, (result) => {
          const elem = document.getElementById('navigationControlsOption');
          this.setElementVisible(elem, result);
        });

    this.setUpHighlightListener_();
    this.setUpTtsButtonClickListener_();
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
   * Populate a select element with the list of TTS voices.
   * @param {string} selectId The id of the select element.
   * @private
   */
  populateVoiceList_(selectId) {
    chrome.tts.getVoices(function(voices) {
      const select = document.getElementById(selectId);
      select.innerHTML = '';

      // Add the system voice.
      const option = document.createElement('option');
      option.voiceName = PrefsManager.SYSTEM_VOICE;
      option.innerText = chrome.i18n.getMessage('select_to_speak_system_voice');
      select.add(option);

      voices.forEach(function(voice) {
        voice.voiceName = voice.voiceName || '';
      });
      voices.sort(function(a, b) {
        return a.voiceName.localeCompare(b.voiceName || '');
      });
      voices.forEach(function(voice) {
        if (!voice.voiceName) {
          return;
        }
        if (!voice.eventTypes.includes(chrome.tts.EventType.START) ||
            !voice.eventTypes.includes(chrome.tts.EventType.END) ||
            !voice.eventTypes.includes(chrome.tts.EventType.WORD) ||
            !voice.eventTypes.includes(chrome.tts.EventType.CANCELLED)) {
          // Required event types for Select-to-Speak.
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
   * Populate a checkbox with its current setting.
   * @param {string} checkboxId The id of the checkbox element.
   * @param {string} pref The name for a chrome.storage pref.
   * @param {?function(boolean): undefined=} opt_onChange A function
   * to be called every time the checkbox state is changed.
   * @private
   */
  syncCheckboxControlToPref_(checkboxId, pref, opt_onChange) {
    const checkbox = document.getElementById(checkboxId);

    function updateFromPref() {
      chrome.storage.sync.get(pref, function(items) {
        const value = items[pref];
        if (value != null) {
          checkbox.checked = value;
          if (opt_onChange) {
            opt_onChange(checkbox.checked);
          }
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
      const setParams = {};
      setParams[pref] = checkbox.checked;
      chrome.storage.sync.set(setParams);
    });

    checkbox.updateFunction = updateFromPref;
    updateFromPref();
    chrome.storage.onChanged.addListener(updateFromPref);
  }

  /**
   * Given the id of an HTML select element and the name of a chrome.storage
   * pref, sync them both ways.
   * @param {string} selectId The id of the select element.
   * @param {string} pref The name of a chrome.storage pref.
   * @param {string} valueKey The key of the option to use as value.
   * @param {?function(string): undefined=} opt_onChange Optional change
   *     listener to call when the setting has been changed.
   */
  syncSelectControlToPref_(selectId, pref, valueKey, opt_onChange) {
    var element = document.getElementById(selectId);

    function updateFromPref() {
      chrome.storage.sync.get(pref, function(items) {
        var value = items[pref];
        element.selectedIndex = -1;
        for (var i = 0; i < element.options.length; ++i) {
          if (element.options[i][valueKey] === value) {
            element.selectedIndex = i;
            break;
          }
        }
        if (opt_onChange) {
          opt_onChange(value);
        }
      });
    }

    element.addEventListener('change', function() {
      var newValue = element.options[element.selectedIndex][valueKey];
      var setParams = {};
      setParams[pref] = newValue;
      chrome.storage.sync.set(setParams);
    });

    element.updateFunction = updateFromPref;
    updateFromPref();
    chrome.storage.onChanged.addListener(updateFromPref);
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
        'highlightColor', 'highlightColor', 'value', onChange);

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
   * Sets up a listener on the TTS settings button.
   * @private
   */
  setUpTtsButtonClickListener_() {
    const button = document.getElementById('ttsSettingsBtn');
    button.addEventListener('click', () => {
      chrome.accessibilityPrivate.openSettingsSubpage(
          'manageAccessibility/tts');
    });
  }
}


new SelectToSpeakOptionsPage();
