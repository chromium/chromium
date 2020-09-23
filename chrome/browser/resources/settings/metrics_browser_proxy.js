// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles metrics for the settings pages. */

// clang-format off
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * Contains all possible recorded interactions across privacy settings pages.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SettingsPrivacyElementInteractions enum in
 * histograms/enums.xml
 * @enum {number}
 */
export const PrivacyElementInteractions = {
  SYNC_AND_GOOGLE_SERVICES: 0,
  CHROME_SIGN_IN: 1,
  DO_NOT_TRACK: 2,
  PAYMENT_METHOD: 3,
  NETWORK_PREDICTION: 4,
  MANAGE_CERTIFICATES: 5,
  SAFE_BROWSING: 6,
  PASSWORD_CHECK: 7,
  IMPROVE_SECURITY: 8,
  COOKIES_ALL: 9,
  COOKIES_INCOGNITO: 10,
  COOKIES_THIRD: 11,
  COOKIES_BLOCK: 12,
  COOKIES_SESSION: 13,
  // Leave this at the end.
  COUNT: 14,
};

/**
 * Contains all safety check interactions.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SafetyCheckInteractions enum in
 * histograms/enums.xml
 * @enum {number}
 */
export const SafetyCheckInteractions = {
  SAFETY_CHECK_START: 0,
  SAFETY_CHECK_UPDATES_RELAUNCH: 1,
  SAFETY_CHECK_PASSWORDS_MANAGE: 2,
  SAFETY_CHECK_SAFE_BROWSING_MANAGE: 3,
  SAFETY_CHECK_EXTENSIONS_REVIEW: 4,
  SAFETY_CHECK_CHROME_CLEANER_REBOOT: 5,
  SAFETY_CHECK_CHROME_CLEANER_REVIEW_INFECTED_STATE: 6,
  SAFETY_CHECK_PASSWORDS_MANAGE_THROUGH_CARET_NAVIGATION: 7,
  SAFETY_CHECK_SAFE_BROWSING_MANAGE_THROUGH_CARET_NAVIGATION: 8,
  SAFETY_CHECK_EXTENSIONS_REVIEW_THROUGH_CARET_NAVIGATION: 9,
  // Leave this at the end.
  COUNT: 10,
};

/**
 * Contains all safe browsing interactions.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the UserAction in safe_browsing_settings_metrics.h.
 * @enum {number}
 */
export const SafeBrowsingInteractions = {
  SAFE_BROWSING_SHOWED: 0,
  SAFE_BROWSING_ENHANCED_PROTECTION_CLICKED: 1,
  SAFE_BROWSING_STANDARD_PROTECTION_CLICKED: 2,
  SAFE_BROWSING_DISABLE_SAFE_BROWSING_CLICKED: 3,
  SAFE_BROWSING_ENHANCED_PROTECTION_EXPAND_ARROW_CLICKED: 4,
  SAFE_BROWSING_STANDARD_PROTECTION_EXPAND_ARROW_CLICKED: 5,
  SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_CONFIRMED: 6,
  SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_DENIED: 7,
  // Leave this at the end.
  COUNT: 8,
};

/** @interface */
export class MetricsBrowserProxy {
  /**
   * Helper function that calls recordAction with one action from
   * tools/metrics/actions/actions.xml.
   * @param {!string} action One action to be recorded.
   */
  recordAction(action) {}

  /**
   * Helper function that calls recordHistogram for the
   * Settings.SafetyCheck.Interactions histogram
   * @param {!SafetyCheckInteractions} interaction
   */
  recordSafetyCheckInteractionHistogram(interaction) {}

  /**
   * Helper function that calls recordHistogram for the
   * SettingsPage.PrivacyElementInteractions histogram
   * @param {!PrivacyElementInteractions} interaction
   */
  recordSettingsPageHistogram(interaction) {}

  /**
   * Helper function that calls recordHistogram for the
   * SafeBrowsing.Settings.UserAction histogram
   * @param {!SafeBrowsingInteractions} interaction
   */
  recordSafeBrowsingInteractionHistogram(interaction) {}
}

/**
 * @implements {MetricsBrowserProxy}
 */
export class MetricsBrowserProxyImpl {
  /** @override */
  recordAction(action) {
    chrome.send('metricsHandler:recordAction', [action]);
  }

  /** @override*/
  recordSafetyCheckInteractionHistogram(interaction) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyCheck.Interactions', interaction,
      SafetyCheckInteractions.COUNT
    ]);
  }

  /** @override*/
  recordSettingsPageHistogram(interaction) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.PrivacyElementInteractions', interaction,
      PrivacyElementInteractions.COUNT
    ]);
  }

  /** @override*/
  recordSafeBrowsingInteractionHistogram(interaction) {
    // TODO(crbug.com/1124491): Set the correct suffix for
    // SafeBrowsing.Settings.UserAction. Use the .Default suffix for now.
    chrome.send('metricsHandler:recordInHistogram', [
      'SafeBrowsing.Settings.UserAction.Default', interaction,
      SafeBrowsingInteractions.COUNT
    ]);
  }
}

addSingletonGetter(MetricsBrowserProxyImpl);
