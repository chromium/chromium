/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/**
 * @fileoverview
 * 'settings-prefs' exposes a singleton model of Chrome settings and
 * preferences, which listens to changes to Chrome prefs whitelisted in
 * chrome.settingsPrivate. When changing prefs in this element's 'prefs'
 * property via the UI, the singleton model tries to set those preferences in
 * Chrome. Whether or not the calls to settingsPrivate.setPref succeed, 'prefs'
 * is eventually consistent with the Chrome pref store.
 */

import {assert} from '//resources/js/assert.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrSettingsPrefs} from './prefs_types.js';

/**
 * Checks whether two values are recursively equal. Only compares serializable
 * data (primitives, serializable arrays and serializable objects).
 * @param {*} val1 Value to compare.
 * @param {*} val2 Value to compare with val1.
 * @return {boolean} True if the values are recursively equal.
 */
function deepEqual(val1, val2) {
  if (val1 === val2) {
    return true;
  }

  if (Array.isArray(val1) || Array.isArray(val2)) {
    if (!Array.isArray(val1) || !Array.isArray(val2)) {
      return false;
    }
    return arraysEqual(
        /** @type {!Array} */ (val1),
        /** @type {!Array} */ (val2));
  }

  if (val1 instanceof Object && val2 instanceof Object) {
    return objectsEqual(val1, val2);
  }

  return false;
}

/**
 * @param {!Array} arr1
 * @param {!Array} arr2
 * @return {boolean} True if the arrays are recursively equal.
 */
function arraysEqual(arr1, arr2) {
  if (arr1.length !== arr2.length) {
    return false;
  }

  for (let i = 0; i < arr1.length; i++) {
    if (!deepEqual(arr1[i], arr2[i])) {
      return false;
    }
  }

  return true;
}

/**
 * @param {!Object} obj1
 * @param {!Object} obj2
 * @return {boolean} True if the objects are recursively equal.
 */
function objectsEqual(obj1, obj2) {
  const keys1 = Object.keys(obj1);
  const keys2 = Object.keys(obj2);
  if (keys1.length !== keys2.length) {
    return false;
  }

  for (let i = 0; i < keys1.length; i++) {
    const key = keys1[i];
    if (!deepEqual(obj1[key], obj2[key])) {
      return false;
    }
  }

  return true;
}

/**
 * Returns a recursive copy of the value.
 * @param {*} val Value to copy. Should be a primitive or only contain
 *     serializable data (primitives, serializable arrays and
 *     serializable objects).
 * @return {*} A deep copy of the value.
 */
function deepCopy(val) {
  if (!(val instanceof Object)) {
    return val;
  }
  return Array.isArray(val) ? deepCopyArray(/** @type {!Array} */ (val)) :
                              deepCopyObject(val);
}

/**
 * @param {!Array} arr
 * @return {!Array} Deep copy of the array.
 */
function deepCopyArray(arr) {
  const copy = [];
  for (let i = 0; i < arr.length; i++) {
    copy.push(deepCopy(arr[i]));
  }
  return copy;
}

/**
 * @param {!Object} obj
 * @return {!Object} Deep copy of the object.
 */
function deepCopyObject(obj) {
  const copy = {};
  const keys = Object.keys(obj);
  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    copy[key] = deepCopy(obj[key]);
  }
  return copy;
}

