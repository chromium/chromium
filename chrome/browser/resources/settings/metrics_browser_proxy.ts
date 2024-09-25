// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles metrics for the settings pages. */

/**
 * Contains all possible recorded interactions across privacy settings pages.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SettingsPrivacyElementInteractions enum in
 * histograms/metadata/settings/enums.xml
 */
// LINT.IfChange(PrivacyElementInteractions)
export enum PrivacyElementInteractions {
  // SYNC_AND_GOOGLE_SERVICES = 0,
  // CHROME_SIGN_IN = 1,
  DO_NOT_TRACK = 2,
  PAYMENT_METHOD = 3,
  // NETWORK_PREDICTION = 4,
  MANAGE_CERTIFICATES = 5,
  // SAFE_BROWSING = 6,
  // PASSWORD_CHECK = 7,
  IMPROVE_SECURITY = 8,
  // COOKIES_ALL = 9,
  // COOKIES_INCOGNITO = 10,
  // COOKIES_THIRD = 11,
  // COOKIES_BLOCK = 12,
  // COOKIES_SESSION = 13,
  // SITE_DATA_REMOVE_ALL = 14,
  // SITE_DATA_REMOVE_FILTERED = 15,
  // SITE_DATA_REMOVE_SITE = 16,
  // COOKIE_DETAILS_REMOVE_ALL = 17,
  // COOKIE_DETAILS_REMOVE_ITEM = 18,
  SITE_DETAILS_CLEAR_DATA = 19,
  THIRD_PARTY_COOKIES_ALLOW = 20,
  THIRD_PARTY_COOKIES_BLOCK_IN_INCOGNITO = 21,
  THIRD_PARTY_COOKIES_BLOCK = 22,
  BLOCK_ALL_THIRD_PARTY_COOKIES = 23,
  IP_PROTECTION = 24,
  FINGERPRINTING_PROTECTION = 25,
  // Max value should be updated whenever new entries are added.
  MAX_VALUE = 26,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/settings/enums.xml:SettingsPrivacyElementInteractions)

/**
 * Contains all Safety Hub card states.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with SafetyHubCardState in
 * histograms/enums.xml and CardState in safety_hub/safety_hub_browser_proxy.ts.
 */
export enum SafetyHubCardState {
  WARNING = 0,
  WEAK = 1,
  INFO = 2,
  SAFE = 3,
  // Max value should be updated whenever new entries are added.
  MAX_VALUE = 4,
}

/**
 * Contains all safety check interactions.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SafetyCheckInteractions enum in
 * histograms/enums.xml
 */
export enum SafetyCheckInteractions {
  RUN_SAFETY_CHECK = 0,
  UPDATES_RELAUNCH = 1,
  PASSWORDS_MANAGE_COMPROMISED_PASSWORDS = 2,
  SAFE_BROWSING_MANAGE = 3,
  EXTENSIONS_REVIEW = 4,
  // Deprecated in https://crbug.com/1407233.
  CHROME_CLEANER_REBOOT = 5,
  // Deprecated in https://crbug.com/1407233.
  CHROME_CLEANER_REVIEW_INFECTED_STATE = 6,
  PASSWORDS_CARET_NAVIGATION = 7,
  SAFE_BROWSING_CARET_NAVIGATION = 8,
  EXTENSIONS_CARET_NAVIGATION = 9,
  // Deprecated in https://crbug.com/1407233.
  CHROME_CLEANER_CARET_NAVIGATION = 10,
  PASSWORDS_MANAGE_WEAK_PASSWORDS = 11,
  UNUSED_SITE_PERMISSIONS_REVIEW = 12,
  // Max value should be updated whenever new entries are added.
  MAX_VALUE = 13,
}

/**
 * Contains all safety check notifications module interactions.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SafetyCheckNotificationsModuleInteractions enum
 * in histograms/enums.xml
 */
