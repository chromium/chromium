// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_AGGREGATOR_H_
#define CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_AGGREGATOR_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile_statistics_common.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/user_annotations/user_annotations_service.h"
#include "device/fido/platform_credential_store.h"

class PrefService;

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace history {
class HistoryService;
}  // namespace history

class ProfileStatisticsAggregator {
  // This class is used internally by ProfileStatistics
  //
  // The class collects statistical information about the profile and returns
  // the information via a callback function. Currently bookmarks, history,
  // logins and sites with autofill forms are counted.

 public:
  ProfileStatisticsAggregator(
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
      base::OnceClosure done_callback);
  ProfileStatisticsAggregator(const ProfileStatisticsAggregator&) = delete;
  ProfileStatisticsAggregator& operator=(const ProfileStatisticsAggregator&) =
      delete;
  ~ProfileStatisticsAggregator();

  void AddCallbackAndStartAggregator(
      profiles::ProfileStatisticsCallback stats_callback);

 private:
  // Start gathering statistics. Also cancels existing statistics tasks.
  void StartAggregator();

  // Callback functions
  // Appends result to |profile_category_stats_|, and then calls
  // the external callback.
  void StatisticsCallback(const char* category, int count);

  // Callback for counters.
  void OnCounterResult(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result);

  // Registers, initializes and starts a BrowsingDataCounter.
  void AddCounter(std::unique_ptr<browsing_data::BrowsingDataCounter> counter);

  const scoped_refptr<autofill::AutofillWebDataService>
      autofill_web_data_service_;
  const raw_ptr<autofill::PersonalDataManager> personal_data_manager_;
  const raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  const raw_ptr<history::HistoryService> history_service_;
  const scoped_refptr<password_manager::PasswordStoreInterface>
      profile_password_store_;
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<user_annotations::UserAnnotationsService>
      user_annotations_service_;

  std::unique_ptr<device::fido::PlatformCredentialStore>
      platform_credential_store_;

  profiles::ProfileCategoryStats profile_category_stats_;

  // Callback function to be called when results arrive. Will be called
  // multiple times (once for each statistics).
  std::vector<profiles::ProfileStatisticsCallback> stats_callbacks_;

  // Callback function to be called when all statistics are calculated.
  base::OnceClosure done_callback_;

  std::vector<std::unique_ptr<browsing_data::BrowsingDataCounter>> counters_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_AGGREGATOR_H_
