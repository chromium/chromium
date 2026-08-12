// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_transport_handler.h"

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_session.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

using browser_actuator::FactoryId;
using browser_actuator::PayloadType;
using browser_actuator::SendMessageError;
using browser_actuator::TransportSession;

class MockTransportSession : public TransportSession {
 public:
  MockTransportSession() = default;
  ~MockTransportSession() override = default;

  MOCK_METHOD(std::string_view, GetSessionId, (), (const, override));
  MOCK_METHOD((base::expected<void, SendMessageError>),
              SendMessage,
              (PayloadType payload_type,
               const google::protobuf::MessageLite& message),
              (override));
  MOCK_METHOD(void,
              OnMessage,
              (PayloadType payload_type,
               const google::protobuf::MessageLite& message),
              (override));
};

class GlicExperimentalTriggeringTransportHandlerTest : public testing::Test {
 public:
  GlicExperimentalTriggeringTransportHandlerTest() = default;
  ~GlicExperimentalTriggeringTransportHandlerTest() override = default;

  void SetUp() override {
    session_ = std::make_unique<testing::NiceMock<MockTransportSession>>();
  }

 protected:
  std::unique_ptr<MockTransportSession> session_;
};

TEST_F(GlicExperimentalTriggeringTransportHandlerTest,
       OptInFailsWhenNoControllerProvided) {
  GlicExperimentalTriggeringTransportHandler handler(
      /*opt_in_controller=*/nullptr, session_.get());

  components_sharing_message::GlicExperimentalTriggering triggering;
  triggering.set_context_id("test_session_id");
  triggering.mutable_request()->mutable_device_opt_in_request();

  EXPECT_CALL(*session_,
              SendMessage(PayloadType::kExperimentalTriggering, testing::_))
      .WillOnce([&](PayloadType type, const google::protobuf::MessageLite& msg)
                    -> base::expected<void, SendMessageError> {
        const auto& response = static_cast<
            const components_sharing_message::GlicExperimentalTriggering&>(msg);
        EXPECT_EQ("test_session_id", response.context_id());
        EXPECT_TRUE(response.has_response());
        EXPECT_EQ(components_sharing_message::GlicExperimentalTriggering::
                      ExperimentalTriggeringResponse::FAILED,
                  response.response().device_opt_in_result());
        return {};
      });

  handler.OnMessage(triggering);
}

TEST_F(GlicExperimentalTriggeringTransportHandlerTest,
       IgnoresNonOptInRequests) {
  GlicExperimentalTriggeringTransportHandler handler(
      /*opt_in_controller=*/nullptr, session_.get());

  components_sharing_message::GlicExperimentalTriggering triggering;
  triggering.set_context_id("test_session_id");
  triggering.mutable_request()->mutable_stop_actuation_request();

  EXPECT_CALL(*session_, SendMessage(testing::_, testing::_)).Times(0);

  handler.OnMessage(triggering);
}

TEST_F(GlicExperimentalTriggeringTransportHandlerTest, FactoryMethods) {
  GlicExperimentalTriggeringTransportHandlerFactory factory(
      /*opt_in_controller=*/nullptr);
  EXPECT_EQ(FactoryId::kExperimentalTriggering, factory.GetFactoryId());

  auto types = factory.GetSupportedPayloadTypes();
  ASSERT_EQ(1u, types.size());
  EXPECT_EQ(PayloadType::kExperimentalTriggering, types[0]);

  auto handler = factory.OnNewSession(session_.get());
  EXPECT_NE(nullptr, handler);
}

}  // namespace
}  // namespace glic
