// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/prefetch/aw_prefetch_manager.h"

#include "android_webview/browser/metrics/aw_metrics_test_utils.h"
#include "android_webview/browser/prefetch/aw_prefetch_manager_data.h"
#include "android_webview/common/aw_features.h"
#include "base/android/jni_android.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class AwPrefetchManagerTest : public AwMetricsTestBase {
 public:
  AwPrefetchManagerTest() = default;

  template <typename... TaskEnvironmentTraits>
  explicit AwPrefetchManagerTest(TaskEnvironmentTraits&&... traits)
      : AwMetricsTestBase(std::forward<TaskEnvironmentTraits>(traits)...) {}

 protected:
  void SetUp() override {
    AwMetricsTestBase::SetUp();
    env_ = base::android::AttachCurrentThread();
    browser_context_ = std::make_unique<content::TestBrowserContext>();
    prefs_->registry()->RegisterStringPref(prefs::kAwPrefetchLatestOrigin, "");
    prefs_->registry()->RegisterBooleanPref(
        prefs::kAwPrefetchLatestJavascriptEnabled, false);
    AwPrefetchManager::SetPrefServiceForTesting(prefs_.get());
  }

  void TearDown() override {
    AwPrefetchManager::SetPrefServiceForTesting(nullptr);
    browser_context_.reset();
    AwMetricsTestBase::TearDown();
  }

  raw_ptr<JNIEnv> env_;
  std::unique_ptr<content::TestBrowserContext> browser_context_;
};

// Tests Max Prefetches and TTL Seconds setter APIs
TEST_F(AwPrefetchManagerTest, UpdateCacheConfig) {
  AwPrefetchManager prefetch_manager(browser_context_.get());

  int actual_ttl_in_sec = 30 * 10;
  size_t actual_max_prefetches = 5;

  int default_ttl_in_sec = kDefaultTtlInSec;
  size_t default_max_prefetches = kDefaultMaxPrefetches;

  prefetch_manager.SetTtlInSec(env_, actual_ttl_in_sec);
  prefetch_manager.SetMaxPrefetches(env_, actual_max_prefetches);

  EXPECT_EQ(actual_ttl_in_sec, prefetch_manager.GetTtlInSec(env_));
  EXPECT_EQ(actual_max_prefetches, prefetch_manager.GetMaxPrefetches(env_));

  prefetch_manager.ClearTtl(env_);
  prefetch_manager.ClearMaxPrefetches(env_);

  EXPECT_EQ(default_ttl_in_sec, prefetch_manager.GetTtlInSec(env_));
  EXPECT_EQ(default_max_prefetches, prefetch_manager.GetMaxPrefetches(env_));
}

TEST_F(AwPrefetchManagerTest, MaxPrefetchReachesLimit) {
  AwPrefetchManager prefetch_manager(browser_context_.get());

  prefetch_manager.SetTtlInSec(env_, /*ttl_in_sec=*/60 * 10);

  prefetch_manager.SetMaxPrefetches(env_, /* max_prefetches=*/3);

  // Add more prefetch requests than the limit.
  for (int i = 0; i < 5; ++i) {
    prefetch_manager.StartPrefetchRequest(
        env_, "https://example.com/" + base::NumberToString(i),
        /*prefetch_params=*/nullptr, /*callback=*/nullptr,
        /*callback_executor=*/nullptr);
  }

  // Check the number of prefetches after exceeding the limit.
  EXPECT_EQ(prefetch_manager.GetAllPrefetchKeysForTesting().size(), 3u);

  // Add one more to trigger a removal
  prefetch_manager.StartPrefetchRequest(
      env_, "https://example.com/last", /*prefetch_params=*/nullptr,
      /*callback=*/nullptr, /*callback_executor=*/nullptr);
  EXPECT_EQ(prefetch_manager.GetAllPrefetchKeysForTesting().size(),
            3u);  // Should still be at the limit
}

