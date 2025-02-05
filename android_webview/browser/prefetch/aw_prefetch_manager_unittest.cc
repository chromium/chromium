// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/prefetch/aw_prefetch_manager.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_context_store.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_feature_list_creator.h"
#include "android_webview/common/aw_features.h"
#include "base/test/task_environment.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class AwPrefetchManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    test_content_client_initializer_ =
        new content::TestContentClientInitializer();
  }

  void TearDown() override {
    // Drain the message queue before destroying
    // |test_content_client_initializer_|, otherwise a posted task may call
    // content::GetNetworkConnectionTracker() after
    // TestContentClientInitializer's destructor sets it to null.
    base::RunLoop().RunUntilIdle();
    delete test_content_client_initializer_;
  }

  // Create the TestBrowserThreads.
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<content::TestContentClientInitializer>
      test_content_client_initializer_;
  content::TestBrowserContext browser_context_;
};

// Tests that setting the CacheConfig on the PrefetchManager applies it
// correctly.
TEST_F(AwPrefetchManagerTest, UpdateCacheConfig) {
  AwPrefetchManager prefetch_manager(&browser_context_);

  prefetch_manager.UpdatePrefetchConfiguration(/*ttl_in_sec*/ 60 * 10,
                                               /* max_prefetches*/ 5);

  EXPECT_EQ(prefetch_manager.GetTtlInSec(), 60 * 10);
  EXPECT_EQ(prefetch_manager.GetMaxPrefetches(), 5);
}
}  // namespace android_webview
