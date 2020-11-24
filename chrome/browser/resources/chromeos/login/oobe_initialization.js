// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Initializes OOBE when the DOM is loaded.
 */

(function() {
'use strict';

function initializeOobe() {
  if (document.readyState === 'loading')
    return;
  document.removeEventListener('DOMContentLoaded', initializeOobe);

  // TODO(crbug.com/1082670): Remove excessive logging after investigation.
  console.warn('1082670 : initializing OOBE');

  try {
    cr.ui.Oobe.initialize();
  } finally {
    // TODO(crbug.com/712078): Do not set readyForTesting in case of that
    // initialize() is failed. Currently, in some situation, initialize()
    // raises an exception unexpectedly. It means testing APIs should not
    // be called then. However, checking it here now causes bots failures
    // unfortunately. So, as a short term workaround, here set
    // readyForTesting even on failures, just to make test bots happy.
    Oobe.readyForTesting = true;
  }

  // Mark initialization complete and wake any callers that might be waiting
  // for OOBE to load.
  cr.ui.Oobe.initializationComplete = true;
  cr.ui.Oobe.initCallbacks.forEach(resolvePromise => resolvePromise());
}

// Install a global error handler so stack traces are included in logs.
window.onerror = function(message, file, line, column, error) {
  if (error && error.stack)
    console.error(error.stack);
};

// TODO(crbug.com/1082670): Remove excessive logging after investigation.
console.warn('1082670 : cr_ui loaded');

/**
 * Final initialization performed after HTML imports are loaded. Loads
 * common elements used in OOBE (Custom Elements).
 */
HTMLImports.whenReady(() => {
  // TODO(crbug.com/1111387) - Remove excessive logging.
  console.warn('HTMLImports ready.');
  loadCommonComponents();

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initializeOobe);
  } else {
    initializeOobe();
  }
});
})();
