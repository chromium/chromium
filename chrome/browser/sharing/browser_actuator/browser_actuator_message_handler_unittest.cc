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
#include "chrome/browser/browser_actuator/transport/transport_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_actuator/internal/transport/features.h"
#include "components/browser_actuator/internal/transport/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/transport/test_support/fake_proto_stream_server.h"
#include "components/browser_actuator/internal/transport/test_support/wait_for.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/transport/common.h"
#include "components/browser_actuator/public/transport/payload_type_mapping.h"
#include "components/browser_actuator/public/transport/transport_channel.h"
#include "components/browser_actuator/public/transport/transport_handler.h"
#include "components/browser_actuator/public/transport/transport_handler_factory.h"
#include "components/browser_actuator/public/transport/transport_handler_factory_registry.h"
#include "components/browser_actuator/public/transport/transport_service.h"
#include "components/browser_actuator/public/transport/transport_session.h"
#include "components/browser_actuator/public/transport/transport_session_registry.h"
#include "components/browser_actuator/test_support/transport/mock_transport_service.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/sharing_message/fake_sharing_handler_registry.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"
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
    // Install the mock before the profile exists: KeyedServices created with
    // the profile (such as ActorKeyedService) look up TransportService at
    // construction and again at shutdown, and must see the same instance.
    profile_ =
        TestingProfile::Builder()
            .AddTestingFactory(
                browser_actuator::transport::TransportServiceFactory::
                    GetInstance(),
                base::BindRepeating([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<
                      browser_actuator::transport::MockTransportService>();
                }))
            .Build();
    mock_service_ =
        static_cast<browser_actuator::transport::MockTransportService*>(
            browser_actuator::transport::TransportServiceFactory::GetForProfile(
                profile_.get()));

    handler_ = std::make_unique<BrowserActuatorMessageHandler>(profile_.get());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<browser_actuator::transport::MockTransportService> mock_service_;
  std::unique_ptr<BrowserActuatorMessageHandler> handler_;
};

class MockTransportSession
    : public browser_actuator::transport::TransportSession {
 public:
  MOCK_METHOD(std::string_view, GetSessionId, (), (const, override));
  MOCK_METHOD(
      (base::expected<void,
                      browser_actuator::transport::SendUpstreamMessageError>),
      SendUpstreamMessage,
      (browser_actuator::transport::PayloadType payload_type,
       const google::protobuf::MessageLite& message),
      (override));
  MOCK_METHOD(void,
              OnMessage,
              (browser_actuator::transport::PayloadType payload_type,
               std::string_view serialized_payload),
              (override));
  MOCK_METHOD(void, Close, (), (override));
};

TEST_F(BrowserActuatorMessageHandlerTest,
       DoesNotCreateSessionWithoutStartSessionPayload) {
  components_sharing_message::SharingMessage message;
  browser_actuator::ActuatorDownstreamMessage* bundled =
      message.mutable_actuator_downstream_message();
  bundled->set_session_id("bundled_session_123");

  EXPECT_CALL(*mock_service_, CreateSession(testing::_)).Times(0);
  EXPECT_CALL(*mock_service_, GetSession(testing::_)).Times(0);

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
  const std::string serialized_command = command.SerializeAsString();

  auto* typed_payload = bundled->add_typed_payloads();
  typed_payload->set_payload_type(
      browser_actuator::ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  typed_payload->mutable_proto_payload()->set_value(serialized_command);

  MockTransportSession mock_session;
  EXPECT_CALL(*mock_service_, CreateSession("bundled_session_123"))
      .WillOnce(testing::Return(
          static_cast<browser_actuator::transport::TransportSession*>(
              &mock_session)));
  EXPECT_CALL(mock_session,
              OnMessage(browser_actuator::transport::PayloadType::kControl,
                        serialized_command));

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());
  EXPECT_TRUE(done_future.Wait());
  EXPECT_EQ(done_future.Get(), nullptr);
}

