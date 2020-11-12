// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "base/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr int kInitialCleanupDelayInMinutes = 2;

using ScheduledRemovalSettings =
    ChromeBrowsingDataLifetimeManager::ScheduledRemovalSettings;

// An observer of all the browsing data removal tasks that are started by the
// ChromeBrowsingDataLifetimeManager that records the the tasks starts and
// completed states as well as their durations.
class BrowsingDataLifetimeManagerRemoverObserver
    : public content::BrowsingDataRemover::Observer {
 public:
  ~BrowsingDataLifetimeManagerRemoverObserver() override = default;

  // Creates an instance of BrowsingDataLifetimeManagerRemoverObserver that
  // manages its own lifetime. The instance will be deleted after
  // |OnBrowsingDataRemoverDone| is called.
  static content::BrowsingDataRemover::Observer* Create(
      content::BrowsingDataRemover* remover,
      bool filterable_deletion) {
    return new BrowsingDataLifetimeManagerRemoverObserver(remover,
                                                          filterable_deletion);
  }

  // content::BrowsingDataRemover::Observer:
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override {
    base::UmaHistogramMediumTimes(duration_histogram(),
                                  base::TimeTicks::Now() - start_time_);
    base::UmaHistogramBoolean(state_histogram(),
                              /*BooleanStartedCompleted.Completed*/ true);
    delete this;
  }

 private:
  BrowsingDataLifetimeManagerRemoverObserver(
      content::BrowsingDataRemover* remover,
      bool filterable_deletion)
      : start_time_(base::TimeTicks::Now()),
        filterable_deletion_(filterable_deletion) {
    browsing_data_remover_observer_.Observe(remover);
    base::UmaHistogramBoolean(state_histogram(),
                              /*BooleanStartedCompleted.Started*/ false);
  }

  const char* duration_histogram() const {
    static constexpr char kDurationScheduledFilterableDeletion[] =
        "History.BrowsingDataLifetime.Duration.ScheduledFilterableDeletion";
    static constexpr char kDurationScheduledUnfilterableDeletion[] =
        "History.BrowsingDataLifetime.Duration.ScheduledUnfilterableDeletion";
    return filterable_deletion_ ? kDurationScheduledFilterableDeletion
                                : kDurationScheduledUnfilterableDeletion;
  }

  const char* state_histogram() const {
    static constexpr char kStateScheduledFilterableDeletion[] =
        "History.BrowsingDataLifetime.State.ScheduledFilterableDeletion";
    static constexpr char kStateScheduledUnfilterableDeletion[] =
        "History.BrowsingDataLifetime.State.ScheduledUnfilterableDeletion";
    return filterable_deletion_ ? kStateScheduledFilterableDeletion
                                : kStateScheduledUnfilterableDeletion;
  }

  base::ScopedObservation<content::BrowsingDataRemover,
                          content::BrowsingDataRemover::Observer>
      browsing_data_remover_observer_{this};
  const base::TimeTicks start_time_;
  const bool filterable_deletion_;
};

uint64_t GetOriginTypeMask(const base::Value& data_types) {
  uint64_t result = 0;
  for (const auto& data_type : data_types.GetList()) {
    std::string data_type_str = data_type.GetString();
    if (data_type_str ==
        browsing_data::policy_data_types::kCookiesAndOtherSiteData) {
      result |= content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;
    } else if (data_type_str ==
               browsing_data::policy_data_types::kHostedAppData) {
      result |= content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
    }
  }
  return result;
}

uint64_t GetRemoveMask(const base::Value& data_types) {
  uint64_t result = 0;
  for (const auto& data_type : data_types.GetList()) {
    std::string data_type_str = data_type.GetString();
    if (data_type_str == browsing_data::policy_data_types::kBrowsingHistory) {
      result |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY;
    } else if (data_type_str ==
               browsing_data::policy_data_types::kDownloadHistory) {
      result |= content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS;
    } else if (data_type_str ==
               browsing_data::policy_data_types::kCookiesAndOtherSiteData) {
      result |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA;
    } else if (data_type_str ==
               browsing_data::policy_data_types::kCachedImagesAndFiles) {
      result |= content::BrowsingDataRemover::DATA_TYPE_CACHE;
    } else if (data_type_str ==
               browsing_data::policy_data_types::kPasswordSignin) {
      result |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS;
    } else if (data_type_str == browsing_data::policy_data_types::kAutofill) {
      result |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA;
    } else if (data_type_str ==
               browsing_data::policy_data_types::kSiteSettings) {
      result |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_CONTENT_SETTINGS;
    } else if (data_type_str ==
               browsing_data::policy_data_types::kHostedAppData) {
      result |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA;
    }
  }
  return result;
}

std::vector<ScheduledRemovalSettings> ConvertToScheduledRemovalSettings(
    const base::Value* browsing_data_settings) {
  std::vector<ScheduledRemovalSettings> scheduled_removals_settings;
  if (!browsing_data_settings)
    return scheduled_removals_settings;
  for (const auto& setting : browsing_data_settings->GetList()) {
    const auto* data_types =
        setting.FindListKey(browsing_data::policy_fields::kDataTypes);
    const auto time_to_live_in_hours =
        setting.FindIntKey(browsing_data::policy_fields::kTimeToLiveInHours);
    scheduled_removals_settings.push_back({GetRemoveMask(*data_types),
                                           GetOriginTypeMask(*data_types),
                                           *time_to_live_in_hours});
  }
  return scheduled_removals_settings;
}

