// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_auto_fetcher.h"

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/offline_pages/test_request_coordinator_builder.h"
#include "chrome/test/base/testing_profile.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/test_request_queue_store.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {
using OfflinePageAutoFetcherScheduleResult =
    chrome::mojom::OfflinePageAutoFetcherScheduleResult;

class OfflinePageAutoFetcherTest : public testing::Test {
 public:
  void SetUp() override {
    RequestCoordinator* coordinator = static_cast<RequestCoordinator*>(
        RequestCoordinatorFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_, base::BindRepeating(&BuildTestRequestCoordinator)));
    queue_store_ = static_cast<TestRequestQueueStore*>(
        coordinator->queue_for_testing()->GetStoreForTesting());
  }

  void TearDown() override {
    browser_thread_bundle_.RunUntilIdle();
    ASSERT_TRUE(fetcher_.IsTaskQueueEmptyForTesting());
  }

  TestRequestQueueStore* queue_store() { return queue_store_; }

  std::vector<std::unique_ptr<SavePageRequest>> GetRequestsSync() {
    bool completed = false;
    std::vector<std::unique_ptr<SavePageRequest>> result;
    queue_store_->GetRequests(base::BindLambdaForTesting(
        [&](bool success,
            std::vector<std::unique_ptr<SavePageRequest>> requests) {
          completed = true;
          result = std::move(requests);
        }));
    browser_thread_bundle_.RunUntilIdle();
    CHECK(completed);
    return result;
  }

 protected:
  content::TestBrowserThreadBundle browser_thread_bundle_;
  TestingProfile profile_;

  // Owned by the request queue.
  TestRequestQueueStore* queue_store_ = nullptr;

  OfflinePageAutoFetcher fetcher_{&profile_};
};

TEST_F(OfflinePageAutoFetcherTest, TryScheduleSuccess) {
  base::MockCallback<
      base::OnceCallback<void(OfflinePageAutoFetcherScheduleResult)>>
      result_callback;
  EXPECT_CALL(result_callback,
              Run(OfflinePageAutoFetcherScheduleResult::kScheduled));
  fetcher_.TrySchedule(false, GURL("http://foo.com"), result_callback.Get());
  browser_thread_bundle_.RunUntilIdle();
  EXPECT_EQ(1ul, GetRequestsSync().size());
}

TEST_F(OfflinePageAutoFetcherTest, AttemptInvalidURL) {
  base::MockCallback<
      base::OnceCallback<void(OfflinePageAutoFetcherScheduleResult)>>
      result_callback;
  EXPECT_CALL(result_callback,
              Run(OfflinePageAutoFetcherScheduleResult::kOtherError));
  fetcher_.TrySchedule(false, GURL("ftp://foo.com"), result_callback.Get());
  browser_thread_bundle_.RunUntilIdle();
  EXPECT_EQ(0ul, GetRequestsSync().size());
}

TEST_F(OfflinePageAutoFetcherTest, TryScheduleDuplicate) {
  base::MockCallback<
      base::RepeatingCallback<void(OfflinePageAutoFetcherScheduleResult)>>
      result_callback;
  EXPECT_CALL(result_callback,
              Run(OfflinePageAutoFetcherScheduleResult::kScheduled))
      .Times(1);
  EXPECT_CALL(result_callback,
              Run(OfflinePageAutoFetcherScheduleResult::kAlreadyScheduled))
      .Times(1);
  // The page should only be saved once, because the fragment is ignored.
  fetcher_.TrySchedule(false, GURL("http://foo.com#A"), result_callback.Get());
  fetcher_.TrySchedule(false, GURL("http://foo.com#Z"), result_callback.Get());
  browser_thread_bundle_.RunUntilIdle();
  EXPECT_EQ(1ul, GetRequestsSync().size());
}

TEST_F(OfflinePageAutoFetcherTest, AttemptAutoScheduleMoreThanMaximum) {
  base::MockCallback<
      base::RepeatingCallback<void(OfflinePageAutoFetcherScheduleResult)>>
      result_callback;
  testing::InSequence in_sequence;
  EXPECT_CALL(result_callback,
              Run(OfflinePageAutoFetcherScheduleResult::kScheduled))
      .Times(3);
  EXPECT_CALL(result_callback,
              Run(OfflinePageAutoFetcherScheduleResult::kNotEnoughQuota))
      .Times(1);
  EXPECT_CALL(result_callback,
              Run(OfflinePageAutoFetcherScheduleResult::kScheduled))
      .Times(1);

  // Three requests within quota.
  fetcher_.TrySchedule(false, GURL("http://foo.com/1"), result_callback.Get());
  fetcher_.TrySchedule(false, GURL("http://foo.com/2"), result_callback.Get());
  fetcher_.TrySchedule(false, GURL("http://foo.com/3"), result_callback.Get());

  // Quota is exhausted.
  fetcher_.TrySchedule(false, GURL("http://foo.com/4"), result_callback.Get());

  // User-requested, quota is not enforced.
  fetcher_.TrySchedule(true, GURL("http://foo.com/5"), result_callback.Get());

  browser_thread_bundle_.RunUntilIdle();
}

TEST_F(OfflinePageAutoFetcherTest, TryScheduleMoreThanMaximumUserRequested) {
  base::MockCallback<
      base::RepeatingCallback<void(OfflinePageAutoFetcherScheduleResult)>>
      result_callback;
  EXPECT_CALL(result_callback,
              Run(OfflinePageAutoFetcherScheduleResult::kScheduled))
      .Times(4);
  fetcher_.TrySchedule(true, GURL("http://foo.com/1"), result_callback.Get());
  fetcher_.TrySchedule(true, GURL("http://foo.com/2"), result_callback.Get());
  fetcher_.TrySchedule(true, GURL("http://foo.com/3"), result_callback.Get());
  fetcher_.TrySchedule(true, GURL("http://foo.com/4"), result_callback.Get());
  browser_thread_bundle_.RunUntilIdle();
}

TEST_F(OfflinePageAutoFetcherTest, CancelSuccess) {
  fetcher_.TrySchedule(false, GURL("http://foo.com"), base::DoNothing());
  browser_thread_bundle_.RunUntilIdle();
  fetcher_.CancelSchedule(GURL("http://foo.com"));
  browser_thread_bundle_.RunUntilIdle();
  EXPECT_EQ(0ul, GetRequestsSync().size());
}

TEST_F(OfflinePageAutoFetcherTest, CancelNotExist) {
  fetcher_.TrySchedule(false, GURL("http://foo.com"), base::DoNothing());
  browser_thread_bundle_.RunUntilIdle();
  fetcher_.CancelSchedule(GURL("http://NOT-FOO.com"));
  browser_thread_bundle_.RunUntilIdle();
  EXPECT_EQ(1ul, GetRequestsSync().size());
}

TEST_F(OfflinePageAutoFetcherTest, CancelQueueEmpty) {
  fetcher_.CancelSchedule(GURL("http://foo.com"));
  browser_thread_bundle_.RunUntilIdle();
}

}  // namespace
}  // namespace offline_pages
