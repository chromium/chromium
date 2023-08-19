// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordManagerProxy is an abstraction over
 * chrome.passwordsPrivate which facilitates testing.
 */

/**
 * Interface for all callbacks to the password API.
 */
export interface PasswordManagerProxy {
  /**
   * Log that the Passwords page was accessed from the Chrome Settings WebUI.
   */
  recordPasswordsPageAccessInSettings(): void;

  /**
   * Records the referrer of a given navigation to the Password Check page.
   */
  recordPasswordCheckReferrer(referrer: PasswordCheckReferrer): void;

  /**
   * Shows new Password Manager UI (chrome://password-manager).
   */
  showPasswordManager(page: PasswordManagerPage): void;
}

/**
 * Represents different referrers when navigating to the Password Check page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Needs to stay in sync with PasswordCheckReferrer in enums.xml and
 * password_check_referrer.h.
 */
export enum PasswordCheckReferrer {
  SAFETY_CHECK = 0,            // Web UI, recorded in JavaScript.
  PASSWORD_SETTINGS = 1,       // Web UI, recorded in JavaScript.
  PHISH_GUARD_DIALOG = 2,      // Native UI, recorded in C++.
  PASSWORD_BREACH_DIALOG = 3,  // Native UI, recorded in C++.
  // Must be last.
  COUNT = 4,
}

// WARNING: Keep synced with
// chrome/browser/ui/webui/settings/password_manager_handler.cc.
export enum PasswordManagerPage {
  PASSWORDS = 0,
  CHECKUP = 1,
}

/**
 * Implementation that accesses the private API.
 */
export class PasswordManagerImpl implements PasswordManagerProxy {
  recordPasswordsPageAccessInSettings() {
    chrome.passwordsPrivate.recordPasswordsPageAccessInSettings();
  }

  /** override */
  recordPasswordCheckReferrer(referrer: PasswordCheckReferrer) {
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.BulkCheck.PasswordCheckReferrer', referrer,
        PasswordCheckReferrer.COUNT);
  }

  showPasswordManager(page: PasswordManagerPage) {
    chrome.send('showPasswordManager', [page]);
  }

  static getInstance(): PasswordManagerProxy {
    return instance || (instance = new PasswordManagerImpl());
  }

  static setInstance(obj: PasswordManagerProxy) {
    instance = obj;
  }
}

let instance: PasswordManagerProxy|null = null;
