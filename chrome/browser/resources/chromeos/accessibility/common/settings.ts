// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class to handle accessing/storing/caching prefs data.
 */
import {TestImportManager} from './testing/test_import_manager.js';

type PrefObject = chrome.settingsPrivate.PrefObject;

export class Settings {
  private listeners_: Record<string, Array<(prefValue: any) => void>> = {};
  private prefs_: Record<string, PrefObject>|null = null;
  static instance?: Settings;

  /**
   * @param keys The settings keys the extension cares about.
   */
  constructor(keys: string[]) {
    keys.forEach(key => this.listeners_[key] = []);
  }

  /**
   * @param keys The settings keys the extension cares about.
   */
  static async init(keys: string[]): Promise<void> {
    if (Settings.instance) {
      throw new Error(
          'Settings.init() should be called at most once in each ' +
          'browser context.');
    }

    Settings.instance = new Settings(keys);
    await Settings.instance.initialFetch_();

    // Add prefs changed listener after initialFetch_() so we don't get updates
    // before we've fetched initially.
    // TODO(b/314203187): Not null asserted, check these to make sure this is
    // correct.
    chrome.settingsPrivate.onPrefsChanged.addListener(
        (updates: PrefObject[]) => Settings.instance!.update_(updates));
  }

  /**
   * Adds a callback to listen to changes to one or more preferences.
   * The callback will be called immediately if there is a value set.
   * @param keys The settings keys being listened to.
   * @param listener The callback when the value changes.
   */
  static addListener(keys: string|string[], listener: (prefValue: any) => void):
      void {
    if (typeof keys === 'string') {
      keys = [keys];
    }

    for (const key of keys) {
      // TODO(b/314203187): Not null asserted, check these to make sure this is
      // correct.
      Settings.instance!.addListener_(key, listener);
    }
  }

  static get(key: string): any {
    // TODO(b/314203187): Not nulls asserted, check these to make sure this is
    // correct.
    Settings.instance!.validate_(key);
    return Settings.instance!.prefs_![key].value;
  }

  static set(key: string, value: any): void {
    // TODO(b/314203187): Not nulls asserted, check these to make sure this is
    // correct.
    Settings.instance!.validate_(key);
    const oldValue = Settings.instance!.prefs_![key].value;
    chrome.settingsPrivate.setPref(key, value);
    Settings.instance!.prefs_![key].value = value;
    if (oldValue !== value) {
      Settings.instance!.listeners_[key].forEach(listener => listener(value));
    }
  }

  // ============ Private methods ============

  /**
   * @param key The settings key being listened to.
   * @param listener The callback when the value changes.
   * @private
   */
  private addListener_(key: string, listener: (prefValue: any) => void): void {
    this.validate_(key);
    this.listeners_[key].push(listener);

    // TODO(b/314203187): Not nulls asserted, check these to make sure this is
    // correct.
    if (this.prefs_![key] !== null) {
      listener(this.prefs_![key].value);
    }
  }

  private async initialFetch_(): Promise<void> {
    const prefs: PrefObject[] = await new Promise(
        resolve => chrome.settingsPrivate.getAllPrefs(resolve));

    const trackedPrefs = prefs!.filter(pref => this.isTracked_(pref.key));
    this.prefs_ =
        Object.fromEntries(trackedPrefs.map(pref => [pref.key, pref]));
  }

  private isTracked_(key: string): boolean {
    // Because we assign to this.prefs_ in initialFetch_(), use listeners_ as
    // the official source of truth on what keys are in scope.
    return key in this.listeners_;
  }

  private update_(updates: PrefObject[]): void {
    for (const pref of updates) {
      if (!this.isTracked_(pref.key)) {
        continue;
      }

      // TODO(b/314203187): Not null asserted, check these to make sure this is
      // correct.
      const oldValue = this.prefs_![pref.key].value;

      if (oldValue === pref.value) {
        continue;
      }

      // TODO(b/314203187): Not null asserted, check these to make sure this is
      // correct.
      this.prefs_![pref.key] = pref;
      this.listeners_[pref.key].forEach(listener => listener(pref.value));
    }
  }

  private validate_(key: string): void {
    if (this.prefs_ === null) {
      throw new Error('Cannot access Settings until init() has resolved.');
    }
    if (!this.isTracked_(key)) {
      throw new Error('Prefs key "' + key + '" is not being tracked.');
    }
    if (!this.prefs_[key]) {
      throw new Error('Settings missing pref with key: ' + key);
    }
  }
}

/** @private {Settings} */
Settings.instance;

TestImportManager.exportForTesting(Settings);
