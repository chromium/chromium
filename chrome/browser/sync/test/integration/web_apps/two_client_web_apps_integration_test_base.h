// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_TWO_CLIENT_WEB_APPS_INTEGRATION_TEST_BASE_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_TWO_CLIENT_WEB_APPS_INTEGRATION_TEST_BASE_H_

#include "base/auto_reset.h"
#include "chrome/browser/sync/test/integration/web_apps/web_apps_sync_test_base.h"
#include "chrome/browser/ui/views/web_apps/web_app_integration_test_driver.h"

namespace web_app::integration_tests {

class TwoClientWebAppsIntegrationTestBase
    : public ::web_app::WebAppsSyncTestBase,
      public WebAppIntegrationTestDriver::TestDelegate,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  TwoClientWebAppsIntegrationTestBase();
  ~TwoClientWebAppsIntegrationTestBase() override;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override;

  // WebAppIntegrationTestDriver::TestDelegate:
  Browser* CreateBrowser(Profile* profile) override;
  void CloseBrowserSynchronously(Browser* browser) override;
  void AddBlankTabAndShow(Browser* browser) override;
  const net::EmbeddedTestServer* EmbeddedTestServer() const override;
  Profile* GetDefaultProfile() override;
  bool IsSyncTest() override;
  void SyncTurnOff() override;
  void SyncTurnOn() override;
  void SyncSignOut(Profile*) override;
  void SyncSignIn(Profile*) override;
  void AwaitWebAppQuiescence() override;
  Profile* GetProfileClient(ProfileClient client) override;

 protected:
  void SetUp() override;
  bool SetupClients() override;

  // BrowserTestBase:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

#if BUILDFLAG(IS_CHROMEOS)
  base::AutoReset<bool> multi_user_window_manager_resetter_;
#endif  // BUIDLFLAG(IS_CHROMEOS)

  base::test::ScopedFeatureList feature_overrides_;
  WebAppIntegrationTestDriver helper_;
};

}  // namespace web_app::integration_tests

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_TWO_CLIENT_WEB_APPS_INTEGRATION_TEST_BASE_H_
