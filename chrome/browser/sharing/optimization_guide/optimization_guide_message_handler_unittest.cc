// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/optimization_guide/optimization_guide_message_handler.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "components/optimization_guide/core/mock_push_notification_manager.h"
#include "components/optimization_guide/core/push_notification_manager.h"
#include "components/optimization_guide/proto/push_notification.pb.h"
#include "components/sharing_message/proto/optimization_guide_push_notification.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;

const char kInvalidHintsPayload[] = "1234";

namespace {

class OptimizationGuideMessageHandlerTest : public testing::Test {
 protected:
  OptimizationGuideMessageHandlerTest() = default;

  void SetUp() override {
    message_handler_ = std::make_unique<OptimizationGuideMessageHandler>(
        push_notification_manager(), /*optimization_guide_logger=*/nullptr);
  }

  optimization_guide::MockPushNotificationManager* push_notification_manager() {
    return &push_notification_manager_;
  }

  // Build a SharingMessage proto.
  components_sharing_message::SharingMessage BuildMessage(
      const std::string& hint_notification_payload_bytes) {
    components_sharing_message::OptimizationGuidePushNotification*
        opt_guide_push_notification =
            new components_sharing_message::OptimizationGuidePushNotification;
    opt_guide_push_notification->set_hint_notification_payload_bytes(
        hint_notification_payload_bytes);
    components_sharing_message::SharingMessage sharing_message;
    sharing_message.set_allocated_optimization_guide_push_notification(
        opt_guide_push_notification);
    return sharing_message;
  }

  // Build a HintNotificationPayload and converts to bytes.
  std::string BuildHintsPayloadAsBytes() {
    optimization_guide::proto::HintNotificationPayload notification;
    std::string bytes;
    notification.SerializeToString(&bytes);
    return bytes;
  }

  void OnMessage(const components_sharing_message::SharingMessage& message) {
    base::MockCallback<SharingMessageHandler::DoneCallback> callback;
    EXPECT_CALL(callback, Run(_));
    message_handler_->OnMessage(message, callback.Get());
  }

 private:
  NiceMock<optimization_guide::MockPushNotificationManager>
      push_notification_manager_;
  std::unique_ptr<OptimizationGuideMessageHandler> message_handler_;
};

// Test the case that HintNotificationPayload can't be parsed from the
// OptimizationGuidePushNotification proto.
TEST_F(OptimizationGuideMessageHandlerTest, OnMessageParseFailed) {
  EXPECT_CALL(*push_notification_manager(), OnNewPushNotification(_)).Times(0);
  OnMessage(BuildMessage(kInvalidHintsPayload));
}

// Verifies the serialized string of proto::HintNotificationPayload equals to
// |expected_hints_payload_bytes|.
MATCHER_P(EqHintsPayload, expected_hints_payload_bytes, "") {
  std::string hints_payload_bytes;
  arg.SerializeToString(&hints_payload_bytes);
  return hints_payload_bytes == expected_hints_payload_bytes;
}

// Test the case that HintNotificationPayload can be successfully parsed from
// the OptimizationGuidePushNotification proto.
TEST_F(OptimizationGuideMessageHandlerTest, OnMessageParseSuccess) {
  std::string hints_payload_as_bytes = BuildHintsPayloadAsBytes();
  EXPECT_CALL(*push_notification_manager(),
              OnNewPushNotification(EqHintsPayload(hints_payload_as_bytes)));
  OnMessage(BuildMessage(hints_payload_as_bytes));
}

}  // namespace
