// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_manager_jstest_base.h"
#include "content/public/test/browser_test.h"

class ImageLoaderJsTest : public FileManagerJsTestBase {
 protected:
  ImageLoaderJsTest()
      : FileManagerJsTestBase(
            base::FilePath(FILE_PATH_LITERAL("image_loader"))) {}
};

// Tests that draw to canvases and test pixels need pixel output turned on.
class CanvasImageLoaderJsTest : public ImageLoaderJsTest {
 public:
  void SetUp() override {
    EnablePixelOutput();
    ImageLoaderJsTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(ImageLoaderJsTest, ImageLoaderClientTest) {
  RunTestURL("image_loader_client_unittest.js");
}

IN_PROC_BROWSER_TEST_F(ImageLoaderJsTest, CacheTest) {
  RunTestURL("cache_unittest.js");
}

IN_PROC_BROWSER_TEST_F(ImageLoaderJsTest, ImageLoaderTest) {
  RunTestURL("image_loader_unittest.js");
}

IN_PROC_BROWSER_TEST_F(ImageLoaderJsTest, SchedulerTest) {
  RunTestURL("scheduler_unittest.js");
}

IN_PROC_BROWSER_TEST_F(CanvasImageLoaderJsTest, ImageOrientation) {
  RunTestURL("image_orientation_unittest.js");
}
