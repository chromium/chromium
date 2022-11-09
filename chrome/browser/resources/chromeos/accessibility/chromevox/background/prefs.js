// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common page for reading and writing preferences from
 * the background context (background page or options page).
 */
import {AbstractTts} from '../common/abstract_tts.js';
import {BridgeConstants} from '../common/bridge_constants.js';
import {BridgeHelper} from '../common/bridge_helper.js';
import {Msgs} from '../common/msgs.js';

import {ChromeVox} from './chromevox.js';
import {ConsoleTts} from './console_tts.js';
import {EventStreamLogger} from './logging/event_stream_logger.js';
import {LogUrlWatcher} from './logging/log_url_watcher.js';
import {Output} from './output/output.js';
import {TtsBackground} from './tts_background.js';

/**
 * This object has default values of preferences and contains the common
 * code for working with preferences shared by the Options and Background
 * pages.
 */
export class ChromeVoxPrefs {
  constructor() {
    localStorage['lastRunVersion'] = chrome.runtime.getManifest().version;

    // Clear per session preferences.
    // This is to keep the position dictionary from growing excessively large.
    localStorage['position'] = '{}';

    // Default per session sticky to off.
    localStorage['sticky'] = false;
  }

  /**
   * Merge the default values of all known prefs with what's found in
   * localStorage.
   */
  static init() {
    ChromeVoxPrefs.instance = new ChromeVoxPrefs();

    // Set the default value of any pref that isn't already in localStorage.
    for (const pref in ChromeVoxPrefs.DEFAULT_PREFS) {
      if (localStorage[pref] === undefined) {
        localStorage[pref] = ChromeVoxPrefs.DEFAULT_PREFS[pref];
      }
    }
    ChromeVoxPrefs.instance.enableOrDisableLogUrlWatcher_();

    BridgeHelper.registerHandler(
        BridgeConstants.ChromeVoxPrefs.TARGET,
        BridgeConstants.ChromeVoxPrefs.Action.GET_PREFS,
        () => ChromeVoxPrefs.instance.getPrefs());
    BridgeHelper.registerHandler(
        BridgeConstants.ChromeVoxPrefs.TARGET,
        BridgeConstants.ChromeVoxPrefs.Action.SET_LOGGING_PREFS,
        (key, value) => ChromeVoxPrefs.instance.setLoggingPrefs(key, value));
    BridgeHelper.registerHandler(
        BridgeConstants.ChromeVoxPrefs.TARGET,
        BridgeConstants.ChromeVoxPrefs.Action.SET_PREF,
        (key, value) => ChromeVoxPrefs.instance.setPref(key, value));
  }

  /**
   * Get the prefs (not including keys).
   * @return {Object<string, string>} A map of all prefs except the key map from
   *     localStorage.
   */
  getPrefs() {
    const prefs = {};
    for (const pref in ChromeVoxPrefs.DEFAULT_PREFS) {
      prefs[pref] = localStorage[pref];
    }
    prefs['version'] = chrome.runtime.getManifest().version;
    return prefs;
  }

  /**
   * Set the value of a pref.
   * @param {string} key The pref key.
   * @param {Object|string|number|boolean} value The new value of the pref.
   */
  setPref(key, value) {
    if (localStorage[key] !== value) {
      localStorage[key] = value;
    }
  }

  /**
   * Set the value of a pref of logging options.
   * @param {ChromeVoxPrefs.loggingPrefs} key The pref key.
   * @param {boolean} value The new value of the pref.
   */
  setLoggingPrefs(key, value) {
    localStorage[key] = value;
    if (key === 'enableSpeechLogging') {
      TtsBackground.console.setEnabled(value);
    } else if (key === 'enableEventStreamLogging') {
      EventStreamLogger.instance.notifyEventStreamFilterChangedAll(value);
    }
    this.enableOrDisableLogUrlWatcher_();
  }

