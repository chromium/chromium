// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the Diagnostics App UI in chromeos/ to
 * provide access to the SessionLogHandler which invokes functions that only
 * exist in chrome/.
 */

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

import {NavigationView} from './diagnostics_types.js';
import {getNavigationViewForPageId} from './diagnostics_utils.js';

/** @interface */
export class DiagnosticsBrowserProxy {
  /** Initialize SessionLogHandler. */
  initialize() {}

  /**
   * Reports navigation events to message handler.
   * @param {string} currentView Label matching ID for view lookup
   */
  recordNavigation(currentView) {}

  /**
   * Saves the session log to the selected location.
   * @return {!Promise<boolean>}
   */
  saveSessionLog() {}

  /**
   * Returns a localized, pluralized string for |name| based on |count|.
   * @param {string} name
   * @param {number} count
   * @return {!Promise<string>}
   */
  getPluralString(name, count) {}
}

/** @implements {DiagnosticsBrowserProxy} */
export class DiagnosticsBrowserProxyImpl {
  constructor() {
    /**
     * View which 'recordNavigation' is leaving.
     * @private
     * @type {?NavigationView}
     */
    this.previousView_ = null;
  }

  /** @override */
  initialize() {
    chrome.send('initialize');
  }

  /** @override */
  recordNavigation(currentView) {
    // First time the function is called will be when the UI is initializing
    // which does not trigger a message as navigation has not occurred.
    if (this.previousView_ === null) {
      this.previousView_ = getNavigationViewForPageId(currentView);
      return;
    }

    /* @type {NavigationView} */
    const currentViewId = getNavigationViewForPageId(currentView);
    chrome.send('recordNavigation', [this.previousView_, currentViewId]);
    this.previousView_ = currentViewId;
  }

  /** @override */
  saveSessionLog() {
    return sendWithPromise('saveSessionLog');
  }

  /** @override */
  getPluralString(name, count) {
    return sendWithPromise('getPluralString', name, count);
  }
}

// The singleton instance_ can be replaced with a test version of this wrapper
// during testing.
addSingletonGetter(DiagnosticsBrowserProxyImpl);
