// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browsing_data/browsing_data_remover_browsertest_base.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/counters/site_data_counting_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/browsing_data/core/features.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/browsing_data_remover_delegate.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/download_test_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

enum class BrowserType { Default, Incognito };

}  // namespace

class ChromeBrowsingDataLifetimeManagerTest
    : public BrowsingDataRemoverBrowserTestBase {
 protected:
  ChromeBrowsingDataLifetimeManagerTest() {
    InitFeatureList(
        {browsing_data::features::kEnableBrowsingDataLifetimeManager});
  }

  ~ChromeBrowsingDataLifetimeManagerTest() override = default;

  void SetUpOnMainThread() override {
    BrowsingDataRemoverBrowserTestBase::SetUpOnMainThread();
    GetBrowser()->profile()->GetPrefs()->Set(syncer::prefs::kSyncManaged,
                                             base::Value(true));
  }
  void ApplyBrowsingDataLifetimeDeletion(base::StringPiece pref) {
    auto* browsing_data_lifetime_manager =
        ChromeBrowsingDataLifetimeManagerFactory::GetForProfile(
            GetBrowser()->profile());
    browsing_data_lifetime_manager->SetEndTimeForTesting(base::Time::Max());
    content::BrowsingDataRemover* remover =
        content::BrowserContext::GetBrowsingDataRemover(
            GetBrowser()->profile());
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    browsing_data_lifetime_manager->SetBrowsingDataRemoverObserverForTesting(
        &completion_observer);
    GetBrowser()->profile()->GetPrefs()->Set(
        browsing_data::prefs::kBrowsingDataLifetime,
        *base::JSONReader::Read(pref));

    completion_observer.BlockUntilCompletion();
  }
};

class ChromeBrowsingDataLifetimeManagerScheduledRemovalTest
    : public ChromeBrowsingDataLifetimeManagerTest,
      public testing::WithParamInterface<BrowserType> {
 protected:
  ChromeBrowsingDataLifetimeManagerScheduledRemovalTest() = default;
  ~ChromeBrowsingDataLifetimeManagerScheduledRemovalTest() override = default;

  void SetUpOnMainThread() override {
    ChromeBrowsingDataLifetimeManagerTest::SetUpOnMainThread();
    if (GetParam() == BrowserType::Incognito)
      UseIncognitoBrowser();
    GetBrowser()->profile()->GetPrefs()->Set(syncer::prefs::kSyncManaged,
                                             base::Value(true));
  }
};

IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerScheduledRemovalTest,
                       PrefChange) {
  static constexpr char kCookiesPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":
      ["cookies_and_other_site_data"]}])";
  static constexpr char kDownloadHistoryPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":["download_history"]}])";

  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ui_test_utils::NavigateToURL(GetBrowser(), url);

  // Add cookie.
  SetDataForType("Cookie");
  EXPECT_TRUE(HasDataForType("Cookie"));

  // Expect that cookies are deleted.
  ApplyBrowsingDataLifetimeDeletion(kCookiesPref);
  EXPECT_FALSE(HasDataForType("Cookie"));

  // Download an item.
  DownloadAnItem();
  VerifyDownloadCount(1u);

  // Change the pref and verify that download history is deleted.
  ApplyBrowsingDataLifetimeDeletion(kDownloadHistoryPref);
  VerifyDownloadCount(0u);
}

IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerScheduledRemovalTest,
                       Download) {
  static constexpr char kPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":["download_history"]}])";
  DownloadAnItem();
  VerifyDownloadCount(1u);
  ApplyBrowsingDataLifetimeDeletion(kPref);
  VerifyDownloadCount(0u);
}

IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerScheduledRemovalTest,
                       History) {
  // No history saved in incognito mode.
  if (IsIncognito())
    return;
  static constexpr char kPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":["browsing_history"]}])";

  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ui_test_utils::NavigateToURL(GetBrowser(), url);

  SetDataForType("History");
  EXPECT_TRUE(HasDataForType("History"));

  ApplyBrowsingDataLifetimeDeletion(kPref);
  EXPECT_FALSE(HasDataForType("History"));
}

IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerScheduledRemovalTest,
                       ContentSettings) {
  static constexpr char kPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":["site_settings"]}])";

  auto* map =
      HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile());
  map->SetContentSettingDefaultScope(GURL("http://host1.com:1"), GURL(),
                                     ContentSettingsType::COOKIES,
                                     CONTENT_SETTING_BLOCK);

  ApplyBrowsingDataLifetimeDeletion(kPref);

  ContentSettingsForOneType host_settings;
  map->GetSettingsForOneType(ContentSettingsType::COOKIES, &host_settings);
  for (const auto& host_setting : host_settings) {
    if (host_setting.source == "webui_allowlist")
      continue;
    EXPECT_EQ(ContentSettingsPattern::Wildcard(), host_setting.primary_pattern);
    EXPECT_EQ(CONTENT_SETTING_ALLOW, host_setting.GetContentSetting());
  }
}

IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerScheduledRemovalTest,
                       SiteData) {
  static constexpr char kPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":
      ["cookies_and_other_site_data"]}])";

  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ui_test_utils::NavigateToURL(GetBrowser(), url);

  const std::vector<std::string> kTypes{
      "Cookie",    "LocalStorage", "FileSystem",    "SessionStorage",
      "IndexedDb", "WebSql",       "ServiceWorker", "CacheStorage"};

  for (const auto& data_type : kTypes) {
    SetDataForType(data_type);
    EXPECT_TRUE(HasDataForType(data_type));
  }

  ApplyBrowsingDataLifetimeDeletion(kPref);

  for (const auto& data_type : kTypes) {
    EXPECT_FALSE(HasDataForType(data_type)) << data_type;
  }
}

IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerScheduledRemovalTest,
                       Cache) {
  static constexpr char kPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":
      ["cached_images_and_files"]}])";

  GURL url = embedded_test_server()->GetURL("/cachetime");

  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url));

  // Check that the cache has been populated by revisiting these pages with the
  // server stopped.
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url));

  ApplyBrowsingDataLifetimeDeletion(kPref);
  EXPECT_NE(net::OK, content::LoadBasicRequest(network_context(), url));
}

// Disabled because "autofill::AddTestProfile" times out when sync is disabled.
IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerScheduledRemovalTest,
                       DISABLED_Autofill) {
  // No autofill data saved in incognito mode.
  if (IsIncognito())
    return;
  static constexpr char kPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":["autofill"]}])";

  autofill::AutofillProfile profile("01234567-89ab-cdef-fedc-ba9876543210",
                                    autofill::test::kEmptyOrigin);
  autofill::test::SetProfileInfo(
      &profile, "Marion", "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox",
      "123 Zoo St.", "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");
  autofill::AddTestProfile(GetBrowser()->profile(), profile);
  auto* personal_data_manager =
      autofill::PersonalDataManagerFactory::GetForProfile(
          GetBrowser()->profile());
  EXPECT_EQ(
      profile.Compare(*personal_data_manager->GetProfileByGUID(profile.guid())),
      0);

  ApplyBrowsingDataLifetimeDeletion(kPref);

  EXPECT_EQ(nullptr, personal_data_manager->GetProfileByGUID(profile.guid()));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeBrowsingDataLifetimeManagerScheduledRemovalTest,
                         ::testing::Values(BrowserType::Default,
                                           BrowserType::Incognito));

class ChromeBrowsingDataLifetimeManagerShutdownTest
    : public ChromeBrowsingDataLifetimeManagerTest {
 protected:
  ChromeBrowsingDataLifetimeManagerShutdownTest() = default;
  ~ChromeBrowsingDataLifetimeManagerShutdownTest() override = default;

  history::HistoryService* history_service() {
    return HistoryServiceFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
  }

  void VerifyHistorySize(size_t expected_size) {
    history::QueryResults history_query_results;
    base::RunLoop run_loop;
    base::CancelableTaskTracker tracker;
    history_service()->QueryHistory(
        std::u16string(), history::QueryOptions(),
        base::BindLambdaForTesting([&](history::QueryResults results) {
          history_query_results = std::move(results);
          run_loop.QuitClosure().Run();
        }),
        &tracker);
    run_loop.Run();
    EXPECT_EQ(history_query_results.size(), expected_size);
  }
};

