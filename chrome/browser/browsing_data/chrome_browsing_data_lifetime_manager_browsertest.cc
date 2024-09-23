// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager.h"

#include <array>
#include <memory>
#include <optional>
#include <string_view>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
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
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/browsing_data/core/browsing_data_policies_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/service/sync_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/browsing_data_remover_delegate.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/download_test_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/ui_test_utils.h"
#else
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#endif

namespace {

using ProviderType = content_settings::ProviderType;

enum class BrowserType { Default, Incognito };

// The precondition required to delete browsing data.
enum class BrowsingDataDeletionCondition {
  SyncDisabled,
  BrowserSigninDisabled
};

struct FeatureConditions {
  BrowsingDataDeletionCondition data_deletion_condition;
  BrowserType browser_type;
};

constexpr std::array<const char*, 6> kSiteDataTypes{
    "Cookie",    "LocalStorage",  "SessionStorage",
    "IndexedDb", "ServiceWorker", "CacheStorage"};

}  // namespace

class ChromeBrowsingDataLifetimeManagerTest
    : public BrowsingDataRemoverBrowserTestBase,
      public testing::WithParamInterface<FeatureConditions> {
 protected:
  ChromeBrowsingDataLifetimeManagerTest() = default;
  ~ChromeBrowsingDataLifetimeManagerTest() override = default;

  void SetUpOnMainThread() override {
    BrowsingDataRemoverBrowserTestBase::SetUpOnMainThread();
    if (GetParam().data_deletion_condition ==
        BrowsingDataDeletionCondition::SyncDisabled) {
      GetProfile()->GetPrefs()->Set(syncer::prefs::internal::kSyncManaged,
                                    base::Value(true));
    } else if (GetParam().data_deletion_condition ==
               BrowsingDataDeletionCondition::BrowserSigninDisabled) {
#if BUILDFLAG(IS_ANDROID)
      GetProfile()->GetPrefs()->Set(prefs::kSigninAllowed, base::Value(false));
#else
      GetProfile()->GetPrefs()->Set(prefs::kSigninAllowedOnNextStartup,
                                    base::Value(false));
#endif  // BUILDFLAG(IS_ANDROID)
    }
  }

  void ApplyBrowsingDataLifetimeDeletion(std::string_view pref) {
    auto* browsing_data_lifetime_manager =
        ChromeBrowsingDataLifetimeManagerFactory::GetForProfile(GetProfile());
    browsing_data_lifetime_manager->SetEndTimeForTesting(base::Time::Max());
    content::BrowsingDataRemover* remover =
        GetProfile()->GetBrowsingDataRemover();
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    browsing_data_lifetime_manager->SetBrowsingDataRemoverObserverForTesting(
        &completion_observer);
    // The pref needs to be cleared so that the browsing data deletion is
    // triggered even if the same pref value is set twice in a row.
    GetProfile()->GetPrefs()->ClearPref(
        browsing_data::prefs::kBrowsingDataLifetime);
    GetProfile()->GetPrefs()->Set(browsing_data::prefs::kBrowsingDataLifetime,
                                  *base::JSONReader::Read(pref));

    completion_observer.BlockUntilCompletion();
  }

  void SetupSiteData(content::WebContents* web_contents) {
    for (const char* data_type : kSiteDataTypes) {
      SetDataForType(data_type, web_contents);
      EXPECT_TRUE(HasDataForType(data_type, web_contents));
    }
  }

  void CheckSiteData(content::WebContents* web_contents, bool has_site_data) {
    for (const char* data_type : kSiteDataTypes) {
      EXPECT_EQ(HasDataForType(data_type), has_site_data) << data_type;
    }
  }
};

class ChromeBrowsingDataLifetimeManagerScheduledRemovalTest
    : public ChromeBrowsingDataLifetimeManagerTest {
 protected:
  ChromeBrowsingDataLifetimeManagerScheduledRemovalTest() = default;
  ~ChromeBrowsingDataLifetimeManagerScheduledRemovalTest() override = default;

  void SetUpOnMainThread() override {
    ChromeBrowsingDataLifetimeManagerTest::SetUpOnMainThread();
#if !BUILDFLAG(IS_ANDROID)
    if (GetParam().browser_type == BrowserType::Incognito) {
      UseIncognitoBrowser();
    }
#endif
  }
};

