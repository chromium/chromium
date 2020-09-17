// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common page for reading and writing preferences from
 * the background context (background page or options page).
 *
 */

goog.provide('ChromeVoxPrefs');
goog.provide('RichTextSpeechStyle');

goog.require('ConsoleTts');
goog.require('EventStreamLogger');
goog.require('ChromeVox');
goog.require('ExtensionBridge');
goog.require('KeyMap');

/**
 * This object has default values of preferences and contains the common
 * code for working with preferences shared by the Options and Background
 * pages.
 */
ChromeVoxPrefs = class {
  constructor() {
    let lastRunVersion = localStorage['lastRunVersion'];
    if (!lastRunVersion) {
      lastRunVersion = '1.16.0';
    }
    let loadExistingSettings = true;
    // TODO(dtseng): Logic below needs clarification. Perhaps needs a
    // 'lastIncompatibleVersion' member.
    if (lastRunVersion == '1.16.0') {
      loadExistingSettings = false;
    }
    localStorage['lastRunVersion'] = chrome.runtime.getManifest().version;

    /**
     * The current mapping from keys to command.
     * @type {!KeyMap}
     * @private
     */
    this.keyMap_ = KeyMap.fromLocalStorage() || KeyMap.fromDefaults();
    this.keyMap_.merge(KeyMap.fromDefaults());

    // Clear per session preferences.
    // This is to keep the position dictionary from growing excessively large.
    localStorage['position'] = '{}';

    // Default per session sticky to off.
    localStorage['sticky'] = false;

    this.init(loadExistingSettings);
  }

  /**
   * Merge the default values of all known prefs with what's found in
   * localStorage.
   * @param {boolean} pullFromLocalStorage or not to pull prefs from local
   * storage. True if we want to respect changes the user has already made
   * to prefs, false if we want to overwrite them. Set false if we've made
   * changes to keyboard shortcuts and need to make sure they aren't
   * overridden by the old keymap in local storage.
   */
  init(pullFromLocalStorage) {
    // Set the default value of any pref that isn't already in localStorage.
    for (const pref in ChromeVoxPrefs.DEFAULT_PREFS) {
      if (localStorage[pref] === undefined) {
        localStorage[pref] = ChromeVoxPrefs.DEFAULT_PREFS[pref];
      }
    }
  }

  /**
   * Switches to another key map.
   * @param {string} selectedKeyMap The id of the keymap in
   * KeyMap.AVAIABLE_KEYMAP_INFO.
   */
  switchToKeyMap(selectedKeyMap) {
    // Switching key maps potentially affects the key codes that involve
    // sequencing. Without resetting this list, potentially stale key
    // codes remain. The key codes themselves get pushed in
    // KeySequence.deserialize which gets called by KeyMap.
    ChromeVox.sequenceSwitchKeyCodes = [];

    // TODO(dtseng): Leaking state about multiple key maps here until we have a
    // class to manage multiple key maps.
    localStorage['currentKeyMap'] = selectedKeyMap;
    this.keyMap_ = KeyMap.fromCurrentKeyMap();
    ChromeVoxKbHandler.handlerKeyMap = this.keyMap_;
    this.keyMap_.toLocalStorage();
    this.keyMap_.resetModifier();
  }

  /**
   * Get the prefs (not including keys).
   * @return {Object} A map of all prefs except the key map from localStorage.
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
   * Reloads the key map from local storage.
   */
  reloadKeyMap() {
    // Get the current key map from localStorage.
    // TODO(dtseng): We currently don't support merges since we write the entire
    // map back to local storage.
    let currentKeyMap = KeyMap.fromLocalStorage();
    if (!currentKeyMap) {
      currentKeyMap = KeyMap.fromCurrentKeyMap();
      currentKeyMap.toLocalStorage();
    }
    this.keyMap_ = currentKeyMap;
  }

  /**
   * Get the key map, from key binding to an array of [command, description].
   * @return {KeyMap} The key map.
   */
  getKeyMap() {
    return this.keyMap_;
  }

  /**
   * Reset to the default key bindings.
   */
  resetKeys() {
    this.keyMap_ = KeyMap.fromDefaults();
    this.keyMap_.toLocalStorage();
  }

  /**
   * Set the value of a pref.
   * @param {string} key The pref key.
   * @param {Object|string|boolean} value The new value of the pref.
   */
  setPref(key, value) {
    if (localStorage[key] != value) {
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
    if (key == 'enableSpeechLogging') {
      ConsoleTts.getInstance().setEnabled(value);
    } else if (key == 'enableEventStreamLogging') {
      EventStreamLogger.instance.notifyEventStreamFilterChangedAll(value);
    }
  }

  /**
   * Delegates to KeyMap.
   * @param {string} command The command to set.
   * @param {KeySequence} newKey The new key to assign it to.
   * @return {boolean} True if the key was bound to the command.
   */
  setKey(command, newKey) {
    if (this.keyMap_.rebind(command, newKey)) {
      this.keyMap_.toLocalStorage();
      return true;
    }
    return false;
  }
};


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
  'brailleTable8': 'en-US-comp8',
  'capitalStrategy': 'increasePitch',
  'currentKeyMap': KeyMap.DEFAULT_KEYMAP,
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
  'valueChanged': true
};


/** @enum {string} */
ChromeVoxPrefs.loggingPrefs = {
  SPEECH: 'enableSpeechLogging',
  BRAILLE: 'enableBrailleLogging',
  EARCON: 'enableEarconLogging',
  EVENT: 'enableEventStreamLogging',
};

/** @type {!ChromeVoxPrefs} */
ChromeVoxPrefs.instance = new ChromeVoxPrefs();
