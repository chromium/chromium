// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/prefetch/aw_prefetch_manager.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_context_store.h"
#include "android_webview/common/aw_features.h"
#include "base/android/jni_android.h"
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
    browser_context_ = std::make_unique<content::TestBrowserContext>();
  }

  void TearDown() override {
    // Drain the message queue before destroying
    // |test_content_client_initializer_|, otherwise a posted task may call
    // content::GetNetworkConnectionTracker() after
    // TestContentClientInitializer's destructor sets it to null.
    base::RunLoop().RunUntilIdle();
    browser_context_.reset();
    delete test_content_client_initializer_;
  }

  // Create the TestBrowserThreads.
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<content::TestContentClientInitializer>
      test_content_client_initializer_;
  std::unique_ptr<content::TestBrowserContext> browser_context_;
};

// Tests that setting the CacheConfig on the PrefetchManager applies it
// correctly.
TEST_F(AwPrefetchManagerTest, UpdateCacheConfig) {
  AwPrefetchManager prefetch_manager(browser_context_.get());
  prefetch_manager.SetTtlInSec(base::android::AttachCurrentThread(),
                               /*ttl_in_sec=*/60 * 10);

  prefetch_manager.SetMaxPrefetches(base::android::AttachCurrentThread(),
                                    /* max_prefetches=*/5);

  EXPECT_EQ(prefetch_manager.GetTtlInSec(base::android::AttachCurrentThread()),
            60 * 10);
  EXPECT_EQ(
      prefetch_manager.GetMaxPrefetches(base::android::AttachCurrentThread()),
      5);
}

TEST_F(AwPrefetchManagerTest, MaxPrefetchReachesLimit) {
  AwPrefetchManager prefetch_manager(browser_context_.get());

  prefetch_manager.SetTtlInSec(base::android::AttachCurrentThread(),
                               /*ttl_in_sec=*/60 * 10);

  prefetch_manager.SetMaxPrefetches(base::android::AttachCurrentThread(),
                                    /* max_prefetches=*/3);

  // Add more prefetch requests than the limit.
  for (int i = 0; i < 5; ++i) {
    prefetch_manager.StartPrefetchRequest(
        base::android::AttachCurrentThread(),
        "https://example.com/" + base::NumberToString(i),
        /*prefetch_params=*/nullptr, /*callback=*/nullptr,
        /*callback_executor=*/nullptr);
  }

  // Check the number of prefetches after exceeding the limit.
  EXPECT_EQ(prefetch_manager.GetAllPrefetchKeysForTesting().size(), 3u);

  // Add one more to trigger a removal
  prefetch_manager.StartPrefetchRequest(
      base::android::AttachCurrentThread(), "https://example.com/last",
      /*prefetch_params=*/nullptr, /*callback=*/nullptr,
      /*callback_executor=*/nullptr);
  EXPECT_EQ(prefetch_manager.GetAllPrefetchKeysForTesting().size(),
            3u);  // Should still be at the limit
}