Polymer({
  is: 'settings-prefs',

  _template: null,

  properties: {
    /**
     * Object containing all preferences, for use by Polymer controls.
     * @type {Object|undefined}
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Map of pref keys to values representing the state of the Chrome
     * pref store as of the last update from the API.
     * @type {Object<*>}
     * @private
     */
    lastPrefValues_: {
      type: Object,
      value() {
        return {};
      },
    },
  },

  observers: [
    'prefsChanged_(prefs.*)',
  ],

  /** @type {SettingsPrivate} */
  settingsApi_: /** @type {SettingsPrivate} */ (chrome.settingsPrivate),

  /** @override */
  created() {
    if (!CrSettingsPrefs.deferInitialization) {
      this.initialize();
    }
  },

  /** @override */
  detached() {
    CrSettingsPrefs.resetForTesting();
  },

  /**
   * @param {SettingsPrivate=} opt_settingsApi SettingsPrivate implementation
   *     to use (chrome.settingsPrivate by default).
   */
  initialize(opt_settingsApi) {
    // Only initialize once (or after resetForTesting() is called).
    if (this.initialized_) {
      return;
    }
    this.initialized_ = true;

    if (opt_settingsApi) {
      this.settingsApi_ = opt_settingsApi;
    }

    /** @private {function(!Array<!chrome.settingsPrivate.PrefObject>)} */
    this.boundPrefsChanged_ = this.onSettingsPrivatePrefsChanged_.bind(this);
    this.settingsApi_.onPrefsChanged.addListener(this.boundPrefsChanged_);
    this.settingsApi_.getAllPrefs(
        this.onSettingsPrivatePrefsFetched_.bind(this));
  },

  /**
   * @param {!{path: string}} e
   * @private
   */
  prefsChanged_(e) {
    // |prefs| can be directly set or unset in tests.
    if (!CrSettingsPrefs.isInitialized || e.path === 'prefs') {
      return;
    }

    const key = this.getPrefKeyFromPath_(e.path);
    const prefStoreValue = this.lastPrefValues_[key];

    const prefObj = /** @type {chrome.settingsPrivate.PrefObject} */ (
        this.get(key, this.prefs));

    // If settingsPrivate already has this value, ignore it. (Otherwise,
    // a change event from settingsPrivate could make us call
    // settingsPrivate.setPref and potentially trigger an IPC loop.)
    if (!deepEqual(prefStoreValue, prefObj.value)) {
      // <if expr="chromeos">
      this.fire(
          'user-action-setting-change',
          {prefKey: key, prefValue: prefObj.value});
      // </if>

      this.settingsApi_.setPref(
          key, prefObj.value,
          /* pageId */ '',
          /* callback */ this.setPrefCallback_.bind(this, key));
    }
  },

  /**
   * Called when prefs in the underlying Chrome pref store are changed.
   * @param {!Array<!chrome.settingsPrivate.PrefObject>} prefs
   *     The prefs that changed.
   * @private
   */
  onSettingsPrivatePrefsChanged_(prefs) {
    if (CrSettingsPrefs.isInitialized) {
      this.updatePrefs_(prefs);
    }
  },

  /**
   * Called when prefs are fetched from settingsPrivate.
   * @param {!Array<!chrome.settingsPrivate.PrefObject>} prefs
   * @private
   */
  onSettingsPrivatePrefsFetched_(prefs) {
    this.updatePrefs_(prefs);
    CrSettingsPrefs.setInitialized();
  },

  /**
   * Checks the result of calling settingsPrivate.setPref.
   * @param {string} key The key used in the call to setPref.
   * @param {boolean} success True if setting the pref succeeded.
   * @private
   */
  setPrefCallback_(key, success) {
    if (!success) {
      this.refresh(key);
    }
  },

  /**
   * Get the current pref value from chrome.settingsPrivate to ensure the UI
   * stays up to date.
   * @param {string} key
   */
  refresh(key) {
    this.settingsApi_.getPref(key, pref => {
      this.updatePrefs_([pref]);
    });
  },

  /**
   * Builds an object structure for the provided |path| within |prefsObject|,
   * ensuring that names that already exist are not overwritten. For example:
   * "a.b.c" -> a = {};a.b={};a.b.c={};
   * @param {string} path Path to the new pref value.
   * @param {*} value The value to expose at the end of the path.
   * @param {Object} prefsObject The prefs object to add the path to.
   * @private
   */
  updatePrefPath_(path, value, prefsObject) {
    const parts = path.split('.');
    let cur = prefsObject;

    for (let part; parts.length && (part = parts.shift());) {
      if (!parts.length) {
        // last part, set the value.
        cur[part] = value;
      } else if (part in cur) {
        cur = cur[part];
      } else {
        cur = cur[part] = {};
      }
    }
  },

  /**
   * Updates the prefs model with the given prefs.
   * @param {!Array<!chrome.settingsPrivate.PrefObject>} newPrefs
   * @private
   */
  updatePrefs_(newPrefs) {
    // Use the existing prefs object or create it.
    const prefs = this.prefs || {};
    newPrefs.forEach(function(newPrefObj) {
      // Use the PrefObject from settingsPrivate to create a copy in
      // lastPrefValues_ at the pref's key.
      this.lastPrefValues_[newPrefObj.key] = deepCopy(newPrefObj.value);

      if (!deepEqual(this.get(newPrefObj.key, prefs), newPrefObj)) {
        // Add the pref to |prefs|.
        this.updatePrefPath_(newPrefObj.key, newPrefObj, prefs);
        // If this.prefs already exists, notify listeners of the change.
        if (prefs === this.prefs) {
          this.notifyPath('prefs.' + newPrefObj.key, newPrefObj);
        }
      }
    }, this);
    if (!this.prefs) {
      this.prefs = prefs;
    }
  },

  /**
   * Given a 'property-changed' path, returns the key of the preference the
   * path refers to. E.g., if the path of the changed property is
   * 'prefs.search.suggest_enabled.value', the key of the pref that changed is
   * 'search.suggest_enabled'.
   * @param {string} path
   * @return {string}
   * @private
   */
  getPrefKeyFromPath_(path) {
    // Skip the first token, which refers to the member variable (this.prefs).
    const parts = path.split('.');
    assert(parts.shift() === 'prefs', 'Path doesn\'t begin with \'prefs\'');

    for (let i = 1; i <= parts.length; i++) {
      const key = parts.slice(0, i).join('.');
      // The lastPrefValues_ keys match the pref keys.
      if (this.lastPrefValues_.hasOwnProperty(key)) {
        return key;
      }
    }
    return '';
  },

  /**
   * Resets the element so it can be re-initialized with a new prefs state.
   */
  resetForTesting() {
    if (!this.initialized_) {
      return;
    }
    this.prefs = undefined;
    this.lastPrefValues_ = {};
    this.initialized_ = false;
    // Remove the listener added in initialize().
    this.settingsApi_.onPrefsChanged.removeListener(this.boundPrefsChanged_);
    this.settingsApi_ =
        /** @type {SettingsPrivate} */ (chrome.settingsPrivate);
  },
});
