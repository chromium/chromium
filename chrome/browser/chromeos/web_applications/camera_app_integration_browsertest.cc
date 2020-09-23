// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/system_web_app_manager_browsertest.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"

class CameraAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  CameraAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kCameraSystemWebApp}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/1129340): Remove this test after CCA supports responsive UI.
// Test that the window of Camera App is not resizeable.
IN_PROC_BROWSER_TEST_P(CameraAppIntegrationTest, WindowNotResizeable) {
  WaitForTestSystemAppInstall();
  Browser* browser;
  LaunchApp(web_app::SystemAppType::CAMERA, &browser);
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  EXPECT_FALSE(browser_view->CanResize());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_MANIFEST_INSTALL_P(
    CameraAppIntegrationTest);