#if BUILDFLAG(IS_ANDROID)
// See https://crbug.com/1432023 for tracking bug.
#define MAYBE_PrefChange DISABLED_PrefChange
#else
#define MAYBE_PrefChange PrefChange
#endif
IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerScheduledRemovalTest,
                       MAYBE_PrefChange) {
  static constexpr char kCookiesPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":
      ["cookies_and_other_site_data"]}])";
  static constexpr char kCachePref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":
      ["cached_images_and_files"]}])";

  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  // Add cookie.
  SetDataForType("Cookie");
  EXPECT_TRUE(HasDataForType("Cookie"));

  // Expect that cookies are deleted.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  ApplyBrowsingDataLifetimeDeletion(kCookiesPref);
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));
  EXPECT_FALSE(HasDataForType("Cookie"));

  url = embedded_test_server()->GetURL("/cachetime");

  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url));

  // Check that the cache has been populated by revisiting these pages with the
  // server stopped.
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url));

  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  ApplyBrowsingDataLifetimeDeletion(kCachePref);
  EXPECT_NE(net::OK, content::LoadBasicRequest(network_context(), url));
}

#if !BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/40169678): Enable this test for android once we figure out if
// it is possible to delete download history on Android while the browser is
// running.
IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerScheduledRemovalTest,
                       Download) {
  static constexpr char kPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":["download_history"]}])";
  DownloadAnItem();
  VerifyDownloadCount(1u);
  ApplyBrowsingDataLifetimeDeletion(kPref);
  // The download is not deleted since the page where it happened is still
  // opened.
  VerifyDownloadCount(1u);

  // Navigate away.
  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));
  VerifyDownloadCount(1u);

  // The download should now be deleted since the page where it happened is not
  // active.
  ApplyBrowsingDataLifetimeDeletion(kPref);
  VerifyDownloadCount(0u);
}
#endif

// Failing crbug.com/1456542.
IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerScheduledRemovalTest,
                       DISABLED_History) {
  // No history saved in incognito mode.
  if (IsIncognito())
    return;
  static constexpr char kPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":["browsing_history"]}])";

  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  SetDataForType("History");
  EXPECT_TRUE(HasDataForType("History"));

  ApplyBrowsingDataLifetimeDeletion(kPref);
  EXPECT_FALSE(HasDataForType("History"));
}

IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerScheduledRemovalTest,
                       ContentSettings) {
  static constexpr char kPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":["site_settings"]}])";

  auto* map = HostContentSettingsMapFactory::GetForProfile(GetProfile());
  map->SetContentSettingDefaultScope(GURL("http://host1.com:1"), GURL(),
                                     ContentSettingsType::COOKIES,
                                     CONTENT_SETTING_BLOCK);

  ApplyBrowsingDataLifetimeDeletion(kPref);

  for (const auto& host_setting :
       map->GetSettingsForOneType(ContentSettingsType::COOKIES)) {
    if (host_setting.source == ProviderType::kWebuiAllowlistProvider) {
      continue;
    }
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
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));
  SetupSiteData(GetActiveWebContents());
  ApplyBrowsingDataLifetimeDeletion(kPref);

  // The site data is not deleted since the page where it happened is still
  // opened.
  CheckSiteData(GetActiveWebContents(), /*has_site_data=*/true);

  // Navigate away.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));

  // The site should now be deleted since the page where it happened is not
  // active.
  ApplyBrowsingDataLifetimeDeletion(kPref);
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  CheckSiteData(GetActiveWebContents(), /*has_site_data=*/false);
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

IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerScheduledRemovalTest,
                       KeepsOtherTabData) {
  if (IsIncognito())
    return;

  static constexpr char kPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":
      ["cookies_and_other_site_data"]}])";

  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  auto* first_tab = GetActiveWebContents();
#if !BUILDFLAG(IS_ANDROID)
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  auto* second_tab = GetActiveWebContents();
#else
  TabModel* tab_model = TabModelList::GetTabModelForWebContents(first_tab);
  TabAndroid* current_tab = TabAndroid::FromWebContents(first_tab);
  std::unique_ptr<content::WebContents> contents = content::WebContents::Create(
      content::WebContents::CreateParams(GetProfile()));
  auto* second_tab = contents.release();
  tab_model->CreateTab(current_tab, second_tab, /*select=*/true);
  ASSERT_TRUE(content::NavigateToURL(second_tab, url));
