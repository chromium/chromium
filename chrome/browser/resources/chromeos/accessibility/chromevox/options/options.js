// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox options page.
 *
 */

goog.provide('OptionsPage');

goog.require('AbstractTts');
goog.require('BluetoothBrailleDisplayUI');
goog.require('ConsoleTts');
goog.require('Msgs');
goog.require('PanelCommand');
goog.require('TtsBackground');
goog.require('BrailleTable');
goog.require('BrailleTranslatorManager');
goog.require('ChromeVox');
goog.require('ChromeVoxKbHandler');
goog.require('ChromeVoxPrefs');
goog.require('ExtensionBridge');

/** @const {string} */
const GOOGLE_TTS_EXTENSION_ID = 'gjjabgpgjpampikjhjpfhneeoapjbjaf';

/** @const {string} */
const ESPEAK_TTS_EXTENSION_ID = 'dakbfdmgjiabojdgbiljlhgjbokobjpg';

/**
 * Class to manage the options page.
 */
OptionsPage = class {
  constructor() {}

  /**
   * Initialize the options page by setting the current value of all prefs, and
   * adding event listeners.
   * @this {OptionsPage}
   */
  static init() {
    OptionsPage.prefs = chrome.extension.getBackgroundPage()['prefs'];
    OptionsPage.consoleTts =
        chrome.extension.getBackgroundPage().ConsoleTts.getInstance();
    OptionsPage.backgroundTts =
        chrome.extension.getBackgroundPage().ChromeVoxState.backgroundTts;
    OptionsPage.populateVoicesSelect();
    BrailleTable.getAll(function(tables) {
      /** @type {!Array<BrailleTable.Table>} */
      OptionsPage.brailleTables = tables;
      OptionsPage.populateBrailleTablesSelect();
    });
    chrome.storage.local.get({'brailleWordWrap': true}, function(items) {
      $('brailleWordWrap').checked = items.brailleWordWrap;
    });

    chrome.storage.local.get({'virtualBrailleRows': 1}, function(items) {
      $('virtual_braille_display_rows_input').value =
          items['virtualBrailleRows'];
    });
    chrome.storage.local.get({'virtualBrailleColumns': 40}, function(items) {
      $('virtual_braille_display_columns_input').value =
          items['virtualBrailleColumns'];
    });
    const changeToInterleave =
        Msgs.getMsg('options_change_current_display_style_interleave');
    const changeToSideBySide =
        Msgs.getMsg('options_change_current_display_style_side_by_side');
    const currentlyDisplayingInterleave =
        Msgs.getMsg('options_current_display_style_interleave');
    const currentlyDisplayingSideBySide =
        Msgs.getMsg('options_current_display_style_side_by_side');
    $('changeDisplayStyle').textContent =
        localStorage['brailleSideBySide'] === 'true' ? changeToInterleave :
                                                       changeToSideBySide;
    $('currentDisplayStyle').textContent =
        localStorage['brailleSideBySide'] === 'true' ?
        currentlyDisplayingSideBySide :
        currentlyDisplayingInterleave;

    const showEventStreamFilters =
        Msgs.getMsg('options_show_event_stream_filters');
    const hideEventStreamFilters =
        Msgs.getMsg('options_hide_event_stream_filters');
    $('toggleEventStreamFilters').textContent = showEventStreamFilters;
    OptionsPage.disableEventStreamFilterCheckBoxes(
        localStorage['enableEventStreamLogging'] === 'false');

    if (localStorage['audioStrategy']) {
      for (let i = 0, opt; opt = $('audioStrategy').options[i]; i++) {
        if (opt.id === localStorage['audioStrategy']) {
          opt.setAttribute('selected', '');
        }
      }
    }
    if (localStorage['capitalStrategy']) {
      for (let i = 0, opt; opt = $('capitalStrategy').options[i]; ++i) {
        if (opt.id === localStorage['capitalStrategy']) {
          opt.setAttribute('selected', '');
        }
      }
    }

    if (localStorage['numberReadingStyle']) {
      for (let i = 0, opt; opt = $('numberReadingStyle').options[i]; ++i) {
        if (opt.id === localStorage['numberReadingStyle']) {
          opt.setAttribute('selected', '');
        }
      }
    }

    if (localStorage[AbstractTts.PUNCTUATION_ECHO]) {
      const currentPunctuationEcho =
          AbstractTts
              .PUNCTUATION_ECHOES[localStorage[AbstractTts.PUNCTUATION_ECHO]];
      for (let i = 0, opt; opt = $('punctuationEcho').options[i]; ++i) {
        if (opt.id === currentPunctuationEcho.name) {
          opt.setAttribute('selected', '');
        }
      }
    }

    $('toggleEventStreamFilters').addEventListener('click', function(evt) {
      if ($('eventStreamFilters').hidden) {
        $('eventStreamFilters').hidden = false;
        $('toggleEventStreamFilters').textContent = hideEventStreamFilters;
      } else {
        $('eventStreamFilters').hidden = true;
        $('toggleEventStreamFilters').textContent = showEventStreamFilters;
      }
    });

    $('openTtsSettings').addEventListener('click', (evt) => {
      chrome.accessibilityPrivate.openSettingsSubpage(
          'manageAccessibility/tts');
    });

    $('enableAllEventStreamFilters').addEventListener('click', () => {
      OptionsPage.setAllEventStreamLoggingFilters(true);
    });
    $('disableAllEventStreamFilters').addEventListener('click', () => {
      OptionsPage.setAllEventStreamLoggingFilters(false);
    });

    $('chromeVoxDeveloperOptions').addEventListener('expanded-changed', () => {
      const hidden = !$('chromeVoxDeveloperOptions')['expanded'];
      $('developerSpeechLogging').hidden = hidden;
      $('developerEarconLogging').hidden = hidden;
      $('developerBrailleLogging').hidden = hidden;
      $('developerEventStream').hidden = hidden;
      $('showDeveloperLog').hidden = hidden;
    });

    $('openDeveloperLog').addEventListener('click', function(evt) {
      const logPage = {url: 'chromevox/background/logging/log.html'};
      chrome.tabs.create(logPage);
    });

    Msgs.addTranslatedMessagesToDom(document);
    OptionsPage.hidePlatformSpecifics();

    OptionsPage.update();

    document.addEventListener('change', OptionsPage.eventListener, false);
    document.addEventListener('click', OptionsPage.eventListener, false);
    document.addEventListener('keydown', OptionsPage.eventListener, false);

    window.addEventListener('storage', (event) => {
      if (event.key === 'speakTextUnderMouse') {
        chrome.accessibilityPrivate.enableMouseEvents(
            event.newValue === String(true));
      }
    });

    ExtensionBridge.addMessageListener(function(message) {
      if (message['prefs']) {
        OptionsPage.update();
      }
    });

    const clearVirtualDisplay = function() {
      const groups = [];
      const sizeOfDisplay =
          parseInt($('virtual_braille_display_rows_input').innerHTML, 10) *
          parseInt($('virtual_braille_display_columns_input').innerHTML, 10);
      for (let i = 0; i < sizeOfDisplay; i++) {
        groups.push(['X', 'X']);
      }
      (new PanelCommand(PanelCommandType.UPDATE_BRAILLE, {groups})).send();
    };

    $('changeDisplayStyle').addEventListener('click', function(evt) {
      const sideBySide = localStorage['brailleSideBySide'] !== 'true';
      localStorage['brailleSideBySide'] = sideBySide;
      $('changeDisplayStyle').textContent =
          sideBySide ? changeToInterleave : changeToSideBySide;
      $('currentDisplayStyle').textContent = sideBySide ?
          currentlyDisplayingSideBySide :
          currentlyDisplayingInterleave;
      clearVirtualDisplay();
    }, true);

    handleNumericalInputPref(
        'virtual_braille_display_rows_input', 'virtualBrailleRows');
    handleNumericalInputPref(
        'virtual_braille_display_columns_input', 'virtualBrailleColumns');

    /** @type {!BluetoothBrailleDisplayUI} */
    OptionsPage.bluetoothBrailleDisplayUI = new BluetoothBrailleDisplayUI();

    const bluetoothBraille = $('bluetoothBraille');
    if (bluetoothBraille) {
      OptionsPage.bluetoothBrailleDisplayUI.attach(bluetoothBraille);
    }

    $('usePitchChanges').addEventListener('click', (evt) => {
      // The capitalStrategy pref depends on the value of usePitchChanges.
      // When usePitchChanges is toggled, we should update the preference value
      // and options for capitalStrategy.
      const checked = evt.target.checked;
      if (!checked) {
        $('announceCapitals').selected = true;
        $('increasePitch').selected = false;
        $('increasePitch').disabled = true;
        localStorage['capitalStrategyBackup'] = localStorage['capitalStrategy'];
        OptionsPage.prefs.setPref('capitalStrategy', 'announceCapitals');
      } else {
        $('increasePitch').disabled = false;
        const capitalStrategyBackup = localStorage['capitalStrategyBackup'];
        if (capitalStrategyBackup) {
          // Restore original capitalStrategy setting.
          $('announceCapitals').selected =
              (capitalStrategyBackup === 'announceCapitals');
          $('increasePitch').selected =
              (capitalStrategyBackup === 'increasePitch');
          OptionsPage.prefs.setPref('capitalStrategy', capitalStrategyBackup);
        }
      }
    });
  }

  /**
   * Update the value of controls to match the current preferences.
   * This happens if the user presses a key in a tab that changes a
   * pref.
   */
  static update() {
    const prefs = OptionsPage.prefs.getPrefs();
    for (const key in prefs) {
      // TODO(rshearer): 'active' is a pref, but there's no place in the
      // options page to specify whether you want ChromeVox active.
      const elements = document.querySelectorAll('*[name="' + key + '"]');
      for (let i = 0; i < elements.length; i++) {
        OptionsPage.setValue(elements[i], prefs[key]);
      }
    }
  }

  /**
   * Populates the voices select with options.
   */
  static populateVoicesSelect() {
    const select = $('voices');

    function setVoiceList() {
      const selectedVoice =
          chrome.extension.getBackgroundPage()['getCurrentVoice']();
      const addVoiceOption = (visibleVoiceName, voiceName) => {
        const option = document.createElement('option');
        option.voiceName = voiceName;
        option.innerText = visibleVoiceName;
        if (selectedVoice === voiceName) {
          option.setAttribute('selected', '');
        }
        select.add(option);
      };
      chrome.tts.getVoices(function(voices) {
        select.innerHTML = '';
        // TODO(plundblad): voiceName can actually be omitted in the TTS engine.
        // We should generate a name in that case.
        voices.forEach(function(voice) {
          voice.voiceName = voice.voiceName || '';
        });
        voices.sort(function(a, b) {
          // Prefer Google tts voices over all others.
          if (a.extensionId === GOOGLE_TTS_EXTENSION_ID &&
              b.extensionId !== GOOGLE_TTS_EXTENSION_ID) {
            return -1;
          }

          // Next, prefer Espeak tts voices.
          if (a.extensionId === ESPEAK_TTS_EXTENSION_ID &&
              b.extensionId !== ESPEAK_TTS_EXTENSION_ID) {
            return -1;
          }

          // Finally, prefer local over remote voices.
          if (!a['remote'] && b['remote']) {
            return -1;
          }

          return 0;
        });
        addVoiceOption(Msgs.getMsg('system_voice'), constants.SYSTEM_VOICE);
        voices.forEach((voice) => {
          addVoiceOption(voice.voiceName, voice.voiceName);
        });
      });
    }

    window.speechSynthesis.onvoiceschanged = setVoiceList;
    setVoiceList();

    select.addEventListener('change', function(evt) {
      const selIndex = select.selectedIndex;
      const sel = select.options[selIndex];
      chrome.storage.local.set({voiceName: sel.voiceName});
    }, true);
  }

  /**
   * Populates the braille select control.
   */
  static populateBrailleTablesSelect() {
    const tables = OptionsPage.brailleTables;
    const populateSelect = function(node, dots) {
      const activeTable = localStorage[node.id] || localStorage['brailleTable'];
      // Gather the display names and sort them according to locale.
      const items = [];
      for (let i = 0, table; table = tables[i]; i++) {
        if (table.dots !== dots) {
          continue;
        }
        items.push({id: table.id, name: BrailleTable.getDisplayName(table)});
      }
      items.sort(function(a, b) {
        return a.name.localeCompare(b.name);
      });
      for (let i = 0, item; item = items[i]; ++i) {
        const elem = document.createElement('option');
        elem.id = item.id;
        elem.textContent = item.name;
        if (item.id === activeTable) {
          elem.setAttribute('selected', '');
        }
        node.appendChild(elem);
      }
    };
    const select6 = $('brailleTable6');
    const select8 = $('brailleTable8');
    populateSelect(select6, '6');
    populateSelect(select8, '8');

    const handleBrailleSelect = function(node) {
      return function(evt) {
        const selIndex = node.selectedIndex;
        const sel = node.options[selIndex];
        localStorage['brailleTable'] = sel.id;
        localStorage[node.id] = sel.id;
        OptionsPage.getBrailleTranslatorManager().refresh(
            localStorage['brailleTable']);
      };
    };

    select6.addEventListener('change', handleBrailleSelect(select6), true);
    select8.addEventListener('change', handleBrailleSelect(select8), true);

    const tableTypeButton = $('brailleTableType');
    const updateTableType = function(setFocus) {
      const currentTableType =
          localStorage['brailleTableType'] || 'brailleTable6';
      if (currentTableType === 'brailleTable6') {
        select6.parentElement.style.display = 'block';
        select8.parentElement.style.display = 'none';
        if (setFocus) {
          select6.focus();
        }
        localStorage['brailleTable'] = localStorage['brailleTable6'];
        localStorage['brailleTableType'] = 'brailleTable6';
        tableTypeButton.textContent =
            Msgs.getMsg('options_braille_table_type_6');
      } else {
        select6.parentElement.style.display = 'none';
        select8.parentElement.style.display = 'block';
        if (setFocus) {
          select8.focus();
        }
        localStorage['brailleTable'] = localStorage['brailleTable8'];
        localStorage['brailleTableType'] = 'brailleTable8';
        tableTypeButton.textContent =
            Msgs.getMsg('options_braille_table_type_8');
      }
      OptionsPage.getBrailleTranslatorManager().refresh(
          localStorage['brailleTable']);
    };
    updateTableType(false);

    tableTypeButton.addEventListener('click', function(evt) {
      const oldTableType = localStorage['brailleTableType'];
      localStorage['brailleTableType'] =
          oldTableType === 'brailleTable6' ? 'brailleTable8' : 'brailleTable6';
      updateTableType(true);
    }, true);
  }

  /**
   * Set the html element for a preference to match the given value.
   * @param {Element} element The HTML control.
   * @param {string} value The new value.
   */
  static setValue(element, value) {
    if (element.tagName === 'INPUT' && element.type === 'checkbox') {
      element.checked = (value === 'true');
    } else if (element.tagName === 'INPUT' && element.type === 'radio') {
      element.checked = (String(element.value) === value);
    } else {
      element.value = value;
    }
  }

  /**
   * Disable event stream logging filter check boxes.
   * Check boxes should be disabled when event stream logging is disabled.
   * @param {boolean} disable
   */
  static disableEventStreamFilterCheckBoxes(disable) {
    const filters = document.querySelectorAll('.option-eventstream > input');
    for (let i = 0; i < filters.length; i++) {
      filters[i].disabled = disable;
    }
  }

  /**
   * Set all event stream logging filter to on or off.
   * @param {boolean} enabled
   */
  static setAllEventStreamLoggingFilters(enabled) {
    for (const checkbox of document.querySelectorAll(
             '.option-eventstream > input')) {
      if (checkbox.checked !== enabled) {
        OptionsPage.setEventStreamFilter(checkbox.name, enabled);
      }
    }
  }

  /**
   * Set the specified event logging filter to on or off.
   * @param {string} name
   * @param {boolean} enabled
   */
  static setEventStreamFilter(name, enabled) {
    OptionsPage.prefs.setPref(name, enabled);
    chrome.extension.getBackgroundPage()
        .EventStreamLogger.instance.notifyEventStreamFilterChanged(
            name, enabled);
  }

  /**
   * Event listener, called when an event occurs in the page that might
   * affect one of the preference controls.
   * @param {Event} event The event.
   * @return {boolean} True if the default action should occur.
   */
  static eventListener(event) {
    window.setTimeout(function() {
      const target = event.target;
      if (target.id === 'brailleWordWrap') {
        chrome.storage.local.set({brailleWordWrap: target.checked});
      } else if (target.className.indexOf('logging') !== -1) {
        OptionsPage.prefs.setLoggingPrefs(target.name, target.checked);
        if (target.name === 'enableEventStreamLogging') {
          OptionsPage.disableEventStreamFilterCheckBoxes(!target.checked);
        }
      } else if (target.className.indexOf('eventstream') !== -1) {
        OptionsPage.setEventStreamFilter(target.name, target.checked);
      } else if (target.id === 'punctuationEcho') {
        const selectedPunctuationEcho = target.options[target.selectedIndex].id;
        const punctuationEcho = AbstractTts.PUNCTUATION_ECHOES.findIndex(
            echo => echo.name === selectedPunctuationEcho);
        OptionsPage.backgroundTts.updatePunctuationEcho(punctuationEcho);
      } else if (target.classList.contains('pref')) {
        if (target.tagName === 'INPUT' && target.type === 'checkbox') {
          OptionsPage.prefs.setPref(target.name, target.checked);
        } else if (target.tagName === 'INPUT' && target.type === 'radio') {
          const key = target.name;
          const elements = document.querySelectorAll('*[name="' + key + '"]');
          for (let i = 0; i < elements.length; i++) {
            if (elements[i].checked) {
              OptionsPage.prefs.setPref(target.name, elements[i].value);
            }
          }
        } else if (target.tagName === 'SELECT') {
          const selIndex = target.selectedIndex;
          const sel = target.options[selIndex];
          const value = sel ? sel.id : 'audioNormal';
          OptionsPage.prefs.setPref(target.id, value);
        }
      }
    }, 0);
    return true;
  }

  /**
   * Hides all elements not matching the current platform.
   */
  static hidePlatformSpecifics() {}

  /**
   * @return {BrailleTranslatorManager}
   */
  static getBrailleTranslatorManager() {
    return chrome.extension.getBackgroundPage()['braille_translator_manager'];
  }
};

