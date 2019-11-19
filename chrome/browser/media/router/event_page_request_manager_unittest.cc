// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/event_page_request_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_page_tracker.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

using MockRequest = base::MockCallback<base::OnceClosure>;

namespace media_router {

namespace {

constexpr char kExtensionId[] = "extension_id";

// ProcessManager with a mocked method subset, for testing extension suspension.
class TestProcessManager : public extensions::ProcessManager {
 public:
  explicit TestProcessManager(content::BrowserContext* context)
      : extensions::ProcessManager(
            context,
            context,
            extensions::ExtensionRegistry::Get(context)) {}
  ~TestProcessManager() override = default;

  static std::unique_ptr<KeyedService> Create(
      content::BrowserContext* context) {
    return std::make_unique<TestProcessManager>(context);
  }

  MOCK_METHOD1(IsEventPageSuspended, bool(const std::string& ext_id));
  MOCK_METHOD2(WakeEventPage,
               bool(const std::string& extension_id,
                    base::OnceCallback<void(bool)> callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestProcessManager);
};

}  // namespace

class EventPageRequestManagerTest : public ::testing::Test {
 public:
  EventPageRequestManagerTest() = default;
  ~EventPageRequestManagerTest() override = default;

 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    // Set up a mock ProcessManager instance.
    extensions::ProcessManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&TestProcessManager::Create));
    process_manager_ = static_cast<TestProcessManager*>(
        extensions::ProcessManager::Get(profile_.get()));
    DCHECK(process_manager_);

    request_manager_.reset(new EventPageRequestManager(profile_.get()));
    request_manager_->SetExtensionId(kExtensionId);
  }

  void TearDown() override {
    request_manager_.reset();
    profile_.reset();
  }

  void ExpectWakeReasonBucketCount(MediaRouteProviderWakeReason reason,
                                   int expected_count) {
    histogram_tester_.ExpectBucketCount("MediaRouter.Provider.WakeReason",
                                        static_cast<int>(reason),
                                        expected_count);
  }

  void ExpectWakeupBucketCount(MediaRouteProviderWakeup wakeup,
                               int expected_count) {
    histogram_tester_.ExpectBucketCount("MediaRouter.Provider.Wakeup",
                                        static_cast<int>(wakeup),
                                        expected_count);
  }

  TestProcessManager* process_manager_ = nullptr;
  std::unique_ptr<EventPageRequestManager> request_manager_;

 private:
  std::unique_ptr<TestingProfile> profile_;
  base::HistogramTester histogram_tester_;
  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(EventPageRequestManagerTest);
};

TEST_F(EventPageRequestManagerTest, SetExtensionId) {
  request_manager_->SetExtensionId(kExtensionId);
  EXPECT_EQ(kExtensionId,
            request_manager_->media_route_provider_extension_id());
}

TEST_F(EventPageRequestManagerTest, RunRequestImmediately) {
  StrictMock<MockRequest> request;
  request_manager_->OnMojoConnectionsReady();

  EXPECT_CALL(request, Run());
  request_manager_->RunOrDefer(
      base::BindOnce(&MockRequest::Run, base::Unretained(&request)),
      MediaRouteProviderWakeReason::DETACH_ROUTE);
}

TEST_F(EventPageRequestManagerTest, DoNotRunRequestAfterConnectionError) {
  StrictMock<MockRequest> request;
  request_manager_->OnMojoConnectionsReady();
  request_manager_->OnMojoConnectionError();
  request_manager_->RunOrDefer(
      base::BindOnce(&MockRequest::Run, base::Unretained(&request)),
      MediaRouteProviderWakeReason::DETACH_ROUTE);
}

