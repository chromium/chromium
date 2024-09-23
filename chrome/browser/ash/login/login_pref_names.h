// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOGIN_PREF_NAMES_H_
#define CHROME_BROWSER_ASH_LOGIN_LOGIN_PREF_NAMES_H_

namespace ash::prefs {

// *************** PROFILE PREFS ***************
// These are attached to the user profile. Here only login/oobe specific prefs
// are presented.

// Last input user method which could be used on the login/lock screens.
inline constexpr char kLastLoginInputMethod[] = "login.last_input_method";

// A boolean pref to indicate if the marketing opt-in screen in OOBE is finished
// for the user.
inline constexpr char kOobeMarketingOptInScreenFinished[] =
    "OobeMarketingOptInScreenFinished";

// Whether the user has chosen to sign up for marketing emails.
inline constexpr char kOobeMarketingOptInChoice[] = "OobeMarketingOptInChoice";

// Time when new user has finished onboarding.
inline constexpr char kOobeOnboardingTime[] = "oobe.onboarding_time";

// A boolean pref to indicate if the gamgee perk is shown in OOBE for the user.
inline constexpr char kOobePerksDiscoveryGamgeeShown[] =
    "OobePerksDiscoveryGamgeeShown";

// Indicates the amount of time for which a user authenticated against GAIA
// without SAML can use offline authentication against a cached password
// before being forced to go through online authentication against GAIA again.
// The time is expressed in days. A value of -1 indicates no limit, meaning
// that this policy will not enforce online authentication. The limit is in
// effect only if GAIA without SAML is used.
inline constexpr char kGaiaOfflineSigninTimeLimitDays[] =
    "gaia.offline_signin_time_limit";

// Indicates that consolidated consent screen was shown. Used to show new terms
// for reven board users after update from CloudReady to Flex.
// TODO(https://crbug.com/1322394): deprecate this pref once update from
// CloudReady won't be available anymore.
inline constexpr char kRevenOobeConsolidatedConsentAccepted[] =
    "RevenOobeConsolidatedConsentAccepted";

// Indicates the amount of time for which a user authenticated via SAML can use
// offline authentication against a cached password before being forced to go
// through online authentication against GAIA again. The time is expressed in
// seconds. A value of -1 indicates no limit, meaning that this policy will not
// enforce online authentication. The limit is in effect only if GAIA redirected
// the user to a SAML IdP during the last online authentication.
inline constexpr char kSAMLOfflineSigninTimeLimit[] =
    "saml.offline_signin_time_limit";

// Indicates the amount of time for which a user authenticated via GAIA
// without SAML can use offline authentication against a cached password
// before being forced to go through online authentication against GAIA again
// when logging in through the lock screen. The time is expressed in days. A
// value of -1 indicates no limit, meaning that this policy will not enforce
// online authentication. The limit is in effect only if GAIA without SAML is
// used.
inline constexpr char kGaiaLockScreenOfflineSigninTimeLimitDays[] =
    "gaia.lock_screen_offline_signin_time_limit";

// Indicates the amount of time for which a user authenticated via SAML can use
// offline authentication against a cached password before being forced to go
// through online authentication against GAIA again when logging in through the
// lock screen. The time is expressed in days. A value of -1 indicates no limit,
// meaning that this policy will not enforce online authentication. The limit is
// in effect only if GAIA redirected the user to a SAML IdP during the last
// online authentication.
inline constexpr char kSamlLockScreenOfflineSigninTimeLimitDays[] =
    "saml.lock_screen_offline_signin_time_limit";

// Enable chrome://password-change page for in-session change of SAML passwords.
// Also enables SAML password expiry notifications, if we have that information.
inline constexpr char kSamlInSessionPasswordChangeEnabled[] =
    "saml.in_session_password_change_enabled";

// The number of days in advance to notify the user that their SAML password
// will expire (works when kSamlInSessionPasswordChangeEnabled is true).
inline constexpr char kSamlPasswordExpirationAdvanceWarningDays[] =
    "saml.password_expiration_advance_warning_days";

// Enable online signin on the lock screen.
inline constexpr char kLockScreenReauthenticationEnabled[] =
    "lock_screen_reauthentication_enabled";

inline constexpr char kActivityTimeAfterOnboarding[] =
    "oobe.activity_time_after_onboarding";

// List of screens selected from the CHOOBE screen. This list is used to resume
// CHOOBE flow if it's not completed yet.
inline constexpr char kChoobeSelectedScreens[] = "oobe.choobe_selected_screens";

// List of screens completed during the CHOOBE part of the onboarding flow.
inline constexpr char kChoobeCompletedScreens[] =
    "oobe.choobe_completed_screens";

//  A boolean pref of the drive pinning screen
inline constexpr char kOobeDrivePinningEnabledDeferred[] =
    "oobe.drive_pinning_defer";

//  A double pref of the display size factor set in the display size screen.
inline constexpr char kOobeDisplaySizeFactorDeferred[] =
    "oobe.display_size_factor_defer";

// List of categories selected from the CategoriesSelection screen.
// This list is used to filter the apps in the new recommended apps screen.
inline constexpr char kOobeCategoriesSelected[] = "oobe.categories_selected";

// *************** OOBE LOCAL STATE PREFS ***************

// A boolean pref of the OOBE complete flag (first OOBE part before login).
inline constexpr char kOobeComplete[] = "OobeComplete";

// The name of the screen that has to be shown if OOBE has been interrupted.
inline constexpr char kOobeScreenPending[] = "OobeScreenPending";

// A time pref stored before the first time pre-login OOBE starts.
inline constexpr char kOobeStartTime[] = "oobe.oobe_start_time";

// Boolean pref to hold guest metrics consent captured during guest OOBE. Guest
// OOBE should only be triggered for guest sessions without a device owner. This
// pref is used to hold that consent across browser restart.
inline constexpr char kOobeGuestMetricsEnabled[] = "oobe.guest_metrics_enabled";

// Indicates that the reven board was updated from CloudReady to Flex.
// TODO(https://crbug.com/1322394): deprecate this pref once update from
// CloudReady won't be available anymore.
inline constexpr char kOobeRevenUpdatedToFlex[] = "OobeRevenUpdatedToFlex";

// This pref should be true if there was a language change from the UI,
// it's value will be written into the OOBE.WelcomeScreen.UserChangedLocale
// metric when we exit the WelcomeScreen.
inline constexpr char kOobeLocaleChangedOnWelcomeScreen[] =
    "OobeLocaleChangedOnWelcomeScreen";

// A boolean pref indicate if the critical update in OOBE applied.
inline constexpr char kOobeCriticalUpdateCompleted[] =
    "OobeCriticalUpdateCompleted";

// A boolean pref indicate if the user is a consumer in OOBE.
// this is used by the update_engine to allow non-critical update during OOBE.
inline constexpr char kOobeIsConsumerSegment[] = "IsConsumerSegment";

// A boolean pref indicate if the consumer update in OOBE applied.
inline constexpr char kOobeConsumerUpdateCompleted[] =
    "OobeConsumerUpdateCompleted";

// The name of the screen that has to be shown after resume from
// consumerUpdateScreen in OOBE.
inline constexpr char kOobeScreenAfterConsumerUpdate[] =
    "OobeScreenAfterConsumerUpdate";

// A string pref containing url parameter name which can be used on SAML IdP web
// page to autofill the username field.
inline constexpr char kUrlParameterToAutofillSAMLUsername[] =
    "saml.UrlParameterToAutofillSAMLUsername";

// A string pref containing the initial metrics client ID at the start of OOBE
// to be later compared with the ID at the end of OOBE. This will determine
// whether the ID was reset during OOBE or the first onboarding experience.
// This pref is cleared before the first session starts.
inline constexpr char kOobeMetricsClientIdAtOobeStart[] =
    "OobeMetricsClientIdAtOobeStart";

// A boolean pref that indicates if `StatsReportingController` ever reported the
// status of metrics to be enabled during OOBE. This pref is only updated during
// pre-login OOBE or the first onboarding experience and cleared before the
// first session starts.
inline constexpr char kOobeMetricsReportedAsEnabled[] =
    "OobeMetricsReportedEnabled";

// A boolean pref that indicates if `StatsReportingController` ever reported a
// switch from enabled to disabled during OOBE. This pref is only updated during
// pre-login OOBE or the first onboarding experience and cleared before the
// first session start.
inline constexpr char kOobeStatsReportingControllerReportedReset[] =
    "OobeStatsReportingControllerReportedReset";

// Time interval (in minutes) by which the user's authentication flow should
// automatically be reloaded. Policy name:
// `DeviceAuthenticationFlowAutoReloadInterval`.
inline constexpr char kAuthenticationFlowAutoReloadInterval[] =
    "AuthenticationFlowAutoReloadInterval";

}  // namespace ash::prefs

#endif  // CHROME_BROWSER_ASH_LOGIN_LOGIN_PREF_NAMES_H_