export enum SafetyCheckNotificationsModuleInteractions {
  BLOCK = 0,
  BLOCK_ALL = 1,
  IGNORE = 2,
  MINIMIZE = 3,
  RESET = 4,
  UNDO_BLOCK = 5,
  UNDO_IGNORE = 6,
  UNDO_RESET = 7,
  OPEN_REVIEW_UI = 8,
  UNDO_BLOCK_ALL = 9,
  GO_TO_SETTINGS = 10,
  // Max value should be updated whenever new entries are added.
  MAX_VALUE = 11,
}

/**
 * Contains all safety check unused site permissions module interactions.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the
 * SafetyCheckUnusedSitePermissionsModuleInteractions enum in
 * histograms/enums.xml
 */
export enum SafetyCheckUnusedSitePermissionsModuleInteractions {
  OPEN_REVIEW_UI = 0,
  ALLOW_AGAIN = 1,
  ACKNOWLEDGE_ALL = 2,
  UNDO_ALLOW_AGAIN = 3,
  UNDO_ACKNOWLEDGE_ALL = 4,
  MINIMIZE = 5,
  GO_TO_SETTINGS = 6,
  // Max value should be updated whenever new entries are added.
  MAX_VALUE = 7,
}

/**
 * Contains all entry points for Safety Hub page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SafetyHubEntryPoint enum in
 * histograms/enums.xml and safety_hub/safety_hub_constants.h.
 */
export enum SafetyHubEntryPoint {
  PRIVACY_SAFE = 0,
  PRIVACY_WARNING = 1,
  SITE_SETTINGS = 2,
  THREE_DOT_MENU = 3,
  NOTIFICATIONS = 4,
  // Max value should be updated whenever new entries are added.
  MAX_VALUE = 5,
}

/**
 * Contains all Safety Hub modules.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SafetyHubModuleType enum in
 * histograms/enums.xml and safety_hub/safety_hub_constants.h.
 */
export enum SafetyHubModuleType {
  PERMISSIONS = 0,
  NOTIFICATIONS = 1,
  SAFE_BROWSING = 2,
  EXTENSIONS = 3,
  PASSWORDS = 4,
  VERSION = 5,
  // Max value should be updated whenever new entries are added.
  MAX_VALUE = 6,
}

/**
 * Contains all safe browsing interactions.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the UserAction in safe_browsing_settings_metrics.h.
 */
export enum SafeBrowsingInteractions {
  SAFE_BROWSING_SHOWED = 0,
  SAFE_BROWSING_ENHANCED_PROTECTION_CLICKED = 1,
  SAFE_BROWSING_STANDARD_PROTECTION_CLICKED = 2,
  SAFE_BROWSING_DISABLE_SAFE_BROWSING_CLICKED = 3,
  SAFE_BROWSING_ENHANCED_PROTECTION_EXPAND_ARROW_CLICKED = 4,
  SAFE_BROWSING_STANDARD_PROTECTION_EXPAND_ARROW_CLICKED = 5,
  SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_CONFIRMED = 6,
  SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_DENIED = 7,
  // Max value should be updated whenever new entries are added.
  MAX_VALUE = 8,
}

/**
 * All Privacy guide interactions with metrics.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with SettingsPrivacyGuideInteractions in emus.xml and
 * PrivacyGuideInteractions in privacy_guide/privacy_guide.h.
 */
export enum PrivacyGuideInteractions {
  WELCOME_NEXT_BUTTON = 0,
  MSBB_NEXT_BUTTON = 1,
  HISTORY_SYNC_NEXT_BUTTON = 2,
  SAFE_BROWSING_NEXT_BUTTON = 3,
  COOKIES_NEXT_BUTTON = 4,
  COMPLETION_NEXT_BUTTON = 5,
  SETTINGS_LINK_ROW_ENTRY = 6,
  PROMO_ENTRY = 7,
  SWAA_COMPLETION_LINK = 8,
  PRIVACY_SANDBOX_COMPLETION_LINK = 9,
  SEARCH_SUGGESTIONS_NEXT_BUTTON = 10,
  TRACKING_PROTECTION_COMPLETION_LINK = 11,
  AD_TOPICS_NEXT_BUTTON = 12,
  // Max value should be updated whenever new entries are added.
  MAX_VALUE = 13,
}

