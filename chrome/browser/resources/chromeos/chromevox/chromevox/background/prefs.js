// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common page for reading and writing preferences from
 * the background context (background page or options page).
 *
 */

goog.provide('cvox.ChromeVoxPrefs');

goog.require('ConsoleTts');
goog.require('EventStreamLogger');
goog.require('cvox.ChromeVox');
goog.require('cvox.ExtensionBridge');
goog.require('cvox.KeyMap');


/**
 * This object has default values of preferences and contains the common
 * code for working with preferences shared by the Options and Background
 * pages.
 * @constructor
 */
cvox.ChromeVoxPrefs = function() {
  var lastRunVersion = localStorage['lastRunVersion'];
  if (!lastRunVersion) {
    lastRunVersion = '1.16.0';
  }
  var loadExistingSettings = true;
  // TODO(dtseng): Logic below needs clarification. Perhaps needs a
  // 'lastIncompatibleVersion' member.
  if (lastRunVersion == '1.16.0') {
    loadExistingSettings = false;
  }
  localStorage['lastRunVersion'] = chrome.runtime.getManifest().version;

  /**
   * The current mapping from keys to command.
   * @type {!cvox.KeyMap}
   * @private
   */
  this.keyMap_ = cvox.KeyMap.fromLocalStorage() || cvox.KeyMap.fromDefaults();
  this.keyMap_.merge(cvox.KeyMap.fromDefaults());

  // Clear per session preferences.
  // This is to keep the position dictionary from growing excessively large.
  localStorage['position'] = '{}';

  // Default per session sticky to off.
  localStorage['sticky'] = false;

  this.init(loadExistingSettings);
};


/**
 * The default value of all preferences except the key map.
 * @const
 * @type {Object<Object>}
 */
