// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Global state for prefs initialization status.
 */

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

class CrSettingsPrefsInternal {
  isInitialized: boolean = false;
  deferInitialization: boolean;
  private initializedResolver_: PromiseResolver<void> = new PromiseResolver();

  constructor() {
    /**
     * Whether to defer initialization. Used in testing to prevent premature
     * initialization when intending to fake the settings API.
     */
    this.deferInitialization = false;
  }

  get initialized(): Promise<void> {
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

export const CrSettingsPrefs: CrSettingsPrefsInternal =
    new CrSettingsPrefsInternal();
