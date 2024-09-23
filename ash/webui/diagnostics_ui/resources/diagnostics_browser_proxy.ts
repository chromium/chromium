// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the Diagnostics App UI in chromeos/ to
 * provide access to the SessionLogHandler which invokes functions that only
 * exist in chrome/.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.js';

import {NavigationView} from './diagnostics_types.js';
import {getNavigationViewForPageId} from './diagnostics_utils.js';

export interface DiagnosticsBrowserProxy {
  /** Initialize SessionLogHandler. */
  initialize(): void;

  /**
   * Reports navigation events to message handler.
   * @param currentView Label matching ID for view lookup
   */
  recordNavigation(currentView: string): void;

  /**
   * Saves the session log to the selected location.
   */
  saveSessionLog(): Promise<boolean>;

  /**
   * Returns a localized, pluralized string for |name| based on |count|.
   */
  getPluralString(name: string, count: number): Promise<string>;
}


export class DiagnosticsBrowserProxyImpl implements DiagnosticsBrowserProxy {
  // View which 'recordNavigation' is leaving.
  previousView: NavigationView|null = null;

  initialize(): void {
    chrome.send('initialize');
  }

  recordNavigation(currentView: string): void {
    // First time the function is called will be when the UI is initializing
    // which does not trigger a message as navigation has not occurred.
    if (this.previousView === null) {
      this.previousView = getNavigationViewForPageId(currentView);
      return;
    }

    const currentViewId = getNavigationViewForPageId(currentView);
    chrome.send('recordNavigation', [this.previousView, currentViewId]);
    this.previousView = currentViewId;
  }

  saveSessionLog(): Promise<boolean> {
    return sendWithPromise('saveSessionLog');
  }

  getPluralString(name: string, count: number): Promise<string> {
    return sendWithPromise('getPluralString', name, count);
  }

  // The singleton instance_ can be replaced with a test version of this wrapper
  // during testing.
  static getInstance(): DiagnosticsBrowserProxyImpl {
    return browserProxy || (browserProxy = new DiagnosticsBrowserProxyImpl());
  }

  static setInstance(obj: DiagnosticsBrowserProxyImpl): void {
    browserProxy = obj;
  }
}

let browserProxy: DiagnosticsBrowserProxyImpl|null = null;
