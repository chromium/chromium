// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chromeos/components/scanning/url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ScanningAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  ScanningAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures({chromeos::features::kScanningUI},
                                          {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the Scanning App installs and launches correctly by running some
// spot checks on the manifest.
IN_PROC_BROWSER_TEST_P(ScanningAppIntegrationTest, ScanningAppInLauncher) {
  const GURL url(chromeos::kChromeUIScanningAppUrl);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(web_app::SystemAppType::SCANNING, url, "Scan"));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_MANIFEST_INSTALL_P(
    ScanningAppIntegrationTest);
