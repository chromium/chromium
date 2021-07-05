// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/web_applications/system_web_app_integration_test.h"
#include "chromeos/components/demo_mode_app_ui/url_constants.h"
#include "content/public/test/browser_test.h"

class DemoModeAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  DemoModeAppIntegrationTest() {
    scoped_feature_list_.InitAndEnableFeature(chromeos::features::kDemoModeSWA);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the Demo Mode App installs and launches correctly
IN_PROC_BROWSER_TEST_P(DemoModeAppIntegrationTest, DemoModeApp) {
  const GURL url(chromeos::kChromeUIDemoModeAppURL);
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      web_app::SystemAppType::DEMO_MODE, url, "Demo Mode App"));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_GUEST_SESSION_P(
    DemoModeAppIntegrationTest);