/**
 * This enum covers all possible combinations of the start and end
 * settings states for each Privacy guide fragment, allowing metrics to see if
 * users change their settings inside of Privacy guide or not. The format is
 * settingAtStart-To-settingAtEnd.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with SettingsPrivacyGuideSettingsStates in enums.xml and
 * PrivacyGuideSettingsStates in privacy_guide/privacy_guide.h.
 */
export enum PrivacyGuideSettingsStates {
  MSBB_ON_TO_ON = 0,
  MSBB_ON_TO_OFF = 1,
  MSBB_OFF_TO_ON = 2,
  MSBB_OFF_TO_OFF = 3,
  BLOCK_3P_INCOGNITO_TO_3P_INCOGNITO = 4,
  BLOCK_3P_INCOGNITO_TO_3P = 5,
  BLOCK_3P_TO_3P_INCOGNITO = 6,
  BLOCK_3P_TO_3P = 7,
  HISTORY_SYNC_ON_TO_ON = 8,
  HISTORY_SYNC_ON_TO_OFF = 9,
  HISTORY_SYNC_OFF_TO_ON = 10,
  HISTORY_SYNC_OFF_TO_OFF = 11,
  SAFE_BROWSING_ENHANCED_TO_ENHANCED = 12,
  SAFE_BROWSING_ENHANCED_TO_STANDARD = 13,
  SAFE_BROWSING_STANDARD_TO_ENHANCED = 14,
  SAFE_BROWSING_STANDARD_TO_STANDARD = 15,
  SEARCH_SUGGESTIONS_ON_TO_ON = 16,
  SEARCH_SUGGESTIONS_ON_TO_OFF = 17,
  SEARCH_SUGGESTIONS_OFF_TO_ON = 18,
  SEARCH_SUGGESTIONS_OFF_TO_OFF = 19,
  AD_TOPICS_ON_TO_ON = 20,
  AD_TOPICS_ON_TO_OFF = 21,
  AD_TOPICS_OFF_TO_ON = 22,
  AD_TOPICS_OFF_TO_OFF = 23,
  // Max value should be updated whenever new entries are added.
  MAX_VALUE = 24,
}

/**
 * This enum is used with metrics to record when a step in the privacy guide is
 * eligible to be shown and/or reached by the user.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with SettingsPrivacyGuideStepsEligibleAndReached in
 * enums.xml and PrivacyGuideStepsEligibleAndReached in
 * privacy_guide/privacy_guide.h.
 */
export enum PrivacyGuideStepsEligibleAndReached {
  MSBB_ELIGIBLE = 0,
  MSBB_REACHED = 1,
  HISTORY_SYNC_ELIGIBLE = 2,
  HISTORY_SYNC_REACHED = 3,
  SAFE_BROWSING_ELIGIBLE = 4,
  SAFE_BROWSING_REACHED = 5,
  COOKIES_ELIGIBLE = 6,
  COOKIES_REACHED = 7,
  COMPLETION_ELIGIBLE = 8,
  COMPLETION_REACHED = 9,
  SEARCH_SUGGESTIONS_ELIGIBLE = 10,
  SEARCH_SUGGESTIONS_REACHED = 11,
  AD_TOPICS_ELIGIBLE = 12,
  AD_TOPICS_REACHED = 13,
  // Leave this at the end.
  COUNT = 14,
}

/**
 * Contains the possible delete browsing data action types.
 * This should be kept in sync with the `DeleteBrowsingDataAction` enum in
 * components/browsing_data/core/browsing_data_utils.h
 */
