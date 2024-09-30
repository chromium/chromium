// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_actions_history_factory.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

using EnablingMethod = QuietNotificationPermissionUiState::EnablingMethod;

// Enable the quiet UX after 3 consecutive denies in adapative activation mode.
constexpr int kConsecutiveDeniesThresholdForActivation = 3u;

constexpr char kIsQuietUiEnabledInPrefs[] =
    "Permissions.QuietNotificationPrompts.RegularProfile.IsEnabledInPrefs";
constexpr char kQuietUiEnabledStateInPrefsChangedTo[] =
    "Permissions.QuietNotificationPrompts.EnabledStateInPrefsChangedTo";

bool DidDenyLastThreeTimes(
    const std::vector<permissions::PermissionActionsHistory::Entry>&
        permission_actions) {
  size_t rolling_denies_in_a_row = 0u;
  for (const auto& entry : base::Reversed(permission_actions)) {
    switch (entry.action) {
      case permissions::PermissionAction::DENIED:
        ++rolling_denies_in_a_row;
        break;
      case permissions::PermissionAction::GRANTED:
        return false;  // Does not satisfy adaptive quiet UI activation
                       // condition.
      case permissions::PermissionAction::DISMISSED:
      case permissions::PermissionAction::IGNORED:
      case permissions::PermissionAction::REVOKED:
      default:
        // Ignored.
        break;
    }

    if (rolling_denies_in_a_row >= kConsecutiveDeniesThresholdForActivation) {
      return true;
    }
  }
  return false;
}

}  // namespace

// AdaptiveQuietNotificationPermissionUiEnabler::Factory ---------------------

// static
AdaptiveQuietNotificationPermissionUiEnabler*
AdaptiveQuietNotificationPermissionUiEnabler::Factory::GetForProfile(
    Profile* profile) {
  return static_cast<AdaptiveQuietNotificationPermissionUiEnabler*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AdaptiveQuietNotificationPermissionUiEnabler::Factory*
AdaptiveQuietNotificationPermissionUiEnabler::Factory::GetInstance() {
  return base::Singleton<
      AdaptiveQuietNotificationPermissionUiEnabler::Factory>::get();
}

AdaptiveQuietNotificationPermissionUiEnabler::Factory::Factory()
    : ProfileKeyedServiceFactory(
          "AdaptiveQuietNotificationPermissionUiEnabler",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

AdaptiveQuietNotificationPermissionUiEnabler::Factory::~Factory() = default;

std::unique_ptr<KeyedService> AdaptiveQuietNotificationPermissionUiEnabler::
    Factory::BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  return std::make_unique<AdaptiveQuietNotificationPermissionUiEnabler>(
      static_cast<Profile*>(context));
}

// AdaptiveQuietNotificationPermissionUiEnabler ------------------------------

// static
AdaptiveQuietNotificationPermissionUiEnabler*
AdaptiveQuietNotificationPermissionUiEnabler::GetForProfile(Profile* profile) {
  return AdaptiveQuietNotificationPermissionUiEnabler::Factory::GetForProfile(
      profile);
}

void AdaptiveQuietNotificationPermissionUiEnabler::PermissionPromptResolved() {
  if ((!QuietNotificationPermissionUiConfig::
           IsAdaptiveActivationDryRunEnabled() ||
       profile_->GetPrefs()->GetBoolean(
           prefs::kHadThreeConsecutiveNotificationPermissionDenies)) &&
      (!QuietNotificationPermissionUiConfig::IsAdaptiveActivationEnabled() ||
       profile_->GetPrefs()->GetBoolean(
           prefs::kEnableQuietNotificationPermissionUi))) {
    return;
  }

  // If the user has disabled the quiet UI, only count events after that
  // occurred.
  const base::Time disable_time = profile_->GetPrefs()->GetTime(
      prefs::kQuietNotificationPermissionUiDisabledTime);

  // Limit how old actions are to be taken into consideration. Default 90 days
  // old. The value could be changed based on Finch experiment but specifying >
  // 90 days is meaningless, as only 90 days worth of history is stored.
  const base::Time cutoff =
      base::Time::Now() -
      QuietNotificationPermissionUiConfig::GetAdaptiveActivationWindowSize();

  const auto actions =
      PermissionActionsHistoryFactory::GetForProfile(profile_)->GetHistory(
          std::max(cutoff, disable_time),
          permissions::RequestType::kNotifications,
          permissions::PermissionActionsHistory::EntryFilter::WANT_ALL_PROMPTS);

  if (!DidDenyLastThreeTimes(actions)) {
    return;
  }

  if (QuietNotificationPermissionUiConfig::
          IsAdaptiveActivationDryRunEnabled()) {
    profile_->GetPrefs()->SetBoolean(
        prefs::kHadThreeConsecutiveNotificationPermissionDenies,
        true /* value */);
  }

  if (QuietNotificationPermissionUiConfig::IsAdaptiveActivationEnabled() &&
      !profile_->GetPrefs()->GetBoolean(
          prefs::kEnableQuietNotificationPermissionUi)) {
    profile_->GetPrefs()->SetBoolean(
        prefs::kEnableQuietNotificationPermissionUi, true /* value */);
    // TODO(crbug.com/40156618): If
    // `kQuietNotificationPermissionShouldShowPromo` stops being a good
    // indicator as to how the quiet UI pref was enabled, remove the
    // |BackfillEnablingMethodIfMissing| logic.
    profile_->GetPrefs()->SetBoolean(
        prefs::kQuietNotificationPermissionShouldShowPromo, true /* value */);
  }
}

AdaptiveQuietNotificationPermissionUiEnabler::
    AdaptiveQuietNotificationPermissionUiEnabler(Profile* profile)
    : profile_(profile) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kEnableQuietNotificationPermissionUi,
      base::BindRepeating(
          &AdaptiveQuietNotificationPermissionUiEnabler::OnQuietUiStateChanged,
          base::Unretained(this)));

  if (profiles::IsRegularUserProfile(profile_)) {
    // Record whether the quiet UI is enabled, but only when notifications are
    // not completely blocked.
    auto* host_content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile_);
    const ContentSetting notifications_setting =
        host_content_settings_map->GetDefaultContentSetting(
            ContentSettingsType::NOTIFICATIONS, nullptr /* provider_id */);

    if (notifications_setting != CONTENT_SETTING_BLOCK) {
      const bool is_quiet_ui_enabled_in_prefs =
          profile_->GetPrefs()->GetBoolean(
              prefs::kEnableQuietNotificationPermissionUi);
      base::UmaHistogramBoolean(kIsQuietUiEnabledInPrefs,
                                is_quiet_ui_enabled_in_prefs);
    }
  }

  BackfillEnablingMethodIfMissing();
  MigrateAdaptiveNotificationQuietingToCPSS();
}

