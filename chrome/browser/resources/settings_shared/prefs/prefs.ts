/* Copyright 2015 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/**
 * @fileoverview
 * 'settings-prefs' exposes a singleton model of Chrome settings and
 * preferences, which listens to changes to Chrome prefs allowed in
 * chrome.settingsPrivate. When changing prefs in this element's 'prefs'
 * property via the UI, the singleton model tries to set those preferences in
 * Chrome. Whether or not the calls to settingsPrivate.setPref succeed, 'prefs'
 * is eventually consistent with the Chrome pref store.
 */

import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrSettingsPrefs} from './prefs_types.js';

/**
 * Checks whether two values are recursively equal. Only compares serializable
 * data (primitives, serializable arrays and serializable objects).
 * @param val1 Value to compare.
 * @param val2 Value to compare with val1.
 * @return Whether the values are recursively equal.
 */
function deepEqual(val1: any, val2: any): boolean {
  if (val1 === val2) {
    return true;
  }

  if (Array.isArray(val1) || Array.isArray(val2)) {
    if (!Array.isArray(val1) || !Array.isArray(val2)) {
      return false;
    }
    return arraysEqual(val1, val2);
  }

  if (val1 instanceof Object && val2 instanceof Object) {
    return objectsEqual(val1, val2);
  }

  return false;
}

/**
 * @return Whether the arrays are recursively equal.
 */