#endif
  DCHECK_NE(first_tab, second_tab);

  SetupSiteData(first_tab);
  SetupSiteData(second_tab);

  ApplyBrowsingDataLifetimeDeletion(kPref);

  // The site data is not deleted since the page where it happened is still
  // opened.
  CheckSiteData(first_tab, /*has_site_data=*/true);
  CheckSiteData(second_tab, /*has_site_data=*/true);

  // Navigate away first tab.
  ASSERT_TRUE(content::NavigateToURL(first_tab, GURL(url::kAboutBlankURL)));

  // The site data is not deleted since the domain of the data is still in use.
  ApplyBrowsingDataLifetimeDeletion(kPref);
  ASSERT_TRUE(content::NavigateToURL(first_tab, url));
  CheckSiteData(first_tab, /*has_site_data=*/true);
  CheckSiteData(second_tab, /*has_site_data=*/true);

  // Navigate away second tab.
  ASSERT_TRUE(content::NavigateToURL(second_tab, GURL(url::kAboutBlankURL)));

  // The site data is not deleted since the domain of the data is still in use.
  ApplyBrowsingDataLifetimeDeletion(kPref);
  ASSERT_TRUE(content::NavigateToURL(second_tab, url));
  CheckSiteData(first_tab, /*has_site_data=*/true);
  CheckSiteData(second_tab, /*has_site_data=*/true);

  // Navigate away both tabs.
  ASSERT_TRUE(content::NavigateToURL(first_tab, GURL(url::kAboutBlankURL)));
  ASSERT_TRUE(content::NavigateToURL(second_tab, GURL(url::kAboutBlankURL)));

  // The site data is not deleted since the domain of the data is still in use.
  ApplyBrowsingDataLifetimeDeletion(kPref);
  ASSERT_TRUE(content::NavigateToURL(first_tab, url));
  ASSERT_TRUE(content::NavigateToURL(second_tab, url));
  CheckSiteData(first_tab, /*has_site_data=*/false);
  CheckSiteData(second_tab, /*has_site_data=*/false);

#if BUILDFLAG(IS_ANDROID)
  for (int i = 0; i < tab_model->GetTabCount(); ++i) {
    if (second_tab == tab_model->GetWebContentsAt(i)) {
      tab_model->CloseTabAt(i);
      break;
    }
  }
#endif
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerScheduledRemovalTest,
                       KeepsOtherWindowData) {
  if (IsIncognito())
    return;

  static constexpr char kPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":
      ["cookies_and_other_site_data"]}])";

  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  SetupSiteData(GetActiveWebContents());

  ApplyBrowsingDataLifetimeDeletion(kPref);

  // The site data is not deleted since the page where it happened is still
  // opened.
  CheckSiteData(GetActiveWebContents(), /*has_site_data=*/true);

  // Open current url in new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
  content::WebContents* new_tab = nullptr;
  for (Browser* b : *BrowserList::GetInstance()) {
    if (b != browser())
      new_tab = b->tab_strip_model()->GetActiveWebContents();
  }

  ASSERT_TRUE(new_tab);
  ASSERT_NE(new_tab, GetActiveWebContents());

  // Navigate away current tab.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));

  // The site data is not deleted since the page's domain is opened in another
  // tab.
  ApplyBrowsingDataLifetimeDeletion(kPref);
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));
  CheckSiteData(GetActiveWebContents(), /*has_site_data=*/true);

  // Navigate away both tabs.
  ASSERT_TRUE(content::NavigateToURL(new_tab, GURL(url::kAboutBlankURL)));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));

  ApplyBrowsingDataLifetimeDeletion(kPref);
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));
  CheckSiteData(GetActiveWebContents(), /*has_site_data=*/false);
}

// Disabled because "autofill::AddTestProfile" times out when sync is disabled.
// TODO(crbug.com/40909863): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_Autofill DISABLED_Autofill
#else
#define MAYBE_Autofill Autofill
#endif
IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerScheduledRemovalTest,
                       MAYBE_Autofill) {
  // No autofill data saved in incognito mode.
  if (IsIncognito())
    return;
  static constexpr char kPref[] =
      R"([{"time_to_live_in_hours": 1, "data_types":["autofill"]}])";

  autofill::AutofillProfile profile(
      "01234567-89ab-cdef-fedc-ba9876543210",
      autofill::AutofillProfile::RecordType::kLocalOrSyncable,
      AddressCountryCode("US"));
  autofill::test::SetProfileInfo(
      &profile, "Marion", "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox",
      "123 Zoo St.", "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");
  autofill::AddTestProfile(GetProfile(), profile);
  auto* personal_data_manager =
      autofill::PersonalDataManagerFactory::GetForBrowserContext(GetProfile());
  EXPECT_EQ(profile.Compare(
                *personal_data_manager->address_data_manager().GetProfileByGUID(
                    profile.guid())),
            0);

  ApplyBrowsingDataLifetimeDeletion(kPref);

  EXPECT_EQ(nullptr,
            personal_data_manager->address_data_manager().GetProfileByGUID(
                profile.guid()));
}
#endif