export enum DeleteBrowsingDataAction {
  CLEAR_BROWSING_DATA_DIALOG = 0,
  CLEAR_BROWSING_DATA_ON_EXIT = 1,
  INCOGNITO_CLOSE_TABS = 2,
  COOKIES_IN_USE_DIALOG = 3,
  SITES_SETTINGS_PAGE = 4,
  HISTORY_PAGE_ENTRIES = 5,
  QUICK_DELETE = 6,
  PAGE_INFO_RESET_PERMISSIONS = 7,
  MAX_VALUE = 8,
}

/**
 * This enum contains the different surfaces of Safety Hub that users can
 * interact with, or on which they can observe a Safety Hub feature.
 *
 * Must be kept in sync with the `safety_hub::SafetyHubSurfaces` enum in
 * chrome/browser/ui/safety_hub/safety_hub_constants.h and `SafetyHubSurfaces`
 * in enums.xml
 */
export enum SafetyHubSurfaces {
  THREE_DOT_MENU = 0,
  SAFETY_HUB_PAGE = 1,
  MAX_VALUE = 2,
}

/**
 * This enum contains the possible user actions for the bulk CVC deletion
 * operation on the payments settings page.
 */
export enum CvcDeletionUserAction {
  HYPERLINK_CLICKED = 'BulkCvcDeletionHyperlinkClicked',
  DIALOG_ACCEPTED = 'BulkCvcDeletionConfirmationDialogAccepted',
  DIALOG_CANCELLED = 'BulkCvcDeletionConfirmationDialogCancelled',
}

/**
 * This enum contains relevant UserAction log names for card benefits-related
 * functionality on the payment methods settings page.
 */
export enum CardBenefitsUserAction {
  CARD_BENEFITS_TERMS_LINK_CLICKED = 'CardBenefits_TermsLinkClicked',
}

export interface MetricsBrowserProxy {
  /**
   * Helper function that calls recordAction with one action from
   * tools/metrics/actions/actions.xml.
   */
  recordAction(action: string): void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.SafetyCheck.Interactions histogram
   */
  recordSafetyCheckInteractionHistogram(interaction: SafetyCheckInteractions):
      void;

  /**
   * Helper function that calls recordHistogram for
   * Settings.SafetyCheck.NotificationsListCount histogram.
   */
  recordSafetyCheckNotificationsListCountHistogram(suggestions: number): void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.SafetyCheck.NotificationsModuleInteractions histogram
   */
  recordSafetyCheckNotificationsModuleInteractionsHistogram(
      interaction: SafetyCheckNotificationsModuleInteractions): void;

  /**
   * Helper function that calls recordBooleanHistogram for the
   * Settings.SafetyCheck.NotificationsModuleEntryPointShown histogram
   */
  recordSafetyCheckNotificationsModuleEntryPointShown(visible: boolean): void;

  /**
   * Helper function that calls recordHistogram for
   * Settings.SafetyCheck.UnusedSitePermissionsListCount histogram.
   */
  recordSafetyCheckUnusedSitePermissionsListCountHistogram(suggestions: number):
      void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.SafetyCheck.UnusedSitePermissionsModuleInteractions histogram
   */
  recordSafetyCheckUnusedSitePermissionsModuleInteractionsHistogram(
      interaction: SafetyCheckUnusedSitePermissionsModuleInteractions): void;

  /**
   * Helper function that calls recordBooleanHistogram for the
   * Settings.SafetyCheck.UnusedSitePermissionsModuleEntryPointShown histogram
   */
  recordSafetyCheckUnusedSitePermissionsModuleEntryPointShown(visible: boolean):
      void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.SafetyHub.EntryPointShown histogram
   */
  recordSafetyHubEntryPointShown(page: SafetyHubEntryPoint): void;

