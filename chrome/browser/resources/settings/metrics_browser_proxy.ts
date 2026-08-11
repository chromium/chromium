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
  // IP_PROTECTION = 24,
  // FINGERPRINTING_PROTECTION = 25,
  // COUNT should be updated whenever new entries are added.
  COUNT = 26,
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
// LINT.IfChange(SafetyHubCardState)
export enum SafetyHubCardState {
  WARNING = 0,
  WEAK = 1,
  INFO = 2,
  SAFE = 3,
  // COUNT should be updated whenever new entries are added.
  COUNT = 4,
}
// LINT.ThenChange(
//   //chrome/browser/resources/settings/safety_hub/safety_hub_browser_proxy.ts:CardState,
//   //tools/metrics/histograms/metadata/settings/enums.xml:SafetyHubCardState
// )

/**
 * Contains all safety check notifications module interactions.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SafetyCheckNotificationsModuleInteractions enum
 * in histograms/enums.xml
 */
// LINT.IfChange(SafetyCheckNotificationsModuleInteractions)
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
  // COUNT should be updated whenever new entries are added.
  COUNT = 11,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/settings/enums.xml:SafetyCheckNotificationsModuleInteractions)

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
// LINT.IfChange(SafetyCheckUnusedSitePermissionsModuleInteractions)
export enum SafetyCheckUnusedSitePermissionsModuleInteractions {
  OPEN_REVIEW_UI = 0,
  ALLOW_AGAIN = 1,
  ACKNOWLEDGE_ALL = 2,
  UNDO_ALLOW_AGAIN = 3,
  UNDO_ACKNOWLEDGE_ALL = 4,
  MINIMIZE = 5,
  GO_TO_SETTINGS = 6,
  // COUNT should be updated whenever new entries are added.
  COUNT = 7,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/settings/enums.xml:SafetyCheckUnusedSitePermissionsModuleInteractions)

/**
 * Contains all entry points for Safety Hub page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SafetyHubEntryPoint enum in
 * histograms/enums.xml and safety_hub/safety_hub_constants.h.
 */
// LINT.IfChange(SafetyHubEntryPoint)
export enum SafetyHubEntryPoint {
  PRIVACY_SAFE = 0,
  PRIVACY_WARNING = 1,
  SITE_SETTINGS = 2,
  THREE_DOT_MENU = 3,
  NOTIFICATIONS = 4,
  // COUNT should be updated whenever new entries are added.
  COUNT = 5,
}
// LINT.ThenChange(
//   //tools/metrics/histograms/metadata/settings/enums.xml:SafetyHubEntryPoint,
//   //chrome/browser/ui/safety_hub/safety_hub_constants.h:SafetyHubEntryPoint
// )

/**
 * Contains all Safety Hub modules.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SafetyHubModuleType enum in
 * histograms/enums.xml and safety_hub/safety_hub_constants.h.
 */
// LINT.IfChange(SafetyHubModuleType)
export enum SafetyHubModuleType {
  PERMISSIONS = 0,
  NOTIFICATIONS = 1,
  SAFE_BROWSING = 2,
  EXTENSIONS = 3,
  PASSWORDS = 4,
  VERSION = 5,
  // COUNT should be updated whenever new entries are added.
  COUNT = 6,
}
// LINT.ThenChange(
//   //tools/metrics/histograms/metadata/settings/enums.xml:SafetyHubModuleType,
//   //chrome/browser/ui/safety_hub/safety_hub_constants.h:SafetyHubModuleType
// )

/**
 * Contains all safe browsing interactions.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the UserAction in safe_browsing_settings_metrics.h.
 */
// LINT.IfChange(SafeBrowsingInteractions)
export enum SafeBrowsingInteractions {
  SAFE_BROWSING_SHOWED = 0,
  SAFE_BROWSING_ENHANCED_PROTECTION_CLICKED = 1,
  SAFE_BROWSING_STANDARD_PROTECTION_CLICKED = 2,
  SAFE_BROWSING_DISABLE_SAFE_BROWSING_CLICKED = 3,
  SAFE_BROWSING_ENHANCED_PROTECTION_EXPAND_ARROW_CLICKED = 4,
  SAFE_BROWSING_STANDARD_PROTECTION_EXPAND_ARROW_CLICKED = 5,
  SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_CONFIRMED = 6,
  SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_DENIED = 7,
  // COUNT should be updated whenever new entries are added.
  COUNT = 8,
}
// LINT.ThenChange(//components/safe_browsing/core/common/safe_browsing_settings_metrics.h:UserAction)