TEST_F(BrowserActuatorMessageHandlerTest,
       HandlesActuatorDownstreamMessageWithExperimentalTriggeringPayload) {
  components_sharing_message::SharingMessage message;
  browser_actuator::ActuatorDownstreamMessage* bundled =
      message.mutable_actuator_downstream_message();
  bundled->set_session_id("bundled_session_123");

  components_sharing_message::GlicExperimentalTriggering triggering;
  triggering.set_context_id("test_context_123");
  triggering.set_glic_experimental_triggering_version(1);
  triggering.mutable_request()->mutable_trigger_actuation_request();
  const std::string serialized_triggering = triggering.SerializeAsString();

  auto* typed_payload = bundled->add_typed_payloads();
  typed_payload->set_payload_type(
      browser_actuator::
          ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING);
  typed_payload->mutable_proto_payload()->set_value(serialized_triggering);

  MockTransportSession mock_session;
  EXPECT_CALL(*mock_service_, CreateSession(testing::_)).Times(0);
  EXPECT_CALL(*mock_service_, GetSession("bundled_session_123"))
      .WillOnce(testing::Return(
          static_cast<browser_actuator::transport::TransportSession*>(
              &mock_session)));
  EXPECT_CALL(
      mock_session,
      OnMessage(
          browser_actuator::transport::PayloadType::kExperimentalTriggering,
          serialized_triggering));

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
  EXPECT_CALL(*mock_service_, GetSession("bundled_session_123"))
      .WillOnce(testing::Return(
          static_cast<browser_actuator::transport::TransportSession*>(
              session_ptr)));
  EXPECT_CALL(
      *session_ptr,
      OnMessage(browser_actuator::transport::PayloadType::kControl, testing::_))
      .WillOnce([&owned_session](browser_actuator::transport::PayloadType,
                                 std::string_view) {
        // Simulate TransportSessionRegistryImpl::DestroySession().
        owned_session.reset();
      });
  // The second payload is a StartSession command; if it fails to create a
  // session (or if a subsequent non-StartSession payload looks up the destroyed
  // session), the loop stops without dispatching to the destroyed session.
  EXPECT_CALL(*mock_service_, CreateSession("bundled_session_123"))
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
  const std::string serialized_start = start_command.SerializeAsString();
  AddControlPayload(bundled, start_command);
  auto* triggering_payload = bundled->add_typed_payloads();
  triggering_payload->set_payload_type(
      browser_actuator::
          ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING);
  triggering_payload->mutable_proto_payload()->set_value("trigger-bytes");
  auto* actuation_payload = bundled->add_typed_payloads();
  actuation_payload->set_payload_type(
      browser_actuator::ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_ACTUATION);
  actuation_payload->mutable_proto_payload()->set_value("actuation-bytes");

  MockTransportSession mock_session;
  auto* session = static_cast<browser_actuator::transport::TransportSession*>(
      &mock_session);
  EXPECT_CALL(*mock_service_, CreateSession("bundled_session_123"))
      .WillOnce(testing::Return(session));
  // First payload (`StartSession`) calls `CreateSession()`; the remaining two
  // payloads call `GetSession()`.
  EXPECT_CALL(*mock_service_, GetSession("bundled_session_123"))
      .Times(2)
      .WillRepeatedly(testing::Return(session));
  EXPECT_CALL(mock_session,
              OnMessage(browser_actuator::transport::PayloadType::kControl,
                        serialized_start));
  EXPECT_CALL(
      mock_session,
      OnMessage(
          browser_actuator::transport::PayloadType::kExperimentalTriggering,
          "trigger-bytes"));
  EXPECT_CALL(mock_session,
              OnMessage(browser_actuator::transport::PayloadType::kActuation,
                        "actuation-bytes"));

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());
  EXPECT_TRUE(done_future.Wait());
  EXPECT_EQ(done_future.Get(), nullptr);
}

