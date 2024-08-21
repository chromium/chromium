// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(b/359146556): Provide more meaningful tests when dummy implementations
// are removed and the actual data providers are implemented.
class BatchUploadBrowserTest : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(BatchUploadBrowserTest, OpenBatchUpload) {
  Profile* profile = browser()->profile();
  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(profile);

  EXPECT_TRUE(batch_upload->OpenBatchUpload(browser()));

  // Only one batch upload dialog should be shown at a time per profile.
  EXPECT_FALSE(batch_upload->OpenBatchUpload(browser()));
  Browser* new_browser = CreateBrowser(profile);
  // Even on other browser windows.
  EXPECT_FALSE(batch_upload->OpenBatchUpload(new_browser));

  // Notify that the dialog was closed.
  batch_upload->CloseDialogForTesting();

  // Dialog can be opened again.
  EXPECT_TRUE(batch_upload->OpenBatchUpload(browser()));
}

IN_PROC_BROWSER_TEST_F(BatchUploadBrowserTest,
                       ShouldShowBatchUploadEntryPointForDataTypePasswords) {
  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(batch_upload->ShouldShowBatchUploadEntryPointForDataType(
      BatchUploadDataType::kPasswords));
}

IN_PROC_BROWSER_TEST_F(BatchUploadBrowserTest,
                       ShouldShowBatchUploadEntryPointForDataTypeAddresses) {
  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(batch_upload->ShouldShowBatchUploadEntryPointForDataType(
      BatchUploadDataType::kAddresses));
}