/**
 * All Privacy guide interactions with metrics.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with SettingsPrivacyGuideInteractions in emus.xml and
 * PrivacyGuideInteractions in privacy_guide/privacy_guide.h.
 */
// LINT.IfChange(PrivacyGuideInteractions)
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
  // TRACKING_PROTECTION_COMPLETION_LINK = 11, // OBSOLETE
  // AD_TOPICS_NEXT_BUTTON = 12, // OBSOLETE
  AI_SETTINGS_COMPLETION_LINK = 13,
  // COUNT should be updated whenever new entries are added.
  COUNT = 14,
}
// LINT.ThenChange(
//   //chrome/browser/privacy_guide/privacy_guide.h:PrivacyGuideInteractions,
//   //tools/metrics/histograms/metadata/settings/enums.xml:SettingsPrivacyGuideInteractions
// )

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
// LINT.IfChange(PrivacyGuideSettingsStates)
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
  // AD_TOPICS_ON_TO_ON = 20, // OBSOLETE
  // AD_TOPICS_ON_TO_OFF = 21, // OBSOLETE
  // AD_TOPICS_OFF_TO_ON = 22, // OBSOLETE
  // AD_TOPICS_OFF_TO_OFF = 23, // OBSOLETE
  // COUNT should be updated whenever new entries are added.
  COUNT = 24,
}
// LINT.ThenChange(
//   //chrome/browser/privacy_guide/privacy_guide.h:PrivacyGuideSettingsStates,
//   //tools/metrics/histograms/metadata/settings/enums.xml:SettingsPrivacyGuideSettingsStates
// )

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
// LINT.IfChange(PrivacyGuideStepsEligibleAndReached)
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
  // AD_TOPICS_ELIGIBLE = 12, // OBSOLETE
  // AD_TOPICS_REACHED = 13, // OBSOLETE
  // Leave this at the end.
  COUNT = 14,
}
// LINT.ThenChange(
//   //chrome/browser/privacy_guide/privacy_guide.h:PrivacyGuideStepsEligibleAndReached,
//   //tools/metrics/histograms/metadata/settings/enums.xml:SettingsPrivacyGuideStepsEligibleAndReached
// )

/**
 * Contains the possible delete browsing data action types.
 * This should be kept in sync with the `DeleteBrowsingDataAction` enum in
 * components/browsing_data/core/browsing_data_utils.h
 */
// LINT.IfChange(DeleteBrowsingDataAction)
export enum DeleteBrowsingDataAction {
  CLEAR_BROWSING_DATA_DIALOG = 0,
  CLEAR_BROWSING_DATA_ON_EXIT = 1,
  INCOGNITO_CLOSE_TABS = 2,
  COOKIES_IN_USE_DIALOG = 3,
  SITES_SETTINGS_PAGE = 4,
  HISTORY_PAGE_ENTRIES = 5,
  QUICK_DELETE = 6,
  PAGE_INFO_RESET_PERMISSIONS = 7,
  RWS_DELETE_ALL_DATA = 8,
  COUNT = 9,
}
// LINT.ThenChange(
//   //components/browsing_data/core/browsing_data_utils.h:DeleteBrowsingDataAction,
//   //tools/metrics/histograms/metadata/privacy/enums.xml:DeleteBrowsingDataAction
// )

/**
 * This enum contains the different surfaces of Safety Hub that users can
 * interact with, or on which they can observe a Safety Hub feature.
 *
 * Must be kept in sync with the `safety_hub::SafetyHubSurfaces` enum in
 * chrome/browser/ui/safety_hub/safety_hub_constants.h and `SafetyHubSurfaces`
 * in enums.xml
 */
// LINT.IfChange(SafetyHubSurfaces)
export enum SafetyHubSurfaces {
  THREE_DOT_MENU = 0,
  SAFETY_HUB_PAGE = 1,
  COUNT = 2,
}
// LINT.ThenChange(
//   //tools/metrics/histograms/metadata/settings/enums.xml:SafetyHubSurfaces,
//   //chrome/browser/ui/safety_hub/safety_hub_constants.h:SafetyHubSurfaces
// )

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

/**
 * Contains all recorded interactions across AI settings page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SettingsAiPageInteractions enum in
 * histograms/metadata/settings/enums.xml
 */