AdaptiveQuietNotificationPermissionUiEnabler::
    ~AdaptiveQuietNotificationPermissionUiEnabler() = default;

void AdaptiveQuietNotificationPermissionUiEnabler::OnQuietUiStateChanged() {
  const bool is_quiet_ui_enabled_in_prefs = profile_->GetPrefs()->GetBoolean(
      prefs::kEnableQuietNotificationPermissionUi);
  base::UmaHistogramBoolean(kQuietUiEnabledStateInPrefsChangedTo,
                            is_quiet_ui_enabled_in_prefs);

  if (is_quiet_ui_enabled_in_prefs) {
    profile_->GetPrefs()->SetInteger(
        prefs::kQuietNotificationPermissionUiEnablingMethod,
        static_cast<int>(EnablingMethod::kManual));
  } else {
    // Reset the promo state so that if the quiet UI is enabled adaptively
    // again, the promo will be shown again.
    profile_->GetPrefs()->ClearPref(
        prefs::kQuietNotificationPermissionShouldShowPromo);
    profile_->GetPrefs()->ClearPref(
        prefs::kQuietNotificationPermissionPromoWasShown);
    profile_->GetPrefs()->ClearPref(
        prefs::kQuietNotificationPermissionUiEnablingMethod);

    // If the users has just turned off the quiet UI remember the time when it
    // happened. Only actions taking place after this point will be considered
    // from now on.
    profile_->GetPrefs()->SetTime(
        prefs::kQuietNotificationPermissionUiDisabledTime, base::Time::Now());
  }
}

void AdaptiveQuietNotificationPermissionUiEnabler::
    BackfillEnablingMethodIfMissing() {
  if (QuietNotificationPermissionUiState::GetQuietUiEnablingMethod(profile_) !=
      EnablingMethod::kUnspecified) {
    return;
  }

  // `kQuietNotificationPermissionUiEnablingMethod` was not populated prior to
  // M88, but `kQuietNotificationPermissionShouldShowPromo` is a solid
  // indicator as to how the setting was enabled in the first place because
  // it's only set to true when the quiet UI has been enabled adaptively.
  const bool has_enabled_adaptively = profile_->GetPrefs()->GetBoolean(
      prefs::kQuietNotificationPermissionShouldShowPromo);

  profile_->GetPrefs()->SetInteger(
      prefs::kQuietNotificationPermissionUiEnablingMethod,
      static_cast<int>(has_enabled_adaptively ? EnablingMethod::kAdaptive
                                              : EnablingMethod::kManual));
}

void AdaptiveQuietNotificationPermissionUiEnabler::
    MigrateAdaptiveNotificationQuietingToCPSS() {
  if (profile_->GetPrefs()->GetBoolean(
          prefs::kDidMigrateAdaptiveNotifiationQuietingToCPSS)) {
    return;
  }

  const bool is_quiet_ui_enabled_in_prefs = profile_->GetPrefs()->GetBoolean(
      prefs::kEnableQuietNotificationPermissionUi);
  const EnablingMethod enabling_method =
      QuietNotificationPermissionUiState::GetQuietUiEnablingMethod(profile_);
  if (is_quiet_ui_enabled_in_prefs &&
      enabling_method == EnablingMethod::kManual) {
    profile_->GetPrefs()->SetBoolean(prefs::kEnableNotificationCPSS,
                                     /*value=*/false);
  } else {
    profile_->GetPrefs()->SetBoolean(prefs::kEnableNotificationCPSS,
                                     /*value=*/true);
    profile_->GetPrefs()->SetBoolean(
        prefs::kEnableQuietNotificationPermissionUi, /*value=*/false);
  }

  profile_->GetPrefs()->SetBoolean(
      prefs::kDidMigrateAdaptiveNotifiationQuietingToCPSS,
      /*value=*/true);
  profile_->GetPrefs()->ClearPref(
      prefs::kQuietNotificationPermissionUiEnablingMethod);
}
