// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/web_apps/two_client_web_apps_integration_test_base.h"

#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/base/consent_level.h"
#include "components/sync/base/user_selectable_type.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/multi_user/multi_user_window_manager.h"
#endif

namespace web_app::integration_tests {

using WebAppIntegration = TwoClientWebAppsIntegrationTestBase;
INSTANTIATE_TEST_SUITE_P(,
                         WebAppIntegration,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

TwoClientWebAppsIntegrationTestBase::TwoClientWebAppsIntegrationTestBase()
    : WebAppsSyncTestBase(TWO_CLIENT),
#if BUILDFLAG(IS_CHROMEOS)
      // TwoClientWebAppsIntegrationTest strategy is using two profiles.
      // However, that does not work with multi-user-sign-in implementation
      // in ChromeOS properly. Disable the feature now.
      // TODO(crbug.com/425160398): Consider to redesign the tests to work
      // with the feature.
      multi_user_window_manager_resetter_(
          ash::MultiUserWindowManager::DisableForTesting()),
#endif  // BUILDFLAG(IS_CHROMEOS)
      helper_(this) {
  if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
    feature_overrides_.InitAndEnableFeature(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }
}

TwoClientWebAppsIntegrationTestBase::~TwoClientWebAppsIntegrationTestBase() =
    default;

SyncTest::SetupSyncMode TwoClientWebAppsIntegrationTestBase::GetSetupSyncMode()
    const {
  return GetParam();
}

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
#if BUILDFLAG(IS_CHROMEOS)
    client->service()->GetUserSettings()->SetSelectedOsTypes(
        /*sync_everything=*/false, /*types=*/{});
#else   // BUILDFLAG(IS_CHROMEOS)
    client->service()->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, /*types=*/{});
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
}

void TwoClientWebAppsIntegrationTestBase::SyncTurnOn() {
  for (SyncServiceImplHarness* client : GetSyncClients()) {
    ASSERT_TRUE(client->SetupSync());
#if BUILDFLAG(IS_CHROMEOS)
    client->service()->GetUserSettings()->SetSelectedOsTypes(
        /*sync_everything=*/false,
        /*types=*/{syncer::UserSelectableOsType::kOsApps});
#else   // BUILDFLAG(IS_CHROMEOS)
    client->service()->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/{syncer::UserSelectableType::kApps});
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
  AwaitWebAppQuiescence();
}

void TwoClientWebAppsIntegrationTestBase::SyncSignOut(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
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
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED();
#else
  for (int i = 0; i < num_clients(); ++i) {
    if (GetProfile(i) != profile) {
      continue;
    }
    ASSERT_TRUE(GetClient(i)->SetupSync());
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
  NOTREACHED();
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

    if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
      ASSERT_TRUE(
          GetClient(i)->SetupSyncWithCustomSettings(base::BindLambdaForTesting(
              [](syncer::SyncUserSettings* user_settings) {
#if BUILDFLAG(IS_CHROMEOS)
                user_settings->SetSelectedTypes(/*sync_everything=*/false,
                                                /*types=*/{});
                user_settings->SetSelectedOsTypes(
                    /*sync_everything=*/false,
                    /*types=*/{syncer::UserSelectableOsType::kOsApps});
#else   // BUILDFLAG(IS_CHROMEOS)
                user_settings->SetSelectedTypes(
                    /*sync_everything=*/false,
                    /*types=*/{syncer::UserSelectableType::kApps});
                user_settings->SetInitialSyncFeatureSetupComplete(
                    syncer::SyncFirstSetupCompleteSource::
                        ADVANCED_FLOW_CONFIRM);
#endif  // BUILDFLAG(IS_CHROMEOS)
              })));
    } else {
      ASSERT_TRUE(GetClient(i)->SignInPrimaryAccount());
      ASSERT_TRUE(GetClient(i)->AwaitSyncTransportActive());
      ASSERT_TRUE(GetClient(i)->DisableAllSelectableTypes());
#if BUILDFLAG(IS_CHROMEOS)
      ASSERT_TRUE(GetClient(i)->DisableAllSelectableOsTypes());
      ASSERT_TRUE(GetClient(i)->EnableSelectableOsType(
          syncer::UserSelectableOsType::kOsApps));
#else
      ASSERT_TRUE(GetClient(i)->EnableSelectableType(
          syncer::UserSelectableType::kApps));
#endif  // BUILDFLAG(IS_CHROMEOS)
    }
  }

  helper_.SetUpOnMainThread();
}

bool TwoClientWebAppsIntegrationTestBase::SetupClients() {
  if (!WebAppsSyncTestBase::SetupClients()) {
    return false;
  }
  for (Profile* profile : GetAllProfiles()) {
    if (!web_app::AreWebAppsEnabled(profile) ||
        !web_app::WebAppProvider::GetForWebApps(profile)) {
      continue;
    }
    auto* web_app_provider = WebAppProvider::GetForWebApps(profile);
    base::RunLoop loop;
    web_app_provider->on_registry_ready().Post(FROM_HERE, loop.QuitClosure());
    loop.Run();

    // The base SyncTest class creates a Browser window for each profile, but
    // does not create any tabs in that window. Our tests require all Browser
    // windows to always have at least one tab, so create these tabs as needed.
    Browser* browser =
        chrome::FindTabbedBrowser(profile, /*match_original_profiles=*/false);
    CHECK(browser);
    if (!browser->tab_strip_model()->count()) {
      AddBlankTabAndShow(browser);
    }
  }
  return true;
}

void TwoClientWebAppsIntegrationTestBase::TearDownOnMainThread() {
  helper_.TearDownOnMainThread();
  WebAppsSyncTestBase::TearDownOnMainThread();
}

}  // namespace web_app::integration_tests
