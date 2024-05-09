// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class to manage Chrome settings prefs for ChromeVox. Wraps
 * around Settings for the underlying gets and sets. Acts as a translation layer
 * for key names, and for migrating. Will automatically migrate the ChromeVox
 * prefs that are listed in the |PREFS| constant at the bottom of the file. The
 * prefs will move out of chrome.storage.local (used by the LocalStorage class)
 * into the Chrome settings prefs system (used by other Accessibility
 * services).
 *
 */
import {LocalStorage} from '/common/local_storage.js';
import {Settings} from '/common/settings.js';
import {StringUtil} from '/common/string_util.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

const PrefObject = chrome.settingsPrivate.PrefObject;

export class SettingsManager {
  static async init() {
    if (SettingsManager.instance) {
      throw new Error(
          'SettingsManager.init() should be called at most once in each ' +
          'browser context.');
    }
    SettingsManager.instance = new SettingsManager();

    await Settings.init(SettingsManager.getAllPrefNames());
    SettingsManager.migrateFromChromeStorage_();
  }

  /**
   * Gets Chrome settings pref name from local storage key name.
   * E.g. 'eventStreamFilters' -> 'settings.a11y.chromevox.event_stream_filters'
   * @param {string} localStorageKey Name of a key used in local storage.
   * @return {string} Corresponding name for the Chrome settings pref.
   * @private
   */
  static getPrefName_(localStorageKey) {
    return 'settings.a11y.chromevox.' +
        StringUtil.camelToSnake(localStorageKey);
  }

  /**
   * Gets all Chrome settings pref names.
   * @private
   */
  static getAllPrefNames() {
    return SettingsManager.PREFS.map(SettingsManager.getPrefName_);
  }

  /**
   * Migrates prefs from chrome.storage.local to Chrome settings prefs.
   * @private
   */
  static migrateFromChromeStorage_() {
    for (const key of SettingsManager.PREFS) {
      let value = LocalStorage.get(key);
      if (value === undefined) {
        continue;
      }
      // Convert virtualBrailleColumns and virtualBrailleRows to numbers.
      if (['virtualBrailleColumns', 'virtualBrailleRows'].includes(key)) {
        value = parseInt(value, 10);
      }
      const prefName = SettingsManager.getPrefName_(key);
      try {
        Settings.set(prefName, value);
        LocalStorage.remove(key);
      } catch {
        console.error('Invalid settings pref name:', prefName);
      }
    }

    // This object starts empty so that we can know if there are any event
    // stream filters left to migrate from LocalStorage.
    let eventStreamFilters = {};
    for (const key of SettingsManager.EVENT_STREAM_FILTERS) {
      const value = LocalStorage.get(key);
      if (value === undefined) {
        continue;
      }
      eventStreamFilters[key] = value;
      LocalStorage.remove(key);
    }

    if (Object.keys(eventStreamFilters).length) {
      // Combine with any event stream filters already in settings prefs.
      eventStreamFilters = {
        ...Settings.get(this.getPrefName_('eventStreamFilters')),
        ...eventStreamFilters,
      };
      Settings.set(this.getPrefName_('eventStreamFilters'), eventStreamFilters);
    }
  }

  /**
   * @param {string} key
   * @param {Function} callback
   */
  static addListenerForKey(key, callback) {
    const pref = SettingsManager.getPrefName_(key);
    Settings.addListener(pref, callback);
  }

  /**
   * @param {string} key
   * @return {*}
   */
  static get(key) {
    const pref = SettingsManager.getPrefName_(key);
    return Settings.get(pref);
  }

  /**
   * @param {string} key
   * @param {string|Function} type A string (for primitives) or type constructor
   *     (for classes) corresponding to the expected type
   * @return {*}
   */
  static getTypeChecked(key, type) {
    const value = SettingsManager.get(key);
    if ((typeof type === 'string') && (typeof value === type)) {
      return value;
    }
    throw new Error(
        'Value in SettingsManager for key "' + key + '" is not a ' + type);
  }

  /**
   * @param {string} key
   * @return {boolean}
   */
  static getBoolean(key) {
    const value = SettingsManager.getTypeChecked(key, 'boolean');
    return Boolean(value);
  }