TEST_F(AwPrefetchManagerTest, RemoveOldestPrefetchHandle) {
  AwPrefetchManager prefetch_manager(browser_context_.get());

  prefetch_manager.SetTtlInSec(env_, /*ttl_in_sec=*/60 * 10);

  prefetch_manager.SetMaxPrefetches(env_, /* max_prefetches=*/2);

  // 1. Make two requests.
  prefetch_manager.StartPrefetchRequest(
      env_, "https://example.com/0", /*prefetch_params=*/nullptr,
      /*callback=*/nullptr, /*callback_executor=*/nullptr);

  prefetch_manager.StartPrefetchRequest(
      env_, "https://example.com/1", /*prefetch_params=*/nullptr,
      /*callback=*/nullptr, /*callback_executor=*/nullptr);

  // 2. Capture the initial prefetches.
  std::vector<int32_t> initial_prefetches =
      prefetch_manager.GetAllPrefetchKeysForTesting();
  EXPECT_EQ(initial_prefetches.size(), 2u);

  // 3. Do the third request.
  prefetch_manager.StartPrefetchRequest(
      env_, "https://example.com/2", /*prefetch_params=*/nullptr,
      /*callback=*/nullptr, /*callback_executor=*/nullptr);

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

  prefetch_manager.SetTtlInSec(env_, /*ttl_in_sec=*/60 * 10);

  // set MaxPrefetches to a big number, 5.
  prefetch_manager.SetMaxPrefetches(env_, /* max_prefetches=*/5);

  // Make five requests.
  for (int i = 0; i < 5; ++i) {
    prefetch_manager.StartPrefetchRequest(
        env_, "https://example.com/" + base::NumberToString(i),
        /*prefetch_params=*/nullptr, /*callback=*/nullptr,
        /*callback_executor=*/nullptr);
  }
  EXPECT_EQ(prefetch_manager.GetAllPrefetchKeysForTesting().size(), 5u);

  // Now, let's lower that number with more than 1. Let's say 2.
  prefetch_manager.SetMaxPrefetches(env_, /* max_prefetches=*/2);

  // Adding another request.
  prefetch_manager.StartPrefetchRequest(
      env_, "https://example.com/6", /*prefetch_params=*/nullptr,
      /*callback=*/nullptr, /*callback_executor=*/nullptr);

  // Should be on the latest setting, 2.
  EXPECT_EQ(prefetch_manager.GetAllPrefetchKeysForTesting().size(), 2u);
}

TEST_F(AwPrefetchManagerTest, PrefetchHandleKeysAlwaysIncrement) {
  AwPrefetchManager prefetch_manager(browser_context_.get());

  prefetch_manager.SetTtlInSec(env_, /*ttl_in_sec=*/60 * 10);
  prefetch_manager.SetMaxPrefetches(env_, /* max_prefetches=*/5);

  // Confirm the initial values.
  int last_prefetch_key = prefetch_manager.GetLastPrefetchKeyForTesting();
  EXPECT_EQ(last_prefetch_key, -1);

  // Add more than the max allowed prefetches (triggering evictions) while
  // ensuring that the prefetch handle keys always increment confirming that the
  // prefetches are both sorted in the order they were added and that their keys
  // are never reused.
  for (int i = 0; i < 10; ++i) {
    int prefetch_key = prefetch_manager.StartPrefetchRequest(
        env_, "https://example.com/" + base::NumberToString(i),
        /*prefetch_params=*/nullptr, /*callback=*/nullptr,
        /*callback_executor=*/nullptr);
    EXPECT_EQ(prefetch_key, last_prefetch_key + 1);
    last_prefetch_key = prefetch_key;
  }
}

class AwPrefetchManagerNoNetworkServiceDedicatedThreadTest
    : public AwPrefetchManagerTest {
 public:
  AwPrefetchManagerNoNetworkServiceDedicatedThreadTest()
      : AwPrefetchManagerTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            content::BrowserTaskEnvironment::REAL_IO_THREAD) {
    // A short-term workaround for new tests to mitigate the integration
    // issue between the dedicated `NetworkService` thread and
    // `BrowserTaskEnvironment` (see crbug.com/493322520). If disabled,
    // `NetworkService` will use IO thread instead. This is valid for these
    // tests because they do not depend on whether the network service is
    // running on a dedicated thread or the IO thread.
    scoped_feature_list_.InitAndDisableFeature(
        content::kNetworkServiceDedicatedThread);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AwPrefetchManagerNoNetworkServiceDedicatedThreadTest,
       DeduplicationWebViewPrefetchOffTheMainThreadDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kWebViewPrefetchOffTheMainThread);

  const std::string prefetch_url = "https://example.com";
  const int ttl_in_sec = 10;

  AwPrefetchManager prefetch_manager(browser_context_.get());
  prefetch_manager.SetTtlInSec(env_, ttl_in_sec);
  prefetch_manager.SetMaxPrefetches(env_, /*max_prefetches=*/5);

  // 1. First request should succeed.
  int key1 = prefetch_manager.StartPrefetchRequest(
      env_, prefetch_url, /*prefetch_params=*/nullptr, /*callback=*/nullptr,
      /*callback_executor=*/nullptr);
  EXPECT_NE(key1, NO_PREFETCH_KEY);

  // 2. Second request for same URL should fail due to deduplication in manager.
  int key2 = prefetch_manager.StartPrefetchRequest(
      env_, prefetch_url, /*prefetch_params=*/nullptr, /*callback=*/nullptr,
      /*callback_executor=*/nullptr);
  EXPECT_EQ(key2, NO_PREFETCH_KEY);

  // 3. Forward the time after TTL.
  task_environment_.FastForwardBy(base::Seconds(ttl_in_sec + 1));

  // 4. Third request for same URL should succeed because prefetch is expired
  // in `PrefetchService`.
  int key3 = prefetch_manager.StartPrefetchRequest(
      env_, prefetch_url, /*prefetch_params=*/nullptr, /*callback=*/nullptr,
      /*callback_executor=*/nullptr);
  EXPECT_NE(key3, NO_PREFETCH_KEY);
}