  /**
   * Helper function that calls recordHistogram for the
   *Settings.SafetyHub.EntryPointClicked histogram
   */
  recordSafetyHubEntryPointClicked(page: SafetyHubEntryPoint): void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.SafetyHub.DashboardWarning histogram
   */
  recordSafetyHubModuleWarningImpression(module: SafetyHubModuleType): void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.SafetyHub.HasDashboardShowAnyWarning histogram
   */
  recordSafetyHubDashboardAnyWarning(visible: boolean): void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.SafetyHub.[card_name].StatusOnClick histogram
   */
  recordSafetyHubCardStateClicked(
      histogramName: string, state: SafetyHubCardState): void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.SafetyHub.NotificationPermissionsModule.Interactions histogram
   */
  recordSafetyHubNotificationPermissionsModuleInteractionsHistogram(
      interaction: SafetyCheckNotificationsModuleInteractions): void;

  /**
   * Helper function that calls recordHistogram for
   * Settings.SafetyHub.NotificationPermissionsModule.ListCount histogram
   */
  recordSafetyHubNotificationPermissionsModuleListCountHistogram(
      suggestions: number): void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.SafetyHub.UnusedSitePermissionsModule.Interactions histogram
   */
  recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram(
      interaction: SafetyCheckUnusedSitePermissionsModuleInteractions): void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.SafetyHub.AbusiveNotificationPermissionRevocation.Interactions
   * histogram
   */
  recordSafetyHubAbusiveNotificationPermissionRevocationInteractionsHistogram(
      interaction: SafetyCheckUnusedSitePermissionsModuleInteractions): void;

  /**
   * Helper function that calls recordHistogram for
   * Settings.SafetyHub.UnusedSitePermissionsModule.ListCount histogram
   */
  recordSafetyHubUnusedSitePermissionsModuleListCountHistogram(
      suggestions: number): void;

  /**
   * Helper function that calls recordHistogram for the
   * SettingsPage.PrivacyElementInteractions histogram
   */
  recordSettingsPageHistogram(interaction: PrivacyElementInteractions): void;

  /**
   * Helper function that calls recordHistogram for the
   * SafeBrowsing.Settings.UserAction histogram
   */
  recordSafeBrowsingInteractionHistogram(interaction: SafeBrowsingInteractions):
      void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.PrivacyGuide.NextNavigation histogram
   */
  recordPrivacyGuideNextNavigationHistogram(interaction:
                                                PrivacyGuideInteractions): void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.PrivacyGuide.EntryExit histogram
   */
  recordPrivacyGuideEntryExitHistogram(interaction: PrivacyGuideInteractions):
      void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.PrivacyGuide.SettingsStates histogram
   */
  recordPrivacyGuideSettingsStatesHistogram(state: PrivacyGuideSettingsStates):
      void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.PrivacyGuide.FlowLength histogram
   */
  recordPrivacyGuideFlowLengthHistogram(steps: number): void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.PrivacyGuide.StepsEligibleAndReached histogram
   */
  recordPrivacyGuideStepsEligibleAndReachedHistogram(
      status: PrivacyGuideStepsEligibleAndReached): void;

  /**
   * Helper function that delegates the metric recording to the
   * recordDeleteBrowsingDataAction backend function.
   */
  recordDeleteBrowsingDataAction(action: DeleteBrowsingDataAction): void;

  /**
   * Helper function that calls records an impression of the provided Safety Hub
   * surface.
   */
  recordSafetyHubImpression(surface: SafetyHubSurfaces): void;

  /**
   * Helper function that calls records an interaction of the provided Safety
   * Hub surface.
   */
  recordSafetyHubInteraction(surface: SafetyHubSurfaces): void;

  // <if expr="_google_chrome and is_win">
  /**
   * Notifies Chrome that the `feature_notifications.enabled` Local State pref
   * was changed via the System settings page.
   *
   * @param enabled True if the toggle was changed to on (enabled), false if it
   * was changed to off (disabled).
   */
  recordFeatureNotificationsChange(enabled: boolean): void;
  // </if>
}

