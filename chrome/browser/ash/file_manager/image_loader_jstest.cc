// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/file_manager/file_manager_jstest_base.h"
#include "content/public/test/browser_test.h"

class ImageLoaderJsTest : public FileManagerJsTestBase {
 protected:
  ImageLoaderJsTest()
      : FileManagerJsTestBase(
            base::FilePath(FILE_PATH_LITERAL("image_loader"))) {}

  void SetUpCommandLine(base::CommandLine* command_lin) override {
    // Until Files SWA is fully launched Image Loader imports using
    // chrome-extension://hh.../ so we force SWA disabled here.
    feature_list_.InitAndDisableFeature(chromeos::features::kFilesSWA);
  }

  base::test::ScopedFeatureList feature_list_;
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
