// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/adaptive_notification_permission_ui_selector.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/stl_util.h"
#include "base/time/default_clock.h"
#include "base/util/values/values_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/permissions/permission_features.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

// Preference storing whether the quiet UX is enabled.
constexpr char kEnableQuietNotificationPermissionUiPrefPath[] =
    "profile.content_settings.enable_quiet_permission_ui.notifications";

// Enable the quiet UX after 3 consecutive denies in adapative activation mode.
constexpr int kConsecutiveDeniesThresholdForActivation = 3u;

// Preference containing a list of past permission actions. This is a JSON list
// with the format:
//
//   "profile.content_settings.permission_actions.notifications": [
//     { "time": "1333333333337", "action": 1 },
//     { "time": "1567957177000", "action": 3 },
//     ...
//   ]
//
constexpr char kNotificationPermissionActionsPrefPath[] =
    "profile.content_settings.permission_actions.notifications";
constexpr char kPermissionActionEntryActionKey[] = "action";
constexpr char kPermissionActionEntryTimestampKey[] = "time";

// Entries in permission actions expire after they become this old.
constexpr base::TimeDelta kPermissionActionMaxAge =
    base::TimeDelta::FromDays(90);

}  // namespace

// AdaptiveNotificationPermissionUiSelector::Factory --------------------------

// static
AdaptiveNotificationPermissionUiSelector*
AdaptiveNotificationPermissionUiSelector::Factory::GetForProfile(
    Profile* profile) {
  return static_cast<AdaptiveNotificationPermissionUiSelector*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AdaptiveNotificationPermissionUiSelector::Factory*
AdaptiveNotificationPermissionUiSelector::Factory::GetInstance() {
  return base::Singleton<
      AdaptiveNotificationPermissionUiSelector::Factory>::get();
}

AdaptiveNotificationPermissionUiSelector::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "AdaptiveNotificationPermissionUiSelector",
          BrowserContextDependencyManager::GetInstance()) {}

AdaptiveNotificationPermissionUiSelector::Factory::~Factory() {}

KeyedService*
AdaptiveNotificationPermissionUiSelector::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AdaptiveNotificationPermissionUiSelector(
      static_cast<Profile*>(context));
}

content::BrowserContext*
AdaptiveNotificationPermissionUiSelector::Factory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

// AdaptiveNotificationPermissionUiSelector -----------------------------------

AdaptiveNotificationPermissionUiSelector::
    AdaptiveNotificationPermissionUiSelector(Profile* profile)
    : profile_(profile), clock_(base::DefaultClock::GetInstance()) {}

AdaptiveNotificationPermissionUiSelector::
    ~AdaptiveNotificationPermissionUiSelector() = default;

// static
AdaptiveNotificationPermissionUiSelector*
AdaptiveNotificationPermissionUiSelector::GetForProfile(Profile* profile) {
  return AdaptiveNotificationPermissionUiSelector::Factory::GetForProfile(
      profile);
}

// static
void AdaptiveNotificationPermissionUiSelector::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kNotificationPermissionActionsPrefPath,
                             PrefRegistry::LOSSY_PREF);
  // TODO(crbug.com/1001857): Consider making this syncable.
  registry->RegisterBooleanPref(kEnableQuietNotificationPermissionUiPrefPath,
                                false /* default_value */);
}

bool AdaptiveNotificationPermissionUiSelector::ShouldShowQuietUi() {
  if (should_show_quiet_ui_.has_value())
    return should_show_quiet_ui_.value();

  if (QuietNotificationsPromptConfig::GetActivation() ==
      QuietNotificationsPromptConfig::Activation::kNever) {
    return false;
  }

  if (QuietNotificationsPromptConfig::GetActivation() ==
      QuietNotificationsPromptConfig::Activation::kAlways) {
    return true;
  }

  DCHECK_EQ(QuietNotificationsPromptConfig::GetActivation(),
            QuietNotificationsPromptConfig::Activation::kAdaptive);
  return profile_->GetPrefs()->GetBoolean(
      kEnableQuietNotificationPermissionUiPrefPath);
}

