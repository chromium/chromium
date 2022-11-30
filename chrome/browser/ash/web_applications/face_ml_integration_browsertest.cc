// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/face_ml_app_ui/url_constants.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests FaceMLApp functionalities when feature is enabled.
class FaceMLAppIntegrationTest : public ash::SystemWebAppIntegrationTest {
 public:
  FaceMLAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kFaceMLApp},
        /*disabled_features=*/{});
  }

 protected:
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

// Tests FaceMLApp functionalities when feature is disabled.
class FaceMLAppDisabledIntegrationTest
    : public ash::SystemWebAppIntegrationTest {};

// Test that the Face ML SWA does not launch when feature is not enabled.
IN_PROC_BROWSER_TEST_P(FaceMLAppDisabledIntegrationTest,
                       FaceMLAppDisabledFeature) {
  WaitForTestSystemAppInstall();
  EXPECT_FALSE(
      GetManager().GetAppIdForSystemApp(ash::SystemWebAppType::FACE_ML));
}

// Test that the Face ML will not be found when navigate to FaceML url.
IN_PROC_BROWSER_TEST_P(FaceMLAppDisabledIntegrationTest,
                       FaceMLAppNotFoundWhenNavigateToUrl) {
  const GURL url(ash::kChromeUIFaceMLAppURL);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetWebUI(),
            nullptr);
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    FaceMLAppDisabledIntegrationTest);
