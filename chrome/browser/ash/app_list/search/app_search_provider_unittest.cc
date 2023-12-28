// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/app_search_provider.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/containers/contains.h"
#include "base/i18n/rtl.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_default_app_list.h"
#include "chrome/browser/ash/app_list/search/app_search_provider_test_base.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "components/crx_file/id_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/stub_icon_loader.h"
#include "extensions/browser/uninstall_reason.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;

namespace app_list::test {

namespace {

constexpr char kGmailQuery[] = "Gmail";
constexpr char kGmailArcName[] = "Gmail ARC";
constexpr char kGmailExtensionName[] = "Gmail Ext";
constexpr char kGmailArcPackage[] = "com.google.android.gm";
constexpr char kGmailArcActivity[] =
    "com.google.android.gm.ConversationListActivityGmail";

constexpr char kRankingAppQuery[] = "testRankingApp";

// Activity and package should match
// chrome/test/data/arc_default_apps/test_app1.json
constexpr char kRankingInternalAppActivity[] = "test.app1.activity";
constexpr char kRankingInternalAppName[] = "testRankingAppInternal";
constexpr char kRankingInternalAppPackageName[] = "test.app1";

constexpr char kRankingNormalAppActivity[] = "test.ranking.app.normal.activity";
constexpr char kRankingNormalAppName[] = "testRankingAppNormal";
constexpr char kRankingNormalAppPackageName[] = "test.ranking.app.normal";

constexpr char kWebAppUrl[] = "https://webappone.com/";
constexpr char kWebAppName[] = "WebApp1";

void UpdateIconKey(apps::AppServiceProxy& proxy, const std::string& app_id) {
  apps::AppType app_type;
  apps::IconKeyPtr icon_key;
  proxy.AppRegistryCache().ForOneApp(
      app_id, [&app_type, &icon_key](const apps::AppUpdate& update) {
        app_type = update.AppType();
        icon_key = std::make_unique<apps::IconKey>(
            /*raw_icon_updated=*/true, update.IconKey()->icon_effects);
      });

  std::vector<apps::AppPtr> apps;
  apps::AppPtr app = std::make_unique<apps::App>(app_type, app_id);
  app->icon_key = std::move(*icon_key);
  apps.push_back(std::move(app));
  proxy.OnApps(std::move(apps), apps::AppType::kUnknown,
               false /* should_notify_initialized */);
}

}  // namespace

class AppSearchProviderTest : public AppSearchProviderTestBase {
 public:
  AppSearchProviderTest()
      : AppSearchProviderTestBase(/*zero_state_provider=*/false) {
  }
  AppSearchProviderTest(const AppSearchProviderTest&) = delete;
  AppSearchProviderTest& operator=(const AppSearchProviderTest&) = delete;
  ~AppSearchProviderTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppSearchProviderTest, Basic) {
  arc_test().SetUp(profile());
  std::vector<arc::mojom::AppInfoPtr> arc_apps;
  for (int i = 0; i < 2; i++)
    arc_apps.emplace_back(arc_test().fake_apps()[i]->Clone());
  arc_test().app_instance()->SendRefreshAppList(arc_apps);

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  InitializeSearchProvider();

  EXPECT_EQ("", RunQuery("!@#$-,-_"));
  EXPECT_EQ("", RunQuery("unmatched query"));

  // Search for "pa" should return both packaged app. The order is undefined
  // because the test only considers textual relevance and the two apps end
  // up having the same score.
  std::string result = RunQuery("pa");
  EXPECT_TRUE(result == "Packaged App 1,Packaged App 2" ||
              result == "Packaged App 2,Packaged App 1");

  // The app with the queried number has a higher relevance score.
  EXPECT_EQ("Packaged App 1,Packaged App 2", RunQuery("packaged 1"));
  EXPECT_EQ("Packaged App 2,Packaged App 1", RunQuery("packaged 2"));

  EXPECT_EQ("Hosted App", RunQuery("host"));

  result = RunQuery("fake");
  EXPECT_TRUE(result == "Fake App 1,Fake App 2" ||
              result == "Fake App 2,Fake App 1");
  result = RunQuery("app2");
  EXPECT_TRUE(result == "Packaged App 2,Fake App 2" ||
              result == "Fake App 2,Packaged App 2");
  arc_test().TearDown();
}

TEST_F(AppSearchProviderTest, NonLatinLocale) {
  base::i18n::SetICUDefaultLocale("sr");

  arc_test().SetUp(profile());

  const std::string test_app_id_1 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  AddExtension(test_app_id_1, "Тестна апликација 1",
               ManifestLocation::kExternalPrefDownload,
               extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  service_->EnableExtension(test_app_id_1);
  const std::string test_app_id_2 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
  AddExtension(test_app_id_2, "Тестна апликација 2",
               ManifestLocation::kExternalPrefDownload,
               extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  service_->EnableExtension(test_app_id_2);

  AddArcApp("Лажна апликација 1", "fake.app.first", "activity");
  AddArcApp("Лажна апликација 2", "fake.app.second", "activity");

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  InitializeSearchProvider();

  EXPECT_EQ("", RunQuery("!@#$-,-_"));
  EXPECT_EQ("", RunQuery("без резултата"));  // no results

  // Search for "Те" should return both packaged app. The order is undefined
  // because the test only considers textual relevance and the two apps end
  // up having the same score.
  std::string result = RunQuery("Те");
  EXPECT_TRUE(result == "Тестна апликација 1,Тестна апликација 2" ||
              result == "Тестна апликација 2,Тестна апликација 1");

  // Serbian, as non-latin local uses exact matching, so only single app will
  // match.
  EXPECT_EQ("Тестна апликација 1", RunQuery("Тестна 1"));
  EXPECT_EQ("Тестна апликација 2", RunQuery("Тестна 2"));

  result = RunQuery("Лажна");
  EXPECT_TRUE(result == "Лажна апликација 2,Лажна апликација 1" ||
              result == "Лажна апликација 1,Лажна апликација 2");
  result = RunQuery("апликација 1");
  EXPECT_TRUE(result == "Тестна апликација 1,Лажна апликација 1" ||
              result == "Лажна апликација 1,Тестна апликација 1");
  arc_test().TearDown();

  base::i18n::SetICUDefaultLocale("en");
}

TEST_F(AppSearchProviderTest, DisableAndEnable) {
  InitializeSearchProvider();

  EXPECT_EQ("Hosted App", RunQuery("host"));

  service_->DisableExtension(kHostedAppId,
                             extensions::disable_reason::DISABLE_USER_ACTION);
  EXPECT_EQ("Hosted App", RunQuery("host"));

  service_->EnableExtension(kHostedAppId);
  EXPECT_EQ("Hosted App", RunQuery("host"));
}

TEST_F(AppSearchProviderTest, UninstallExtension) {
  InitializeSearchProvider();

  EXPECT_EQ("Packaged App 1", RunQuery("app 1 p"));
  service_->UninstallExtension(
      kPackagedApp1Id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  // Uninstalling an app should update the result list without needing to start
  // a new search.
  EXPECT_EQ("", GetSortedResultsString());

  // Rerunning the query also should return no results.
  EXPECT_EQ("", RunQuery("pa1"));

  // Let uninstall code to clean up.
  base::RunLoop().RunUntilIdle();
}

TEST_F(AppSearchProviderTest, InstallUninstallArc) {
  arc_test().SetUp(profile());
  std::vector<arc::mojom::AppInfoPtr> arc_apps;
  arc_test().app_instance()->SendRefreshAppList(arc_apps);

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  InitializeSearchProvider();

  EXPECT_EQ("", GetSortedResultsString());
  EXPECT_EQ("", RunQuery("fake1"));

  arc_apps.emplace_back(arc_test().fake_apps()[0]->Clone());
  arc_test().app_instance()->SendRefreshAppList(arc_apps);

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("Fake App 1", RunQuery("fake1"));

  arc_apps.clear();
  arc_test().app_instance()->SendRefreshAppList(arc_apps);

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("", GetSortedResultsString());
  EXPECT_EQ("", RunQuery("fake1"));

  // Let uninstall code to clean up.
  base::RunLoop().RunUntilIdle();

  arc_test().TearDown();
}

TEST_F(AppSearchProviderTest, NoResultsAfterClearingSearch) {
  InitializeSearchProvider();

  EXPECT_EQ("", RunQuery("Gmail"));
  ClearSearch();

  AddExtension(extension_misc::kGmailAppId, kGmailExtensionName,
               ManifestLocation::kExternalPrefDownload,
               extensions::Extension::NO_FLAGS);
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  // If matching extension is installed after the user has cleared search, the
  // query results should not get updated.
  EXPECT_EQ("", GetSortedResultsString());
}

TEST_F(AppSearchProviderTest, FilterDuplicate) {
  arc_test().SetUp(profile());

  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(profile_.get());
  ASSERT_TRUE(extension_prefs);

  AddExtension(extension_misc::kGmailAppId, kGmailExtensionName,
               ManifestLocation::kExternalPrefDownload,
               extensions::Extension::NO_FLAGS);

  const std::string arc_gmail_app_id =
      AddArcApp(kGmailArcName, kGmailArcPackage, kGmailArcActivity);
  arc_test().arc_app_list_prefs()->SetLastLaunchTime(arc_gmail_app_id);

  std::unique_ptr<ArcAppListPrefs::AppInfo> arc_gmail_app_info =
      arc_test().arc_app_list_prefs()->GetApp(arc_gmail_app_id);
  ASSERT_TRUE(arc_gmail_app_info);

  EXPECT_FALSE(arc_gmail_app_info->last_launch_time.is_null());
  EXPECT_FALSE(arc_gmail_app_info->install_time.is_null());

  extension_prefs->SetLastLaunchTime(
      extension_misc::kGmailAppId,
      arc_gmail_app_info->last_launch_time - base::Seconds(1));

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  InitializeSearchProvider();
  EXPECT_EQ(kGmailArcName, RunQuery(kGmailQuery));

  extension_prefs->SetLastLaunchTime(
      extension_misc::kGmailAppId,
      arc_gmail_app_info->last_launch_time + base::Seconds(1));

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  InitializeSearchProvider();
  EXPECT_EQ(kGmailExtensionName, RunQuery(kGmailQuery));
  arc_test().TearDown();
}

TEST_F(AppSearchProviderTest, WebApp) {
  const webapps::AppId app_id = web_app::test::InstallDummyWebApp(
      testing_profile(), kWebAppName, GURL(kWebAppUrl));

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  InitializeSearchProvider();
  EXPECT_EQ("WebApp1", RunQuery("WebA"));
}

class AppSearchProviderCrostiniTest : public AppSearchProviderTest {
 public:
  void SetUp() override {
    ash::ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();
    AppSearchProviderTest::SetUp();
  }

  void TearDown() override {
    profile_.reset();
    AppSearchProviderTest::TearDown();

    // |profile_| is initialized in AppListTestBase::SetUp but not destroyed in
    // the ::TearDown method, but we need it to go away before shutting down
    // DBusThreadManager to ensure all keyed services that might rely on DBus
    // clients are destroyed.
    profile_.reset();
    ash::SeneschalClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::ChunneldClient::Shutdown();
  }
};

TEST_F(AppSearchProviderCrostiniTest, CrostiniApp) {
  // This both allows Crostini UI and enables Crostini.
  crostini::CrostiniTestHelper crostini_test_helper(testing_profile());
  crostini_test_helper.ReInitializeAppServiceIntegration();
  InitializeSearchProvider();

  // Search based on keywords and name
  auto testApp = crostini_test_helper.BasicApp("goodApp");
  std::map<std::string, std::set<std::string>> keywords;
  keywords[""] = {"wow", "amazing", "excellent app"};
  crostini_test_helper.UpdateAppKeywords(testApp, keywords);
  testApp.set_executable_file_name("executable");
  crostini_test_helper.AddApp(testApp);

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("goodApp", RunQuery("wow"));
  EXPECT_EQ("goodApp", RunQuery("amazing"));
  EXPECT_EQ("goodApp", RunQuery("excellent app"));
  EXPECT_EQ("goodApp", RunQuery("good"));
  EXPECT_EQ("goodApp", RunQuery("executable"));
  EXPECT_EQ("goodApp", RunQuery("wow amazing"));
  EXPECT_EQ("", RunQuery("terrible"));
}

TEST_F(AppSearchProviderCrostiniTest, CrostiniAppWithExactMathing) {
  // Set a non-latin locale, which don't support fuzzy matching.
  base::i18n::SetICUDefaultLocale("sr");
  // This both allows Crostini UI and enables Crostini.
  crostini::CrostiniTestHelper crostini_test_helper(testing_profile());
  crostini_test_helper.ReInitializeAppServiceIntegration();
  InitializeSearchProvider();

  // Search based on keywords and name
  auto testApp = crostini_test_helper.BasicApp("goodApp");
  std::map<std::string, std::set<std::string>> keywords;
  keywords[""] = {"wow", "amazing", "excellent app"};
  crostini_test_helper.UpdateAppKeywords(testApp, keywords);
  testApp.set_executable_file_name("executable");
  crostini_test_helper.AddApp(testApp);

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("goodApp", RunQuery("wow"));
  EXPECT_EQ("goodApp", RunQuery("amazing"));
  EXPECT_EQ("goodApp", RunQuery("excellent app"));
  EXPECT_EQ("goodApp", RunQuery("good"));
  EXPECT_EQ("goodApp", RunQuery("executable"));
  EXPECT_EQ("", RunQuery("terrible"));

  base::i18n::SetICUDefaultLocale("en");
}

TEST_F(AppSearchProviderTest, AppServiceIconCache) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  ASSERT_NE(proxy, nullptr);

  apps::StubIconLoader stub_icon_loader;
  apps::IconLoader* old_icon_loader =
      proxy->OverrideInnerIconLoaderForTesting(&stub_icon_loader);

  // Insert dummy map values so that the stub_icon_loader knows of these apps.
  stub_icon_loader.update_version_by_app_id_[kPackagedApp1Id] = 1;
  stub_icon_loader.update_version_by_app_id_[kPackagedApp2Id] = 2;

  // The stub_icon_loader should start with no LoadIconFromIconKey calls.
  InitializeSearchProvider();
  EXPECT_EQ(0, stub_icon_loader.NumLoadIconFromIconKeyCalls());

  // Running the "pa" query should get two hits (for "Packaged App #"), which
  // should lead to 2 LoadIconFromIconKey calls on the stub_icon_loader.
  RunQuery("pa");
  EXPECT_EQ(2, stub_icon_loader.NumLoadIconFromIconKeyCalls());

  // Issuing the same "pa" query should hit the AppServiceDataSource's icon
  // cache, with no further calls to the wrapped stub_icon_loader.
  RunQuery("pa");
  EXPECT_EQ(2, stub_icon_loader.NumLoadIconFromIconKeyCalls());

  // The number of LoadIconFromIconKey calls should not change, when hiding the
  // UI (i.e. calling ViewClosing).
  CallViewClosing();

  EXPECT_NE("", GetSortedResultsString());
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, stub_icon_loader.NumLoadIconFromIconKeyCalls());

  // The icon has been added to the map, so issuing the same "pa" query should
  // not call the wrapped stub_icon_loader.
  RunQuery("pa");
  EXPECT_EQ(2, stub_icon_loader.NumLoadIconFromIconKeyCalls());

  // Update the icon key to remove the app icon from cache.
  UpdateIconKey(*proxy, kPackagedApp2Id);

  // The icon has been removed from the cache, so issuing the same "pa" query
  // should call the wrapped stub_icon_loader.
  RunQuery("pa");
  EXPECT_EQ(3, stub_icon_loader.NumLoadIconFromIconKeyCalls());

  proxy->OverrideInnerIconLoaderForTesting(old_icon_loader);
}

TEST_F(AppSearchProviderTest, FuzzyAppSearchTest) {
  InitializeSearchProvider();
  EXPECT_EQ("Packaged App 1,Packaged App 2", RunQuery("pa"));
  std::string result = RunQuery("ackaged");
  EXPECT_TRUE(result == "Packaged App 1,Packaged App 2" ||
              result == "Packaged App 2,Packaged App 1");
}

class AppSearchProviderOemAppTest
    : public AppSearchProviderTestBase,
      public ::testing::WithParamInterface</*test_zero_state_search=*/bool> {
 public:
  AppSearchProviderOemAppTest()
      : AppSearchProviderTestBase(test_zero_state_search()) {}

  AppSearchProviderOemAppTest(const AppSearchProviderOemAppTest&) = delete;
  AppSearchProviderOemAppTest& operator=(const AppSearchProviderOemAppTest&) =
      delete;

  ~AppSearchProviderOemAppTest() override = default;

  bool test_zero_state_search() const { return GetParam(); }
};

TEST_P(AppSearchProviderOemAppTest, OemResultsOnFirstBoot) {
  // Disable the pre-installed high-priority extensions. This test simulates
  // a brand new profile being added to a device, and should not include these.
  service_->UninstallExtension(
      kHostedAppId, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  service_->UninstallExtension(
      kPackagedApp1Id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  service_->UninstallExtension(
      kPackagedApp2Id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);

  base::RunLoop().RunUntilIdle();

  // OEM-installed apps should only appear as the first app results
  // if the profile is running for the first time on a device.
  profile_->SetIsNewProfile(true);
  ASSERT_TRUE(profile()->IsNewProfile());

  extensions::ExtensionPrefs* const prefs =
      extensions::ExtensionPrefs::Get(profile());
  ASSERT_TRUE(prefs);
  const char* kOemAppNames[] = {"OemExtension1", "OemExtension2",
                                "OemExtension3", "OemExtension4",
                                "OemExtension5"};

  for (auto* app_id : kOemAppNames) {
    const std::string internal_app_id = crx_file::id_util::GenerateId(app_id);

    AddExtension(internal_app_id, app_id,
                 ManifestLocation::kExternalPrefDownload,
                 extensions::Extension::WAS_INSTALLED_BY_OEM);

    service_->EnableExtension(internal_app_id);

    EXPECT_TRUE(prefs->WasInstalledByOem(internal_app_id));
  }

  // Allow OEM app install to finish.
  base::RunLoop().RunUntilIdle();
  InitializeSearchProvider();

  std::string results_string =
      test_zero_state_search() ? RunZeroStateSearch() : RunQuery("Oem");
  std::vector<std::string> results = base::SplitString(
      results_string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (auto* app : kOemAppNames) {
    EXPECT_TRUE(base::Contains(results, app));
  }
}

enum class TestArcAppInstallType {
  CONTROLLED_BY_POLICY,
  INSTALLED_BY_DEFAULT,
};

class AppSearchProviderWithArcAppInstallType
    : public AppSearchProviderTest,
      public ::testing::WithParamInterface<TestArcAppInstallType> {
 public:
  AppSearchProviderWithArcAppInstallType() = default;

  AppSearchProviderWithArcAppInstallType(
      const AppSearchProviderWithArcAppInstallType&) = delete;
  AppSearchProviderWithArcAppInstallType& operator=(
      const AppSearchProviderWithArcAppInstallType&) = delete;

  ~AppSearchProviderWithArcAppInstallType() override = default;
};

// TODO (879413): Enable this after resolving flakiness.
TEST_P(AppSearchProviderWithArcAppInstallType,
       DISABLED_InstallInternallyRanking) {
  const bool default_app =
      GetParam() == TestArcAppInstallType::INSTALLED_BY_DEFAULT;
  if (default_app) {
    ArcDefaultAppList::UseTestAppsDirectory();
    arc_test().set_wait_default_apps(true);
  }
  arc_test().SetUp(profile());

  ArcAppListPrefs* const prefs = arc_test().arc_app_list_prefs();
  ASSERT_TRUE(prefs);

  // Install normal app.
  const std::string normal_app_id =
      AddArcApp(kRankingNormalAppName, kRankingNormalAppPackageName,
                kRankingNormalAppActivity);

  // Wait a bit to make sure time is updated.
  WaitTimeUpdated();

  if (GetParam() == TestArcAppInstallType::CONTROLLED_BY_POLICY) {
    const std::string policy = base::StringPrintf(
        "{\"applications\":[{\"installType\":\"FORCE_INSTALLED\","
        "\"packageName\":"
        "\"%s\"}]}",
        kRankingInternalAppPackageName);
    prefs->OnPolicySent(policy);
  }

  // Reinstall default app to make install time after normall app install time.
  if (default_app) {
    static_cast<arc::mojom::AppHost*>(prefs)->OnPackageAppListRefreshed(
        kRankingInternalAppPackageName, {} /* apps */);
  }

  const std::string internal_app_id =
      AddArcApp(kRankingInternalAppName, kRankingInternalAppPackageName,
                kRankingInternalAppActivity);

  EXPECT_EQ(default_app, prefs->IsDefault(internal_app_id));

  std::unique_ptr<ArcAppListPrefs::AppInfo> normal_app =
      prefs->GetApp(normal_app_id);
  std::unique_ptr<ArcAppListPrefs::AppInfo> internal_app =
      prefs->GetApp(internal_app_id);
  ASSERT_TRUE(normal_app && internal_app);

  EXPECT_LT(normal_app->install_time, internal_app->install_time);

  // Installed internally app has runking below other apps, even if its install
  // time is later.
  InitializeSearchProvider();
  EXPECT_EQ(std::string(kRankingNormalAppName) + "," +
                std::string(kRankingInternalAppName),
            RunQuery(kRankingAppQuery));

  // Using installed internally app moves it in ranking up.
  WaitTimeUpdated();
  prefs->SetLastLaunchTime(internal_app_id);
  InitializeSearchProvider();
  EXPECT_EQ(std::string(kRankingInternalAppName) + "," +
                std::string(kRankingNormalAppName),
            RunQuery(kRankingAppQuery));
  arc_test().TearDown();
}

INSTANTIATE_TEST_SUITE_P(All, AppSearchProviderOemAppTest, ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(
    All,
    AppSearchProviderWithArcAppInstallType,
    ::testing::ValuesIn({TestArcAppInstallType::CONTROLLED_BY_POLICY,
                         TestArcAppInstallType::INSTALLED_BY_DEFAULT}));

}  // namespace app_list::test
