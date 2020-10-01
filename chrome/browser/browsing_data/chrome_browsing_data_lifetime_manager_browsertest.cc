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
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/counters/site_data_counting_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
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
#include "components/prefs/testing_pref_service.h"
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

namespace {

static const char* kExampleHost = "example.com";
enum class BrowserType { Default, Incognito };

}  // namespace

// TODO(ydago) : Extract this into a base class shared with BrowsingDataRemover
// browser tests.
class ChromeBrowsingDataLifetimeManagerTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<BrowserType> {
 public:
  ChromeBrowsingDataLifetimeManagerTest() {
    feature_list_.InitAndEnableFeature(
        browsing_data::features::kEnableBrowsingDataLifetimeManager);
  }
  ~ChromeBrowsingDataLifetimeManagerTest() override = default;

  void SetUpOnMainThread() override {
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    host_resolver()->AddRule(kExampleHost, "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(path);
    ASSERT_TRUE(embedded_test_server()->Start());
    if (GetParam() == BrowserType::Incognito)
      UseIncognitoBrowser();
  }

  bool RunScriptAndGetBool(const std::string& script) {
    bool data;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        GetBrowser()->tab_strip_model()->GetActiveWebContents(), script,
        &data));
    return data;
  }

  void SetDataForType(const std::string& type) {
    ASSERT_TRUE(RunScriptAndGetBool("set" + type + "()"))
        << "Couldn't create data for: " << type;
  }

  bool HasDataForType(const std::string& type) {
    return RunScriptAndGetBool("has" + type + "()");
  }

  void VerifyDownloadCount(size_t expected) {
    content::DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(GetBrowser()->profile());
    std::vector<download::DownloadItem*> downloads;
    download_manager->GetAllDownloads(&downloads);
    EXPECT_EQ(expected, downloads.size());
  }

  void DownloadAnItem() {
    // Start a download.
    content::DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(GetBrowser()->profile());
    std::unique_ptr<content::DownloadTestObserver> observer(
        new content::DownloadTestObserverTerminal(
            download_manager, 1,
            content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));

    GURL download_url = ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("downloads"),
        base::FilePath().AppendASCII("a_zip_file.zip"));
    ui_test_utils::NavigateToURL(GetBrowser(), download_url);
    observer->WaitForFinished();
  }

  network::mojom::NetworkContext* network_context() const {
    return content::BrowserContext::GetDefaultStoragePartition(
               GetBrowser()->profile())
        ->GetNetworkContext();
  }

  // Call to use an Incognito browser rather than the default.
  void UseIncognitoBrowser() {
    ASSERT_EQ(nullptr, incognito_browser_);
    incognito_browser_ = CreateIncognitoBrowser();
  }

  bool IsIncognito() { return incognito_browser_ != nullptr; }

  Browser* GetBrowser() const {
    return incognito_browser_ ? incognito_browser_ : browser();
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

 private:
  Browser* incognito_browser_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerTest,
                       ScheduledRemovalDownload) {
  static constexpr char kPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":["download_history"]}])";
  DownloadAnItem();
  VerifyDownloadCount(1u);
  ApplyBrowsingDataLifetimeDeletion(kPref);
  VerifyDownloadCount(0u);
}

IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerTest,
                       ScheduledRemovalHistory) {
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

IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerTest,
                       ScheduledRemovalContentSettings) {
  static constexpr char kPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":["site_settings"]}])";

  auto* map =
      HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile());
  map->SetContentSettingDefaultScope(GURL("http://host1.com:1"), GURL(),
                                     ContentSettingsType::COOKIES,
                                     std::string(), CONTENT_SETTING_BLOCK);

  ApplyBrowsingDataLifetimeDeletion(kPref);

  ContentSettingsForOneType host_settings;
  map->GetSettingsForOneType(ContentSettingsType::COOKIES, std::string(),
                             &host_settings);
  for (const auto& host_setting : host_settings) {
    if (host_setting.source == "webui_allowlist")
      continue;
    EXPECT_EQ(ContentSettingsPattern::Wildcard(), host_setting.primary_pattern);
    EXPECT_EQ(CONTENT_SETTING_ALLOW, host_setting.GetContentSetting());
  }
}

IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerTest,
                       ScheduledRemovalSiteData) {
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

IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerTest,
                       ScheduledRemovalCache) {
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

IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerTest,
                       ScheduledRemovalAutofill) {
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
  autofill::AddTestProfile(GetBrowser(), profile);
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
                         ChromeBrowsingDataLifetimeManagerTest,
                         ::testing::Values(BrowserType::Default,
                                           BrowserType::Incognito));
