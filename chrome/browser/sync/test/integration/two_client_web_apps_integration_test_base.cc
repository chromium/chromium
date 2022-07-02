// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/two_client_web_apps_integration_test_base.h"

#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "services/network/public/cpp/network_switches.h"

namespace web_app::integration_tests {

TwoClientWebAppsIntegrationTestBase::TwoClientWebAppsIntegrationTestBase()
    : WebAppsSyncTestBase(TWO_CLIENT), helper_(this) {}

// WebAppIntegrationBrowserTestBase::TestDelegate
Browser* TwoClientWebAppsIntegrationTestBase::CreateBrowser(Profile* profile) {
  return InProcessBrowserTest::CreateBrowser(profile);
}

void TwoClientWebAppsIntegrationTestBase::AddBlankTabAndShow(Browser* browser) {
  InProcessBrowserTest::AddBlankTabAndShow(browser);
}

const net::EmbeddedTestServer*
TwoClientWebAppsIntegrationTestBase::EmbeddedTestServer() const {
  return embedded_test_server();
}

std::vector<Profile*> TwoClientWebAppsIntegrationTestBase::GetAllProfiles() {
  return SyncTest::GetAllProfiles();
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
    client->StartSyncService();
  }
  AwaitWebAppQuiescence();
}

void TwoClientWebAppsIntegrationTestBase::AwaitWebAppQuiescence() {
  ASSERT_TRUE(apps_helper::AwaitWebAppQuiescence(GetAllProfiles()));
}

void TwoClientWebAppsIntegrationTestBase::SetUp() {
  helper_.SetUp();
  SyncTest::SetUp();
}

void TwoClientWebAppsIntegrationTestBase::SetUpOnMainThread() {
  SyncTest::SetUpOnMainThread();
  ASSERT_TRUE(SetupSync());
  helper_.SetUpOnMainThread();
}

bool TwoClientWebAppsIntegrationTestBase::SetupClients() {
  if (!SyncTest::SetupClients()) {
    return false;
  }

  for (Profile* profile : GetAllProfiles()) {
    auto* web_app_provider = WebAppProvider::GetForTest(profile);
    base::RunLoop loop;
    web_app_provider->on_registry_ready().Post(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }
  return true;
}

void TwoClientWebAppsIntegrationTestBase::TearDownOnMainThread() {
  helper_.TearDownOnMainThread();
  SyncTest::TearDownOnMainThread();
}

void TwoClientWebAppsIntegrationTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  SyncTest::SetUpCommandLine(command_line);
  ASSERT_TRUE(embedded_test_server()->Start());
  command_line->AppendSwitch("disable-fake-server-failure-output");
}

}  // namespace web_app::integration_tests
