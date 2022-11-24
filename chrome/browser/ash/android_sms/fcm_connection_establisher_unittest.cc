// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/android_sms/fcm_connection_establisher.h"

#include <utility>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/ash/android_sms/android_sms_urls.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/fake_service_worker_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

namespace ash {
namespace android_sms {

class FcmConnectionEstablisherTest : public testing::Test {
 public:
  FcmConnectionEstablisherTest(const FcmConnectionEstablisherTest&) = delete;
  FcmConnectionEstablisherTest& operator=(const FcmConnectionEstablisherTest&) =
      delete;

 protected:
  FcmConnectionEstablisherTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}
  ~FcmConnectionEstablisherTest() override = default;

  void VerifyTransferrableMessage(const char* expected,
                                  blink::TransferableMessage message) {
    auto payload = blink::DecodeToWebMessagePayload(std::move(message));
    EXPECT_EQ(base::UTF8ToUTF16(expected),
              absl::get<std::u16string>(payload.value()));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(FcmConnectionEstablisherTest, TestEstablishConnection) {
  auto mock_retry_timer = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* mock_retry_timer_ptr = mock_retry_timer.get();
  base::HistogramTester histogram_tester;

  content::FakeServiceWorkerContext fake_service_worker_context;
  FcmConnectionEstablisher fcm_connection_establisher(
      std::move(mock_retry_timer));
  auto& message_dispatch_calls =
      fake_service_worker_context
          .start_service_worker_and_dispatch_message_calls();

  // Verify that message is dispatch to service worker.
  fcm_connection_establisher.EstablishConnection(
      GetAndroidMessagesURL(),
      ConnectionEstablisher::ConnectionMode::kStartConnection,
      &fake_service_worker_context);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, message_dispatch_calls.size());
  EXPECT_EQ(GetAndroidMessagesURL(), std::get<GURL>(message_dispatch_calls[0]));
  VerifyTransferrableMessage(FcmConnectionEstablisher::kStartFcmMessage,
                             std::move(std::get<blink::TransferableMessage>(
                                 message_dispatch_calls[0])));

  // Return success to result callback and verify that no retries are attempted
  // and success histogram is recorded.
  std::move(std::get<content::ServiceWorkerContext::ResultCallback>(
                message_dispatch_calls[0]))
      .Run(true /* status */);
  ASSERT_EQ(1u, message_dispatch_calls.size());
  EXPECT_FALSE(mock_retry_timer_ptr->IsRunning());
  histogram_tester.ExpectBucketCount(
      "AndroidSms.FcmMessageDispatchSuccess",
      FcmConnectionEstablisher::MessageType::kStart, 1);

  // Verify that when multiple requests are sent only the first one is
  // dispatched while the others are queued.
  fcm_connection_establisher.EstablishConnection(
      GetAndroidMessagesURL(),
      ConnectionEstablisher::ConnectionMode::kStartConnection,
      &fake_service_worker_context);
  fcm_connection_establisher.EstablishConnection(
      GetAndroidMessagesURL(),
      ConnectionEstablisher::ConnectionMode::kResumeExistingConnection,
      &fake_service_worker_context);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, message_dispatch_calls.size());
  VerifyTransferrableMessage(FcmConnectionEstablisher::kStartFcmMessage,
                             std::move(std::get<blink::TransferableMessage>(
                                 message_dispatch_calls[1])));

  // Verify that if the first request fails then it's retried
  std::move(std::get<content::ServiceWorkerContext::ResultCallback>(
                message_dispatch_calls[1]))
      .Run(false /* status */);
  ASSERT_EQ(2u, message_dispatch_calls.size());
  EXPECT_TRUE(mock_retry_timer_ptr->IsRunning());
  // Retry shouldn't count success.
  histogram_tester.ExpectBucketCount(
      "AndroidSms.FcmMessageDispatchSuccess",
      FcmConnectionEstablisher::MessageType::kStart, 1);
  mock_retry_timer_ptr->Fire();
  ASSERT_EQ(3u, message_dispatch_calls.size());
  VerifyTransferrableMessage(FcmConnectionEstablisher::kStartFcmMessage,
                             std::move(std::get<blink::TransferableMessage>(
                                 message_dispatch_calls[2])));

  // Verify that if the first request succeeds then the next message is
  // dispatched
  std::move(std::get<content::ServiceWorkerContext::ResultCallback>(
                message_dispatch_calls[2]))
      .Run(true /* status */);
  ASSERT_EQ(4u, message_dispatch_calls.size());
  EXPECT_FALSE(mock_retry_timer_ptr->IsRunning());
  VerifyTransferrableMessage(FcmConnectionEstablisher::kResumeFcmMessage,
                             std::move(std::get<blink::TransferableMessage>(
                                 message_dispatch_calls[3])));

  // Complete second request and verify that no more retries are scheduled.
  std::move(std::get<content::ServiceWorkerContext::ResultCallback>(
                message_dispatch_calls[3]))
      .Run(true /* status */);
  EXPECT_FALSE(mock_retry_timer_ptr->IsRunning());

  // Verify that max retries are attempted before abandoning request
  fcm_connection_establisher.EstablishConnection(
      GetAndroidMessagesURL(),
      ConnectionEstablisher::ConnectionMode::kStartConnection,
      &fake_service_worker_context);
  base::RunLoop().RunUntilIdle();

  int last_retry_bucket_count = histogram_tester.GetBucketCount(
      "AndroidSms.FcmMessageDispatchRetry",
      static_cast<base::HistogramBase::Sample>(
          FcmConnectionEstablisher::MessageType::kStart));

  int retry_count = 0;
  while (true) {
    ASSERT_EQ(5u + retry_count, message_dispatch_calls.size());
    std::move(std::get<content::ServiceWorkerContext::ResultCallback>(
                  message_dispatch_calls[4 + retry_count]))
        .Run(false /* status */);
    if (mock_retry_timer_ptr->IsRunning()) {
      mock_retry_timer_ptr->Fire();
      retry_count++;
    } else {
      break;
    }
  }

  EXPECT_EQ(FcmConnectionEstablisher::kMaxRetryCount, retry_count);
  histogram_tester.ExpectBucketCount(
      "AndroidSms.FcmMessageDispatchRetry",
      FcmConnectionEstablisher::MessageType::kStart,
      retry_count + last_retry_bucket_count);
  histogram_tester.ExpectBucketCount(
      "AndroidSms.FcmMessageDispatchFailure",
      FcmConnectionEstablisher::MessageType::kStart, 1);
}

TEST_F(FcmConnectionEstablisherTest, TestTearDownConnection) {
  content::FakeServiceWorkerContext fake_service_worker_context;
  FcmConnectionEstablisher fcm_connection_establisher(
      std::make_unique<base::MockOneShotTimer>());
  auto& message_dispatch_calls =
      fake_service_worker_context
          .start_service_worker_and_dispatch_message_calls();

  // Verify that message is dispatch to service worker.
  fcm_connection_establisher.TearDownConnection(GetAndroidMessagesURL(),
                                                &fake_service_worker_context);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, message_dispatch_calls.size());
  EXPECT_EQ(GetAndroidMessagesURL(), std::get<GURL>(message_dispatch_calls[0]));
  VerifyTransferrableMessage(FcmConnectionEstablisher::kStopFcmMessage,
                             std::move(std::get<blink::TransferableMessage>(
                                 message_dispatch_calls[0])));
}

}  // namespace android_sms
}  // namespace ash
