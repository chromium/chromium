// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_manager_jstest_base.h"
#include "content/public/test/browser_test.h"

class ZipArchiverJsTest : public FileManagerJsTestBase {
 protected:
  ZipArchiverJsTest()
      : FileManagerJsTestBase(base::FilePath(FILE_PATH_LITERAL(
            "chrome/browser/resources/chromeos/zip_archiver/test"))) {}
};

IN_PROC_BROWSER_TEST_F(ZipArchiverJsTest, RequestTest) {
  RunTest(base::FilePath(FILE_PATH_LITERAL("request_unittest.html")));
}
