// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_jstest_base.h"
#include "content/public/test/browser_test.h"

class ImageLoaderJsTest : public FileManagerJsTestBase {
 protected:
  ImageLoaderJsTest() : FileManagerJsTestBase(
      base::FilePath(FILE_PATH_LITERAL("ui/file_manager/image_loader"))) {}
};

IN_PROC_BROWSER_TEST_F(ImageLoaderJsTest, ImageLoaderClientTest) {
  RunTestURL("image_loader_client_unittest_gen.html");
}

IN_PROC_BROWSER_TEST_F(ImageLoaderJsTest, CacheTest) {
  RunTestURL("cache_unittest_gen.html");
}

IN_PROC_BROWSER_TEST_F(ImageLoaderJsTest, ImageLoaderTest) {
  RunTestURL("image_loader_unittest_gen.html");
}

IN_PROC_BROWSER_TEST_F(ImageLoaderJsTest, SchedulerTest) {
  RunTestURL("scheduler_unittest_gen.html");
}