TEST_F(AwPrefetchManagerTest, RemoveOldestPrefetchHandle) {
  AwPrefetchManager prefetch_manager(browser_context_.get());

  prefetch_manager.SetTtlInSec(base::android::AttachCurrentThread(),
                               /*ttl_in_sec=*/60 * 10);

  prefetch_manager.SetMaxPrefetches(base::android::AttachCurrentThread(),
                                    /* max_prefetches=*/2);

  // 1. Make two requests.
  prefetch_manager.StartPrefetchRequest(
      base::android::AttachCurrentThread(), "https://example.com/0",
      /*prefetch_params=*/nullptr, /*callback=*/nullptr,
      /*callback_executor=*/nullptr);

  prefetch_manager.StartPrefetchRequest(
      base::android::AttachCurrentThread(), "https://example.com/1",
      /*prefetch_params=*/nullptr, /*callback=*/nullptr,
      /*callback_executor=*/nullptr);

  // 2. Capture the initial prefetches.
  std::vector<int32_t> initial_prefetches =
      prefetch_manager.GetAllPrefetchKeysForTesting();
  EXPECT_EQ(initial_prefetches.size(), 2u);

  // 3. Do the third request.
  prefetch_manager.StartPrefetchRequest(
      base::android::AttachCurrentThread(), "https://example.com/2",
      /*prefetch_params=*/nullptr, /*callback=*/nullptr,
      /*callback_executor=*/nullptr);

  std::vector<int32_t> current_prefetches =
      prefetch_manager.GetAllPrefetchKeysForTesting();
  EXPECT_EQ(current_prefetches.size(), 2u);

  // Verify that the oldest prefetch is removed.
  auto it0 = std::find(current_prefetches.cbegin(), current_prefetches.cend(),
                       initial_prefetches.front());
  EXPECT_EQ(it0, current_prefetches.cend());

  // Last added element isn't included in the initials
  auto it1 = std::find(initial_prefetches.cbegin(), initial_prefetches.cend(),
                       current_prefetches.back());
  EXPECT_EQ(it1, initial_prefetches.cend());

  EXPECT_EQ(current_prefetches.at(0), initial_prefetches.at(1));
}

TEST_F(AwPrefetchManagerTest, UpdateMaxPrefetchesIsRespected) {
  AwPrefetchManager prefetch_manager(browser_context_.get());

  prefetch_manager.SetTtlInSec(base::android::AttachCurrentThread(),
                               /*ttl_in_sec=*/60 * 10);

  // set MaxPrefetches to a big number, 5.
  prefetch_manager.SetMaxPrefetches(base::android::AttachCurrentThread(),
                                    /* max_prefetches=*/5);

  // Make five requests.
  for (int i = 0; i < 5; ++i) {
    prefetch_manager.StartPrefetchRequest(
        base::android::AttachCurrentThread(),
        "https://example.com/" + base::NumberToString(i),
        /*prefetch_params=*/nullptr, /*callback=*/nullptr,
        /*callback_executor=*/nullptr);
  }
  EXPECT_EQ(prefetch_manager.GetAllPrefetchKeysForTesting().size(), 5u);

  // Now, let's lower that number with more than 1. Let's say 2.
  prefetch_manager.SetMaxPrefetches(base::android::AttachCurrentThread(),
                                    /* max_prefetches=*/2);

  // Adding another request.
  prefetch_manager.StartPrefetchRequest(
      base::android::AttachCurrentThread(), "https://example.com/6",
      /*prefetch_params=*/nullptr, /*callback=*/nullptr,
      /*callback_executor=*/nullptr);

  // Should be on the latest setting, 2.
  EXPECT_EQ(prefetch_manager.GetAllPrefetchKeysForTesting().size(), 2u);
}

TEST_F(AwPrefetchManagerTest, PrefetchHandleKeysAlwaysIncrement) {
  AwPrefetchManager prefetch_manager(browser_context_.get());

  prefetch_manager.SetTtlInSec(base::android::AttachCurrentThread(),
                               /*ttl_in_sec=*/60 * 10);
  prefetch_manager.SetMaxPrefetches(base::android::AttachCurrentThread(),
                                    /* max_prefetches=*/5);

  // Confirm the initial values.
  int last_prefetch_key = prefetch_manager.GetLastPrefetchKeyForTesting();
  EXPECT_EQ(last_prefetch_key, -1);

  // Add more than the max allowed prefetches (triggering evictions) while
  // ensuring that the prefetch handle keys always increment confirming that the
  // prefetches are both sorted in the order they were added and that their keys
  // are never reused.
  for (int i = 0; i < 10; ++i) {
    int prefetch_key = prefetch_manager.StartPrefetchRequest(
        base::android::AttachCurrentThread(),
        "https://example.com/" + base::NumberToString(i),
        /*prefetch_params=*/nullptr, /*callback=*/nullptr,
        /*callback_executor=*/nullptr);
    EXPECT_EQ(prefetch_key, last_prefetch_key + 1);
    last_prefetch_key = prefetch_key;
  }
}

}  // namespace android_webview