function arraysEqual(arr1: any[], arr2: any[]): boolean {
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
 * @return Whether the objects are recursively equal.
 */
function objectsEqual(
    obj1: {[key: string]: any}, obj2: {[key: string]: any}): boolean {
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

export class SettingsPrefsElement extends PolymerElement {
  static get is() {
    return 'settings-prefs';
  }

  static get properties() {
    return {
      /**
       * Object containing all preferences, for use by Polymer controls.
       */
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }

  static get observers() {
    return [
      'prefsChanged_(prefs.*)',
    ];
  }

  prefs: {[key: string]: any}|undefined;

  /**
   * Map of pref keys to values representing the state of the Chrome
   * pref store as of the last update from the API.
   */
  private lastPrefValues_: Map<string, any> = new Map();

  private settingsApi_: typeof chrome.settingsPrivate = chrome.settingsPrivate;
  private initialized_: boolean = false;
  private boundPrefsChanged_:
      (prefs: chrome.settingsPrivate.PrefObject[]) => void;

  constructor() {
    super();

    if (!CrSettingsPrefs.deferInitialization) {
      this.initialize();
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    CrSettingsPrefs.resetForTesting();
  }

  /**
   * @param settingsApi SettingsPrivate implementation to use
   *     (chrome.settingsPrivate by default).
   */
  initialize(settingsApi?: typeof chrome.settingsPrivate) {
    // Only initialize once (or after resetForTesting() is called).
    if (this.initialized_) {
      return;
    }
    this.initialized_ = true;

    if (settingsApi) {
      this.settingsApi_ = settingsApi;
    }

    this.boundPrefsChanged_ = this.onSettingsPrivatePrefsChanged_.bind(this);
    this.settingsApi_.onPrefsChanged.addListener(this.boundPrefsChanged_);
    this.settingsApi_.getAllPrefs().then((prefs) => {
      this.updatePrefs_(prefs);
      CrSettingsPrefs.setInitialized();
    });
  }

  private prefsChanged_(e: {path: string}) {
    // |prefs| can be directly set or unset in tests.
    if (!CrSettingsPrefs.isInitialized || e.path === 'prefs') {
      return;
    }

    const key = this.getPrefKeyFromPath_(e.path);
    const prefStoreValue = this.lastPrefValues_.get(key);

    const prefObj = this.get(key, this.prefs);

    // If settingsPrivate already has this value, ignore it. (Otherwise,
    // a change event from settingsPrivate could make us call
    // settingsPrivate.setPref and potentially trigger an IPC loop.)
    if (!deepEqual(prefStoreValue, prefObj.value)) {
      // <if expr="chromeos_ash">
      this.dispatchEvent(new CustomEvent('user-action-setting-change', {
        bubbles: true,
        composed: true,
        detail: {prefKey: key, prefValue: prefObj.value},
      }));
      // </if>

      this.settingsApi_
          .setPref(
              key, prefObj.value,
              /* pageId */ '')
          .then(success => {
            if (!success) {
              this.refresh(key);
            }
          });
    }
  }

  /**
   * Called when prefs in the underlying Chrome pref store are changed.
   */
  private onSettingsPrivatePrefsChanged_(
      prefs: chrome.settingsPrivate.PrefObject[]) {
    if (CrSettingsPrefs.isInitialized) {
      this.updatePrefs_(prefs);
    }
  }

  /**
   * Get the current pref value from chrome.settingsPrivate to ensure the UI
   * stays up to date.
   */
  refresh(key: string) {
    this.settingsApi_.getPref(key).then(pref => {
      this.updatePrefs_([pref]);
    });
  }

  /**
   * Builds an object structure for the provided |path| within |prefsObject|,
   * ensuring that names that already exist are not overwritten. For example:
   * "a.b.c" -> a = {};a.b={};a.b.c={};
   * @param path Path to the new pref value.
   * @param value The value to expose at the end of the path.
   * @param prefsObject The prefs object to add the path to.
   */
  private updatePrefPath_(
      path: string, value: any, prefsObject: {[key: string]: any}) {
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
  }

  /**
   * Updates the prefs model with the given prefs.
   */
  private updatePrefs_(newPrefs: chrome.settingsPrivate.PrefObject[]) {
    // Use the existing prefs object or create it.
    const prefs = this.prefs || {};
    newPrefs.forEach((newPrefObj) => {
      // Use the PrefObject from settingsPrivate to create a copy in
      // lastPrefValues_ at the pref's key.
      this.lastPrefValues_.set(
          newPrefObj.key, structuredClone(newPrefObj.value));

      if (!deepEqual(this.get(newPrefObj.key, prefs), newPrefObj)) {
        // Add the pref to |prefs|.
        this.updatePrefPath_(newPrefObj.key, newPrefObj, prefs);
        // If this.prefs already exists, notify listeners of the change.
        if (prefs === this.prefs) {
          this.notifyPath('prefs.' + newPrefObj.key, newPrefObj);
        }
      }
    });
    if (!this.prefs) {
      this.prefs = prefs;
    }
  }

  /**
   * Given a 'property-changed' path, returns the key of the preference the
   * path refers to. E.g., if the path of the changed property is
   * 'prefs.search.suggest_enabled.value', the key of the pref that changed is
   * 'search.suggest_enabled'.
   */
  private getPrefKeyFromPath_(path: string): string {
    // Skip the first token, which refers to the member variable (this.prefs).
    const parts = path.split('.');
    assert(parts.shift() === 'prefs', 'Path doesn\'t begin with \'prefs\'');

    for (let i = 1; i <= parts.length; i++) {
      const key = parts.slice(0, i).join('.');
      // The lastPrefValues_ keys match the pref keys.
      if (this.lastPrefValues_.has(key)) {
        return key;
      }
    }
    return '';
  }

  /**
   * Resets the element so it can be re-initialized with a new prefs state.
   */
  resetForTesting() {
    if (!this.initialized_) {
      return;
    }
    this.prefs = undefined;
    this.lastPrefValues_.clear();
    this.initialized_ = false;
    // Remove the listener added in initialize().
    this.settingsApi_.onPrefsChanged.removeListener(this.boundPrefsChanged_);
    this.settingsApi_ = chrome.settingsPrivate;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-prefs': SettingsPrefsElement;
  }
}

customElements.define(SettingsPrefsElement.is, SettingsPrefsElement);
