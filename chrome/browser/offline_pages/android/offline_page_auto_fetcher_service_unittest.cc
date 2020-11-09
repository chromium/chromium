// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher_service.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/offline_pages/test_request_coordinator_builder.h"
#include "chrome/common/offline_page_auto_fetcher.mojom.h"
#include "components/offline_pages/core/auto_fetch.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/request_coordinator_stub_taco.h"
#include "components/offline_pages/core/background/test_request_queue_store.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {
using testing::_;
using OfflinePageAutoFetcherScheduleResult =
    chrome::mojom::OfflinePageAutoFetcherScheduleResult;

const int kTabId = 1;
ClientId TestClientId() {
  return auto_fetch::MakeClientId(auto_fetch::ClientIdMetadata(kTabId));
}
SavePageRequest TestRequest(ClientId client_id = TestClientId()) {
  return SavePageRequest(123, GURL("http://www.url.com"), client_id,
                         base::Time(), true);
}

class MockDelegate : public OfflinePageAutoFetcherService::Delegate {
 public:
  MOCK_METHOD5(ShowAutoFetchCompleteNotification,
               void(const base::string16& pageTitle,
                    const std::string& original_url,
                    const std::string& final_url,
                    int android_tab_id,
                    int64_t offline_id));
};

class TestOfflinePageModel : public StubOfflinePageModel {
 public:
  // Change signature for the mocked method to make it easier to use.
  MOCK_METHOD1(GetPageByOfflineId_, OfflinePageItem*(int64_t offline_id));
  void GetPageByOfflineId(int64_t offline_id,
                          SingleOfflinePageItemCallback callback) override {
    std::move(callback).Run(GetPageByOfflineId_(offline_id));
  }
};

class OfflinePageAutoFetcherServiceTest : public testing::Test {
 public:
  void SetUp() override {
    request_coordinator_taco_.CreateRequestCoordinator();
    service_ = std::make_unique<OfflinePageAutoFetcherService>(
        request_coordinator(), &offline_page_model_, &delegate_);
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
    service_.reset();
  }
  RequestCoordinator* request_coordinator() {
    return request_coordinator_taco_.request_coordinator();
  }
  TestRequestQueueStore* queue_store() {
    return static_cast<TestRequestQueueStore*>(
        request_coordinator()->queue_for_testing()->GetStoreForTesting());
  }

  std::vector<std::unique_ptr<SavePageRequest>> GetRequestsSync() {
    bool completed = false;
    std::vector<std::unique_ptr<SavePageRequest>> result;
    queue_store()->GetRequests(base::BindLambdaForTesting(
        [&](bool success,
            std::vector<std::unique_ptr<SavePageRequest>> requests) {
          completed = true;
          result = std::move(requests);
        }));
    task_environment_.RunUntilIdle();
    CHECK(completed);
    return result;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  MockDelegate delegate_;
  TestOfflinePageModel offline_page_model_;

  RequestCoordinatorStubTaco request_coordinator_taco_;
  std::unique_ptr<OfflinePageAutoFetcherService> service_;
};

TEST_F(OfflinePageAutoFetcherServiceTest, TryScheduleSuccess) {
  base::MockCallback<
      base::OnceCallback<void(OfflinePageAutoFetcherScheduleResult)>>
      result_callback;
  EXPECT_CALL(result_callback,
              Run(OfflinePageAutoFetcherScheduleResult::kScheduled));
  service_->TrySchedule(false, GURL("http://foo.com"), kTabId,
                        result_callback.Get());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1ul, GetRequestsSync().size());
}

TEST_F(OfflinePageAutoFetcherServiceTest, AttemptInvalidURL) {
  base::MockCallback<
      base::OnceCallback<void(OfflinePageAutoFetcherScheduleResult)>>
      result_callback;
  EXPECT_CALL(result_callback,
              Run(OfflinePageAutoFetcherScheduleResult::kOtherError));
  service_->TrySchedule(false, GURL("ftp://foo.com"), kTabId,
                        result_callback.Get());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0ul, GetRequestsSync().size());
}

TEST_F(OfflinePageAutoFetcherServiceTest, TryScheduleDuplicate) {
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
  service_->TrySchedule(false, GURL("http://foo.com#A"), kTabId,
                        result_callback.Get());
  service_->TrySchedule(false, GURL("http://foo.com#Z"), kTabId,
                        result_callback.Get());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1ul, GetRequestsSync().size());
}