TEST_F(AwPrefetchManagerNoNetworkServiceDedicatedThreadTest,
       DeduplicationWebViewPrefetchOffTheMainThreadEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kWebViewPrefetchOffTheMainThread,
                                 ::features::kPrefetchOffTheMainThread},
                                {});

  const std::string prefetch_url = "https://example.com";
  const int ttl_in_sec = 10;

  AwPrefetchManager prefetch_manager(browser_context_.get());
  prefetch_manager.SetTtlInSec(env_, ttl_in_sec);
  prefetch_manager.SetMaxPrefetches(env_, /*max_prefetches=*/5);

  // 1. First request should succeed.
  int key1 = prefetch_manager.StartPrefetchRequest(
      env_, prefetch_url, /*prefetch_params=*/nullptr, /*callback=*/nullptr,
      /*callback_executor=*/nullptr);
  EXPECT_NE(key1, NO_PREFETCH_KEY);

  // 2. Second request for same URL should fail due to deduplication in manager.
  int key2 = prefetch_manager.StartPrefetchRequest(
      env_, prefetch_url, /*prefetch_params=*/nullptr, /*callback=*/nullptr,
      /*callback_executor=*/nullptr);
  EXPECT_EQ(key2, NO_PREFETCH_KEY);

  // 3. Forward the time after TTL.
  task_environment_.FastForwardBy(base::Seconds(ttl_in_sec + 1));

  // 4. Third request for same URL should still fail because `AwPrefetchManager`
  // doesn't track staleness.
  int key3 = prefetch_manager.StartPrefetchRequest(
      env_, prefetch_url, /*prefetch_params=*/nullptr, /*callback=*/nullptr,
      /*callback_executor=*/nullptr);
  EXPECT_EQ(key3, NO_PREFETCH_KEY);
}

// Tests that the latest prefetch origin and JavaScript enabled status are
// updated on a (pre)prefetch request.
TEST_F(AwPrefetchManagerNoNetworkServiceDedicatedThreadTest,
       UpdatePrefsOnPrefetchRequest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kWebViewPrefetchOffTheMainThread,
                                 ::features::kPrefetchOffTheMainThread},
                                {});

  prefs_->SetString(prefs::kAwPrefetchLatestOrigin, "");
  prefs_->SetBoolean(prefs::kAwPrefetchLatestJavascriptEnabled, true);

  AwPrefetchManager prefetch_manager(browser_context_.get());

  ASSERT_EQ(prefs_->GetString(prefs::kAwPrefetchLatestOrigin), "");
  ASSERT_TRUE(prefs_->GetBoolean(prefs::kAwPrefetchLatestJavascriptEnabled));

  // Start a prefetch request.
  const std::string prefetch_url = "https://example.com/foo";
  prefetch_manager.StartPrefetchRequest(env_, prefetch_url,
                                        /*prefetch_params=*/nullptr,
                                        /*callback=*/nullptr,
                                        /*callback_executor=*/nullptr);

  // A prefetch request should update the latest prefetch origin.
  // Also, the latest JavaScript enabled status should be updated to false,
  // since `prefetch_params` is nullptr in this test environment.
  EXPECT_EQ(prefs_->GetString(prefs::kAwPrefetchLatestOrigin),
            url::Origin::Create(GURL(prefetch_url)).Serialize());
  EXPECT_FALSE(prefs_->GetBoolean(prefs::kAwPrefetchLatestJavascriptEnabled));

  // Start a pre-prefetch request with a different origin.
  const std::string pre_prefetch_url = "https://another.example.com/foo";
  base::test::TestFuture<AwPrefetchKey> prefetch_key_future;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](AwPrefetchManager* manager_ptr, JNIEnv* env,
             const std::string& url) {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow_blocking;
            return manager_ptr->StartPrePrefetchRequest(
                env, url, /*prefetch_params=*/nullptr,
                /*callback=*/nullptr, /*callback_executor=*/nullptr);
          },
          &prefetch_manager, env_.get(), pre_prefetch_url),
      prefetch_key_future.GetCallback());

  std::ignore = prefetch_key_future.Take();

  // The latest prefetch origin should be updated to the origin of the
  // PrePrefetch request.
  EXPECT_EQ(prefs_->GetString(prefs::kAwPrefetchLatestOrigin),
            url::Origin::Create(GURL(pre_prefetch_url)).Serialize());
  EXPECT_FALSE(prefs_->GetBoolean(prefs::kAwPrefetchLatestJavascriptEnabled));
}

}  // namespace android_webview
