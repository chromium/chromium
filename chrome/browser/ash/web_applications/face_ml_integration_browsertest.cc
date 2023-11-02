// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/face_ml_app_ui/url_constants.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class FaceMLAppIntegrationTest : public ash::SystemWebAppIntegrationTest {
 public:
  FaceMLAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kFaceMLApp},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the Face ML SWA installs and launches correctly.
IN_PROC_BROWSER_TEST_P(FaceMLAppIntegrationTest, FaceMLApp) {
  const GURL url(ash::kChromeUIFaceMLAppURL);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(ash::SystemWebAppType::FACE_ML, url, "Face ML"));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    FaceMLAppIntegrationTest);
