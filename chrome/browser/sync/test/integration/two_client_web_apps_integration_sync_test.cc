// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/views/web_apps/web_app_integration_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/network_switches.h"

namespace web_app {

namespace {

const char kTestCaseFileName[] =
    "web_app_integration_browsertest_sync_cases.csv";

// Returns the path of the requested file in the test data directory.
base::FilePath GetTestFileDir() {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
  file_path = file_path.Append(FILE_PATH_LITERAL("chrome"));
  file_path = file_path.Append(FILE_PATH_LITERAL("test"));
  file_path = file_path.Append(FILE_PATH_LITERAL("data"));
  return file_path.Append(FILE_PATH_LITERAL("web_apps"));
}

std::vector<std::string> BuildAllPlatformTestCaseSet() {
  return WebAppIntegrationBrowserTestBase::BuildAllPlatformTestCaseSet(
      GetTestFileDir(), kTestCaseFileName);
}

}  // anonymous namespace

class TwoClientWebAppsIntegrationSyncTest
    : public SyncTest,
      public WebAppIntegrationBrowserTestBase::TestDelegate,
      public testing::WithParamInterface<std::string> {
 public:
  TwoClientWebAppsIntegrationSyncTest() : SyncTest(TWO_CLIENT), helper_(this) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Disable WebAppsCrosapi, so that Web Apps get synced in the Ash browser.
    scoped_feature_list_.InitAndDisableFeature(features::kWebAppsCrosapi);
#endif
  }

  // WebAppIntegrationBrowserTestBase::TestDelegate
  Browser* CreateBrowser(Profile* profile) override {
    return InProcessBrowserTest::CreateBrowser(profile);
  }

  void AddBlankTabAndShow(Browser* browser) override {
    InProcessBrowserTest::AddBlankTabAndShow(browser);
  }

  net::EmbeddedTestServer* EmbeddedTestServer() override {
    return embedded_test_server();
  }

  std::vector<Profile*> GetAllProfiles() override {
    return SyncTest::GetAllProfiles();
  }

  bool IsSyncTest() override { return true; }

  bool UserSigninInternal() override { return SyncTest::SetupSync(); }

  void TurnSyncOff() override {
    for (auto* client : GetSyncClients()) {
      client->StopSyncServiceAndClearData();
    }
  }

  void TurnSyncOn() override {
    for (auto* client : GetSyncClients()) {
      ASSERT_TRUE(client->StartSyncService());
    }
    ASSERT_TRUE(AwaitQuiescence());
    apps_helper::AwaitWebAppQuiescence(GetAllProfiles());
  }

  WebAppIntegrationBrowserTestBase helper_;

 private:
  // InProcessBrowserTest
  void SetUp() override {
    helper_.SetUp(GetChromeTestDataDir());
    SyncTest::SetUp();
  }

  // BrowserTestBase
  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();
    ASSERT_TRUE(SetupClients());
    helper_.SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SyncTest::SetUpCommandLine(command_line);
    ASSERT_TRUE(embedded_test_server()->Start());
    command_line->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
        helper_.GetInstallableAppURL("site_a").GetOrigin().spec());
    command_line->AppendSwitch("disable-fake-server-failure-output");
  }

  bool SetupClients() override {
    if (!SyncTest::SetupClients()) {
      return false;
    }

    for (Profile* profile : GetAllProfiles()) {
      auto* web_app_provider = WebAppProvider::Get(profile);
      base::RunLoop loop;
      web_app_provider->on_registry_ready().Post(FROM_HERE, loop.QuitClosure());
      loop.Run();
    }
    return true;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test is a part of the web app integration test suite, which is
// documented in //chrome/browser/ui/views/web_apps/README.md. For information
// about diagnosing, debugging and/or disabling tests, please look to the
// README file.
IN_PROC_BROWSER_TEST_P(TwoClientWebAppsIntegrationSyncTest, Default) {
  helper_.ParseParams(GetParam());
  // Since this test framework differs from traditional browser tests, print
  // some useful information for sheriffs and developers to help identify,
  // diagnose, and disable failing tests.
  LOG(INFO) << helper_.BuildLogForTest(helper_.testing_actions(), IsSyncTest());

  for (const auto& action : helper_.testing_actions()) {
    helper_.ExecuteAction(action);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         TwoClientWebAppsIntegrationSyncTest,
                         testing::ValuesIn(BuildAllPlatformTestCaseSet()));

namespace {
// TODO(jarrydg@chromium.org): Remove the macro disabling the following tests
// when they can compile. https://crbug.com/1215791
#if false
IN_PROC_BROWSER_TEST_F(
    TwoClientWebAppsIntegrationSyncTest,
    WebAppIntegration_InstOmniboxSiteA_WindowCreated_SwitchProfileClientUserAClient2_InListNotLclyInstSiteA_NavSiteA_InstIconShown_LaunchIconShown) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  InstallOmniboxIcon("SiteA");
  CheckWindowCreated();
  SwitchProfileClients("UserAClient2");
  CheckAppInListNotLocallyInstalled("SiteA");
  NavigateBrowser("SiteA");
  CheckInstallIconShown();
  CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppSyncIntegrationTestBase,
    WebAppIntegration_InstOmniboxSiteA_WindowCreated_SwitchProfileClientUserAClient2_InListNotLclyInstSiteA_TurnSyncOff) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  InstallOmniboxIcon("SiteA");
  CheckWindowCreated();
  SwitchProfileClients("UserAClient2");
  CheckAppInListNotLocallyInstalled("SiteA");
  SyncTurnOff();
}

IN_PROC_BROWSER_TEST_F(
    WebAppSyncIntegrationTestBase,
    WebAppIntegration_InstMenuOptionSiteA_WindowCreated_SwitchProfileClientUserAClient2_InListNotLclyInstSiteA_NavSiteA_InstIconShown_LaunchIconShown) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  InstallMenuOption("SiteA");
  CheckWindowCreated();
  SwitchProfileClients("UserAClient2");
  CheckAppInListNotLocallyInstalled("SiteA");
  NavigateBrowser("SiteA");
  CheckInstallIconShown();
  CheckLaunchIconNotShown();
}

IN_PROC_BROWSER_TEST_F(
    WebAppSyncIntegrationTestBase,
    WebAppIntegration_InstMenuOptionSiteA_WindowCreated_SwitchProfileClientUserAClient2_InListNotLclyInstSiteA_TurnSyncOff) {
  // Test contents are generated by script. Please do not modify!
  // See `chrome/test/webapps/README.md` for more info.
  // Sheriffs: Disabling this test is supported.
  InstallMenuOption("SiteA");
  CheckWindowCreated();
  SwitchProfileClients("UserAClient2");
  CheckAppInListNotLocallyInstalled("SiteA");
  SyncTurnOff();
}
#endif

}  // namespace
}  // namespace web_app
