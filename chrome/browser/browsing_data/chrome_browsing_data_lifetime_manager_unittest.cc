// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager.h"

#include <memory>
#include <optional>

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync/test/test_sync_user_settings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/browsing_data_remover_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_browsing_data_remover_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
std::unique_ptr<KeyedService> CreateTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}
}  // namespace

TEST(ChromeBrowsingDataLifetimeManager, ScheduledRemovalWithSyncDisabled) {
  content::BrowserTaskEnvironment browser_task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile::Builder builder;
  builder.SetIsNewProfile(true);
  auto testing_profile = builder.Build();
  testing_profile->GetPrefs()->Set(syncer::prefs::internal::kSyncManaged,
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

  base::Time current_time = base::Time::Now();
  base::Time delete_start_time = base::Time::Min();

  // delete data up until the `time_to_live_in_hours` for every type.
  base::Time delete_end_time_1 = current_time - base::Hours(1);
  base::Time delete_end_time_2 = current_time - base::Hours(2);
  base::Time delete_end_time_3 = current_time - base::Hours(3);

  // Each scheduled removal is called once the prefs are set.
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_1, remove_mask_1_filterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_1, remove_mask_1_unfilterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_2, remove_mask_2, origin_mask_2);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_3, remove_mask_3_filterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_3, remove_mask_3_unfilterable, 0);

  testing_profile->GetPrefs()->Set(browsing_data::prefs::kBrowsingDataLifetime,
                                   *base::JSONReader::Read(kPref));
  browser_task_environment.RunUntilIdle();
  delegate.VerifyAndClearExpectations();

  // Data will be cleared after another 30 minutes.
  delete_end_time_1 += base::Minutes(30);
  delete_end_time_2 += base::Minutes(30);
  delete_end_time_3 += base::Minutes(30);

  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_1, remove_mask_1_filterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_1, remove_mask_1_unfilterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_2, remove_mask_2, origin_mask_2);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_3, remove_mask_3_filterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_3, remove_mask_3_unfilterable, 0);

  // Data will be cleared after another 30 minutes.
  delete_end_time_1 += base::Minutes(30);
  delete_end_time_2 += base::Minutes(30);
  delete_end_time_3 += base::Minutes(30);

  // Each scheduled removal is called once every 30 minutes.
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_1, remove_mask_1_filterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_1, remove_mask_1_unfilterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_2, remove_mask_2, origin_mask_2);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_3, remove_mask_3_filterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_3, remove_mask_3_unfilterable, 0);

  browser_task_environment.FastForwardBy(base::Hours(1));
  delegate.VerifyAndClearExpectations();
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST(ChromeBrowsingDataLifetimeManager,
     ScheduledRemovalWithBrowserSigninDisabled) {
  signin_util::ResetForceSigninForTesting();
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

  base::Time current_time = base::Time::Now();
  base::Time delete_start_time = base::Time::Min();

  // delete data up until the `time_to_live_in_hours` for every type.
  base::Time delete_end_time_1 = current_time - base::Hours(1);
  base::Time delete_end_time_2 = current_time - base::Hours(2);

  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_1, remove_mask_1_filterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_1, remove_mask_1_unfilterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_2, remove_mask_2, origin_mask_2);

  // If browser sign in is disabled, the scheduled deletions should proceed as
  // usual.
  testing_profile->GetPrefs()->Set(prefs::kSigninAllowed, base::Value(false));

  testing_profile->GetPrefs()->Set(browsing_data::prefs::kBrowsingDataLifetime,
                                   *base::JSONReader::Read(kPref));

  // Delete until 30 minutes from current delete end time because a task is
  // scheduled for 30 minutes from now.
  // Doing `delete_start_time` - 1 hour + 30 minutes is written in an expanded
  // form for better understanding.
  delete_end_time_1 += base::Minutes(30);
  delete_end_time_2 += base::Minutes(30);

  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_1, remove_mask_1_filterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_1, remove_mask_1_unfilterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_2, remove_mask_2, origin_mask_2);

  // Data will be cleared after another 30 minutes.
  delete_end_time_1 += base::Minutes(30);
  delete_end_time_2 += base::Minutes(30);

  // Each scheduled removal is called once every 30 minutes.
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_1, remove_mask_1_filterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_1, remove_mask_1_unfilterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_2, remove_mask_2, origin_mask_2);

  browser_task_environment.FastForwardBy(base::Hours(1));
  delegate.VerifyAndClearExpectations();
}
#endif

TEST(ChromeBrowsingDataLifetimeManager,
     ScheduledRemovalWithBrowserSyncTypeDisabled) {
  signin_util::ResetForceSigninForTesting();
  content::BrowserTaskEnvironment browser_task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile::Builder builder;
  builder.SetIsNewProfile(true);
  builder.AddTestingFactory(TrustedVaultServiceFactory::GetInstance(),
                            TrustedVaultServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                            SyncServiceFactory::GetDefaultFactory());

  auto testing_profile = builder.Build();
  syncer::TestSyncService* sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          testing_profile.get(), base::BindRepeating(&CreateTestSyncService)));

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

  // If required sync types get disabled by policy, the scheduled deletions
  // should proceed as usual.
  auto* settings = sync_service->GetUserSettings();
  settings->SetSelectedType(syncer::UserSelectableType::kPreferences,
                            /*is_type_on=*/false);
  settings->SetSelectedType(syncer::UserSelectableType::kCookies,
                            /*is_type_on=*/false);
  settings->SetTypeIsManaged(syncer::UserSelectableType::kPreferences,
                             /*is_managed=*/true);
  settings->SetTypeIsManaged(syncer::UserSelectableType::kCookies,
                             /*is_managed=*/true);

  base::Time current_time = base::Time::Now();
  base::Time delete_start_time = base::Time::Min();

  // Delete until 30 minutes from current delete end time because a task is
  // scheduled for 30 minutes from now.
  // Doing `delete_start_time` - 1 hour + 30 minutes is written in an expanded
  // form for better understanding.
  base::Time delete_end_time_1 =
      current_time - base::Hours(1) + base::Minutes(30);
  base::Time delete_end_time_2 =
      current_time - base::Hours(2) + base::Minutes(30);

  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_1, remove_mask_1_filterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_1, remove_mask_1_unfilterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_2, remove_mask_2, origin_mask_2);

  // Data will be cleared after another 30 minutes.
  delete_end_time_1 += base::Minutes(30);
  delete_end_time_2 += base::Minutes(30);

  // Each scheduled removal is called once every 30 minutes.
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_1, remove_mask_1_filterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_1, remove_mask_1_unfilterable, 0);
  delegate.ExpectCallDontCareAboutFilterBuilder(
      delete_start_time, delete_end_time_2, remove_mask_2, origin_mask_2);

  browser_task_environment.FastForwardBy(base::Hours(1));
  delegate.VerifyAndClearExpectations();
}
