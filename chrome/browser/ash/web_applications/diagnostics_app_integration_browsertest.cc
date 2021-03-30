// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chromeos/components/diagnostics_ui/url_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class DiagnosticsAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  DiagnosticsAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures({chromeos::features::kDiagnosticsApp},
                                          {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the Diagnostics App installs and launches correctly by running some
// spot checks on the manifest.
IN_PROC_BROWSER_TEST_P(DiagnosticsAppIntegrationTest,
                       DiagnosticsAppInLauncher) {
  const GURL url(chromeos::kChromeUIDiagnosticsAppUrl);
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      web_app::SystemAppType::DIAGNOSTICS, url, "Diagnostics"));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    DiagnosticsAppIntegrationTest);
