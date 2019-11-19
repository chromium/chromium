// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/ack_message_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_message_sender.h"
#include "components/sync/protocol/sharing_message.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kTestMessageId[] = "test_message_id";

class MockSharingMessageSender : public SharingMessageSender {
 public:
  MockSharingMessageSender()
      : SharingMessageSender(nullptr, nullptr, nullptr) {}
  ~MockSharingMessageSender() override = default;

  MOCK_METHOD3(
      OnAckReceived,
      void(chrome_browser_sharing::MessageType message_type,
           const std::string& fcm_message_id,
           std::unique_ptr<chrome_browser_sharing::ResponseMessage> response));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSharingMessageSender);
};

class AckMessageHandlerTest : public testing::Test {
 protected:
  AckMessageHandlerTest()
      : ack_message_handler_(&mock_response_callback_helper_) {}

  testing::NiceMock<MockSharingMessageSender> mock_response_callback_helper_;
  AckMessageHandler ack_message_handler_;
};

MATCHER_P(ProtoEquals, message, "") {
  if (!arg)
    return false;

  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg->SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

}  // namespace

TEST_F(AckMessageHandlerTest, OnMessageNoResponse) {
  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);
  sharing_message.mutable_ack_message()->set_original_message_type(
      chrome_browser_sharing::CLICK_TO_CALL_MESSAGE);

  base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::Eq(nullptr)));

  EXPECT_CALL(
      mock_response_callback_helper_,
      OnAckReceived(testing::Eq(chrome_browser_sharing::CLICK_TO_CALL_MESSAGE),
                    testing::Eq(kTestMessageId), testing::Eq(nullptr)));

  ack_message_handler_.OnMessage(std::move(sharing_message),
                                 done_callback.Get());
}

TEST_F(AckMessageHandlerTest, OnMessageWithResponse) {
  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_ack_message()->set_original_message_id(
      kTestMessageId);
  sharing_message.mutable_ack_message()->set_original_message_type(
      chrome_browser_sharing::CLICK_TO_CALL_MESSAGE);
  sharing_message.mutable_ack_message()->mutable_response_message();

  chrome_browser_sharing::ResponseMessage response_message_copy =
      sharing_message.ack_message().response_message();

  base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::Eq(nullptr)));

  EXPECT_CALL(
      mock_response_callback_helper_,
      OnAckReceived(testing::Eq(chrome_browser_sharing::CLICK_TO_CALL_MESSAGE),
                    testing::Eq(kTestMessageId),
                    ProtoEquals(response_message_copy)));

  ack_message_handler_.OnMessage(std::move(sharing_message),
                                 done_callback.Get());
}