export class MetricsBrowserProxyImpl implements MetricsBrowserProxy {
  recordAction(action: string) {
    chrome.send('metricsHandler:recordAction', [action]);
  }

  recordSafetyCheckInteractionHistogram(interaction: SafetyCheckInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyCheck.Interactions',
      interaction,
      SafetyCheckInteractions.MAX_VALUE,
    ]);
  }

  recordSafetyCheckNotificationsListCountHistogram(suggestions: number) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyCheck.NotificationsListCount',
      suggestions,
      99 /*max value for Notification suggestions*/,
    ]);
  }

  recordSafetyCheckNotificationsModuleInteractionsHistogram(
      interaction: SafetyCheckNotificationsModuleInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyCheck.NotificationsModuleInteractions',
      interaction,
      SafetyCheckNotificationsModuleInteractions.MAX_VALUE,
    ]);
  }

  recordSafetyCheckNotificationsModuleEntryPointShown(visible: boolean) {
    chrome.send('metricsHandler:recordBooleanHistogram', [
      'Settings.SafetyCheck.NotificationsModuleEntryPointShown',
      visible,
    ]);
  }

  recordSafetyCheckUnusedSitePermissionsListCountHistogram(suggestions:
                                                               number) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyCheck.UnusedSitePermissionsListCount',
      suggestions,
      99 /*max value for length of revoked permissions list*/,
    ]);
  }

  recordSafetyCheckUnusedSitePermissionsModuleInteractionsHistogram(
      interaction: SafetyCheckUnusedSitePermissionsModuleInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyCheck.UnusedSitePermissionsModuleInteractions',
      interaction,
      SafetyCheckUnusedSitePermissionsModuleInteractions.MAX_VALUE,
    ]);
  }

  recordSafetyCheckUnusedSitePermissionsModuleEntryPointShown(visible:
                                                                  boolean) {
    chrome.send('metricsHandler:recordBooleanHistogram', [
      'Settings.SafetyCheck.UnusedSitePermissionsModuleEntryPointShown',
      visible,
    ]);
  }

  recordSafetyHubCardStateClicked(
      histogramName: string, state: SafetyHubCardState) {
    chrome.send(
        'metricsHandler:recordInHistogram',
        [histogramName, state, SafetyHubCardState.MAX_VALUE]);
  }

  recordSafetyHubEntryPointShown(page: SafetyHubEntryPoint) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.EntryPointImpression',
      page,
      SafetyHubEntryPoint.MAX_VALUE,
    ]);
  }

  recordSafetyHubEntryPointClicked(page: SafetyHubEntryPoint) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.EntryPointInteraction',
      page,
      SafetyHubEntryPoint.MAX_VALUE,
    ]);
  }

  recordSafetyHubModuleWarningImpression(module: SafetyHubModuleType) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.DashboardWarning',
      module,
      SafetyHubModuleType.MAX_VALUE,
    ]);
  }

  recordSafetyHubDashboardAnyWarning(visible: boolean) {
    chrome.send('metricsHandler:recordBooleanHistogram', [
      'Settings.SafetyHub.HasDashboardShowAnyWarning',
      visible,
    ]);
  }

  recordSafetyHubNotificationPermissionsModuleInteractionsHistogram(
      interaction: SafetyCheckNotificationsModuleInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.NotificationPermissionsModule.Interactions',
      interaction,
      SafetyCheckNotificationsModuleInteractions.MAX_VALUE,
    ]);
  }

  recordSafetyHubNotificationPermissionsModuleListCountHistogram(suggestions:
                                                                     number) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.NotificationPermissionsModule.ListCount',
      suggestions,
      99 /*max value for Notification Permissions suggestions*/,
    ]);
  }

  recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram(
      interaction: SafetyCheckUnusedSitePermissionsModuleInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.UnusedSitePermissionsModule.Interactions',
      interaction,
      SafetyCheckUnusedSitePermissionsModuleInteractions.MAX_VALUE,
    ]);
  }

  recordSafetyHubAbusiveNotificationPermissionRevocationInteractionsHistogram(
      interaction: SafetyCheckUnusedSitePermissionsModuleInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.AbusiveNotificationPermissionRevocation.Interactions',
      interaction,
      SafetyCheckUnusedSitePermissionsModuleInteractions.MAX_VALUE,
    ]);
  }

  recordSafetyHubUnusedSitePermissionsModuleListCountHistogram(suggestions:
                                                                   number) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.UnusedSitePermissionsModule.ListCount',
      suggestions,
      99 /*max value for Unused Site Permissions suggestions*/,
    ]);
  }

  recordSettingsPageHistogram(interaction: PrivacyElementInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.PrivacyElementInteractions',
      interaction,
      PrivacyElementInteractions.MAX_VALUE,
    ]);
  }

  recordSafeBrowsingInteractionHistogram(interaction:
                                             SafeBrowsingInteractions) {
    // TODO(crbug.com/40717279): Set the correct suffix for
    // SafeBrowsing.Settings.UserAction. Use the .Default suffix for now.
    chrome.send('metricsHandler:recordInHistogram', [
      'SafeBrowsing.Settings.UserAction.Default',
      interaction,
      SafeBrowsingInteractions.MAX_VALUE,
    ]);
  }

  recordPrivacyGuideNextNavigationHistogram(interaction:
                                                PrivacyGuideInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.PrivacyGuide.NextNavigation',
      interaction,
      PrivacyGuideInteractions.MAX_VALUE,
    ]);
  }

  recordPrivacyGuideEntryExitHistogram(interaction: PrivacyGuideInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.PrivacyGuide.EntryExit',
      interaction,
      PrivacyGuideInteractions.MAX_VALUE,
    ]);
  }

  recordPrivacyGuideSettingsStatesHistogram(state: PrivacyGuideSettingsStates) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.PrivacyGuide.SettingsStates',
      state,
      PrivacyGuideSettingsStates.MAX_VALUE,
    ]);
  }

  recordPrivacyGuideFlowLengthHistogram(steps: number) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.PrivacyGuide.FlowLength', steps,
      5, /*max number of the settings related steps in privacy guide is 4*/
    ]);
  }

  recordPrivacyGuideStepsEligibleAndReachedHistogram(
      status: PrivacyGuideStepsEligibleAndReached) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.PrivacyGuide.StepsEligibleAndReached',
      status,
      PrivacyGuideStepsEligibleAndReached.COUNT,
    ]);
  }

  recordDeleteBrowsingDataAction(action: DeleteBrowsingDataAction): void {
    chrome.send('metricsHandler:recordInHistogram', [
      'Privacy.DeleteBrowsingData.Action',
      action,
      DeleteBrowsingDataAction.MAX_VALUE,
    ]);
  }

  recordSafetyHubImpression(surface: SafetyHubSurfaces): void {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.Impression',
      surface,
      SafetyHubSurfaces.MAX_VALUE,
    ]);
  }

  recordSafetyHubInteraction(surface: SafetyHubSurfaces): void {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.Interaction',
      surface,
      SafetyHubSurfaces.MAX_VALUE,
    ]);
  }

  // <if expr="_google_chrome and is_win">
  recordFeatureNotificationsChange(enabled: boolean): void {
    chrome.metricsPrivate.recordBoolean(
        'Windows.FeatureNotificationsSettingChange', enabled);
  }
  // </if>

  static getInstance(): MetricsBrowserProxy {
    return instance || (instance = new MetricsBrowserProxyImpl());
  }

  static setInstance(obj: MetricsBrowserProxy) {
    instance = obj;
  }
}

let instance: MetricsBrowserProxy|null = null;