  /**
   * @param {string} key
   * @return {number}
   */
  static getNumber(key) {
    const value = SettingsManager.getTypeChecked(key, 'number');
    if (isNaN(value)) {
      throw new Error('Value in SettingsManager for key "' + key + '" is NaN');
    }
    return Number(value);
  }

  /**
   * @param {string} key
   * @return {string}
   */
  static getString(key) {
    const value = SettingsManager.getTypeChecked(key, 'string');
    return String(value);
  }

  /**
   * @param {string} key
   * @param {*} value
   */
  static set(key, value) {
    const pref = SettingsManager.getPrefName_(key);
    Settings.set(pref, value);
  }

  /**
   * Get event stream filters from the event_stream_filter dictionary pref.
   * @return {!Object<string, boolean>}
   */
  static getEventStreamFilters() {
    return Settings.get(this.getPrefName_('eventStreamFilters'));
  }

  /**
   * Set an event stream filter on the event_stream_filter dictionary pref.
   * @param {string} key
   * @param {boolean} value
   */
  static setEventStreamFilter(key, value) {
    if (!SettingsManager.EVENT_STREAM_FILTERS.includes(key)) {
      throw new Error('Cannot set unknown event stream filter: ' + key);
    }

    const eventStreamFilters =
        Settings.get(this.getPrefName_('eventStreamFilters'));
    eventStreamFilters[key] = value;
    Settings.set(this.getPrefName_('eventStreamFilters'), eventStreamFilters);
  }
}

/** @type {SettingsManager} */
SettingsManager.instance;

/**
 * List of the prefs used in ChromeVox, including in options page, each stored
 * as a Chrome settings pref.
 * @const {!Array<string>}
 */
SettingsManager.PREFS = [
  'announceDownloadNotifications',
  'announceRichTextAttributes',
  'audioStrategy',
  'autoRead',
  'brailleSideBySide',
  'brailleTable',
  'brailleTable6',
  'brailleTable8',
  'brailleTableType',
  'brailleWordWrap',
  'capitalStrategy',
  'capitalStrategyBackup',
  'enableBrailleLogging',
  'enableEarconLogging',
  'enableEventStreamLogging',
  'enableSpeechLogging',
  'eventStreamFilters',
  'languageSwitching',
  'menuBrailleCommands',
  'numberReadingStyle',
  'preferredBrailleDisplayAddress',
  'punctuationEcho',
  'smartStickyMode',
  'speakTextUnderMouse',
  'usePitchChanges',
  'useVerboseMode',
  'virtualBrailleColumns',
  'virtualBrailleRows',
  'voiceName',
];

/**
 * List of event stream filters used on the ChromeVox options page to indicate
 * which events to log, stored together in a Chrome settings dictionary pref.
 * @const {!Array<string>}
 */
SettingsManager.EVENT_STREAM_FILTERS = [
  'activedescendantchanged',
  'alert',
  // TODO(crbug.com/1464633) Fully remove ariaAttributeChangedDeprecated
  // starting in 122, because although it was removed in 118, it is still
  // present in earlier versions of LaCros.
  'ariaAttributeChangedDeprecated',
  'autocorrectionOccured',
  'blur',
  'checkedStateChanged',
  'childrenChanged',
  'clicked',
  'documentSelectionChanged',
  'documentTitleChanged',
  'expandedChanged',
  'focus',
  'focusContext',
  'hide',
  'hitTestResult',
  'hover',
  'imageFrameUpdated',
  'invalidStatusChanged',
  'layoutComplete',
  'liveRegionChanged',
  'liveRegionCreated',
  'loadComplete',
  'locationChanged',
  'mediaStartedPlaying',
  'mediaStoppedPlaying',
  'menuEnd',
  'menuItemSelected',
  'menuListValueChangedDeprecated',
  'menuPopupEnd',
  'menuPopupStart',
  'menuStart',
  'mouseCanceled',
  'mouseDragged',
  'mouseMoved',
  'mousePressed',
  'mouseReleased',
  'rowCollapsed',
  'rowCountChanged',
  'rowExpanded',
  'scrollPositionChanged',
  'scrolledToAnchor',
  'selectedChildrenChanged',
  'selection',
  'selectionAdd',
  'selectionRemove',
  'show',
  'stateChanged',
  'textChanged',
  'textSelectionChanged',
  'treeChanged',
  'valueInTextFieldChanged',
];

TestImportManager.exportForTesting(SettingsManager);
