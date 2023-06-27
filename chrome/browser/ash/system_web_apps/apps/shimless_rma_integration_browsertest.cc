// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "ash/webui/shimless_rma/url_constants.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

class ShimlessRMAIntegrationTest : public ash::SystemWebAppIntegrationTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SystemWebAppIntegrationTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kLaunchRma);
  }

 protected:
  base::HistogramTester histogram_tester_;
};

// Test that the Shimless RMA App installs and launches correctly by
// running some spot checks on the manifest.
IN_PROC_BROWSER_TEST_P(ShimlessRMAIntegrationTest, ShimlessRMASWAValid) {
  const GURL url(ash::kChromeUIShimlessRMAUrl);
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      ash::SystemWebAppType::SHIMLESS_RMA, url, "Shimless RMA"));

  histogram_tester_.ExpectBucketCount(
      "Webapp.InstallResult.System.Apps.ShimlessRMA",
      webapps::InstallResultCode::kSuccessOfflineOnlyInstall, 1);
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    ShimlessRMAIntegrationTest);
