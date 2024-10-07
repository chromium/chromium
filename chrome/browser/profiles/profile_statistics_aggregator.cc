// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_statistics_aggregator.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/counters/signin_data_counter.h"
#include "chrome/browser/profiles/profile_statistics_common.h"
#include "components/browsing_data/core/counters/autofill_counter.h"
#include "components/browsing_data/core/counters/bookmark_counter.h"
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "content/public/browser/browser_thread.h"

using browsing_data::BrowsingDataCounter;

ProfileStatisticsAggregator::ProfileStatisticsAggregator(
    scoped_refptr<autofill::AutofillWebDataService> autofill_web_data_service,
    autofill::PersonalDataManager* personal_data_manager,
    bookmarks::BookmarkModel* bookmark_model,
    history::HistoryService* history_service,
    scoped_refptr<password_manager::PasswordStoreInterface>
        profile_password_store,
    PrefService* pref_service,
    user_annotations::UserAnnotationsService* user_annotations_service,
    std::unique_ptr<device::fido::PlatformCredentialStore>
        platform_credential_store,
    base::OnceClosure done_callback)
    : autofill_web_data_service_(std::move(autofill_web_data_service)),
      personal_data_manager_(personal_data_manager),
      bookmark_model_(bookmark_model),
      history_service_(history_service),
      profile_password_store_(profile_password_store),
      pref_service_(pref_service),
      user_annotations_service_(user_annotations_service),
      platform_credential_store_(std::move(platform_credential_store)),
      done_callback_(std::move(done_callback)) {}

ProfileStatisticsAggregator::~ProfileStatisticsAggregator() {}

void ProfileStatisticsAggregator::AddCallbackAndStartAggregator(
    profiles::ProfileStatisticsCallback stats_callback) {
  if (stats_callback)
    stats_callbacks_.push_back(std::move(stats_callback));
  StartAggregator();
}

void ProfileStatisticsAggregator::AddCounter(
    std::unique_ptr<BrowsingDataCounter> counter) {
  counter->InitWithoutPref(
      base::Time(),
      base::BindRepeating(&ProfileStatisticsAggregator::OnCounterResult,
                          base::Unretained(this)));
  counter->Restart();
  counters_.push_back(std::move(counter));
}

void ProfileStatisticsAggregator::StartAggregator() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  profile_category_stats_.clear();

  // Cancel tasks.
  counters_.clear();

  // Initiate bookmark counting.
  AddCounter(std::make_unique<browsing_data::BookmarkCounter>(bookmark_model_));

  // Initiate history counting.
  AddCounter(std::make_unique<browsing_data::HistoryCounter>(
      history_service_,
      browsing_data::HistoryCounter::GetUpdatedWebHistoryServiceCallback(),
      /*sync_service=*/nullptr));

  // Only count local passwords.
  AddCounter(std::make_unique<browsing_data::SigninDataCounter>(
      profile_password_store_,
      /*account_store=*/nullptr, pref_service_, /*sync_service=*/nullptr,
      std::move(platform_credential_store_)));

  // Initiate autofill counting.
  AddCounter(std::make_unique<browsing_data::AutofillCounter>(
      personal_data_manager_, autofill_web_data_service_,
      user_annotations_service_,
      /*sync_service=*/nullptr));
}

void ProfileStatisticsAggregator::OnCounterResult(
    std::unique_ptr<BrowsingDataCounter::Result> result) {
  if (!result->Finished())
    return;
  const char* pref_name = result->source()->GetPrefName();
  auto* finished_result =
      static_cast<BrowsingDataCounter::FinishedResult*>(result.get());
  int count = finished_result->Value();
  if (pref_name == browsing_data::BookmarkCounter::kPrefName) {
    StatisticsCallback(profiles::kProfileStatisticsBookmarks, count);
  } else if (pref_name == browsing_data::prefs::kDeleteBrowsingHistory) {
    StatisticsCallback(profiles::kProfileStatisticsBrowsingHistory, count);
  } else if (pref_name == browsing_data::prefs::kDeletePasswords) {
    const auto* signin_result =
        static_cast<const browsing_data::SigninDataCounter::SigninDataResult*>(
            result.get());

    auto profile_passwords = signin_result->Value();
    auto signin_data_count = signin_result->WebAuthnCredentialsValue();

    StatisticsCallback(profiles::kProfileStatisticsPasswords,
                       profile_passwords + signin_data_count);
  } else if (pref_name == browsing_data::prefs::kDeleteFormData) {
    StatisticsCallback(profiles::kProfileStatisticsAutofill, count);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void ProfileStatisticsAggregator::StatisticsCallback(const char* category,
                                                     int count) {
  profiles::ProfileCategoryStat datum;
  datum.category = category;
  datum.count = count;
  profile_category_stats_.push_back(datum);
  for (const auto& stats_callback : stats_callbacks_) {
    DCHECK(stats_callback);
    stats_callback.Run(profile_category_stats_);
  }

  if (profile_category_stats_.size() ==
      profiles::kProfileStatisticsCategories.size()) {
    DCHECK(done_callback_);
    std::move(done_callback_).Run();
  }
}
