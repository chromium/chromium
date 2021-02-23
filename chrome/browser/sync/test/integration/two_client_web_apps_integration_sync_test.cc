// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/views/web_apps/web_app_integration_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/network_switches.h"

namespace web_app {

namespace {

const std::string kTestCaseFileName =
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
  TwoClientWebAppsIntegrationSyncTest() : SyncTest(TWO_CLIENT), helper_(this) {}

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
    AwaitWebAppQuiescence();
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
        helper_.GetInstallableAppURL().GetOrigin().spec());
    command_line->AppendSwitch("disable-fake-server-failure-output");
  }

  // Helpers
  void AwaitWebAppQuiescence() {
    // Wait until all pending app installs and uninstalls have finished.
    std::vector<std::unique_ptr<WebAppInstallObserver>> install_observers;
    std::vector<std::unique_ptr<WebAppInstallObserver>> uninstall_observers;

    for (auto* profile : GetAllProfiles()) {
      install_observers.push_back(SetupSyncInstallObserverForProfile(profile));
      uninstall_observers.push_back(
          SetupSyncUninstallObserverForProfile(profile));
    }
    for (const auto& observer : install_observers) {
      if (!observer) {
        continue;
      }
      // This actually waits for all observed apps to be installed.
      observer->AwaitNextInstall();
    }
    for (const auto& observer : uninstall_observers) {
      if (!observer) {
        continue;
      }
      // This actually waits for all observed apps to be uninstalled.
      WebAppInstallObserver::AwaitNextUninstall(observer.get());
    }

    for (auto* profile : GetAllProfiles()) {
      DCHECK(GetAppIdsToBeSyncInstalledForProfile(profile).empty());

      std::set<AppId> apps_in_sync_uninstall =
          helper_.GetProviderForProfile(profile)
              ->registry_controller()
              .AsWebAppSyncBridge()
              ->GetAppsInSyncUninstallForTest();
      DCHECK(apps_in_sync_uninstall.empty());
    }
  }

  std::set<AppId> GetAppIdsToBeSyncInstalledForProfile(Profile* profile) {
    WebAppRegistrar* registrar =
        helper_.GetProviderForProfile(profile)->registrar().AsWebAppRegistrar();
    // Make sure that |registrar| is a WebAppRegistrar instance.
    DCHECK(registrar);
    std::vector<AppId> profile_apps = registrar->GetAppsInSyncInstall();

    return std::set<AppId>(profile_apps.begin(), profile_apps.end());
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

  std::unique_ptr<WebAppInstallObserver> SetupSyncInstallObserverForProfile(
      Profile* profile) {
    std::set<AppId> apps_to_be_sync_installed =
        GetAppIdsToBeSyncInstalledForProfile(profile);

    if (apps_to_be_sync_installed.empty()) {
      return nullptr;
    }
    return WebAppInstallObserver::CreateInstallListener(
        profile, apps_to_be_sync_installed);
  }

  std::unique_ptr<WebAppInstallObserver> SetupSyncUninstallObserverForProfile(
      Profile* profile) {
    base::flat_map<Profile*, std::unique_ptr<WebAppInstallObserver>> output;

    std::set<AppId> apps_in_sync_uninstall =
        helper_.GetProviderForProfile(profile)
            ->registry_controller()
            .AsWebAppSyncBridge()
            ->GetAppsInSyncUninstallForTest();

    if (apps_in_sync_uninstall.empty()) {
      return nullptr;
    }
    return WebAppInstallObserver::CreateUninstallListener(
        profile, apps_in_sync_uninstall);
  }
};

IN_PROC_BROWSER_TEST_P(TwoClientWebAppsIntegrationSyncTest, Default) {
  helper_.ParseParams(GetParam());

  for (const auto& action : helper_.testing_actions()) {
    helper_.ExecuteAction(action);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         TwoClientWebAppsIntegrationSyncTest,
                         testing::ValuesIn(BuildAllPlatformTestCaseSet()));

}  // namespace web_app
