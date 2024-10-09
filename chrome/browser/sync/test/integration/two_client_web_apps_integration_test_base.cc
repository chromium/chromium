// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/two_client_web_apps_integration_test_base.h"

#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/base/consent_level.h"
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
    client->service()->GetUserSettings()->SetSelectedOsTypes(
        /*sync_everything=*/false, /*types=*/{});
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
    client->service()->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, /*types=*/{});
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }
}

void TwoClientWebAppsIntegrationTestBase::SyncTurnOn() {
  for (SyncServiceImplHarness* client : GetSyncClients()) {
    ASSERT_TRUE(client->SetupSync());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    client->service()->GetUserSettings()->SetSelectedOsTypes(
        /*sync_everything=*/false,
        /*types=*/{syncer::UserSelectableOsType::kOsApps});
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
    client->service()->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/{syncer::UserSelectableType::kApps});
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }
  AwaitWebAppQuiescence();
}

void TwoClientWebAppsIntegrationTestBase::SyncSignOut(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  NOTREACHED();
#else
  for (int i = 0; i < num_clients(); ++i) {
    if (GetProfile(i) != profile) {
      continue;
    }
    GetClient(i)->SignOutPrimaryAccount();
  }
  AwaitWebAppQuiescence();
#endif
}

void TwoClientWebAppsIntegrationTestBase::SyncSignIn(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  NOTREACHED();
#else
  for (int i = 0; i < num_clients(); ++i) {
    if (GetProfile(i) != profile) {
      continue;
    }
    ASSERT_TRUE(
        GetClient(i)->SignInPrimaryAccount(signin::ConsentLevel::kSync));
  }
  AwaitWebAppQuiescence();
#endif
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
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void TwoClientWebAppsIntegrationTestBase::SetUp() {
  helper_.SetUp();
  WebAppsSyncTestBase::SetUp();
}

void TwoClientWebAppsIntegrationTestBase::SetUpOnMainThread() {
  WebAppsSyncTestBase::SetUpOnMainThread();
  ASSERT_TRUE(SetupClients());

  // To limit flakiness due to other sync types, only enable the sync type for
  // web apps.
  for (int i = 0; i < num_clients(); ++i) {
    Profile* profile = GetProfile(i);
    if (!web_app::AreWebAppsEnabled(profile) ||
        !web_app::WebAppProvider::GetForWebApps(profile)) {
      continue;
    }

    ASSERT_TRUE(GetClient(i)->SetupSync(
        base::BindLambdaForTesting([](syncer::SyncUserSettings* user_settings) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
          user_settings->SetSelectedTypes(/*sync_everything=*/false,
                                          /*types=*/{});
          user_settings->SetSelectedOsTypes(
              /*sync_everything=*/false,
              /*types=*/{syncer::UserSelectableOsType::kOsApps});
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
          user_settings->SetSelectedTypes(
              /*sync_everything=*/false,
              /*types=*/{syncer::UserSelectableType::kApps});
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
        })));
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