  /**
   * Sets the value of the sticky mode pref, as well as updating the listeners
   * and announcing.
   * @param {boolean} value
   */
  setAndAnnounceStickyPref(value) {
    chrome.accessibilityPrivate.setKeyboardListener(true, value);
    new Output()
        .withInitialSpeechProperties(AbstractTts.PERSONALITY_ANNOTATION)
        .withString(
            value ? Msgs.getMsg('sticky_mode_enabled') :
                    Msgs.getMsg('sticky_mode_disabled'))
        .go();
    this.setPref('sticky', value);
    ChromeVox.isStickyPrefOn = value;
  }

  enableOrDisableLogUrlWatcher_() {
    for (const pref of Object.values(ChromeVoxPrefs.loggingPrefs)) {
      if (localStorage[pref]) {
        LogUrlWatcher.create();
        return;
      }
    }
    LogUrlWatcher.destroy();
  }
}


/**
 * The default value of all preferences except the key map.
 * @const
 * @type {Object<Object>}
 */
ChromeVoxPrefs.DEFAULT_PREFS = {
  'announceDownloadNotifications': true,
  'announceRichTextAttributes': true,
  'audioStrategy': 'audioNormal',
  'autoRead': false,
  'brailleCaptions': false,
  'brailleSideBySide': true,
  'brailleTableType': 'brailleTable8',
  'brailleTable6': 'en-UEB-g2',
  'brailleTable8': 'en-nabcc',
  'capitalStrategy': 'increasePitch',
  'cvoxKey': '',
  'enableBrailleLogging': false,
  'enableEarconLogging': false,
  'enableSpeechLogging': false,
  'earcons': true,
  'enableEventStreamLogging': false,
  'focusFollowsMouse': false,
  'granularity': undefined,
  'languageSwitching': false,
  'menuBrailleCommands': false,
  'numberReadingStyle': 'asWords',
  'position': '{}',
  'smartStickyMode': true,
  'speakTextUnderMouse': false,
  'sticky': false,
  'typingEcho': 0,
  'useIBeamCursor': false,
  'useClassic': false,
  'usePitchChanges': true,
  'useVerboseMode': true,

  // eventStreamFilters
  'activedescendantchanged': true,
  'alert': true,
  'ariaAttributeChanged': true,
  'autocorrectionOccured': true,
  'blur': true,
  'checkedStateChanged': true,
  'childrenChanged': true,
  'clicked': true,
  'documentSelectionChanged': true,
  'documentTitleChanged': true,
  'expandedChanged': true,
  'focus': true,
  'focusContext': true,
  'imageFrameUpdated': true,
  'hide': true,
  'hitTestResult': true,
  'hover': true,
  'invalidStatusChanged': true,
  'layoutComplete': true,
  'liveRegionCreated': true,
  'liveRegionChanged': true,
  'loadComplete': true,
  'locationChanged': true,
  'mediaStartedPlaying': true,
  'mediaStoppedPlaying': true,
  'menuEnd': true,
  'menuListItemSelected': true,
  'menuListValueChanged': true,
  'menuPopupEnd': true,
  'menuPopupStart': true,
  'menuStart': true,
  'mouseCanceled': true,
  'mouseDragged': true,
  'mouseMoved': true,
  'mousePressed': true,
  'mouseReleased': true,
  'rowCollapsed': true,
  'rowCountChanged': true,
  'rowExpanded': true,
  'scrollPositionChanged': true,
  'scrolledToAnchor': true,
  'selectedChildrenChanged': true,
  'selection': true,
  'selectionAdd': true,
  'selectionRemove': true,
  'show': true,
  'stateChanged': true,
  'textChanged': true,
  'textSelectionChanged': true,
  'treeChanged': true,
  'valueInTextFieldChanged': true,
};


/** @enum {string} */
ChromeVoxPrefs.loggingPrefs = {
  SPEECH: 'enableSpeechLogging',
  BRAILLE: 'enableBrailleLogging',
  EARCON: 'enableEarconLogging',
  EVENT: 'enableEventStreamLogging',
};

/** @type {ChromeVoxPrefs} */
ChromeVoxPrefs.instance;
