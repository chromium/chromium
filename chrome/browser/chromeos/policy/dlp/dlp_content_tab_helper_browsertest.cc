// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_tab_helper.h"

#include "base/path_service.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_content_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"

namespace policy {

using testing::_;
using testing::Return;

namespace {

const DlpContentRestrictionSet kEmptyRestrictionSet;
const DlpContentRestrictionSet kScreenshotRestrictionSet(
    DlpContentRestriction::kScreenshot);

}  // namespace

class DlpContentTabHelperBrowserTest
    : public extensions::PlatformAppBrowserTest {
 public:
  DlpContentTabHelperBrowserTest()
      : scoped_dlp_content_manager_(&mock_dlp_content_manager_) {}

 protected:
  void SetUp() override {
    extensions::PlatformAppBrowserTest::SetUp();
  }

  void TearDown() override {
    extensions::PlatformAppBrowserTest::TearDown();
  }

  MockDlpContentManager mock_dlp_content_manager_;
  ScopedDlpContentManagerForTesting scoped_dlp_content_manager_;
};

IN_PROC_BROWSER_TEST_F(DlpContentTabHelperBrowserTest, PlatformApp) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ExtensionTestMessageListener launched_listener("Launched", false);

  // Install Platform App
  content::WindowedNotificationObserver app_loaded_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  const extensions::Extension* extension = InstallPlatformApp("dlp_test");
  ASSERT_TRUE(extension);

  // Restrict screenshot for Platform App
  GURL kUrl = GURL("chrome-extension://" + extension->id() + "/index.html");
  EXPECT_CALL(mock_dlp_content_manager_, GetRestrictionSetForURL(GURL()))
      .Times(1)
      .WillOnce(Return(kEmptyRestrictionSet));
  EXPECT_CALL(mock_dlp_content_manager_, GetRestrictionSetForURL(kUrl))
      .Times(1)
      .WillOnce(Return(kScreenshotRestrictionSet));
  EXPECT_CALL(mock_dlp_content_manager_,
              OnConfidentialityChanged(_, kScreenshotRestrictionSet))
      .Times(1);
  EXPECT_CALL(mock_dlp_content_manager_, OnVisibilityChanged(_)).Times(1);

  // Launch Platform App
  LaunchPlatformApp(extension);
  app_loaded_observer.Wait();
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  content::WebContents* web_contents = GetFirstAppWindowWebContents();
  EXPECT_TRUE(web_contents);
  EXPECT_NE(nullptr,
            policy::DlpContentTabHelper::FromWebContents(web_contents));
  EXPECT_CALL(mock_dlp_content_manager_,
              OnConfidentialityChanged(_, kEmptyRestrictionSet))
      .Times(1);
  EXPECT_CALL(mock_dlp_content_manager_, OnWebContentsDestroyed(_)).Times(2);
}

}  // namespace policy
