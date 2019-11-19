// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 */
let SelectToSpeakOptionsPage = function() {
  this.init_();
};

SelectToSpeakOptionsPage.prototype = {
  /**
   * Translate the page and sync all of the control values to the
   * values loaded from chrome.storage.
   */
  init_: function() {
    this.addTranslatedMessagesToDom_();
    this.populateVoiceList_('voice');
    window.speechSynthesis.onvoiceschanged = (function() {
      this.populateVoiceList_('voice');
    }.bind(this));
    this.syncSelectControlToPref_('voice', 'voice', 'voiceName');
    this.syncCheckboxControlToPref_(
        'wordHighlight', 'wordHighlight', function(checked) {
          let elem = document.getElementById('highlightSubOption');
          let select = document.getElementById('highlightColor');
          if (checked) {
            elem.classList.remove('hidden');
            elem.setAttribute('aria-hidden', false);
            select.disabled = false;
          } else {
            elem.classList.add('hidden');
            elem.setAttribute('aria-hidden', true);
            select.disabled = true;
          }
        });
    this.setUpHighlightListener_();
    this.setUpTtsButtonClickListener_();
    chrome.metricsPrivate.recordUserAction(
        'Accessibility.CrosSelectToSpeak.LoadSettings');
  },

  /**
   * Processes an HTML DOM, replacing text content with translated text messages
   * on elements marked up for translation.  Elements whose class attributes
   * contain the 'i18n' class name are expected to also have an msgid
   * attribute. The value of the msgid attributes are looked up as message
   * IDs and the resulting text is used as the text content of the elements.
   * @private
   */
  addTranslatedMessagesToDom_: function() {
    var elts = document.querySelectorAll('.i18n');
    for (var i = 0; i < elts.length; i++) {
      var msgid = elts[i].getAttribute('msgid');
      if (!msgid) {
        throw new Error('Element has no msgid attribute: ' + elts[i]);
      }
      var translated = chrome.i18n.getMessage('select_to_speak_' + msgid);
      if (elts[i].tagName == 'INPUT') {
        elts[i].setAttribute('placeholder', translated);
      } else {
        elts[i].textContent = translated;
      }
      elts[i].classList.add('i18n-processed');
    }
  },

  /**
   * Populate a select element with the list of TTS voices.
   * @param {string} selectId The id of the select element.
   * @private
   */
  populateVoiceList_: function(selectId) {
    chrome.tts.getVoices(function(voices) {
      let select = document.getElementById(selectId);
      select.innerHTML = '';

      // Add the system voice.
      let option = document.createElement('option');
      option.voiceName = PrefsManager.SYSTEM_VOICE;
      option.innerText = chrome.i18n.getMessage('select_to_speak_system_voice');
      select.add(option);

      voices.forEach(function(voice) {
        voice.voiceName = voice.voiceName || '';
      });
      voices.sort(function(a, b) {
        return a.voiceName.localeCompare(b.voiceName);
      });
      voices.forEach(function(voice) {
        if (!voice.voiceName) {
          return;
        }
        if (!voice.eventTypes.includes('start') ||
            !voice.eventTypes.includes('end') ||
            !voice.eventTypes.includes('word') ||
            !voice.eventTypes.includes('cancelled')) {
          // Required event types for Select-to-Speak.
          return;
        }
        let option = document.createElement('option');
        option.voiceName = voice.voiceName;
        option.innerText = option.voiceName;
        select.add(option);
      });
      if (select.updateFunction) {
        select.updateFunction();
      }
    });
  },

  /**
   * Populate a checkbox with its current setting.
   * @param {string} checkboxId The id of the checkbox element.
   * @param {string} pref The name for a chrome.storage pref.
   * @param {?function(boolean): undefined=} opt_onChange A function
   * to be called every time the checkbox state is changed.
   * @private
   */
  syncCheckboxControlToPref_: function(checkboxId, pref, opt_onChange) {
    let checkbox = document.getElementById(checkboxId);

    function updateFromPref() {
      chrome.storage.sync.get(pref, function(items) {
        let value = items[pref];
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
      let setParams = {};
      setParams[pref] = checkbox.checked;
      chrome.storage.sync.set(setParams);
    });

    checkbox.updateFunction = updateFromPref;
    updateFromPref();
    chrome.storage.onChanged.addListener(updateFromPref);
  },

  /**
   * Given the id of an HTML select element and the name of a chrome.storage
   * pref, sync them both ways.
   * @param {string} selectId The id of the select element.
   * @param {string} pref The name of a chrome.storage pref.
   * @param {string} valueKey The key of the option to use as value.
   * @param {?function(string): undefined=} opt_onChange Optional change
   *     listener to call when the setting has been changed.
   */
  syncSelectControlToPref_: function(selectId, pref, valueKey, opt_onChange) {
    var element = document.getElementById(selectId);

    function updateFromPref() {
      chrome.storage.sync.get(pref, function(items) {
        var value = items[pref];
        element.selectedIndex = -1;
        for (var i = 0; i < element.options.length; ++i) {
          if (element.options[i][valueKey] == value) {
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
  },

  /**
   * Sets up the highlight listeners and preferences.
   * @private
   */
  setUpHighlightListener_: function() {
    let onChange = function(value) {
      let examples = document.getElementsByClassName('highlight');
      for (let i = 0; i < examples.length; i++) {
        examples[i].style.background = value;
      }
    };

    this.syncSelectControlToPref_(
        'highlightColor', 'highlightColor', 'value', onChange);

    document.getElementById('wordHighlightOption')
        .addEventListener('click', function(e) {
          e.stopPropagation();
          let checkbox = document.getElementById('wordHighlight');
          // Make sure it isn't the auto-generated click itself.
          if (e.srcElement !== checkbox) {
            checkbox.click();
          }
        });
  },

  /**
   * Sets up a listener on the TTS settings button.
   * @private
   */
  setUpTtsButtonClickListener_: function() {
    let button = document.getElementById('ttsSettingsBtn');
    button.addEventListener('click', () => {
      chrome.accessibilityPrivate.openSettingsSubpage(
          'manageAccessibility/tts');
    });
  }
};

new SelectToSpeakOptionsPage();
