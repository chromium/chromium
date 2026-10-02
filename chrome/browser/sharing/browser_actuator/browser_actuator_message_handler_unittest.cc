// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/browser_actuator/browser_actuator_message_handler.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_actuator/browser_actuator_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/transport_session.h"
#include "components/browser_actuator/test_support/mock_browser_actuator_service.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BrowserActuatorMessageHandlerTest : public testing::Test {
 public:
  BrowserActuatorMessageHandlerTest() {
    feature_list_.InitWithFeatures(
        {browser_actuator::kBrowserActuator,
         browser_actuator::kEnableBrowserActuatorForGlicExperimentalTriggering},
        {});
  }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    browser_actuator::BrowserActuatorServiceFactory::GetInstance()
        ->SetTestingFactory(
            profile_.get(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  browser_actuator::MockBrowserActuatorService>();
            }));
    mock_service_ = static_cast<browser_actuator::MockBrowserActuatorService*>(
        browser_actuator::BrowserActuatorServiceFactory::GetForProfile(
            profile_.get()));
    EXPECT_CALL(*mock_service_, IsInitialized())
        .WillRepeatedly(testing::Return(true));

    handler_ = std::make_unique<BrowserActuatorMessageHandler>(profile_.get());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<browser_actuator::MockBrowserActuatorService> mock_service_;
  std::unique_ptr<BrowserActuatorMessageHandler> handler_;
};

TEST_F(BrowserActuatorMessageHandlerTest, HandlesInitialSharingMessage) {
  components_sharing_message::SharingMessage message;
  auto* triggering = message.mutable_glic_experimental_triggering();
  triggering->set_context_id("test_context_123");
  triggering->set_glic_experimental_triggering_version(1);
  message.mutable_server_channel_configuration();
  triggering->mutable_request()->mutable_device_opt_in_request();

  EXPECT_CALL(*mock_service_, GetOrCreateSession("test_context_123"));

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());
  EXPECT_TRUE(done_future.Wait());
  EXPECT_EQ(done_future.Get(), nullptr);
}

TEST_F(BrowserActuatorMessageHandlerTest, HandlesTriggerActuationMessage) {
  components_sharing_message::SharingMessage message;
  auto* triggering = message.mutable_glic_experimental_triggering();
  triggering->set_context_id("test_context_123");
  triggering->set_glic_experimental_triggering_version(1);
  triggering->mutable_request()->mutable_trigger_actuation_request();

  EXPECT_CALL(*mock_service_, GetOrCreateSession("test_context_123"));

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());
  EXPECT_TRUE(done_future.Wait());
  EXPECT_EQ(done_future.Get(), nullptr);
}

TEST_F(BrowserActuatorMessageHandlerTest, IgnoresUnsupportedMessage) {
  components_sharing_message::SharingMessage message;
  auto* triggering = message.mutable_glic_experimental_triggering();
  triggering->set_context_id("test_context_123");
  triggering->set_glic_experimental_triggering_version(1);
  triggering->mutable_request()->mutable_stop_actuation_request();

  EXPECT_CALL(*mock_service_, GetOrCreateSession(testing::_)).Times(0);

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());
  EXPECT_TRUE(done_future.Wait());
  EXPECT_EQ(done_future.Get(), nullptr);
}

TEST_F(BrowserActuatorMessageHandlerTest, IgnoresMessageWithNoRequest) {
  components_sharing_message::SharingMessage message;
  auto* triggering = message.mutable_glic_experimental_triggering();
  triggering->set_context_id("test_context_123");
  triggering->set_glic_experimental_triggering_version(1);

  EXPECT_CALL(*mock_service_, GetOrCreateSession(testing::_)).Times(0);

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());
  EXPECT_TRUE(done_future.Wait());
  EXPECT_EQ(done_future.Get(), nullptr);
}

class MockTransportSession : public browser_actuator::TransportSession {
 public:
  MOCK_METHOD(std::string_view, GetSessionId, (), (const, override));
  MOCK_METHOD(
      (base::expected<void, browser_actuator::SendUpstreamMessageError>),
      SendUpstreamMessage,
      (browser_actuator::PayloadType payload_type,
       const google::protobuf::MessageLite& message),
      (override));
  MOCK_METHOD(void,
              OnMessage,
              (browser_actuator::PayloadType payload_type,
               const google::protobuf::MessageLite& message),
              (override));
};

TEST_F(BrowserActuatorMessageHandlerTest, HandlesActuatorDownstreamMessage) {
  components_sharing_message::SharingMessage message;
  browser_actuator::ActuatorDownstreamMessage* bundled =
      message.mutable_actuator_downstream_message();
  bundled->set_session_id("bundled_session_123");

  EXPECT_CALL(*mock_service_, GetOrCreateSession("bundled_session_123"));

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());
  EXPECT_TRUE(done_future.Wait());
  EXPECT_EQ(done_future.Get(), nullptr);
}

