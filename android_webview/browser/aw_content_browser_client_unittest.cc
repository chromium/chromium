// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_content_browser_client.h"

#include "android_webview/browser/aw_feature_list_creator.h"
#include "base/test/task_environment.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class AwContentBrowserClientTest : public testing::Test {
 protected:
  void SetUp() override {
    mojo::core::Init();
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(AwContentBrowserClientTest, DisableCreatingThreadPool) {
  AwFeatureListCreator aw_feature_list_creator;
  AwContentBrowserClient client(&aw_feature_list_creator);
  EXPECT_TRUE(client.ShouldCreateThreadPool());

  AwContentBrowserClient::DisableCreatingThreadPool();
  EXPECT_FALSE(client.ShouldCreateThreadPool());
}

}  // namespace android_webview
