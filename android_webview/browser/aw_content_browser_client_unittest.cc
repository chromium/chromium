// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_content_browser_client.h"

#include "android_webview/browser/aw_feature_list_creator.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class AwContentBrowserClientTest : public testing::Test {
 protected:
  void SetUp() override {
    mojo::core::Init();
  }
};

TEST_F(AwContentBrowserClientTest, DisableCreatingThreadPool) {
  AwFeatureListCreator aw_feature_list_creator;
  AwContentBrowserClient client(&aw_feature_list_creator);
  EXPECT_TRUE(client.CreateThreadPool("Hello"));
  EXPECT_TRUE(base::ThreadPoolInstance::Get());

  // Have to start the threed pool to shut it down, have to shut it down to
  // destroy it.
  base::ThreadPoolInstance::Get()->Start(
      base::ThreadPoolInstance::InitParams(1));
  base::ThreadPoolInstance::Get()->Shutdown();
  base::ThreadPoolInstance::Get()->JoinForTesting();
  base::ThreadPoolInstance::Set(nullptr);

  AwContentBrowserClient::DisableCreatingThreadPool();
  EXPECT_FALSE(client.CreateThreadPool("Hello"));
  EXPECT_FALSE(base::ThreadPoolInstance::Get());
}

}  // namespace android_webview