cvox.ChromeVoxPrefs.DEFAULT_PREFS = {
  'active': true,
  'audioStrategy': 'audioNormal',
  'autoRead': false,
  'brailleCaptions': false,
  'brailleSideBySide': true,
  'brailleTableType': 'brailleTable8',
  'brailleTable6': 'en-UEB-g2',
  'brailleTable8': 'en-US-comp8',
  // TODO(dtseng): Leaking state about multiple key maps here until we have a
  // class to manage multiple key maps. Also, this doesn't belong as a pref;
  // should just store in local storage.
  'currentKeyMap': cvox.KeyMap.DEFAULT_KEYMAP,
  'cvoxKey': '',
  'enableBrailleLogging': false,
  'enableEarconLogging': true,
  'enableSpeechLogging': true,
  'earcons': true,
  'enableEventStreamLogging': false,
  'focusFollowsMouse': false,
  'granularity': undefined,
  'position': '{}',
  'siteSpecificEnhancements': true,
  'siteSpecificScriptBase':
      'https://ssl.gstatic.com/accessibility/javascript/ext/',
  'siteSpecificScriptLoader':
      'https://ssl.gstatic.com/accessibility/javascript/ext/loader.js',
  'speakTextUnderMouse': false,
  'sticky': false,
  'typingEcho': 0,
  'useIBeamCursor': cvox.ChromeVox.isMac,
  'useClassic': false,
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


/**
 * Merge the default values of all known prefs with what's found in
 * localStorage.
 * @param {boolean} pullFromLocalStorage or not to pull prefs from local
 * storage. True if we want to respect changes the user has already made
 * to prefs, false if we want to overwrite them. Set false if we've made
 * changes to keyboard shortcuts and need to make sure they aren't
 * overridden by the old keymap in local storage.
 */
cvox.ChromeVoxPrefs.prototype.init = function(pullFromLocalStorage) {
  // Set the default value of any pref that isn't already in localStorage.
  for (var pref in cvox.ChromeVoxPrefs.DEFAULT_PREFS) {
    if (localStorage[pref] === undefined) {
      localStorage[pref] = cvox.ChromeVoxPrefs.DEFAULT_PREFS[pref];
    }
  }
};

/**
 * Switches to another key map.
 * @param {string} selectedKeyMap The id of the keymap in
 * cvox.KeyMap.AVAIABLE_KEYMAP_INFO.
 */
cvox.ChromeVoxPrefs.prototype.switchToKeyMap = function(selectedKeyMap) {
  // Switching key maps potentially affects the key codes that involve
  // sequencing. Without resetting this list, potentially stale key
  // codes remain. The key codes themselves get pushed in
  // cvox.KeySequence.deserialize which gets called by cvox.KeyMap.
  cvox.ChromeVox.sequenceSwitchKeyCodes = [];

  // TODO(dtseng): Leaking state about multiple key maps here until we have a
  // class to manage multiple key maps.
  localStorage['currentKeyMap'] = selectedKeyMap;
  this.keyMap_ = cvox.KeyMap.fromCurrentKeyMap();
  cvox.ChromeVoxKbHandler.handlerKeyMap = this.keyMap_;
  this.keyMap_.toLocalStorage();
  this.keyMap_.resetModifier();
  this.sendPrefsToAllTabs(false, true);
};


/**
 * Get the prefs (not including keys).
 * @return {Object} A map of all prefs except the key map from localStorage.
 */
cvox.ChromeVoxPrefs.prototype.getPrefs = function() {
  var prefs = {};
  for (var pref in cvox.ChromeVoxPrefs.DEFAULT_PREFS) {
    prefs[pref] = localStorage[pref];
  }
  prefs['version'] = chrome.runtime.getManifest().version;
  return prefs;
};


/**
 * Reloads the key map from local storage.
 */
cvox.ChromeVoxPrefs.prototype.reloadKeyMap = function() {
  // Get the current key map from localStorage.
  // TODO(dtseng): We currently don't support merges since we write the entire
  // map back to local storage.
  var currentKeyMap = cvox.KeyMap.fromLocalStorage();
  if (!currentKeyMap) {
    currentKeyMap = cvox.KeyMap.fromCurrentKeyMap();
    currentKeyMap.toLocalStorage();
  }
  this.keyMap_ = currentKeyMap;
};


/**
 * Get the key map, from key binding to an array of [command, description].
 * @return {cvox.KeyMap} The key map.
 */
cvox.ChromeVoxPrefs.prototype.getKeyMap = function() {
  return this.keyMap_;
};


/**
 * Reset to the default key bindings.
 */
cvox.ChromeVoxPrefs.prototype.resetKeys = function() {
  this.keyMap_ = cvox.KeyMap.fromDefaults();
  this.keyMap_.toLocalStorage();
  this.sendPrefsToAllTabs(false, true);
};


/**
 * Send all of the settings to all tabs.
 * @param {boolean} sendPrefs Whether to send the prefs.
 * @param {boolean} sendKeyBindings Whether to send the key bindings.
 */
cvox.ChromeVoxPrefs.prototype.sendPrefsToAllTabs = function(
    sendPrefs, sendKeyBindings) {
  var context = this;
  var message = {};
  if (sendPrefs) {
    message['prefs'] = context.getPrefs();
  }
  if (sendKeyBindings) {
    // Note that cvox.KeyMap stringifies to a minimal object when message gets
    // passed to the content script.
    message['keyBindings'] = this.keyMap_.toJSON();
  }
  chrome.windows.getAll({populate: true}, function(windows) {
    for (var i = 0; i < windows.length; i++) {
      var tabs = windows[i].tabs;
      for (var j = 0; j < tabs.length; j++) {
        chrome.tabs.sendMessage(tabs[j].id, message);
      }
    }
  });
};

/**
 * Send all of the settings over the specified port.
 * @param {Port} port The port representing the connection to a content script.
 */
cvox.ChromeVoxPrefs.prototype.sendPrefsToPort = function(port) {
  port.postMessage(
      {'keyBindings': this.keyMap_.toJSON(), 'prefs': this.getPrefs()});
};


/**
 * Set the value of a pref and update all active tabs if it's changed.
 * @param {string} key The pref key.
 * @param {Object|string} value The new value of the pref.
 */
cvox.ChromeVoxPrefs.prototype.setPref = function(key, value) {
  if (localStorage[key] != value) {
    localStorage[key] = value;
    this.sendPrefsToAllTabs(true, false);
  }
};

/** @enum {string} */
cvox.ChromeVoxPrefs.loggingPrefs = {
  SPEECH: 'enableSpeechLogging',
  BRAILLE: 'enableBrailleLogging',
  EARCON: 'enableEarconLogging',
  EVENT: 'enableEventStreamLogging',
};

/**
 * Set the value of a pref of logging options.
 * @param {cvox.ChromeVoxPrefs.loggingPrefs} key The pref key.
 * @param {boolean} value The new value of the pref.
 */
cvox.ChromeVoxPrefs.prototype.setLoggingPrefs = function(key, value) {
  localStorage[key] = value;
  if (key == 'enableSpeechLogging')
    ConsoleTts.getInstance().setEnabled(value);
  else if (key == 'enableEventStreamLogging')
    EventStreamLogger.instance.notifyEventStreamFilterChangedAll(value);
};

/**
 * Delegates to cvox.KeyMap.
 * @param {string} command The command to set.
 * @param {cvox.KeySequence} newKey The new key to assign it to.
 * @return {boolean} True if the key was bound to the command.
 */
cvox.ChromeVoxPrefs.prototype.setKey = function(command, newKey) {
  if (this.keyMap_.rebind(command, newKey)) {
    this.keyMap_.toLocalStorage();
    this.sendPrefsToAllTabs(false, true);
    return true;
  }
  return false;
};
