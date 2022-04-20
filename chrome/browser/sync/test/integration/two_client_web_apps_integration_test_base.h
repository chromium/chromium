// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_TWO_CLIENT_WEB_APPS_INTEGRATION_TEST_BASE_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_TWO_CLIENT_WEB_APPS_INTEGRATION_TEST_BASE_H_

#include "chrome/browser/sync/test/integration/web_apps_sync_test_base.h"
#include "chrome/browser/ui/views/web_apps/web_app_integration_test_driver.h"
#include "content/public/test/browser_test.h"

namespace base {
class CommandLine;
}

namespace web_app::integration_tests {

class TwoClientWebAppsIntegrationTestBase
    : public ::web_app::WebAppsSyncTestBase,
      public WebAppIntegrationTestDriver::TestDelegate {
 public:
  TwoClientWebAppsIntegrationTestBase();

  // WebAppIntegrationBrowserTestBase::TestDelegate:
  Browser* CreateBrowser(Profile* profile) override;
  void AddBlankTabAndShow(Browser* browser) override;
  const net::EmbeddedTestServer* EmbeddedTestServer() const override;
  std::vector<Profile*> GetAllProfiles() override;
  bool IsSyncTest() override;
  void SyncTurnOff() override;
  void SyncTurnOn() override;
  void AwaitWebAppQuiescence() override;

 protected:
  void SetUp() override;
  bool SetupClients() override;

  // BrowserTestBase:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  WebAppIntegrationTestDriver helper_;
};

}  // namespace web_app::integration_tests

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_TWO_CLIENT_WEB_APPS_INTEGRATION_TEST_BASE_H_