// LINT.IfChange(AiPageInteractions)
export enum AiPageInteractions {
  HISTORY_SEARCH_CLICK = 0,
  // COMPARE_CLICK = 1, // DEPRECATED
  COMPOSE_CLICK = 2,
  // TAB_ORGANIZATION_CLICK = 3, // DEPRECATED
  // WALLPAPER_SEARCH_CLICK = 4, // DEPRECATED
  AUTOFILL_AI_CLICK = 5,
  PASSWORD_CHANGE_CLICK = 6,
  AI_SUGGESTIONS_CLICK = 7,
  SKILLS_CLICK = 8,
  INDIGO_CLICK = 9,
  GOOGLE_SEARCH_AI_MODE_WORKSPACE_CLICK = 10,
  INLINE_CUE_MENU_CLICK = 11,
  COUNT = 12,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/settings/enums.xml:SettingsAiPageInteractions)

/**
 * Contains all recorded interactions in the AI History Search settings page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SettingsAiPageHistorySearchInteractions enum in
 * histograms/metadata/settings/enums.xml
 */
// LINT.IfChange(AiPageHistorySearchInteractions)
export enum AiPageHistorySearchInteractions {
  HISTORY_SEARCH_ENABLED = 0,
  HISTORY_SEARCH_DISABLED = 1,
  FEATURE_LINK_CLICKED = 2,
  LEARN_MORE_LINK_CLICKED = 3,
  COUNT = 4,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/settings/enums.xml:SettingsAiPageHistorySearchInteractions)

/**
 * Contains all recorded interactions in the AI Compose settings page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SettingsAiPageComposeInteractions enum in
 * histograms/metadata/settings/enums.xml
 */
// LINT.IfChange(AiPageComposeInteractions)
export enum AiPageComposeInteractions {
  LEARN_MORE_LINK_CLICKED = 0,
  COMPOSE_PROACTIVE_NUDGE_ENABLED = 1,
  COMPOSE_PROACTIVE_NUDGE_DISABLED = 2,
  COUNT = 3,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/settings/enums.xml:SettingsAiPageComposeInteractions)

/**
 * Contains all recorded interactions in the AI Suggestions settings page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SettingsAiPageSuggestionsInteractions enum in
 * histograms/metadata/settings/enums.xml
 */
// LINT.IfChange(AiPageSuggestionsInteractions)
export enum AiPageSuggestionsInteractions {
  SUGGESTIONS_ENABLED = 0,
  SUGGESTIONS_DISABLED = 1,
  LEARN_MORE_LINK_CLICKED = 2,
  SYNC_SETTINGS_LINK_CLICKED = 3,
  COUNT = 4,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/settings/enums.xml:SettingsAiPageAiSuggestionsInteractions)

/**
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the AutofillSettingsReferrer enum in
 * histograms/metadata/autofill/enums.xml
 */
// LINT.IfChange(AutofillSettingsReferrer)
export enum AutofillSettingsReferrer {
  // PROFILE_MENU = 0,
  SETTINGS_MENU = 1,
  AUTOFILL_AND_PASSWORDS_PAGE = 2,
  // FILLING_FLOW_DROPDOWN = 3,
  // SETTINGS_SEARCH = 4,
  COUNT = 5,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:AutofillSettingsReferrer)

/**
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the YourSavedInfoDataCategory enum in
 * histograms/metadata/autofill/enums.xml
 */
// LINT.IfChange(YourSavedInfoDataCategory)
export enum YourSavedInfoDataCategory {
  PASSWORD_MANAGER = 0,
  PAYMENTS = 1,
  CONTACT_INFO = 2,
  IDENTITY_DOCS = 3,
  TRAVEL = 4,
  SHOPPING = 5,
  COUNT = 6,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:YourSavedInfoDataCategory)

/**
 * A specific kind of saved user's information.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the YourSavedInfoDataChip enum in
 * histograms/metadata/autofill/enums.xml
 */
// LINT.IfChange(YourSavedInfoDataChip)
export enum YourSavedInfoDataChip {
  PASSWORDS = 0,
  PASSKEYS = 1,
  CREDIT_CARDS = 2,
  PAY_OVER_TIME = 3,
  IBANS = 4,
  LOYALTY_CARDS = 5,
  ADDRESSES = 6,
  DRIVERS_LICENSES = 7,
  NATIONAL_ID_CARDS = 8,
  PASSPORTS = 9,
  FLIGHT_RESERVATIONS = 10,
  TRAVEL_INFO = 11,
  VEHICLES = 12,
  SHIPMENTS = 13,
  ORDERS = 14,
  COUNT = 15,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:YourSavedInfoDataChip)

