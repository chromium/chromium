// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/connection_establisher_impl.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "content/public/test/fake_service_worker_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace chromeos {

namespace android_sms {

class ConnectionEstablisherImplTest : public testing::Test {
 protected:
  ConnectionEstablisherImplTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~ConnectionEstablisherImplTest() override = default;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  DISALLOW_COPY_AND_ASSIGN(ConnectionEstablisherImplTest);
};

TEST_F(ConnectionEstablisherImplTest, EstablishConnection) {
  content::FakeServiceWorkerContext fake_service_worker_context;
  ConnectionEstablisherImpl connection_establisher;
  auto& message_dispatch_calls =
      fake_service_worker_context
          .start_service_worker_and_dispatch_long_running_message_calls();

  // Verify that long running message dispatch is called.
  connection_establisher.EstablishConnection(
      &fake_service_worker_context,
      ConnectionEstablisher::ConnectionMode::kStartConnection);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, message_dispatch_calls.size());
  EXPECT_EQ(GetAndroidMessagesURL(), std::get<GURL>(message_dispatch_calls[0]));
  base::string16 message_string;
  blink::DecodeStringMessage(
      std::get<blink::TransferableMessage>(message_dispatch_calls[0])
          .owned_encoded_message,
      &message_string);
  EXPECT_EQ(
      base::UTF8ToUTF16(ConnectionEstablisherImpl::kStartStreamingMessage),
      message_string);

  // Verify that message is not dispatched again if previous result callback has
  // not returned yet.
  connection_establisher.EstablishConnection(
      &fake_service_worker_context,
      ConnectionEstablisher::ConnectionMode::kStartConnection);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, message_dispatch_calls.size());

  // Verify that message is dispatched again if previous result callback
  // returns.
  std::move(std::get<content::ServiceWorkerContext::ResultCallback>(
                message_dispatch_calls[0]))
      .Run(true);
  connection_establisher.EstablishConnection(
      &fake_service_worker_context,
      ConnectionEstablisher::ConnectionMode::kStartConnection);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, message_dispatch_calls.size());

  std::move(std::get<content::ServiceWorkerContext::ResultCallback>(
                message_dispatch_calls[1]))
      .Run(true);
  connection_establisher.EstablishConnection(
      &fake_service_worker_context,
      ConnectionEstablisher::ConnectionMode::kResumeExistingConnection);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, message_dispatch_calls.size());
  EXPECT_EQ(GetAndroidMessagesURL(), std::get<GURL>(message_dispatch_calls[2]));
  base::string16 resume_message_string;
  blink::DecodeStringMessage(
      std::get<blink::TransferableMessage>(message_dispatch_calls[2])
          .owned_encoded_message,
      &resume_message_string);
  EXPECT_EQ(
      base::UTF8ToUTF16(ConnectionEstablisherImpl::kResumeStreamingMessage),
      resume_message_string);
}

}  // namespace android_sms

}  // namespace chromeos
