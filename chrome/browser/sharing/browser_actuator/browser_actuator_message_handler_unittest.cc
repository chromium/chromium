// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/browser_actuator/browser_actuator_message_handler.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_actuator/browser_actuator_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_actuator/internal/features.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/transport/test_support/fake_proto_stream_server.h"
#include "components/browser_actuator/internal/transport/test_support/wait_for.h"
#include "components/browser_actuator/public/browser_actuator_service.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/payload_type_mapping.h"
#include "components/browser_actuator/public/transport_channel.h"
#include "components/browser_actuator/public/transport_handler.h"
#include "components/browser_actuator/public/transport_handler_factory.h"
#include "components/browser_actuator/public/transport_handler_factory_registry.h"
#include "components/browser_actuator/public/transport_session.h"
#include "components/browser_actuator/public/transport_session_registry.h"
#include "components/browser_actuator/test_support/mock_browser_actuator_service.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/sharing_message/fake_sharing_handler_registry.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_fcm_handler.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
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

// End-to-end integration test: verifies that when an FCM
// `gcm::IncomingMessage` carrying a `SharingMessage` arrives at
// `SharingFCMHandler`, `BrowserActuatorMessageHandler` provisions the real
// `BrowserActuatorService`, starts the downstream transport channel, and sends
// the `StartSessionAck` upstream.
class BrowserActuatorFcmIntegrationTest : public testing::Test {
 protected:
  BrowserActuatorFcmIntegrationTest() {
    feature_list_.InitWithFeatures(
        {browser_actuator::kBrowserActuator,
         browser_actuator::kBrowserActuatorChannelEnabled,
         browser_actuator::kBrowserActuatorInternals,
         browser_actuator::kEnableBrowserActuatorForGlicExperimentalTriggering},
        {});
  }

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);
    identity_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    identity_adaptor_->identity_test_env()->MakePrimaryAccountAvailable(
        "user@example.com", signin::ConsentLevel::kSignin);
    identity_adaptor_->identity_test_env()->SetAutomaticIssueOfAccessTokens(
        true);

    handler_ = std::make_unique<BrowserActuatorMessageHandler>(profile_.get());
    handler_registry_.SetSharingHandler(
        components_sharing_message::SharingMessage::kActuatorDownstreamMessage,
        handler_.get());
    handler_registry_.SetSharingHandler(
        components_sharing_message::SharingMessage::kGlicExperimentalTriggering,
        handler_.get());
    fcm_handler_ = std::make_unique<SharingFCMHandler>(
        /*gcm_driver=*/nullptr, &device_info_tracker_,
        /*sharing_channel_sender=*/nullptr, &handler_registry_);
  }

  void TearDown() override {
    fcm_handler_.reset();
    handler_.reset();
    identity_adaptor_.reset();
    profile_.reset();
  }

  browser_actuator::SendSessionMessageRequest WaitForUpstreamRequest() {
    const GURL upstream_url = browser_actuator::GetSendSessionMessageEndpoint();
    browser_actuator::SendSessionMessageRequest parsed;
    bool received = browser_actuator::WaitFor([&]() {
      auto* pending = test_url_loader_factory_.pending_requests();
      for (const auto& req : *pending) {
        if (req.request.url == upstream_url) {
          std::string body = network::GetUploadData(req.request);
          EXPECT_TRUE(parsed.ParseFromString(body));
          browser_actuator::SendSessionMessageResponse resp;
          test_url_loader_factory_.SimulateResponseForPendingRequest(
              upstream_url.spec(), resp.SerializeAsString(), net::HTTP_OK,
              network::TestURLLoaderFactory::kUrlMatchPrefix);
          return true;
        }
      }
      return false;
    });
    EXPECT_TRUE(received);
    return parsed;
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> identity_adaptor_;
  syncer::FakeDeviceInfoTracker device_info_tracker_;
  FakeSharingHandlerRegistry handler_registry_;
  std::unique_ptr<BrowserActuatorMessageHandler> handler_;
  std::unique_ptr<SharingFCMHandler> fcm_handler_;
};