#if !BUILDFLAG(IS_ANDROID)
class ChromeBrowsingDataLifetimeManagerShutdownTest
    : public ChromeBrowsingDataLifetimeManagerTest {
 protected:
  ChromeBrowsingDataLifetimeManagerShutdownTest() = default;
  ~ChromeBrowsingDataLifetimeManagerShutdownTest() override = default;

  history::HistoryService* history_service() {
    return HistoryServiceFactory::GetForProfile(
        GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
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

IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerShutdownTest,
                       PRE_PRE_BrowserShutdown) {
  // browsing_history
  history_service()->AddPage(GURL("https://www.website.com"),
                             base::Time::FromSecondsSinceUnixEpoch(1000),
                             history::VisitSource::SOURCE_BROWSED);
  VerifyHistorySize(1u);

  // download_history
  DownloadAnItem();
  VerifyDownloadCount(1u);

  // site_settings
  auto* map = HostContentSettingsMapFactory::GetForProfile(GetProfile());
  map->SetContentSettingDefaultScope(GURL("http://host1.com:1"), GURL(),
                                     ContentSettingsType::COOKIES,
                                     CONTENT_SETTING_BLOCK);

  bool has_pref_setting = false;
  for (const auto& host_setting :
       map->GetSettingsForOneType(ContentSettingsType::COOKIES)) {
    if (host_setting.source == ProviderType::kWebuiAllowlistProvider) {
      continue;
    }
    if (host_setting.source == ProviderType::kPrefProvider) {
      has_pref_setting = true;
      EXPECT_EQ(ContentSettingsPattern::FromURL(GURL("http://host1.com:1")),
                host_setting.primary_pattern);
      EXPECT_EQ(CONTENT_SETTING_BLOCK, host_setting.GetContentSetting());
    }
  }
  EXPECT_TRUE(has_pref_setting);

  // Ensure nothing gets deleted when the browser closes.
  static constexpr char kPref[] = R"([])";
  GetProfile()->GetPrefs()->Set(
      browsing_data::prefs::kClearBrowsingDataOnExitList,
      *base::JSONReader::Read(kPref));
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerShutdownTest,
                       PRE_BrowserShutdown) {
  // browsing_history
  VerifyHistorySize(1u);

  // download_history
  VerifyDownloadCount(1u);

  // site_settings
  auto* map = HostContentSettingsMapFactory::GetForProfile(GetProfile());
  bool has_pref_setting = false;
  for (const auto& host_setting :
       map->GetSettingsForOneType(ContentSettingsType::COOKIES)) {
    if (host_setting.source == ProviderType::kWebuiAllowlistProvider) {
      continue;
    }
    if (host_setting.source == ProviderType::kPrefProvider) {
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
  GetProfile()->GetPrefs()->Set(
      browsing_data::prefs::kClearBrowsingDataOnExitList,
      *base::JSONReader::Read(kPref));
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_P(ChromeBrowsingDataLifetimeManagerShutdownTest,
                       BrowserShutdown) {
  // browsing_history
  VerifyHistorySize(0u);

  // download_history
  VerifyDownloadCount(0u);

  // site_settings
  auto* map = HostContentSettingsMapFactory::GetForProfile(GetProfile());

  for (const auto& host_setting :
       map->GetSettingsForOneType(ContentSettingsType::COOKIES)) {
    if (host_setting.source == ProviderType::kWebuiAllowlistProvider) {
      continue;
    }
    EXPECT_EQ(ContentSettingsPattern::Wildcard(), host_setting.primary_pattern);
    EXPECT_EQ(CONTENT_SETTING_ALLOW, host_setting.GetContentSetting());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeBrowsingDataLifetimeManagerShutdownTest,
    ::testing::ValuesIn(std::vector<FeatureConditions> {
      {BrowsingDataDeletionCondition::SyncDisabled, BrowserType::Incognito},
          {BrowsingDataDeletionCondition::SyncDisabled, BrowserType::Default},
#if !BUILDFLAG(IS_CHROMEOS)
          {BrowsingDataDeletionCondition::BrowserSigninDisabled,
           BrowserType::Incognito},
      {
        BrowsingDataDeletionCondition::BrowserSigninDisabled,
            BrowserType::Default
      }
#endif  // !BUILDFLAG(IS_CHROMEOS)
    }));
#endif  // !BUILDFLAG(IS_ANDROID)

// Browser signin can only be tested on desktop after restart.
INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeBrowsingDataLifetimeManagerScheduledRemovalTest,
    ::testing::ValuesIn(std::vector<FeatureConditions> {
      {BrowsingDataDeletionCondition::SyncDisabled, BrowserType::Incognito},
          {BrowsingDataDeletionCondition::SyncDisabled, BrowserType::Default},
#if BUILDFLAG(IS_ANDROID)
          {BrowsingDataDeletionCondition::BrowserSigninDisabled,
           BrowserType::Incognito},
      {
        BrowsingDataDeletionCondition::BrowserSigninDisabled,
            BrowserType::Default
      }
#endif  // BUILDFLAG(IS_ANDROID)
    }));
