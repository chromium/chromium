// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/print_preview_cros/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class PrintPreviewCrosAppIntegrationTest
    : public ash::SystemWebAppIntegrationTest {
 public:
  PrintPreviewCrosAppIntegrationTest() {
    features_.InitAndEnableFeature(ash::features::kPrintPreviewCrosApp);
  }

 private:
  base::test::ScopedFeatureList features_;
};

}  // namespace

// Test that the Print Preview CrOS App installs and launches correctly. Runs
// some spot checks on the manifest.
IN_PROC_BROWSER_TEST_P(PrintPreviewCrosAppIntegrationTest,
                       PrintPreviewCrosApp) {
  const GURL url{ash::kChromeUIPrintPreviewCrosURL};
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(ash::SystemWebAppType::PRINT_PREVIEW_CROS, url,
                              /*title=*/"Print"));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    PrintPreviewCrosAppIntegrationTest);
