// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_pref_store.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/safe_search_util.h"
#include "chrome/common/pref_names.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/prefs/pref_value_map.h"
#include "components/signin/public/base/signin_pref_names.h"
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

bool SupervisedUserPrefStore::GetValue(const std::string& key,
                                       const base::Value** value) const {
  return prefs_->GetValue(key, value);
}

std::unique_ptr<base::DictionaryValue> SupervisedUserPrefStore::GetValues()
    const {
  return prefs_->AsDictionaryValue();
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
    const base::DictionaryValue* settings) {
  std::unique_ptr<PrefValueMap> old_prefs = std::move(prefs_);
  prefs_.reset(new PrefValueMap);
  if (settings) {
    // Set hardcoded prefs and defaults.
    prefs_->SetInteger(prefs::kDefaultSupervisedUserFilteringBehavior,
                       SupervisedUserURLFilter::ALLOW);
    prefs_->SetBoolean(prefs::kForceGoogleSafeSearch, true);
    prefs_->SetInteger(prefs::kForceYouTubeRestrict,
                       safe_search_util::YOUTUBE_RESTRICT_MODERATE);
    prefs_->SetBoolean(prefs::kHideWebStoreIcon, false);
    prefs_->SetBoolean(prefs::kSigninAllowed, false);
    prefs_->SetBoolean(feed::prefs::kEnableSnippets, false);

    // Copy supervised user settings to prefs.
    for (const auto& entry : kSupervisedUserSettingsPrefMapping) {
      const base::Value* value = NULL;
      if (settings->GetWithoutPathExpansion(entry.settings_name, &value))
        prefs_->SetValue(entry.pref_name, value->Clone());
    }

    // Manually set preferences that aren't direct copies of the settings value.
    {
      bool record_history = true;
      settings->GetBoolean(supervised_users::kRecordHistory, &record_history);
      prefs_->SetBoolean(prefs::kAllowDeletingBrowserHistory, !record_history);
      prefs_->SetInteger(prefs::kIncognitoModeAvailability,
                         record_history ? IncognitoModePrefs::DISABLED
                                        : IncognitoModePrefs::ENABLED);
    }

    {
      // Note that |prefs::kForceGoogleSafeSearch| is set automatically as part
      // of |kSupervisedUserSettingsPrefMapping|, but this can't be done for
      // |prefs::kForceYouTubeRestrict| because it is an int, not a bool.
      bool force_safe_search = true;
      settings->GetBoolean(supervised_users::kForceSafeSearch,
                           &force_safe_search);
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
      bool permissions_disallowed = true;
      settings->GetBoolean(supervised_users::kGeolocationDisabled,
                           &permissions_disallowed);
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
