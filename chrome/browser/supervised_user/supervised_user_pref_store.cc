// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_pref_store.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/safe_search_util.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/prefs/pref_value_map.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "extensions/buildflags/buildflags.h"

namespace {

struct SupervisedUserSettingsPrefMappingEntry {
  const char* settings_name;
  const char* pref_name;
};

SupervisedUserSettingsPrefMappingEntry kSupervisedUserSettingsPrefMapping[] = {
    {
        supervised_users::kContentPackDefaultFilteringBehavior,
        prefs::kDefaultSupervisedUserFilteringBehavior,
    },
    {
        supervised_users::kContentPackManualBehaviorHosts,
        prefs::kSupervisedUserManualHosts,
    },
    {
        supervised_users::kContentPackManualBehaviorURLs,
        prefs::kSupervisedUserManualURLs,
    },
    {
        supervised_users::kForceSafeSearch,
        prefs::kForceGoogleSafeSearch,
    },
    {
        supervised_users::kSafeSitesEnabled,
        prefs::kSupervisedUserSafeSites,
    },
    {
        supervised_users::kSigninAllowed,
        prefs::kSigninAllowed,
    },
    {
        supervised_users::kUserName,
        prefs::kProfileName,
    },
};

}  // namespace

SupervisedUserPrefStore::SupervisedUserPrefStore(
    SupervisedUserSettingsService* supervised_user_settings_service) {
  user_settings_subscription_ =
      supervised_user_settings_service->SubscribeForSettingsChange(
          base::BindRepeating(&SupervisedUserPrefStore::OnNewSettingsAvailable,
                              base::Unretained(this)));

  // The SupervisedUserSettingsService must be created before the PrefStore, and
  // it will notify the PrefStore to destroy both subscriptions when it is shut
  // down.
  shutdown_subscription_ =
      supervised_user_settings_service->SubscribeForShutdown(
          base::BindRepeating(
              &SupervisedUserPrefStore::OnSettingsServiceShutdown,
              base::Unretained(this)));
}

bool SupervisedUserPrefStore::GetValue(base::StringPiece key,
                                       const base::Value** value) const {
  return prefs_->GetValue(key, value);
}

base::Value::Dict SupervisedUserPrefStore::GetValues() const {
  return prefs_->AsDict();
}

void SupervisedUserPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void SupervisedUserPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool SupervisedUserPrefStore::HasObservers() const {
  return !observers_.empty();
}

bool SupervisedUserPrefStore::IsInitializationComplete() const {
  return !!prefs_;
}

SupervisedUserPrefStore::~SupervisedUserPrefStore() {
}

void SupervisedUserPrefStore::OnNewSettingsAvailable(
    const base::Value::Dict& settings) {
  std::unique_ptr<PrefValueMap> old_prefs = std::move(prefs_);
  prefs_ = std::make_unique<PrefValueMap>();
  if (!settings.empty()) {
    // Set hardcoded prefs and defaults.
    prefs_->SetInteger(prefs::kDefaultSupervisedUserFilteringBehavior,
                       SupervisedUserURLFilter::ALLOW);
    prefs_->SetBoolean(prefs::kForceGoogleSafeSearch, true);
    prefs_->SetInteger(prefs::kForceYouTubeRestrict,
                       safe_search_util::YOUTUBE_RESTRICT_MODERATE);
    prefs_->SetBoolean(prefs::kHideWebStoreIcon, false);
    prefs_->SetBoolean(prefs::kSigninAllowed, false);
    prefs_->SetBoolean(feed::prefs::kEnableSnippets, false);

#if BUILDFLAG(IS_ANDROID)
    prefs_->SetBoolean(autofill::prefs::kAutofillWalletImportEnabled, false);
#endif

    // Copy supervised user settings to prefs.
    for (const auto& entry : kSupervisedUserSettingsPrefMapping) {
      const base::Value* value = settings.Find(entry.settings_name);
      if (value)
        prefs_->SetValue(entry.pref_name, value->Clone());
    }

    // Manually set preferences that aren't direct copies of the settings value.
    {
      // Allow history deletion for supervised accounts on supported platforms.
      bool allow_history_deletion = base::FeatureList::IsEnabled(
          supervised_users::kAllowHistoryDeletionForChildAccounts);
      prefs_->SetBoolean(prefs::kAllowDeletingBrowserHistory,
                         allow_history_deletion);
      // Incognito is disabled for supervised users across platforms.
      // First-party sites use signed-in cookies to ensure that parental
      // restrictions are applied for Unicorn accounts.
      prefs_->SetInteger(
          prefs::kIncognitoModeAvailability,
          static_cast<int>(IncognitoModePrefs::Availability::kDisabled));
    }

    {
      // Note that |prefs::kForceGoogleSafeSearch| is set automatically as part
      // of |kSupervisedUserSettingsPrefMapping|, but this can't be done for
      // |prefs::kForceYouTubeRestrict| because it is an int, not a bool.
      bool force_safe_search =
          settings.FindBool(supervised_users::kForceSafeSearch).value_or(true);
      prefs_->SetInteger(
          prefs::kForceYouTubeRestrict,
          force_safe_search ? safe_search_util::YOUTUBE_RESTRICT_MODERATE
                            : safe_search_util::YOUTUBE_RESTRICT_OFF);
    }

#if BUILDFLAG(ENABLE_EXTENSIONS)
    {
      // TODO(crbug/1024646): Update Kids Management server to set a new bit for
      // extension permissions. Until then, rely on other side effects of the
      // "Permissions for sites, apps and extensions" setting, like geolocation
      // being disallowed.
      bool permissions_disallowed =
          settings.FindBool(supervised_users::kGeolocationDisabled)
              .value_or(true);
      prefs_->SetBoolean(prefs::kSupervisedUserExtensionsMayRequestPermissions,
                         !permissions_disallowed);
      base::UmaHistogramBoolean(
          "SupervisedUsers.ExtensionsMayRequestPermissions",
          !permissions_disallowed);
    }
#endif
  }

  if (!old_prefs) {
    for (Observer& observer : observers_)
      observer.OnInitializationCompleted(true);
    return;
  }

  std::vector<std::string> changed_prefs;
  prefs_->GetDifferingKeys(old_prefs.get(), &changed_prefs);

  // Send out change notifications.
  for (const std::string& pref : changed_prefs) {
    for (Observer& observer : observers_)
      observer.OnPrefValueChanged(pref);
  }
}

void SupervisedUserPrefStore::OnSettingsServiceShutdown() {
  user_settings_subscription_ = {};
  shutdown_subscription_ = {};
}