void AdaptiveNotificationPermissionUiSelector::DisableQuietUi() {
  profile_->GetPrefs()->ClearPref(kEnableQuietNotificationPermissionUiPrefPath);

  // Clear interaction history so that if we are in adaptive mode, and the
  // triggering conditions are met, we won't turn it back on immediately.
  ClearInteractionHistory(base::Time(), base::Time::Max());
}

void AdaptiveNotificationPermissionUiSelector::RecordPermissionPromptOutcome(
    PermissionAction action) {
  ListPrefUpdate update(profile_->GetPrefs(),
                        kNotificationPermissionActionsPrefPath);
  base::Value::ListStorage& permission_actions = update.Get()->GetList();

  // Discard permission actions older than |kPermissionActionMaxAge|.
  const base::Time cutoff = clock_->Now() - kPermissionActionMaxAge;
  update->EraseListValueIf([cutoff](const base::Value& entry) {
    const base::Optional<base::Time> timestamp =
        util::ValueToTime(entry.FindKey(kPermissionActionEntryTimestampKey));
    return !timestamp || *timestamp < cutoff;
  });

  // Record the new permission action.
  base::Value::DictStorage new_action_attributes;
  new_action_attributes.emplace(
      kPermissionActionEntryTimestampKey,
      std::make_unique<base::Value>(util::TimeToValue(clock_->Now())));
  new_action_attributes.emplace(
      kPermissionActionEntryActionKey,
      std::make_unique<base::Value>(static_cast<int>(action)));
  permission_actions.emplace_back(std::move(new_action_attributes));

  // Turn on quiet UX if adapative activation is enabled and the user has three
  // denies (ignoring dismisses/ignores) in a row.
  if (QuietNotificationsPromptConfig::GetActivation() ==
          QuietNotificationsPromptConfig::Activation::kAdaptive &&
      !profile_->GetPrefs()->GetBoolean(
          kEnableQuietNotificationPermissionUiPrefPath)) {
    size_t rolling_denies_in_a_row = 0u;
    bool recently_accepted_prompt = false;

    for (auto it = permission_actions.rbegin(); it != permission_actions.rend();
         ++it) {
      const base::Optional<int> past_action_as_int =
          it->FindIntKey(kPermissionActionEntryActionKey);
      DCHECK(past_action_as_int);

      const PermissionAction past_action =
          static_cast<PermissionAction>(*past_action_as_int);

      switch (past_action) {
        case PermissionAction::DENIED:
          ++rolling_denies_in_a_row;
          break;
        case PermissionAction::GRANTED:
          recently_accepted_prompt = true;
          break;
        case PermissionAction::DISMISSED:
        case PermissionAction::IGNORED:
        case PermissionAction::REVOKED:
        default:
          // Ignored.
          break;
      }

      if (rolling_denies_in_a_row >= kConsecutiveDeniesThresholdForActivation) {
        profile_->GetPrefs()->SetBoolean(
            kEnableQuietNotificationPermissionUiPrefPath, true /* value */);
        break;
      }

      if (recently_accepted_prompt)
        break;
    }
  }
}

void AdaptiveNotificationPermissionUiSelector::ClearInteractionHistory(
    const base::Time& delete_begin,
    const base::Time& delete_end) {
  DCHECK(!delete_end.is_null());

  if (delete_begin.is_null() && delete_end.is_max()) {
    profile_->GetPrefs()->ClearPref(kNotificationPermissionActionsPrefPath);
    return;
  }

  ListPrefUpdate update(profile_->GetPrefs(),
                        kNotificationPermissionActionsPrefPath);

  update->EraseListValueIf([delete_begin, delete_end](const auto& entry) {
    const base::Optional<base::Time> timestamp =
        util::ValueToTime(entry.FindKey(kPermissionActionEntryTimestampKey));
    return (!timestamp ||
            (*timestamp >= delete_begin && *timestamp < delete_end));
  });
}
