// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload.h"

#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(b/359146556): Provide more meaningful tests when dummy implementations
// are removed and the actual data providers are implemented.
class BatchUploadBrowserTest : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(BatchUploadBrowserTest, OpenBatchUpload) {
  EXPECT_TRUE(OpenBatchUpload(browser()));
}

IN_PROC_BROWSER_TEST_F(BatchUploadBrowserTest,
                       ShouldShowBatchUploadEntryPointForDataTypePasswords) {
  EXPECT_TRUE(ShouldShowBatchUploadEntryPointForDataType(
      *browser()->profile(), BatchUploadDataType::kPasswords));
}

IN_PROC_BROWSER_TEST_F(BatchUploadBrowserTest,
                       ShouldShowBatchUploadEntryPointForDataTypeAddresses) {
  EXPECT_TRUE(ShouldShowBatchUploadEntryPointForDataType(
      *browser()->profile(), BatchUploadDataType::kAddresses));
}
