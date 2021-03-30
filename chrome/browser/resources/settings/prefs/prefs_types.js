// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Types for CrSettingsPrefsElement.
 */

/**
 * Global state for prefs status.
 */
export const CrSettingsPrefs = (function() {
  const CrSettingsPrefsInternal = {
    /**
     * Resolves the CrSettingsPrefs.initialized promise.
     */
    setInitialized() {
      /** @public {boolean} */
      CrSettingsPrefsInternal.isInitialized = true;
      CrSettingsPrefsInternal.resolve_();
    },

    /**
     * Restores state for testing.
     */
    resetForTesting() {
      CrSettingsPrefsInternal.setup_();
    },

    /**
     * Whether to defer initialization. Used in testing to prevent premature
     * initialization when intending to fake the settings API.
     * @type {boolean}
     */
    deferInitialization: false,

    /**
     * Called to set up the promise and resolve methods.
     * @private
     */
    setup_() {
      CrSettingsPrefsInternal.isInitialized = false;
      /**
       * Promise to be resolved when all settings have been initialized.
       * @type {!Promise}
       */
      CrSettingsPrefsInternal.initialized = new Promise(function(resolve) {
        CrSettingsPrefsInternal.resolve_ = resolve;
      });
    },
  };

  CrSettingsPrefsInternal.setup_();

  return CrSettingsPrefsInternal;
})();
