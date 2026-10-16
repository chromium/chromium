// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper for navigating to the Password Manager UI and recording
 * related metrics from settings.
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
   * Shows new Password Manager UI (chrome://password-manager).
   */
  showPasswordManager(page: PasswordManagerPage): void;
}

/**
 * Enumerates referrers that can trigger a navigation to the manage passwords
 * page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
// LINT.IfChange(ManagePasswordsReferrer)
export enum ManagePasswordsReferrer {
  CHROME_SETTINGS = 0,
  MANAGE_PASSWORDS_BUBBLE = 1,
  PASSWORD_CONTEXT_MENU = 2,
  PASSWORD_DROPDOWN = 3,
  PASSWORD_GENERATION_CONFIRMATION = 4,
  // PROFILE_CHOOSER = 5,
  PASSWORDS_ACCESSORY_SHEET = 6,
  TOUCH_TO_FILL = 7,
  SAFE_STATE_BUBBLE = 8,
  PASSWORD_BREACH_DIALOG = 9,
  SAFETY_CHECK = 10,
  SAVE_UPDATE_BUBBLE = 11,
  PASSWORD_GENERATION_PROMPT = 12,
  PASSWORDS_GOOGLE_WEBSITE = 13,
  // AUTOMATED_PASSWORD_CHANGE_SUCCESS_LINK = 14,
  BIOMETRIC_AUTHENTICATION_BEFORE_FILLING_DIALOG = 15,
  CHROME_MENU_ITEM = 16,
  SHARED_PASSWORDS_NOTIFICATION_BUBBLE = 17,
  SEARCH_PASSWORDS_WIDGET = 18,
  ADD_USERNAME_BUBBLE = 19,
  OMNIBOX_PEDAL_SUGGESTION = 20,
  // DEFAULT_STORE_CHANGED_BUBBLE = 21,
  MANAGE_PASSWORD_DETAILS_BUBBLE = 22,
  PASSKEY_SAVED_CONFIRMATION_BUBBLE = 23,
  PASSKEY_DELETED_CONFIRMATION_BUBBLE = 24,
  PASSKEY_UPDATED_CONFIRMATION_BUBBLE = 25,
  PASSKEY_NOT_ACCEPTED_BUBBLE = 26,
  // ACCESS_LOSS_WARNING = 27,
  PASSKEY_UPGRADE_BUBBLE = 28,
  PASSWORD_CHANGE_INFO_BUBBLE = 29,
  CHROME_SETTINGS_AUTOFILL_AND_PASSWORDS = 30,
  // Must be last.
  COUNT = 31,
}
// LINT.ThenChange(
//   //components/password_manager/core/browser/manage_passwords_referrer.h:ManagePasswordsReferrer,
//   //tools/metrics/histograms/metadata/password/enums.xml:ManagePasswordsReferrer
// )

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
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.ManagePasswordsReferrer',
        ManagePasswordsReferrer.CHROME_SETTINGS,
        ManagePasswordsReferrer.COUNT);
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