/**
 * The ChromeVoxPrefs object.
 * @type {ChromeVoxPrefs}
 */
OptionsPage.prefs;

/**
 * The ConsoleTts object.
 * @type {ConsoleTts}
 */
OptionsPage.consoleTts;

/**
 * The TtsBackground object.
 * @type {TtsBackground}
 */
OptionsPage.backgroundTts;

/**
 * Adds event listeners to input boxes to update local storage values and
 * make sure that the input is a positive nonempty number between 1 and 99.
 * @param {string} id Id of the input box.
 * @param {string} pref Preference key in localStorage to access and modify.
 */
const handleNumericalInputPref = function(id, pref) {
  $(id).addEventListener('input', function(evt) {
    if ($(id).value === '') {
      return;
    } else if (
        parseInt($(id).value, 10) < 1 || parseInt($(id).value, 10) > 99) {
      chrome.storage.local.get(pref, function(items) {
        $(id).value = items[pref];
      });
    } else {
      const items = {};
      items[pref] = $(id).value;
      chrome.storage.local.set(items);
    }
  }, true);

  $(id).addEventListener('focusout', function(evt) {
    if ($(id).value === '') {
      chrome.storage.local.get(pref, function(items) {
        $(id).value = items[pref];
      });
    }
  }, true);
};


document.addEventListener('DOMContentLoaded', function() {
  OptionsPage.init();
}, false);

window.addEventListener('beforeunload', function(e) {
  OptionsPage.bluetoothBrailleDisplayUI.detach();
});