TEST_F(OfflinePageAutoFetcherServiceTest, AttemptAutoScheduleMoreThanMaximum) {
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
  service_->TrySchedule(false, GURL("http://foo.com/1"), kTabId,
                        result_callback.Get());
  service_->TrySchedule(false, GURL("http://foo.com/2"), kTabId,
                        result_callback.Get());
  service_->TrySchedule(false, GURL("http://foo.com/3"), kTabId,
                        result_callback.Get());

  // Quota is exhausted.
  service_->TrySchedule(false, GURL("http://foo.com/4"), kTabId,
                        result_callback.Get());

  // User-requested, quota is not enforced.
  service_->TrySchedule(true, GURL("http://foo.com/5"), kTabId,
                        result_callback.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(OfflinePageAutoFetcherServiceTest,
       TryScheduleMoreThanMaximumUserRequested) {
  base::MockCallback<
      base::RepeatingCallback<void(OfflinePageAutoFetcherScheduleResult)>>
      result_callback;
  EXPECT_CALL(result_callback,
              Run(OfflinePageAutoFetcherScheduleResult::kScheduled))
      .Times(4);
  service_->TrySchedule(true, GURL("http://foo.com/1"), kTabId,
                        result_callback.Get());
  service_->TrySchedule(true, GURL("http://foo.com/2"), kTabId,
                        result_callback.Get());
  service_->TrySchedule(true, GURL("http://foo.com/3"), kTabId,
                        result_callback.Get());
  service_->TrySchedule(true, GURL("http://foo.com/4"), kTabId,
                        result_callback.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(OfflinePageAutoFetcherServiceTest, CancelSuccess) {
  service_->TrySchedule(false, GURL("http://foo.com"), kTabId,
                        base::DoNothing());
  task_environment_.RunUntilIdle();
  service_->CancelSchedule(GURL("http://foo.com"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0ul, GetRequestsSync().size());
}

TEST_F(OfflinePageAutoFetcherServiceTest, CancelNotExist) {
  service_->TrySchedule(false, GURL("http://foo.com"), kTabId,
                        base::DoNothing());
  task_environment_.RunUntilIdle();
  service_->CancelSchedule(GURL("http://NOT-FOO.com"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1ul, GetRequestsSync().size());
}

TEST_F(OfflinePageAutoFetcherServiceTest, CancelQueueEmpty) {
  service_->CancelSchedule(GURL("http://foo.com"));
  task_environment_.RunUntilIdle();
}

// Simulate a completed auto fetch request, and verify that
// ShowAutoFetchCompleteNotification() is called on the delegate.
TEST_F(OfflinePageAutoFetcherServiceTest, NotifyOnAutoFetchCompleted) {
  const SavePageRequest kTestRequest = TestRequest();
  const int64_t kOfflineId = 1234;
  OfflinePageItem returned_item(kTestRequest.url(), kOfflineId,
                                kTestRequest.client_id(), base::FilePath(),
                                2000);
  returned_item.title = base::ASCIIToUTF16("Cows");
  EXPECT_CALL(offline_page_model_,
              GetPageByOfflineId_(kTestRequest.request_id()))
      .WillOnce(testing::Return(&returned_item));
  EXPECT_CALL(delegate_, ShowAutoFetchCompleteNotification(
                             returned_item.title, kTestRequest.url().spec(),
                             kTestRequest.url().spec(), kTabId, kOfflineId));
  service_->OnCompleted(kTestRequest,
                        RequestNotifier::BackgroundSavePageResult::SUCCESS);
  task_environment_.RunUntilIdle();
}

// Simulate a failed auto-fetch request, and verify that
// it is ignored.
TEST_F(OfflinePageAutoFetcherServiceTest, DontNotifyOnAutoFetchFail) {
  const SavePageRequest kTestRequest = TestRequest();
  EXPECT_CALL(offline_page_model_, GetPageByOfflineId_(_)).Times(0);
  service_->OnCompleted(
      kTestRequest, RequestNotifier::BackgroundSavePageResult::LOADING_FAILURE);
  task_environment_.RunUntilIdle();
}

// Simulate a completed non-auto-fetch request, and verify that
// it is ignored.
TEST_F(OfflinePageAutoFetcherServiceTest, DontNotifyOnOtherRequestCompleted) {
  const ClientId kClientId("other-namespace", "id");
  const SavePageRequest kTestRequest = TestRequest(kClientId);
  EXPECT_CALL(offline_page_model_, GetPageByOfflineId_(_)).Times(0);
  service_->OnCompleted(kTestRequest,
                        RequestNotifier::BackgroundSavePageResult::SUCCESS);

  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace offline_pages