base::flat_set<GURL> GetOpenedUrls(Profile* profile) {
  base::flat_set<GURL> result;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile) {
      continue;
    }
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
      result.insert(browser->tab_strip_model()->GetWebContentsAt(0)->GetURL());
    }
  }
  return result;
}

}  // namespace

namespace browsing_data {

namespace policy_data_types {

const char kBrowsingHistory[] = "browsing_history";
const char kDownloadHistory[] = "download_history";
const char kCookiesAndOtherSiteData[] = "cookies_and_other_site_data";
const char kCachedImagesAndFiles[] = "cached_images_and_files";
const char kPasswordSignin[] = "password_signin";
const char kAutofill[] = "autofill";
const char kSiteSettings[] = "site_settings";
const char kHostedAppData[] = "hosted_app_data";

}  // namespace policy_data_types

namespace policy_fields {

const char kTimeToLiveInHours[] = "time_to_live_in_hours";
const char kDataTypes[] = "data_types";

}  // namespace policy_fields
}  // namespace browsing_data

ChromeBrowsingDataLifetimeManager::ChromeBrowsingDataLifetimeManager(
    content::BrowserContext* browser_context)
    : profile_(Profile::FromBrowserContext(browser_context)) {
  DCHECK(!profile_->IsGuestSession() || profile_->IsOffTheRecord());
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      browsing_data::prefs::kBrowsingDataLifetime,
      base::BindRepeating(
          &ChromeBrowsingDataLifetimeManager::UpdateScheduledRemovalSettings,
          base::Unretained(this)));

  // When the service is instantiated, wait a few minutes after Chrome startup
  // to start deleting data.
  content::GetUIThreadTaskRunner(
      {
          base::TaskPriority::BEST_EFFORT,
          base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
      })
      ->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ChromeBrowsingDataLifetimeManager::
                             UpdateScheduledRemovalSettings,
                         weak_ptr_factory_.GetWeakPtr()),
          base::TimeDelta::FromMinutes(kInitialCleanupDelayInMinutes));
}

ChromeBrowsingDataLifetimeManager::~ChromeBrowsingDataLifetimeManager() =
    default;

void ChromeBrowsingDataLifetimeManager::Shutdown() {
  pref_change_registrar_.RemoveAll();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ChromeBrowsingDataLifetimeManager::UpdateScheduledRemovalSettings() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  scheduled_removals_settings_ =
      ConvertToScheduledRemovalSettings(profile_->GetPrefs()->GetList(
          browsing_data::prefs::kBrowsingDataLifetime));

  if (!scheduled_removals_settings_.empty())
    StartScheduledBrowsingDataRemoval();
}

void ChromeBrowsingDataLifetimeManager::StartScheduledBrowsingDataRemoval() {
  bool sync_enabled = ProfileSyncServiceFactory::IsSyncAllowed(profile_);

  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(profile_);

  int smallest_time_to_live = std::numeric_limits<int>::max();
  for (const auto& removal_settings : scheduled_removals_settings_) {
    if (removal_settings.time_to_live_in_hours <= 0)
      continue;

    auto deletion_end_time = end_time_for_testing_.value_or(
        base::Time::Now() -
        base::TimeDelta::FromHours(removal_settings.time_to_live_in_hours));
    auto filterable_remove_mask =
        removal_settings.remove_mask &
        ChromeBrowsingDataRemoverDelegate::FILTERABLE_DATA_TYPES;
    if (filterable_remove_mask && sync_enabled) {
      auto filter_builder = content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::Mode::kPreserve);
      for (const auto& url : GetOpenedUrls(profile_)) {
        filter_builder->AddRegisterableDomain(url.spec());
      }
      remover->RemoveWithFilterAndReply(
          base::Time::Min(), deletion_end_time, filterable_remove_mask,
          removal_settings.origin_type_mask, std::move(filter_builder),
          testing_data_remover_observer_
              ? testing_data_remover_observer_
              : BrowsingDataLifetimeManagerRemoverObserver::Create(
                    remover, /*filterable_deletion=*/true));
    }

    auto unfilterable_remove_mask =
        removal_settings.remove_mask &
        ~ChromeBrowsingDataRemoverDelegate::FILTERABLE_DATA_TYPES;
    if (unfilterable_remove_mask && sync_enabled) {
      remover->RemoveAndReply(
          base::Time::Min(), deletion_end_time, unfilterable_remove_mask,
          removal_settings.origin_type_mask,
          testing_data_remover_observer_
              ? testing_data_remover_observer_
              : BrowsingDataLifetimeManagerRemoverObserver::Create(
                    remover, /*filterable_deletion=*/false));
    }

    smallest_time_to_live =
        std::min(removal_settings.time_to_live_in_hours, smallest_time_to_live);
  }
  if (smallest_time_to_live < std::numeric_limits<int>::max()) {
    content::GetUIThreadTaskRunner(
        {
            base::TaskPriority::BEST_EFFORT,
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
        })
        ->PostDelayedTask(FROM_HERE,
                          base::BindOnce(&ChromeBrowsingDataLifetimeManager::
                                             StartScheduledBrowsingDataRemoval,
                                         weak_ptr_factory_.GetWeakPtr()),
                          base::TimeDelta::FromHours(smallest_time_to_live));
  }
}