/**
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the YourSavedInfoDataCategory enum in
 * histograms/metadata/autofill/enums.xml
 */
// LINT.IfChange(YourSavedInfoRelatedService)
export enum YourSavedInfoRelatedService {
  GOOGLE_PASSWORD_MANAGER = 0,
  GOOGLE_WALLET = 1,
  GOOGLE_ACCOUNT = 2,
  COUNT = 3,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:YourSavedInfoRelatedService)

// LINT.IfChange(SuggestionsFromGeminiEntryPoint)
export enum SuggestionsFromGeminiEntryPoint {
  YOUR_SAVED_INFO = 0,
  TRAVEL = 1,
  SHOPPING = 2,
  IDENTITY_DOCS = 3,
  COUNT = 4,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:SuggestionsFromGeminiEntryPoint)

// LINT.IfChange(SuggestionsFromGeminiAction)
export enum SuggestionsFromGeminiAction {
  MANAGE_CONNECTED_APPS_CLICK = 0,
  TOGGLE_ON = 1,
  TOGGLE_OFF = 2,
  COUNT = 3,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:SuggestionsFromGeminiAction)

export interface MetricsBrowserProxy {
  /**
   * Helper function that calls recordAction with one action from
   * tools/metrics/actions/actions.xml.
   */
  recordAction(action: string): void;

  /**
   * Helper function that calls recordBooleanHistogram with the histogramName.
   */
  recordBooleanHistogram(histogramName: string, visible: boolean): void;

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
   * Records a click on the Suggestions from Gemini link across Your saved info
   * and category subpages with a corresponding metric and user action.
   */
  recordSuggestionsFromGeminiEntryPointClick(
      entryPoint: SuggestionsFromGeminiEntryPoint): void;

  /**
   * Records an action triggered inside the Suggestions from Gemini subpage
   * with a corresponding metric and user action.
   */
  recordSuggestionsFromGeminiAction(action: SuggestionsFromGeminiAction): void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.PrivacyGuide.NextNavigation histogram
   */
  recordPrivacyGuideNextNavigationHistogram(
      interaction: PrivacyGuideInteractions): void;

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

  /**
   * Helper function that calls recordHistogram for the
   * Settings.AiPage.Interactions histogram
   */
  recordAiPageInteractions(interaction: AiPageInteractions): void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.AiPage.HistorySearch.Interactions histogram
   */
  recordAiPageHistorySearchInteractions(
      interaction: AiPageHistorySearchInteractions): void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.AiPage.Compose.Interactions histogram
   */
  recordAiPageComposeInteractions(interaction: AiPageComposeInteractions): void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.AiPage.Suggestions.Interactions histogram
   */
  recordAiPageSuggestionsInteractions(
      interaction: AiPageSuggestionsInteractions): void;

  /**
   * Records a referrer to one of Autofill settings pages.
   */
  recordAutofillSettingsReferrer(
      histogramName: string, referrer: AutofillSettingsReferrer): void;

  /**
   * Records a click on a category link on the Your saved info page with
   * a corresponding metric and user action.
   */
  recordYourSavedInfoCategoryClick(category: YourSavedInfoDataCategory): void;

  /**
   * Records a click on a data chip on the Your saved info page with
   * a corresponding metric and user action.
   */
  recordYourSavedInfoDataChipClick(chip: YourSavedInfoDataChip): void;

  /**
   * Records a click on a related service link on the Your saved info page with
   * a corresponding metric and user action.
   */
  recordYourSavedInfoRelatedServiceClick(service: YourSavedInfoRelatedService):
      void;
}

export const SAFETY_HUB_SUGGESTIONS_MAX_VALUE = 98;

export class MetricsBrowserProxyImpl implements MetricsBrowserProxy {
  recordAction(action: string) {
    chrome.send('metricsHandler:recordAction', [action]);
  }

  recordBooleanHistogram(histogramName: string, visible: boolean): void {
    chrome.send('metricsHandler:recordBooleanHistogram', [
      histogramName,
      visible,
    ]);
  }

  recordSafetyHubCardStateClicked(
      histogramName: string, state: SafetyHubCardState) {
    chrome.send(
        'metricsHandler:recordInHistogram',
        [histogramName, state, SafetyHubCardState.COUNT]);
  }