IN_PROC_BROWSER_TEST_F(ChromeBrowsingDataLifetimeManagerShutdownTest,
                       PRE_PRE_BrowserShutdown) {
  // browsing_history
  history_service()->AddPage(GURL("https://www.website.com"),
                             base::Time::FromDoubleT(1000),
                             history::VisitSource::SOURCE_BROWSED);
  VerifyHistorySize(1u);

  // download_history
  DownloadAnItem();
  VerifyDownloadCount(1u);

  // site_settings
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile());
  map->SetContentSettingDefaultScope(GURL("http://host1.com:1"), GURL(),
                                     ContentSettingsType::COOKIES,
                                     CONTENT_SETTING_BLOCK);

  ContentSettingsForOneType host_settings;
  bool has_pref_setting = false;
  map->GetSettingsForOneType(ContentSettingsType::COOKIES, &host_settings);
  for (const auto& host_setting : host_settings) {
    if (host_setting.source == "webui_allowlist")
      continue;
    if (host_setting.source == "preference") {
      has_pref_setting = true;
      EXPECT_EQ(ContentSettingsPattern::FromURL(GURL("http://host1.com:1")),
                host_setting.primary_pattern);
      EXPECT_EQ(CONTENT_SETTING_BLOCK, host_setting.GetContentSetting());
    }
  }
  EXPECT_TRUE(has_pref_setting);

  // Ensure nothing gets deleted when the browser closes.
  static constexpr char kPref[] = R"([])";
  GetBrowser()->profile()->GetPrefs()->Set(
      browsing_data::prefs::kClearBrowsingDataOnExitList,
      *base::JSONReader::Read(kPref));
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(ChromeBrowsingDataLifetimeManagerShutdownTest,
                       PRE_BrowserShutdown) {
  // browsing_history
  VerifyHistorySize(1u);

  // download_history
  VerifyDownloadCount(1u);

  // site_settings
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile());
  ContentSettingsForOneType host_settings;
  bool has_pref_setting = false;
  map->GetSettingsForOneType(ContentSettingsType::COOKIES, &host_settings);
  for (const auto& host_setting : host_settings) {
    if (host_setting.source == "webui_allowlist")
      continue;
    if (host_setting.source == "preference") {
      has_pref_setting = true;
      EXPECT_EQ(ContentSettingsPattern::FromURL(GURL("http://host1.com:1")),
                host_setting.primary_pattern);
      EXPECT_EQ(CONTENT_SETTING_BLOCK, host_setting.GetContentSetting());
    }
  }
  EXPECT_TRUE(has_pref_setting);

  // Ensure data gets deleted when the browser closes.
  static constexpr char kPref[] =
      R"(["browsing_history", "download_history", "cookies_and_other_site_data",
      "cached_images_and_files", "password_signin", "autofill", "site_settings",
      "hosted_app_data"])";
  GetBrowser()->profile()->GetPrefs()->Set(
      browsing_data::prefs::kClearBrowsingDataOnExitList,
      *base::JSONReader::Read(kPref));
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(ChromeBrowsingDataLifetimeManagerShutdownTest,
                       BrowserShutdown) {
  // browsing_history
  VerifyHistorySize(0u);

  // download_history
  VerifyDownloadCount(0u);

  // site_settings
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile());

  ContentSettingsForOneType host_settings;
  map->GetSettingsForOneType(ContentSettingsType::COOKIES, &host_settings);
  for (const auto& host_setting : host_settings) {
    if (host_setting.source == "webui_allowlist")
      continue;
    EXPECT_EQ(ContentSettingsPattern::Wildcard(), host_setting.primary_pattern);
    EXPECT_EQ(CONTENT_SETTING_ALLOW, host_setting.GetContentSetting());
  }
}
