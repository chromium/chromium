// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chromeos/components/eche_app_ui/url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class EcheAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  EcheAppIntegrationTest() {
    scoped_feature_list_.InitAndEnableFeature(chromeos::features::kEcheSWA);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the Eche App installs and launches correctly. Runs some spot checks
// on the manifest.
IN_PROC_BROWSER_TEST_P(EcheAppIntegrationTest, EcheApp) {
  const GURL url(chromeos::kChromeUIEcheAppURL);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(web_app::SystemAppType::ECHE, url, "Eche App"));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    EcheAppIntegrationTest);