  recordSafetyHubEntryPointShown(page: SafetyHubEntryPoint) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.EntryPointImpression',
      page,
      SafetyHubEntryPoint.COUNT,
    ]);
  }

  recordSafetyHubEntryPointClicked(page: SafetyHubEntryPoint) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.EntryPointInteraction',
      page,
      SafetyHubEntryPoint.COUNT,
    ]);
  }

  recordSafetyHubModuleWarningImpression(module: SafetyHubModuleType) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.DashboardWarning',
      module,
      SafetyHubModuleType.COUNT,
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
      SafetyCheckNotificationsModuleInteractions.COUNT,
    ]);
  }

  recordSafetyHubNotificationPermissionsModuleListCountHistogram(
      suggestions: number) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.NotificationPermissionsModule.ListCount',
      Math.min(suggestions, SAFETY_HUB_SUGGESTIONS_MAX_VALUE),
      SAFETY_HUB_SUGGESTIONS_MAX_VALUE + 1,
    ]);
  }

  recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram(
      interaction: SafetyCheckUnusedSitePermissionsModuleInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.UnusedSitePermissionsModule.Interactions',
      interaction,
      SafetyCheckUnusedSitePermissionsModuleInteractions.COUNT,
    ]);
  }

  recordSafetyHubAbusiveNotificationPermissionRevocationInteractionsHistogram(
      interaction: SafetyCheckUnusedSitePermissionsModuleInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.AbusiveNotificationPermissionRevocation.Interactions',
      interaction,
      SafetyCheckUnusedSitePermissionsModuleInteractions.COUNT,
    ]);
  }

  recordSafetyHubUnusedSitePermissionsModuleListCountHistogram(
      suggestions: number) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.UnusedSitePermissionsModule.ListCount',
      Math.min(suggestions, SAFETY_HUB_SUGGESTIONS_MAX_VALUE),
      SAFETY_HUB_SUGGESTIONS_MAX_VALUE + 1,
    ]);
  }

  recordSettingsPageHistogram(interaction: PrivacyElementInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.PrivacyElementInteractions',
      interaction,
      PrivacyElementInteractions.COUNT,
    ]);
  }

  recordSafeBrowsingInteractionHistogram(
      interaction: SafeBrowsingInteractions) {
    // TODO(crbug.com/40717279): Set the correct suffix for
    // SafeBrowsing.Settings.UserAction. Use the .Default suffix for now.
    chrome.send('metricsHandler:recordInHistogram', [
      'SafeBrowsing.Settings.UserAction.Default',
      interaction,
      SafeBrowsingInteractions.COUNT,
    ]);
  }

  recordPrivacyGuideNextNavigationHistogram(
      interaction: PrivacyGuideInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.PrivacyGuide.NextNavigation',
      interaction,
      PrivacyGuideInteractions.COUNT,
    ]);
  }

  recordPrivacyGuideEntryExitHistogram(interaction: PrivacyGuideInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.PrivacyGuide.EntryExit',
      interaction,
      PrivacyGuideInteractions.COUNT,
    ]);
  }

  recordPrivacyGuideSettingsStatesHistogram(state: PrivacyGuideSettingsStates) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.PrivacyGuide.SettingsStates',
      state,
      PrivacyGuideSettingsStates.COUNT,
    ]);
  }

  recordPrivacyGuideFlowLengthHistogram(steps: number) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.PrivacyGuide.FlowLength',
      steps,
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
      DeleteBrowsingDataAction.COUNT,
    ]);
  }

  recordSafetyHubImpression(surface: SafetyHubSurfaces): void {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.Impression',
      surface,
      SafetyHubSurfaces.COUNT,
    ]);
  }

  recordSafetyHubInteraction(surface: SafetyHubSurfaces): void {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyHub.Interaction',
      surface,
      SafetyHubSurfaces.COUNT,
    ]);
  }

  // <if expr="_google_chrome and is_win">
  recordFeatureNotificationsChange(enabled: boolean): void {
    chrome.metricsPrivate.recordBoolean(
        'Windows.FeatureNotificationsSettingChange', enabled);
  }
  // </if>

  recordAiPageInteractions(interaction: AiPageInteractions): void {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.AiPage.Interactions',
      interaction,
      AiPageInteractions.COUNT,
    ]);
  }

  recordAiPageHistorySearchInteractions(
      interaction: AiPageHistorySearchInteractions): void {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.AiPage.HistorySearch.Interactions',
      interaction,
      AiPageHistorySearchInteractions.COUNT,
    ]);
  }

  recordAiPageComposeInteractions(interaction: AiPageComposeInteractions):
      void {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.AiPage.Compose.Interactions',
      interaction,
      AiPageComposeInteractions.COUNT,
    ]);
  }

  recordAiPageSuggestionsInteractions(
      interaction: AiPageSuggestionsInteractions): void {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.AiPage.Suggestions.Interactions',
      interaction,
      AiPageSuggestionsInteractions.COUNT,
    ]);
  }

  recordAutofillSettingsReferrer(
      histogramName: string, referrer: AutofillSettingsReferrer) {
    chrome.send(
        'metricsHandler:recordInHistogram',
        [histogramName, referrer, AutofillSettingsReferrer.COUNT]);
  }

  recordYourSavedInfoCategoryClick(category: YourSavedInfoDataCategory) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Autofill.YourSavedInfoSettingsPage.CategoryLinkClick',
      category,
      YourSavedInfoDataCategory.COUNT,
    ]);
    if (category !== YourSavedInfoDataCategory.COUNT) {
      this.recordAction(`Settings.YourSavedInfo.CategoryClick.${
          YourSavedInfoDataCategory[category]}`);
    }
  }

  recordYourSavedInfoDataChipClick(chip: YourSavedInfoDataChip) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Autofill.YourSavedInfoSettingsPage.DataChipClick',
      chip,
      YourSavedInfoDataChip.COUNT,
    ]);
    if (chip !== YourSavedInfoDataChip.COUNT) {
      this.recordAction(
          `Settings.YourSavedInfo.ChipClick.${YourSavedInfoDataChip[chip]}`);
    }
  }

  recordYourSavedInfoRelatedServiceClick(service: YourSavedInfoRelatedService) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Autofill.YourSavedInfoSettingsPage.RelatedServiceLinkClick',
      service,
      YourSavedInfoRelatedService.COUNT,
    ]);
    if (service !== YourSavedInfoRelatedService.COUNT) {
      this.recordAction(`Settings.YourSavedInfo.RelatedServiceClick.${
          YourSavedInfoRelatedService[service]}`);
    }
  }

  recordSuggestionsFromGeminiEntryPointClick(
      entryPoint: SuggestionsFromGeminiEntryPoint) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Autofill.YourSavedInfoSettingsPage.SuggestionsFromGeminiLinkClick',
      entryPoint,
      SuggestionsFromGeminiEntryPoint.COUNT,
    ]);
    if (entryPoint !== SuggestionsFromGeminiEntryPoint.COUNT) {
      const actionMap = {
        [SuggestionsFromGeminiEntryPoint.YOUR_SAVED_INFO]:
            'PersonalContext.Settings.EntryPoint.AutofillAndPasswordsSettings',
        [SuggestionsFromGeminiEntryPoint.TRAVEL]:
            'PersonalContext.Settings.EntryPoint.TravelSettings',
        [SuggestionsFromGeminiEntryPoint.SHOPPING]:
            'PersonalContext.Settings.EntryPoint.ShoppingSettings',
        [SuggestionsFromGeminiEntryPoint.IDENTITY_DOCS]:
            'PersonalContext.Settings.EntryPoint.IdentityDocsSettings',
      };
      this.recordAction(actionMap[entryPoint]);
    }
  }

  recordSuggestionsFromGeminiAction(action: SuggestionsFromGeminiAction) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Autofill.YourSavedInfoSettingsPage.SuggestionsFromGeminiAction',
      action,
      SuggestionsFromGeminiAction.COUNT,
    ]);
    if (action !== SuggestionsFromGeminiAction.COUNT) {
      const actionMap = {
        [SuggestionsFromGeminiAction.MANAGE_CONNECTED_APPS_CLICK]:
            'PersonalContext.Settings.ManageConnectedAppsClick',
        [SuggestionsFromGeminiAction.TOGGLE_ON]:
            'PersonalContext.Settings.ToggledOn',
        [SuggestionsFromGeminiAction.TOGGLE_OFF]:
            'PersonalContext.Settings.ToggledOff',
      };
      this.recordAction(actionMap[action]);
    }
  }

  static getInstance(): MetricsBrowserProxy {
    return instance || (instance = new MetricsBrowserProxyImpl());
  }

  static setInstance(obj: MetricsBrowserProxy) {
    instance = obj;
  }
}

let instance: MetricsBrowserProxy|null = null;
