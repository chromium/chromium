// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

/**
 * @fileoverview A helper object used by the "SafetyCheck" to interact with
 * the browser.
 */
/**
 * Constants used in safety check C++ to JS communication.
 * Their values need be kept in sync with their counterparts in
 * chrome/browser/ui/webui/settings/safety_check_handler.h and
 * chrome/browser/ui/webui/settings/safety_check_handler.cc
 */
export enum SafetyCheckCallbackConstants {
  PARENT_CHANGED = 'safety-check-parent-status-changed',
  UPDATES_CHANGED = 'safety-check-updates-status-changed',
  PASSWORDS_CHANGED = 'safety-check-passwords-status-changed',
  SAFE_BROWSING_CHANGED = 'safety-check-safe-browsing-status-changed',
  EXTENSIONS_CHANGED = 'safety-check-extensions-status-changed',
}

/**
 * States of the safety check parent element.
 * Needs to be kept in sync with ParentStatus in
 * chrome/browser/ui/webui/settings/safety_check_handler.h
 */
export enum SafetyCheckParentStatus {
  BEFORE = 0,
  CHECKING = 1,
  AFTER = 2,
}

/**
 * States of the safety check updates element.
 * Needs to be kept in sync with UpdateStatus in
 * components/safety_check/safety_check.h
 */
export enum SafetyCheckUpdatesStatus {
  CHECKING = 0,
  UPDATED = 1,
  UPDATING = 2,
  RELAUNCH = 3,
  DISABLED_BY_ADMIN = 4,
  FAILED_OFFLINE = 5,
  FAILED = 6,
  UNKNOWN = 7,
  // Only used in Android but listed here to keep enum in sync.
  OUTDATED = 8,
  UPDATE_TO_ROLLBACK_VERSION_DISALLOWED = 9,
}

/**
 * States of the safety check passwords element.
 * Needs to be kept in sync with PasswordsStatus in
 * chrome/browser/ui/webui/settings/safety_check_handler.h
 */
export enum SafetyCheckPasswordsStatus {
  CHECKING = 0,
  SAFE = 1,
  // Indicates that at least one compromised password exists. Weak, reused or
  // muted compromised password warnings may exist as well.
  COMPROMISED = 2,
  OFFLINE = 3,
  NO_PASSWORDS = 4,
  SIGNED_OUT = 5,
  QUOTA_LIMIT = 6,
  ERROR = 7,
  FEATURE_UNAVAILABLE = 8,
  // Indicates that no compromised or reused passwords exist, but there is at
  // least one weak password.
  WEAK_PASSWORDS_EXIST = 9,
  // Indicates that no compromised passwords exist, but there is at least one
  // reused password.
  // Not yet supported on Desktop.
  REUSED_PASSWORDS_EXIST = 10,
  // Indicates no weak or reused passwords exist, but there is
  // at least one compromised password warning that has been muted by the user.
  // Not yet supported on Desktop.
  MUTED_COMPROMISED_EXIST = 11,
}

/**
 * States of the safety check safe browsing element.
 * Needs to be kept in sync with SafeBrowsingStatus in
 * chrome/browser/ui/webui/settings/safety_check_handler.h
 */
export enum SafetyCheckSafeBrowsingStatus {
  CHECKING = 0,
  // Enabled is deprecated; kept not to break old UMA metrics (enums.xml).
  ENABLED = 1,
  DISABLED = 2,
  DISABLED_BY_ADMIN = 3,
  DISABLED_BY_EXTENSION = 4,
  ENABLED_STANDARD = 5,
  ENABLED_ENHANCED = 6,
  ENABLED_STANDARD_AVAILABLE_ENHANCED = 7,
}

/**
 * States of the safety check extensions element.
 * Needs to be kept in sync with ExtensionsStatus in
 * chrome/browser/ui/webui/settings/safety_check_handler.h
 */
export enum SafetyCheckExtensionsStatus {
  CHECKING = 0,
  ERROR = 1,
  NO_BLOCKLISTED_EXTENSIONS = 2,
  BLOCKLISTED_ALL_DISABLED = 3,
  BLOCKLISTED_REENABLED_ALL_BY_USER = 4,
  BLOCKLISTED_REENABLED_SOME_BY_USER = 5,
  BLOCKLISTED_REENABLED_ALL_BY_ADMIN = 6,
}

export interface SafetyCheckBrowserProxy {
  /** Run the safety check. */
  runSafetyCheck(): void;

  /**
   * Get the display string for the safety check parent, showing how long ago
   * safety check last ran. Also triggers string updates to be sent to all SC
   * children that have timestamp-based display strings.
   */
  getParentRanDisplayString(): Promise<string>;
}

export class SafetyCheckBrowserProxyImpl implements SafetyCheckBrowserProxy {
  runSafetyCheck() {
    chrome.send('performSafetyCheck');
  }

  getParentRanDisplayString() {
    return sendWithPromise('getSafetyCheckRanDisplayString');
  }

  static getInstance(): SafetyCheckBrowserProxy {
    return instance || (instance = new SafetyCheckBrowserProxyImpl());
  }

  static setInstance(obj: SafetyCheckBrowserProxy) {
    instance = obj;
  }
}

let instance: SafetyCheckBrowserProxy|null = null;