TEST_F(EventPageRequestManagerTest, RunRequestsOnConnectionsReady) {
  StrictMock<MockRequest> request1;
  StrictMock<MockRequest> request2;

  ON_CALL(*process_manager_, IsEventPageSuspended(kExtensionId))
      .WillByDefault(Return(true));
  EXPECT_CALL(*process_manager_, WakeEventPage(kExtensionId, _))
      .WillOnce([](const std::string& extension_id,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
        return true;
      });
  request_manager_->RunOrDefer(
      base::BindOnce(&MockRequest::Run, base::Unretained(&request1)),
      MediaRouteProviderWakeReason::DETACH_ROUTE);

  ON_CALL(*process_manager_, IsEventPageSuspended(kExtensionId))
      .WillByDefault(Return(false));
  request_manager_->RunOrDefer(
      base::BindOnce(&MockRequest::Run, base::Unretained(&request2)),
      MediaRouteProviderWakeReason::DETACH_ROUTE);

  EXPECT_CALL(request1, Run());
  EXPECT_CALL(request2, Run());
  request_manager_->OnMojoConnectionsReady();
  ExpectWakeReasonBucketCount(MediaRouteProviderWakeReason::DETACH_ROUTE, 1);
  ExpectWakeupBucketCount(MediaRouteProviderWakeup::SUCCESS, 1);
}

TEST_F(EventPageRequestManagerTest, DropOldestPendingRequest) {
  StrictMock<MockRequest> request1;
  MockRequest request2;
  request_manager_->RunOrDefer(
      base::BindOnce(&MockRequest::Run, base::Unretained(&request1)),
      MediaRouteProviderWakeReason::DETACH_ROUTE);
  for (int i = 0; i < EventPageRequestManager::kMaxPendingRequests; i++) {
    request_manager_->RunOrDefer(
        base::BindOnce(&MockRequest::Run, base::Unretained(&request2)),
        MediaRouteProviderWakeReason::DETACH_ROUTE);
  }
  // Now the call on |request1| should have been pushed out of the queue and not
  // be called.
  EXPECT_CALL(request2, Run())
      .Times(EventPageRequestManager::kMaxPendingRequests);
  request_manager_->OnMojoConnectionsReady();
}

TEST_F(EventPageRequestManagerTest, FailedWakeupDrainsQueue) {
  StrictMock<MockRequest> request1;
  StrictMock<MockRequest> request2;

  EXPECT_CALL(*process_manager_, IsEventPageSuspended(kExtensionId))
      .WillOnce(Return(true));
  EXPECT_CALL(*process_manager_, WakeEventPage(kExtensionId, _))
      .WillOnce([](const std::string& extension_id,
                   base::OnceCallback<void(bool)> callback) {
        // Run the callback with false to indicate that the wakeup failed.
        // This call drains the request queue, so |request1| won't be
        // called.
        std::move(callback).Run(false);
        return true;
      });
  request_manager_->RunOrDefer(
      base::BindOnce(&MockRequest::Run, base::Unretained(&request1)),
      MediaRouteProviderWakeReason::CREATE_ROUTE);

  EXPECT_CALL(*process_manager_, IsEventPageSuspended(kExtensionId))
      .WillRepeatedly(Return(false));
  // Requests that come in after queue is drained should be queued.
  request_manager_->RunOrDefer(
      base::BindOnce(&MockRequest::Run, base::Unretained(&request2)),
      MediaRouteProviderWakeReason::CREATE_ROUTE);
  EXPECT_CALL(request2, Run());
  request_manager_->OnMojoConnectionsReady();
}

TEST_F(EventPageRequestManagerTest, TooManyWakeupAttemptsDrainsQueue) {
  StrictMock<MockRequest> request1;
  StrictMock<MockRequest> request2;

  // After these calls the request queue is drained, so |request1| won't be
  // called.
  EXPECT_CALL(*process_manager_, IsEventPageSuspended(kExtensionId))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*process_manager_, WakeEventPage(kExtensionId, _))
      .WillRepeatedly(Return(true));
  for (int i = 0; i <= EventPageRequestManager::kMaxWakeupAttemptCount; i++) {
    request_manager_->RunOrDefer(
        base::BindOnce(&MockRequest::Run, base::Unretained(&request1)),
        MediaRouteProviderWakeReason::CREATE_ROUTE);
  }
  EXPECT_CALL(*process_manager_, IsEventPageSuspended(kExtensionId))
      .WillRepeatedly(Return(false));

  // Requests that come in after queue is drained should be queued.
  request_manager_->RunOrDefer(
      base::BindOnce(&MockRequest::Run, base::Unretained(&request2)),
      MediaRouteProviderWakeReason::CREATE_ROUTE);
  EXPECT_CALL(request2, Run());
  request_manager_->OnMojoConnectionsReady();
}

}  // namespace media_router