TEST_F(BrowserActuatorMessageHandlerTest,
       HandlesActuatorDownstreamMessageWithControlPayload) {
  components_sharing_message::SharingMessage message;
  browser_actuator::ActuatorDownstreamMessage* bundled =
      message.mutable_actuator_downstream_message();
  bundled->set_session_id("bundled_session_123");

  browser_actuator::ControlCommand command;
  command.mutable_start_session();

  auto* typed_payload = bundled->add_typed_payloads();
  typed_payload->set_payload_type(
      browser_actuator::ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  typed_payload->mutable_proto_payload()->set_value(
      command.SerializeAsString());

  MockTransportSession mock_session;
  EXPECT_CALL(*mock_service_, GetOrCreateSession("bundled_session_123"))
      .WillOnce(testing::Return(
          static_cast<browser_actuator::TransportSession*>(&mock_session)));
  EXPECT_CALL(*mock_service_, GetSession("bundled_session_123"))
      .WillRepeatedly(testing::Return(
          static_cast<browser_actuator::TransportSession*>(&mock_session)));
  EXPECT_CALL(mock_session,
              OnMessage(browser_actuator::PayloadType::kControl, testing::_));

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());
  EXPECT_TRUE(done_future.Wait());
  EXPECT_EQ(done_future.Get(), nullptr);
}

void AddControlPayload(browser_actuator::ActuatorDownstreamMessage* bundled,
                       const browser_actuator::ControlCommand& command) {
  auto* typed_payload = bundled->add_typed_payloads();
  typed_payload->set_payload_type(
      browser_actuator::ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  typed_payload->mutable_proto_payload()->set_value(
      command.SerializeAsString());
}

// Regression test: a payload that synchronously destroys the session (e.g.
// CloseSession) must not cause later payloads in the same bundle to be
// dispatched to the destroyed session.
TEST_F(BrowserActuatorMessageHandlerTest,
       StopsProcessingPayloadsAfterSessionDestroyed) {
  components_sharing_message::SharingMessage message;
  browser_actuator::ActuatorDownstreamMessage* bundled =
      message.mutable_actuator_downstream_message();
  bundled->set_session_id("bundled_session_123");

  browser_actuator::ControlCommand close_command;
  close_command.mutable_close_session();
  AddControlPayload(bundled, close_command);
  browser_actuator::ControlCommand start_command;
  start_command.mutable_start_session();
  AddControlPayload(bundled, start_command);

  // Heap-allocate so that, if the handler reuses the stale pointer, ASan
  // reports a heap-use-after-free.
  auto owned_session = std::make_unique<MockTransportSession>();
  MockTransportSession* session_ptr = owned_session.get();
  EXPECT_CALL(*mock_service_, GetOrCreateSession("bundled_session_123"))
      .WillOnce(testing::Return(
          static_cast<browser_actuator::TransportSession*>(session_ptr)));
  EXPECT_CALL(*session_ptr,
              OnMessage(browser_actuator::PayloadType::kControl, testing::_))
      .WillOnce([&owned_session](browser_actuator::PayloadType,
                                 const google::protobuf::MessageLite&) {
        // Simulate TransportSessionRegistryImpl::DestroySession().
        owned_session.reset();
      });
  // The handler re-resolves the session at the top of every iteration: the
  // first lookup finds it, the second finds it destroyed and stops, so the
  // StartSession payload is never dispatched.
  EXPECT_CALL(*mock_service_, GetSession("bundled_session_123"))
      .WillOnce(testing::Return(
          static_cast<browser_actuator::TransportSession*>(session_ptr)))
      .WillOnce(testing::Return(nullptr));

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());
  EXPECT_TRUE(done_future.Wait());
  EXPECT_EQ(done_future.Get(), nullptr);
  EXPECT_FALSE(owned_session);
}

TEST_F(BrowserActuatorMessageHandlerTest,
       ProcessesAllPayloadsWhileSessionAlive) {
  components_sharing_message::SharingMessage message;
  browser_actuator::ActuatorDownstreamMessage* bundled =
      message.mutable_actuator_downstream_message();
  bundled->set_session_id("bundled_session_123");

  browser_actuator::ControlCommand start_command;
  start_command.mutable_start_session();
  AddControlPayload(bundled, start_command);
  AddControlPayload(bundled, start_command);

  MockTransportSession mock_session;
  auto* session =
      static_cast<browser_actuator::TransportSession*>(&mock_session);
  EXPECT_CALL(*mock_service_, GetOrCreateSession("bundled_session_123"))
      .WillOnce(testing::Return(session));
  // One lookup per payload.
  EXPECT_CALL(*mock_service_, GetSession("bundled_session_123"))
      .Times(2)
      .WillRepeatedly(testing::Return(session));
  EXPECT_CALL(mock_session,
              OnMessage(browser_actuator::PayloadType::kControl, testing::_))
      .Times(2);

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());
  EXPECT_TRUE(done_future.Wait());
  EXPECT_EQ(done_future.Get(), nullptr);
}

}  // namespace
