// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/system_web_app_manager_browsertest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"

class CameraAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  CameraAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kCameraSystemWebApp}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(CameraAppIntegrationTest, MainUrlNavigation) {
  WaitForTestSystemAppInstall();

  GURL main_camera_app_url("chrome://camera-app/views/main.html");
  content::TestNavigationObserver navigation_observer(main_camera_app_url);
  navigation_observer.StartWatchingNewWebContents();
  ASSERT_EQ(1u, chrome::GetTotalBrowserCount());

  ui_test_utils::SendToOmniboxAndSubmit(browser(), main_camera_app_url.spec());
  navigation_observer.Wait();

  // We now have two browsers, one for the chrome window, one for the Camera
  // app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(main_camera_app_url, chrome::FindLastActive()
                                     ->tab_strip_model()
                                     ->GetActiveWebContents()
                                     ->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_P(CameraAppIntegrationTest, OtherPageUrlNavigation) {
  WaitForTestSystemAppInstall();

  // TODO(crbug.com/980846): Change it to test page once the corresponding CL is
  // merged.
  GURL other_page_camera_app_url("chrome://camera-app/js/main.js");
  ASSERT_EQ(1u, chrome::GetTotalBrowserCount());

  ui_test_utils::SendToOmniboxAndSubmit(browser(),
                                        other_page_camera_app_url.spec());

  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(
      other_page_camera_app_url,
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_MANIFEST_INSTALL_P(
    CameraAppIntegrationTest);