TEST_F(BrowserActuatorFcmIntegrationTest,
       FcmStartSessionStartsChannelAndSendsStartSessionAck) {
  browser_actuator::FakeProtoStreamServer stream_server(
      &test_url_loader_factory_, browser_actuator::GetWatchSessionsEndPoint());

  components_sharing_message::SharingMessage sharing_msg;
  auto* bundled = sharing_msg.mutable_actuator_downstream_message();
  bundled->set_session_id("fcm-session-1");
  bundled->set_sequence_number(1);
  browser_actuator::ControlCommand start_cmd;
  start_cmd.mutable_start_session();
  auto* typed = bundled->add_typed_payloads();
  typed->set_payload_type(
      browser_actuator::ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  typed->mutable_proto_payload()->set_type_url(
      std::string(browser_actuator::ExpectedTypeUrl(
          browser_actuator::PayloadType::kControl)));
  typed->mutable_proto_payload()->set_value(start_cmd.SerializeAsString());

  gcm::IncomingMessage fcm_msg;
  fcm_msg.message_id = "0:1563805165426489%0bb84dcff9fd7ecd";
  ASSERT_TRUE(sharing_msg.SerializeToString(&fcm_msg.raw_data));

  fcm_handler_->OnMessage(kSharingFCMAppID, fcm_msg);

  // 1. Verify the downstream channel started for the session.
  stream_server.WaitForConnection();
  ASSERT_EQ(stream_server.last_watch_request().sessions_size(), 1);
  EXPECT_EQ(stream_server.last_watch_request().sessions(0).session_id(),
            "fcm-session-1");

  // 2. Verify StartSessionAck was sent upstream.
  browser_actuator::SendSessionMessageRequest upstream_req =
      WaitForUpstreamRequest();
  const auto& upstream_msg = upstream_req.actuator_upstream_message();
  EXPECT_EQ(upstream_msg.session_id(), "fcm-session-1");
  EXPECT_EQ(upstream_msg.client_sequence_number(), 1);
  ASSERT_EQ(upstream_msg.typed_payloads_size(), 1);
  EXPECT_EQ(upstream_msg.typed_payloads(0).payload_type(),
            browser_actuator::ACTUATOR_UPSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  browser_actuator::ControlCommand ack_cmd;
  ASSERT_TRUE(ack_cmd.ParseFromString(
      upstream_msg.typed_payloads(0).proto_payload().value()));
  EXPECT_TRUE(ack_cmd.has_start_session_ack());

  auto* service =
      browser_actuator::BrowserActuatorServiceFactory::GetForProfile(
          profile_.get());
  ASSERT_NE(service, nullptr);
  EXPECT_NE(
      service->GetFactory(browser_actuator::FactoryId::kSessionStreamRecorder),
      nullptr);

  // 3. Verify the started stream delivers subsequent downstream CloseSession.
  browser_actuator::ActuatorDownstreamMessage close_msg;
  close_msg.set_session_id("fcm-session-1");
  close_msg.set_sequence_number(2);
  browser_actuator::ControlCommand close_cmd;
  close_cmd.mutable_close_session();
  auto* close_typed = close_msg.add_typed_payloads();
  close_typed->set_payload_type(
      browser_actuator::ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  close_typed->mutable_proto_payload()->set_type_url(
      std::string(browser_actuator::ExpectedTypeUrl(
          browser_actuator::PayloadType::kControl)));
  close_typed->mutable_proto_payload()->set_value(
      close_cmd.SerializeAsString());
  stream_server.PushMessage(close_msg);

  EXPECT_TRUE(browser_actuator::WaitFor([&]() {
    return service->GetChannel()->GetSessionRegistry()->GetSession(
               "fcm-session-1") == nullptr;
  }));
}

class EchoTriggeringHandler : public browser_actuator::TransportHandler {
 public:
  explicit EchoTriggeringHandler(browser_actuator::TransportSession* session)
      : session_(session) {}
  void OnMessage(browser_actuator::PayloadType payload_type,
                 std::string_view serialized_payload) override {
    components_sharing_message::GlicExperimentalTriggering parsed;
    ASSERT_TRUE(parsed.ParseFromString(std::string(serialized_payload)));
    components_sharing_message::GlicExperimentalTriggering reply;
    reply.set_context_id("ack:" + parsed.context_id());
    EXPECT_TRUE(session_->SendUpstreamMessage(payload_type, reply).has_value());
  }

 private:
  raw_ptr<browser_actuator::TransportSession> session_;
};

class EchoTriggeringFactory : public browser_actuator::TransportHandlerFactory {
 public:
  browser_actuator::FactoryId GetFactoryId() const override {
    return browser_actuator::FactoryId::kExperimentalTriggering;
  }
  std::vector<browser_actuator::PayloadType> GetSupportedPayloadTypes()
      const override {
    return {browser_actuator::PayloadType::kExperimentalTriggering};
  }
  std::unique_ptr<browser_actuator::TransportHandler> OnNewSession(
      browser_actuator::TransportSession* session) override {
    return std::make_unique<EchoTriggeringHandler>(session);
  }
};

TEST_F(BrowserActuatorFcmIntegrationTest,
       FcmGlicExperimentalTriggeringStartsChannelAndSendsUpstreamReply) {
  browser_actuator::FakeProtoStreamServer stream_server(
      &test_url_loader_factory_, browser_actuator::GetWatchSessionsEndPoint());
  auto* service =
      browser_actuator::BrowserActuatorServiceFactory::GetForProfile(
          profile_.get());
  ASSERT_NE(service, nullptr);
  EchoTriggeringFactory triggering_factory;
  service->GetChannel()->GetHandlerFactoryRegistry()->RegisterFactory(
      &triggering_factory);
  base::ScopedClosureRunner unregister_runner(base::BindOnce(
      [](browser_actuator::BrowserActuatorService* s,
         browser_actuator::TransportHandlerFactory* f) {
        s->GetChannel()->GetHandlerFactoryRegistry()->UnregisterFactory(f);
      },
      service, &triggering_factory));

  components_sharing_message::SharingMessage sharing_msg;
  auto* triggering = sharing_msg.mutable_glic_experimental_triggering();
  triggering->set_context_id("glic-fcm-session-1");
  triggering->set_glic_experimental_triggering_version(1);
  triggering->mutable_request()->mutable_trigger_actuation_request();

  gcm::IncomingMessage fcm_msg;
  fcm_msg.message_id = "0:1563805165426489%0bb84dcff9fd7ece";
  ASSERT_TRUE(sharing_msg.SerializeToString(&fcm_msg.raw_data));

  fcm_handler_->OnMessage(kSharingFCMAppID, fcm_msg);

  stream_server.WaitForConnection();
  ASSERT_EQ(stream_server.last_watch_request().sessions_size(), 1);
  EXPECT_EQ(stream_server.last_watch_request().sessions(0).session_id(),
            "glic-fcm-session-1");

  browser_actuator::SendSessionMessageRequest upstream_req =
      WaitForUpstreamRequest();
  const auto& upstream_msg = upstream_req.actuator_upstream_message();
  EXPECT_EQ(upstream_msg.session_id(), "glic-fcm-session-1");
  ASSERT_EQ(upstream_msg.typed_payloads_size(), 1);
  EXPECT_EQ(
      upstream_msg.typed_payloads(0).payload_type(),
      browser_actuator::ACTUATOR_UPSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING);
  components_sharing_message::GlicExperimentalTriggering reply;
  ASSERT_TRUE(reply.ParseFromString(
      upstream_msg.typed_payloads(0).proto_payload().value()));
  EXPECT_EQ(reply.context_id(), "ack:glic-fcm-session-1");
}

}  // namespace