TEST_F(BrowserActuatorMessageHandlerTest,
       SkipsExperimentalTriggeringPayloadWhenFeatureDisabled) {
  base::test::ScopedFeatureList disabled_triggering;
  disabled_triggering.InitAndDisableFeature(
      browser_actuator::kEnableBrowserActuatorForGlicExperimentalTriggering);

  components_sharing_message::SharingMessage message;
  browser_actuator::ActuatorDownstreamMessage* bundled =
      message.mutable_actuator_downstream_message();
  bundled->set_session_id("bundled_session_123");

  auto* triggering_payload = bundled->add_typed_payloads();
  triggering_payload->set_payload_type(
      browser_actuator::
          ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING);
  triggering_payload->mutable_proto_payload()->set_value("trigger-bytes");
  auto* actuation_payload = bundled->add_typed_payloads();
  actuation_payload->set_payload_type(
      browser_actuator::ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_ACTUATION);
  actuation_payload->mutable_proto_payload()->set_value("actuation-bytes");

  MockTransportSession mock_session;
  auto* session = static_cast<browser_actuator::transport::TransportSession*>(
      &mock_session);
  EXPECT_CALL(*mock_service_, CreateSession(testing::_)).Times(0);
  EXPECT_CALL(*mock_service_, GetSession("bundled_session_123"))
      .Times(1)
      .WillRepeatedly(testing::Return(session));
  EXPECT_CALL(
      mock_session,
      OnMessage(
          browser_actuator::transport::PayloadType::kExperimentalTriggering,
          testing::_))
      .Times(0);
  EXPECT_CALL(mock_session,
              OnMessage(browser_actuator::transport::PayloadType::kActuation,
                        "actuation-bytes"));

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
// `TransportService`, starts the downstream transport channel, and sends
// the `StartSessionAck` upstream.
class BrowserActuatorFcmIntegrationTest : public testing::Test {
 protected:
  BrowserActuatorFcmIntegrationTest() {
    feature_list_.InitWithFeatures(
        {browser_actuator::kBrowserActuator,
         browser_actuator::transport::kBrowserActuatorChannelEnabled,
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
    bool received = browser_actuator::transport::WaitFor([&]() {
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
  browser_actuator::transport::FakeProtoStreamServer stream_server(
      &test_url_loader_factory_, browser_actuator::GetWatchSessionsEndPoint());

  components_sharing_message::SharingMessage sharing_msg;
  auto* bundled = sharing_msg.mutable_actuator_downstream_message();
  bundled->set_session_id("fcm-session-1");
  bundled->set_sequence_number(0);
  browser_actuator::ControlCommand start_cmd;
  start_cmd.mutable_start_session();
  auto* typed = bundled->add_typed_payloads();
  typed->set_payload_type(
      browser_actuator::ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  typed->mutable_proto_payload()->set_type_url(
      std::string(browser_actuator::transport::ExpectedTypeUrl(
          browser_actuator::transport::PayloadType::kControl)));
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
  EXPECT_TRUE(upstream_msg.has_responding_to_sequence_number());
  EXPECT_EQ(upstream_msg.responding_to_sequence_number(), 0);
  ASSERT_EQ(upstream_msg.typed_payloads_size(), 1);
  EXPECT_EQ(upstream_msg.typed_payloads(0).payload_type(),
            browser_actuator::ACTUATOR_UPSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  browser_actuator::ControlCommand ack_cmd;
  ASSERT_TRUE(ack_cmd.ParseFromString(
      upstream_msg.typed_payloads(0).proto_payload().value()));
  EXPECT_TRUE(ack_cmd.has_start_session_ack());

  browser_actuator::transport::TransportService* service =
      browser_actuator::transport::TransportServiceFactory::GetForProfile(
          profile_.get());
  ASSERT_NE(service, nullptr);
  EXPECT_NE(service->GetStreamRecorderFactory(), nullptr);

  // 3. Verify the started stream delivers subsequent downstream CloseSession.
  browser_actuator::ActuatorDownstreamMessage close_msg;
  close_msg.set_session_id("fcm-session-1");
  close_msg.set_sequence_number(1);
  browser_actuator::ControlCommand close_cmd;
  close_cmd.mutable_close_session();
  auto* close_typed = close_msg.add_typed_payloads();
  close_typed->set_payload_type(
      browser_actuator::ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  close_typed->mutable_proto_payload()->set_type_url(
      std::string(browser_actuator::transport::ExpectedTypeUrl(
          browser_actuator::transport::PayloadType::kControl)));
  close_typed->mutable_proto_payload()->set_value(
      close_cmd.SerializeAsString());
  stream_server.PushMessage(close_msg);

  EXPECT_TRUE(browser_actuator::transport::WaitFor([&]() {
    return service->GetChannel()->GetSessionRegistry()->GetSession(
               "fcm-session-1") == nullptr;
  }));
}

class EchoTriggeringHandler
    : public browser_actuator::transport::TransportHandler {
 public:
  explicit EchoTriggeringHandler(
      browser_actuator::transport::TransportSession* session)
      : session_(session) {}
  void OnMessage(browser_actuator::transport::PayloadType payload_type,
                 std::string_view serialized_payload) override {
    components_sharing_message::GlicExperimentalTriggering parsed;
    ASSERT_TRUE(parsed.ParseFromString(std::string(serialized_payload)));
    components_sharing_message::GlicExperimentalTriggering reply;
    reply.set_context_id("ack:" + parsed.context_id());
    EXPECT_TRUE(session_->SendUpstreamMessage(payload_type, reply).has_value());
  }

 private:
  raw_ptr<browser_actuator::transport::TransportSession> session_;
};

class EchoTriggeringFactory
    : public browser_actuator::transport::TransportHandlerFactory {
 public:
  browser_actuator::transport::FactoryId GetFactoryId() const override {
    return browser_actuator::transport::FactoryId::kExperimentalTriggering;
  }
  std::vector<browser_actuator::transport::PayloadType>
  GetSupportedPayloadTypes() const override {
    return {browser_actuator::transport::PayloadType::kExperimentalTriggering};
  }
  std::unique_ptr<browser_actuator::transport::TransportHandler> OnNewSession(
      browser_actuator::transport::TransportSession* session) override {
    return std::make_unique<EchoTriggeringHandler>(session);
  }
};

TEST_F(BrowserActuatorFcmIntegrationTest,
       FcmGlicExperimentalTriggeringStartsChannelAndSendsUpstreamReply) {
  browser_actuator::transport::FakeProtoStreamServer stream_server(
      &test_url_loader_factory_, browser_actuator::GetWatchSessionsEndPoint());
  browser_actuator::transport::TransportService* service =
      browser_actuator::transport::TransportServiceFactory::GetForProfile(
          profile_.get());
  ASSERT_NE(service, nullptr);
  EchoTriggeringFactory triggering_factory;
  service->GetChannel()->GetHandlerFactoryRegistry()->RegisterFactory(
      &triggering_factory);
  base::ScopedClosureRunner unregister_runner(base::BindOnce(
      [](browser_actuator::transport::TransportService* s,
         browser_actuator::transport::TransportHandlerFactory* f) {
        s->GetChannel()->GetHandlerFactoryRegistry()->UnregisterFactory(f);
      },
      service, &triggering_factory));

  components_sharing_message::SharingMessage sharing_msg;
  auto* bundled = sharing_msg.mutable_actuator_downstream_message();
  bundled->set_session_id("glic-fcm-session-1");
  bundled->set_sequence_number(0);

  browser_actuator::ControlCommand start_cmd;
  start_cmd.mutable_start_session();
  auto* start_typed = bundled->add_typed_payloads();
  start_typed->set_payload_type(
      browser_actuator::ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  start_typed->mutable_proto_payload()->set_type_url(
      std::string(browser_actuator::transport::ExpectedTypeUrl(
          browser_actuator::transport::PayloadType::kControl)));
  start_typed->mutable_proto_payload()->set_value(
      start_cmd.SerializeAsString());

  components_sharing_message::GlicExperimentalTriggering triggering;
  triggering.set_context_id("glic-fcm-session-1");
  triggering.set_glic_experimental_triggering_version(1);
  triggering.mutable_request()->mutable_trigger_actuation_request();

  auto* typed = bundled->add_typed_payloads();
  typed->set_payload_type(
      browser_actuator::
          ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING);
  typed->mutable_proto_payload()->set_type_url(
      std::string(browser_actuator::transport::ExpectedTypeUrl(
          browser_actuator::transport::PayloadType::kExperimentalTriggering)));
  typed->mutable_proto_payload()->set_value(triggering.SerializeAsString());

  gcm::IncomingMessage fcm_msg;
  fcm_msg.message_id = "0:1563805165426489%0bb84dcff9fd7ece";
  ASSERT_TRUE(sharing_msg.SerializeToString(&fcm_msg.raw_data));

  fcm_handler_->OnMessage(kSharingFCMAppID, fcm_msg);

  stream_server.WaitForConnection();
  ASSERT_EQ(stream_server.last_watch_request().sessions_size(), 1);
  EXPECT_EQ(stream_server.last_watch_request().sessions(0).session_id(),
            "glic-fcm-session-1");

  // 1. Wait for StartSessionAck from the StartSession control payload.
  browser_actuator::SendSessionMessageRequest ack_req =
      WaitForUpstreamRequest();
  EXPECT_EQ(ack_req.actuator_upstream_message().session_id(),
            "glic-fcm-session-1");

  // 2. Wait for the GlicExperimentalTriggering reply.
  browser_actuator::SendSessionMessageRequest upstream_req =
      WaitForUpstreamRequest();
  const auto& upstream_msg = upstream_req.actuator_upstream_message();
  EXPECT_EQ(upstream_msg.session_id(), "glic-fcm-session-1");
  EXPECT_FALSE(upstream_msg.has_responding_to_sequence_number());
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
