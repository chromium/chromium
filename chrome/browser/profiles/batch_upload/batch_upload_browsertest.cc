// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

class BatchUploadWithFeatureOffBrowserTest : public InProcessBrowserTest {
 public:
  BatchUploadWithFeatureOffBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(switches::kBatchUploadDesktop);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BatchUploadWithFeatureOffBrowserTest, BatchUploadNull) {
  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(browser()->profile());
  EXPECT_FALSE(batch_upload);
}

// TODO(b/359146556): Provide more meaningful tests when dummy implementations
// are removed and the actual data providers are implemented.
class BatchUploadBrowserTest : public InProcessBrowserTest {
 public:
  // Opens the batch upload dialog using `batch_upload_service` in `browser.
  // Waits for the batch upload url to load if opening the view was successful.
  bool OpenBatchUpload(BatchUploadService* batch_upload_service,
                       Browser* browser) {
    content::TestNavigationObserver observer{
        GURL(chrome::kChromeUIBatchUploadURL)};
    observer.StartWatchingNewWebContents();

    bool is_opened = batch_upload_service->OpenBatchUpload(browser);
    if (is_opened) {
      observer.Wait();
    }

    return is_opened;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kBatchUploadDesktop};
};

IN_PROC_BROWSER_TEST_F(BatchUploadBrowserTest, OpenBatchUpload) {
  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(batch_upload);

  EXPECT_TRUE(OpenBatchUpload(batch_upload, browser()));
}

IN_PROC_BROWSER_TEST_F(
    BatchUploadBrowserTest,
    ClosingBrowserWithBatchUploadShouldStillAlowYouToOpenANewOne) {
  Profile* profile = browser()->profile();
  Browser* browser_2 = CreateBrowser(profile);

  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(batch_upload);

  // Second browser opens dialog.
  EXPECT_TRUE(OpenBatchUpload(batch_upload, browser_2));

  // Trying to open a dialog while it is still opened on another browser fails.
  // Only one batch upload dialog should be shown at a time per profile.
  EXPECT_FALSE(OpenBatchUpload(batch_upload, browser()));

  // Closing the browser that is displaying the dialog.
  CloseBrowserSynchronously(browser_2);

  // We can now display the dialog on the other browser.
  EXPECT_TRUE(OpenBatchUpload(batch_upload, browser()));
}

IN_PROC_BROWSER_TEST_F(BatchUploadBrowserTest,
                       ShouldShowBatchUploadEntryPointForDataTypePasswords) {
  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(batch_upload);

  EXPECT_TRUE(batch_upload->ShouldShowBatchUploadEntryPointForDataType(
      BatchUploadDataType::kPasswords));
}

IN_PROC_BROWSER_TEST_F(BatchUploadBrowserTest,
                       ShouldShowBatchUploadEntryPointForDataTypeAddresses) {
  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(batch_upload);

  EXPECT_TRUE(batch_upload->ShouldShowBatchUploadEntryPointForDataType(
      BatchUploadDataType::kAddresses));
}
