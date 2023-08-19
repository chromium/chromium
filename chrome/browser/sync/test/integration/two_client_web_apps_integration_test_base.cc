// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/two_client_web_apps_integration_test_base.h"

#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/sync/base/user_selectable_type.h"

namespace web_app::integration_tests {

TwoClientWebAppsIntegrationTestBase::TwoClientWebAppsIntegrationTestBase()
    : WebAppsSyncTestBase(TWO_CLIENT), helper_(this) {}

// WebAppIntegrationTestDriver::TestDelegate
Browser* TwoClientWebAppsIntegrationTestBase::CreateBrowser(Profile* profile) {
  return InProcessBrowserTest::CreateBrowser(profile);
}

void TwoClientWebAppsIntegrationTestBase::CloseBrowserSynchronously(
    Browser* browser) {
  InProcessBrowserTest::CloseBrowserSynchronously(browser);
}

void TwoClientWebAppsIntegrationTestBase::AddBlankTabAndShow(Browser* browser) {
  InProcessBrowserTest::AddBlankTabAndShow(browser);
}

const net::EmbeddedTestServer*
TwoClientWebAppsIntegrationTestBase::EmbeddedTestServer() const {
  return embedded_test_server();
}

Profile* TwoClientWebAppsIntegrationTestBase::GetDefaultProfile() {
  return GetProfile(0);
}

bool TwoClientWebAppsIntegrationTestBase::IsSyncTest() {
  return true;
}

void TwoClientWebAppsIntegrationTestBase::SyncTurnOff() {
  for (SyncServiceImplHarness* client : GetSyncClients()) {
    client->StopSyncServiceAndClearData();
  }
}

void TwoClientWebAppsIntegrationTestBase::SyncTurnOn() {
  for (SyncServiceImplHarness* client : GetSyncClients()) {
    ASSERT_TRUE(client->EnableSyncFeature());
  }
  AwaitWebAppQuiescence();
}

void TwoClientWebAppsIntegrationTestBase::AwaitWebAppQuiescence() {
  ASSERT_TRUE(apps_helper::AwaitWebAppQuiescence(GetAllProfiles()));
}

Profile* TwoClientWebAppsIntegrationTestBase::GetProfileClient(
    ProfileClient client) {
  switch (client) {
    case ProfileClient::kClient1:
      return GetProfile(0);
    case ProfileClient::kClient2:
      return GetProfile(1);
  }
  NOTREACHED();
  return nullptr;
}

void TwoClientWebAppsIntegrationTestBase::SetUp() {
  helper_.SetUp();
  WebAppsSyncTestBase::SetUp();
}

void TwoClientWebAppsIntegrationTestBase::SetUpOnMainThread() {
  WebAppsSyncTestBase::SetUpOnMainThread();
  ASSERT_TRUE(SetupSync());

  // To limit flakiness due to other sync types, only enable the sync type for
  // web apps.
  for (int i = 0; i < num_clients(); ++i) {
    Profile* profile = GetProfile(i);
    if (!web_app::AreWebAppsEnabled(profile) ||
        !web_app::WebAppProvider::GetForWebApps(profile)) {
      continue;
    }
#if BUILDFLAG(IS_CHROMEOS_ASH)
    GetSyncService(i)->GetUserSettings()->SetSelectedTypes(false, {});
    GetSyncService(i)->GetUserSettings()->SetSelectedOsTypes(
        false, {syncer::UserSelectableOsType::kOsApps});
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
    GetSyncService(i)->GetUserSettings()->SetSelectedTypes(
        false, {syncer::UserSelectableType::kApps});
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  helper_.SetUpOnMainThread();
}

bool TwoClientWebAppsIntegrationTestBase::SetupClients() {
  if (!WebAppsSyncTestBase::SetupClients())
    return false;
  for (Profile* profile : GetAllProfiles()) {
    if (!web_app::AreWebAppsEnabled(profile) ||
        !web_app::WebAppProvider::GetForWebApps(profile)) {
      continue;
    }
    auto* web_app_provider = WebAppProvider::GetForWebApps(profile);
    base::RunLoop loop;
    web_app_provider->on_registry_ready().Post(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }
  return true;
}

void TwoClientWebAppsIntegrationTestBase::TearDownOnMainThread() {
  helper_.TearDownOnMainThread();
  WebAppsSyncTestBase::TearDownOnMainThread();
}

void TwoClientWebAppsIntegrationTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  WebAppsSyncTestBase::SetUpCommandLine(command_line);
  ASSERT_TRUE(embedded_test_server()->Start());
}

}  // namespace web_app::integration_tests
