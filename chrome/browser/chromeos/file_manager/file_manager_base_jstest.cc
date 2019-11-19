// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_jstest_base.h"

class FileManagerBaseJsTest : public FileManagerJsTestBase {
 protected:
  FileManagerBaseJsTest()
      : FileManagerJsTestBase(
            base::FilePath(FILE_PATH_LITERAL("ui/file_manager/base/js"))) {}
};

IN_PROC_BROWSER_TEST_F(FileManagerBaseJsTest, VolumeManagerTypesTest) {
  RunTestURL("volume_manager_types_unittest_gen.html");
}
