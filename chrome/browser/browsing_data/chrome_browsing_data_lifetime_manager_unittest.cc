// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/core/features.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/pref_names.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/browsing_data_remover_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_browsing_data_remover_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

TEST(ChromeBrowsingDataLifetimeManager, ScheduledRemoval) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      browsing_data::features::kEnableBrowsingDataLifetimeManager);
  content::BrowserTaskEnvironment browser_task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile::Builder builder;
  builder.SetIsNewProfile(true);
  auto testing_profile = builder.Build();
  testing_profile->GetPrefs()->Set(syncer::prefs::kSyncManaged,
                                   base::Value(true));

  content::MockBrowsingDataRemoverDelegate delegate;
  auto* remover = testing_profile->GetBrowsingDataRemover();
  remover->SetEmbedderDelegate(&delegate);
  static constexpr char kPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":["cached_images_and_files",
      "site_settings"]}, {"time_to_live_in_hours": 2, "data_types":
      ["cookies_and_other_site_data", "hosted_app_data"]},
      {"time_to_live_in_hours": 3, "data_types":["browsing_history",
      "download_history", "password_signin", "autofill"]}])";
  uint64_t remove_mask_1_filterable =
      content::BrowsingDataRemover::DATA_TYPE_CACHE;
  uint64_t remove_mask_1_unfilterable =
      chrome_browsing_data_remover::DATA_TYPE_CONTENT_SETTINGS;
  uint64_t remove_mask_2 = chrome_browsing_data_remover::DATA_TYPE_SITE_DATA;
  uint64_t origin_mask_2 =
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
      content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
  uint64_t remove_mask_3_filterable =
      content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS;
  uint64_t remove_mask_3_unfilterable =
      chrome_browsing_data_remover::DATA_TYPE_HISTORY |
      chrome_browsing_data_remover::DATA_TYPE_PASSWORDS |
      chrome_browsing_data_remover::DATA_TYPE_FORM_DATA;
  // Each scheduled removal is called once the prefs are set.
  delegate.ExpectCallDontCareAboutFilterBuilder(
      base::Time::Min(), base::Time::Now() - base::Hours(1),
      remove_mask_1_filterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      base::Time::Min(), base::Time::Now() - base::Hours(1),
      remove_mask_1_unfilterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      base::Time::Min(), base::Time::Now() - base::Hours(2), remove_mask_2,
      origin_mask_2);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      base::Time::Min(), base::Time::Now() - base::Hours(3),
      remove_mask_3_filterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      base::Time::Min(), base::Time::Now() - base::Hours(3),
      remove_mask_3_unfilterable, 0);

  testing_profile->GetPrefs()->Set(browsing_data::prefs::kBrowsingDataLifetime,
                                   *base::JSONReader::Read(kPref));
  browser_task_environment.RunUntilIdle();
  delegate.VerifyAndClearExpectations();
  // Each scheduled removal is called once every hour.
  delegate.ExpectCallDontCareAboutFilterBuilder(
      base::Time::Min(), base::Time::Now(), remove_mask_1_filterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      base::Time::Min(), base::Time::Now(), remove_mask_1_unfilterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      base::Time::Min(), base::Time::Now() - base::Hours(1), remove_mask_2,
      origin_mask_2);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      base::Time::Min(), base::Time::Now() - base::Hours(2),
      remove_mask_3_filterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      base::Time::Min(), base::Time::Now() - base::Hours(2),
      remove_mask_3_unfilterable, 0);
  browser_task_environment.FastForwardBy(base::Hours(1));
  delegate.VerifyAndClearExpectations();
}

TEST(ChromeBrowsingDataLifetimeManager, ScheduledRemovalWithSync) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      browsing_data::features::kEnableBrowsingDataLifetimeManager);
  content::BrowserTaskEnvironment browser_task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile::Builder builder;
  builder.SetIsNewProfile(true);
  auto testing_profile = builder.Build();

  content::MockBrowsingDataRemoverDelegate delegate;
  auto* remover = testing_profile->GetBrowsingDataRemover();
  remover->SetEmbedderDelegate(&delegate);
  static constexpr char kPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":["cached_images_and_files",
      "site_settings"]}, {"time_to_live_in_hours": 2, "data_types":
      ["cookies_and_other_site_data", "hosted_app_data"]}])";
  uint64_t remove_mask_1_filterable =
      content::BrowsingDataRemover::DATA_TYPE_CACHE;
  uint64_t remove_mask_1_unfilterable =
      chrome_browsing_data_remover::DATA_TYPE_CONTENT_SETTINGS;
  uint64_t remove_mask_2 = chrome_browsing_data_remover::DATA_TYPE_SITE_DATA;
  uint64_t origin_mask_2 =
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
      content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;

  // Sync is enabled, so no deletion should be made.
  testing_profile->GetPrefs()->Set(browsing_data::prefs::kBrowsingDataLifetime,
                                   *base::JSONReader::Read(kPref));
  browser_task_environment.RunUntilIdle();
  delegate.VerifyAndClearExpectations();

  // If sync gets disabled, the scheduled deletions should proceed as usual.
  testing_profile->GetPrefs()->Set(syncer::prefs::kSyncManaged,
                                   base::Value(true));

  // Each scheduled removal is called once every lowest time_to_live_in_hours,
  // ere every 1 hour.
  delegate.ExpectCallDontCareAboutFilterBuilder(
      base::Time::Min(), base::Time::Now(), remove_mask_1_filterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      base::Time::Min(), base::Time::Now(), remove_mask_1_unfilterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      base::Time::Min(), base::Time::Now() - base::Hours(1), remove_mask_2,
      origin_mask_2);
  browser_task_environment.FastForwardBy(base::Hours(1));
  delegate.VerifyAndClearExpectations();
}
