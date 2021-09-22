// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Global state for prefs initialization status.
 */

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

class CrSettingsPrefsInternal {
  constructor() {
    /** @type {boolean} */
    this.isInitialized = false;

    /**
     * Whether to defer initialization. Used in testing to prevent premature
     * initialization when intending to fake the settings API.
     * @type {boolean}
     */
    this.deferInitialization = false;

    /** @private {!PromiseResolver} */
    this.initializedResolver_ = new PromiseResolver();
  }

  /** @return {!Promise} */
  get initialized() {
    return this.initializedResolver_.promise;
  }

  /** Resolves the |initialized| promise. */
  setInitialized() {
    this.isInitialized = true;
    this.initializedResolver_.resolve();
  }

  /** Restores state for testing. */
  resetForTesting() {
    this.isInitialized = false;
    this.initializedResolver_ = new PromiseResolver();
  }
}

/** @type {!CrSettingsPrefsInternal} */
export const CrSettingsPrefs = new CrSettingsPrefsInternal();
