// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_transport_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_coordinator.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/browser_actuator/public/transport/common.h"
#include "components/browser_actuator/public/transport/payload_type_mapping.h"
#include "components/browser_actuator/public/transport/transport_session.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

using browser_actuator::transport::FactoryId;
using browser_actuator::transport::PayloadType;
using browser_actuator::transport::SendUpstreamMessageError;
using browser_actuator::transport::TransportSession;

class MockTransportSession : public TransportSession {
 public:
  MockTransportSession() = default;
  ~MockTransportSession() override = default;

  MOCK_METHOD(std::string_view, GetSessionId, (), (const, override));
  MOCK_METHOD((base::expected<void, SendUpstreamMessageError>),
              SendUpstreamMessage,
              (PayloadType payload_type,
               const google::protobuf::MessageLite& message),
              (override));
  MOCK_METHOD(void,
              OnMessage,
              (PayloadType payload_type, std::string_view serialized_payload),
              (override));
  MOCK_METHOD(void, Close, (), (override));
};

class MockGlicExperimentalTriggeringCoordinator
    : public GlicExperimentalTriggeringCoordinator {
 public:
  explicit MockGlicExperimentalTriggeringCoordinator(Profile* profile)
      : GlicExperimentalTriggeringCoordinator(profile) {}
  ~MockGlicExperimentalTriggeringCoordinator() override = default;

  MOCK_METHOD(std::optional<ExperimentalTriggeringResponse>,
              OnRequest,
              (const std::string& context_id,
               const ExperimentalTriggeringRequest& request,
               ScopedIncomingMessageResultLogger result_logger,
               GlicExperimentalTriggeringUpdateCallback update_callback,
               tabs::TabInterface* prepared_tab),
              (override));
};

class GlicExperimentalTriggeringTransportHandlerTest : public testing::Test {
 public:
  GlicExperimentalTriggeringTransportHandlerTest() = default;
  ~GlicExperimentalTriggeringTransportHandlerTest() override = default;

  void SetUp() override {
    scoped_platform_management_override_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetInstance()->GetForPlatform(),
            policy::EnterpriseManagementAuthority::NONE);

    feature_list_.InitAndEnableFeature(features::kGlicExperimentalTriggering);
    ASSERT_TRUE(profile_manager_.SetUp());

    TestingProfile::TestingFactories testing_factories;
    testing_factories.emplace_back(
        GlicKeyedServiceFactory::GetInstance(),
        base::BindRepeating(&GlicExperimentalTriggeringTransportHandlerTest::
                                CreateGlicKeyedService,
                            base::Unretained(this)));

    profile_ = profile_manager_.CreateTestingProfile(
        "test_profile", std::move(testing_factories));

    session_ = std::make_unique<testing::NiceMock<MockTransportSession>>();
    ON_CALL(*session_, GetSessionId())
        .WillByDefault(testing::Return("test_session_id"));
  }

  void TearDown() override {
    session_.reset();
    profile_ = nullptr;
  }

  std::unique_ptr<KeyedService> CreateGlicKeyedService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto service = std::make_unique<testing::NiceMock<MockGlicKeyedService>>(
        profile, IdentityManagerFactory::GetForProfile(profile),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_,
        ContextualCueingServiceFactory::GetForProfile(profile),
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile));
    return service;
  }

  void OptIn() {
    auto* glic_service =
        GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/true);
    ASSERT_TRUE(glic_service);
    glic_service->enabling().SetCompletedFre(
        glic::prefs::FreStatus::kCompleted);
    glic_service->enabling().SetUserEnabledActuationOnWeb(true);
    glic_service->enabling().SetExperimentalTriggeringEnabled(true);
  }

 protected:
  GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      scoped_platform_management_override_;
  base::test::ScopedFeatureList feature_list_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;
  GlicProfileManager glic_profile_manager_;
  std::unique_ptr<MockTransportSession> session_;
};

TEST_F(GlicExperimentalTriggeringTransportHandlerTest,
       FailsWhenNoCoordinatorProvided) {
  GlicExperimentalTriggeringTransportHandler handler(profile_, session_.get(),
                                                     /*coordinator=*/nullptr);

  components_sharing_message::GlicExperimentalTriggering triggering;
  triggering.set_context_id("test_session_id");
  triggering.mutable_task_metadata()->set_conversation_id("conv_123");
  triggering.mutable_task_metadata()->set_task_id("task_456");
  triggering.mutable_task_metadata()->set_sender_sequence_number(42);
  triggering.mutable_request()->mutable_device_opt_in_request();

  base::HistogramTester histogram_tester;

  EXPECT_CALL(*session_, SendUpstreamMessage(
                             PayloadType::kExperimentalTriggering, testing::_))
      .WillOnce([&](PayloadType type, const google::protobuf::MessageLite& msg)
                    -> base::expected<void, SendUpstreamMessageError> {
        const auto& response = static_cast<
            const components_sharing_message::GlicExperimentalTriggering&>(msg);
        EXPECT_EQ("test_session_id", response.context_id());
        EXPECT_TRUE(response.has_response());
        EXPECT_TRUE(response.response().has_task_update());
        EXPECT_EQ(components_sharing_message::GlicExperimentalTriggering::
                      ExperimentalTriggeringResponse::TaskUpdate::FAILED,
                  response.response().task_update().state());
        EXPECT_TRUE(response.has_task_metadata());
        EXPECT_EQ("conv_123", response.task_metadata().conversation_id());
        EXPECT_EQ("task_456", response.task_metadata().task_id());
        EXPECT_EQ(42, response.task_metadata().last_seen_sequence_number());
        EXPECT_EQ(0, response.task_metadata().sender_sequence_number());
        return {};
      });
  EXPECT_CALL(*session_, Close()).Times(1);

  handler.OnMessage(PayloadType::kExperimentalTriggering,
                    triggering.SerializeAsString());

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult."
      "BrowserActuatorTransport",
      GlicExperimentalTriggeringIncomingMessageResult::kCoordinatorUnavailable,
      1);
}

TEST_F(GlicExperimentalTriggeringTransportHandlerTest,
       HandlesDeviceOptInRequest) {
  OptIn();

  testing::NiceMock<MockGlicExperimentalTriggeringCoordinator> mock_coordinator(
      profile_);
  EXPECT_CALL(mock_coordinator, OnRequest("test_session_id", testing::_,
                                          testing::_, testing::_, nullptr))
      .WillOnce([](const std::string& context_id,
                   const ExperimentalTriggeringRequest& request,
                   ScopedIncomingMessageResultLogger result_logger,
                   GlicExperimentalTriggeringUpdateCallback update_callback,
                   tabs::TabInterface* prepared_tab)
                    -> std::optional<ExperimentalTriggeringResponse> {
        result_logger.set_result(
            GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
        ExperimentalTriggeringResponse response;
        response.context_id = context_id;
        TaskMetadata meta;
        meta.conversation_id = "conv_123";
        meta.last_seen_sequence_number = 42;
        response.task_metadata = std::move(meta);
        response.device_opt_in_result = DeviceOptInResult::kAccepted;
        return response;
      });

  GlicExperimentalTriggeringTransportHandler handler(
      profile_, session_.get(), mock_coordinator.GetWeakPtr());

  components_sharing_message::GlicExperimentalTriggering triggering;
  triggering.set_context_id("test_session_id");
  triggering.mutable_task_metadata()->set_conversation_id("conv_123");
  triggering.mutable_task_metadata()->set_sender_sequence_number(42);
  triggering.mutable_request()->mutable_device_opt_in_request();

  base::HistogramTester histogram_tester;

  EXPECT_CALL(*session_, SendUpstreamMessage(
                             PayloadType::kExperimentalTriggering, testing::_))
      .WillOnce([&](PayloadType type, const google::protobuf::MessageLite& msg)
                    -> base::expected<void, SendUpstreamMessageError> {
        const auto& response = static_cast<
            const components_sharing_message::GlicExperimentalTriggering&>(msg);
        EXPECT_EQ("test_session_id", response.context_id());
        EXPECT_TRUE(response.has_response());
        EXPECT_EQ(components_sharing_message::GlicExperimentalTriggering::
                      ExperimentalTriggeringResponse::ACCEPTED,
                  response.response().device_opt_in_result());
        EXPECT_EQ(42, response.task_metadata().last_seen_sequence_number());
        return {};
      });
  EXPECT_CALL(*session_, Close()).Times(0);

  handler.OnMessage(PayloadType::kExperimentalTriggering,
                    triggering.SerializeAsString());

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult."
      "BrowserActuatorTransport",
      GlicExperimentalTriggeringIncomingMessageResult::kSuccess, 1);
}

TEST_F(GlicExperimentalTriggeringTransportHandlerTest, HandlesMissingPayload) {
  GlicExperimentalTriggeringCoordinator coordinator(profile_);
  GlicExperimentalTriggeringTransportHandler handler(profile_, session_.get(),
                                                     coordinator.GetWeakPtr());

  components_sharing_message::GlicExperimentalTriggering triggering;
  triggering.set_context_id("test_session_id");

  base::HistogramTester histogram_tester;

  EXPECT_CALL(*session_, SendUpstreamMessage(
                             PayloadType::kExperimentalTriggering, testing::_))
      .WillOnce([&](PayloadType type, const google::protobuf::MessageLite& msg)
                    -> base::expected<void, SendUpstreamMessageError> {
        const auto& response = static_cast<
            const components_sharing_message::GlicExperimentalTriggering&>(msg);
        EXPECT_EQ("test_session_id", response.context_id());
        EXPECT_TRUE(response.has_response());
        EXPECT_TRUE(response.response().has_task_update());
        EXPECT_EQ(components_sharing_message::GlicExperimentalTriggering::
                      ExperimentalTriggeringResponse::TaskUpdate::FAILED,
                  response.response().task_update().state());
        return {};
      });

  handler.OnMessage(PayloadType::kExperimentalTriggering,
                    triggering.SerializeAsString());

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult."
      "BrowserActuatorTransport",
      GlicExperimentalTriggeringIncomingMessageResult::kMissingPayload, 1);
}

TEST_F(GlicExperimentalTriggeringTransportHandlerTest, FactoryMethods) {
  GlicExperimentalTriggeringTransportHandlerFactory factory(profile_);
  EXPECT_EQ(FactoryId::kExperimentalTriggering, factory.GetFactoryId());

  auto types = factory.GetSupportedPayloadTypes();
  ASSERT_EQ(1u, types.size());
  EXPECT_EQ(PayloadType::kExperimentalTriggering, types[0]);

  auto handler = factory.OnNewSession(session_.get());
  EXPECT_NE(nullptr, handler);
}

TEST_F(GlicExperimentalTriggeringTransportHandlerTest,
       SharesCoordinatorAcrossSessions) {
  auto mock_coordinator = std::make_unique<
      testing::NiceMock<MockGlicExperimentalTriggeringCoordinator>>(profile_);
  auto* raw_coordinator = mock_coordinator.get();
  GlicExperimentalTriggeringTransportHandlerFactory factory(
      profile_, std::move(mock_coordinator));

  GlicExperimentalTriggeringUpdateCallback captured_update_callback;
  EXPECT_CALL(*raw_coordinator, OnRequest("shared_context_id", testing::_,
                                          testing::_, testing::_, nullptr))
      .Times(2)
      .WillRepeatedly(
          [&](const std::string& context_id,
              const ExperimentalTriggeringRequest& request,
              ScopedIncomingMessageResultLogger result_logger,
              GlicExperimentalTriggeringUpdateCallback update_callback,
              tabs::TabInterface* prepared_tab)
              -> std::optional<ExperimentalTriggeringResponse> {
            result_logger.set_result(
                GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
            captured_update_callback = std::move(update_callback);
            return std::nullopt;
          });

  components_sharing_message::GlicExperimentalTriggering triggering;
  triggering.set_context_id("shared_context_id");
  triggering.mutable_request()->mutable_device_opt_in_request();

  EXPECT_CALL(*session_, SendUpstreamMessage(testing::_, testing::_)).Times(0);

  // Session 1 handles initial message and is then torn down.
  auto handler_1 = factory.OnNewSession(session_.get());
  handler_1->OnMessage(PayloadType::kExperimentalTriggering,
                       triggering.SerializeAsString());
  handler_1.reset();

  // Invoking the callback captured from Session 1 after handler_1 destruction
  // is safely a no-op.
  ASSERT_TRUE(captured_update_callback);
  ExperimentalTriggeringResponse dropped_response;
  dropped_response.context_id = "shared_context_id";
  captured_update_callback.Run(std::move(dropped_response));

  // Session 2 is created on the same factory and rebinds the update callback.
  testing::NiceMock<MockTransportSession> session_2;
  ON_CALL(session_2, GetSessionId())
      .WillByDefault(testing::Return("test_session_id_2"));
  auto handler_2 = factory.OnNewSession(&session_2);
  handler_2->OnMessage(PayloadType::kExperimentalTriggering,
                       triggering.SerializeAsString());

  ASSERT_TRUE(captured_update_callback);
  EXPECT_CALL(session_2, SendUpstreamMessage(
                             PayloadType::kExperimentalTriggering, testing::_))
      .WillOnce([](PayloadType type, const google::protobuf::MessageLite& msg)
                    -> base::expected<void, SendUpstreamMessageError> {
        const auto& response = static_cast<
            const components_sharing_message::GlicExperimentalTriggering&>(msg);
        EXPECT_EQ("shared_context_id", response.context_id());
        return {};
      });

  ExperimentalTriggeringResponse async_response;
  async_response.context_id = "shared_context_id";
  async_response.device_opt_in_result = DeviceOptInResult::kAccepted;
  captured_update_callback.Run(std::move(async_response));
}

TEST_F(GlicExperimentalTriggeringTransportHandlerTest,
       HandlerSurvivesFactoryDestruction) {
  auto factory =
      std::make_unique<GlicExperimentalTriggeringTransportHandlerFactory>(
          profile_);
  auto handler = factory->OnNewSession(session_.get());

  // Destroy the factory (and its owned coordinator) while the session handler
  // is still alive, matching KeyedService shutdown ordering.
  factory.reset();

  components_sharing_message::GlicExperimentalTriggering triggering;
  triggering.set_context_id("test_session_id");
  triggering.mutable_request()->mutable_device_opt_in_request();

  base::HistogramTester histogram_tester;
  EXPECT_CALL(*session_, SendUpstreamMessage(
                             PayloadType::kExperimentalTriggering, testing::_))
      .WillOnce([](PayloadType type, const google::protobuf::MessageLite& msg)
                    -> base::expected<void, SendUpstreamMessageError> {
        const auto& response = static_cast<
            const components_sharing_message::GlicExperimentalTriggering&>(msg);
        EXPECT_EQ(components_sharing_message::GlicExperimentalTriggering::
                      ExperimentalTriggeringResponse::TaskUpdate::FAILED,
                  response.response().task_update().state());
        return {};
      });

  handler->OnMessage(PayloadType::kExperimentalTriggering,
                     triggering.SerializeAsString());

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult."
      "BrowserActuatorTransport",
      GlicExperimentalTriggeringIncomingMessageResult::kCoordinatorUnavailable,
      1);
}

TEST_F(GlicExperimentalTriggeringTransportHandlerTest,
       ClosesSessionOnlyOnTerminalResponse) {
  auto mock_coordinator = std::make_unique<
      testing::NiceMock<MockGlicExperimentalTriggeringCoordinator>>(profile_);
  auto* raw_coordinator = mock_coordinator.get();
  GlicExperimentalTriggeringTransportHandlerFactory factory(
      profile_, std::move(mock_coordinator));

  GlicExperimentalTriggeringUpdateCallback captured_update_callback;
  EXPECT_CALL(*raw_coordinator, OnRequest("test_session_id", testing::_,
                                          testing::_, testing::_, nullptr))
      .WillOnce([&](const std::string& context_id,
                    const ExperimentalTriggeringRequest& request,
                    ScopedIncomingMessageResultLogger result_logger,
                    GlicExperimentalTriggeringUpdateCallback update_callback,
                    tabs::TabInterface* prepared_tab)
                    -> std::optional<ExperimentalTriggeringResponse> {
        result_logger.set_result(
            GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
        captured_update_callback = std::move(update_callback);
        ExperimentalTriggeringResponse starting_response;
        starting_response.context_id = context_id;
        TaskUpdate starting_update;
        starting_update.state = TaskUpdate::State::kStarting;
        starting_response.task_update = std::move(starting_update);
        return starting_response;
      });

  auto handler = factory.OnNewSession(session_.get());

  components_sharing_message::GlicExperimentalTriggering triggering;
  triggering.set_context_id("test_session_id");
  triggering.mutable_request()->mutable_trigger_actuation_request();

  EXPECT_CALL(*session_, SendUpstreamMessage(
                             PayloadType::kExperimentalTriggering, testing::_))
      .WillRepeatedly(
          testing::Return(base::expected<void, SendUpstreamMessageError>()));
  EXPECT_CALL(*session_, Close()).Times(0);

  // Non-terminal kStarting response must not close the session.
  handler->OnMessage(PayloadType::kExperimentalTriggering,
                     triggering.SerializeAsString());
  ASSERT_TRUE(captured_update_callback);

  // Non-terminal kYield response must not close the session.
  ExperimentalTriggeringResponse yield_response;
  yield_response.context_id = "test_session_id";
  TaskUpdate yield_update;
  yield_update.state = TaskUpdate::State::kYield;
  yield_response.task_update = std::move(yield_update);
  captured_update_callback.Run(std::move(yield_response));
  testing::Mock::VerifyAndClearExpectations(session_.get());

  // Terminal kComplete response closes the session.
  EXPECT_CALL(*session_, SendUpstreamMessage(
                             PayloadType::kExperimentalTriggering, testing::_))
      .WillOnce(
          testing::Return(base::expected<void, SendUpstreamMessageError>()));
  EXPECT_CALL(*session_, Close()).Times(1);

  ExperimentalTriggeringResponse complete_response;
  complete_response.context_id = "test_session_id";
  TaskUpdate complete_update;
  complete_update.state = TaskUpdate::State::kComplete;
  complete_response.task_update = std::move(complete_update);
  captured_update_callback.Run(std::move(complete_response));
  testing::Mock::VerifyAndClearExpectations(session_.get());

  // Terminal execute_actions_response closes the session.
  EXPECT_CALL(*session_, SendUpstreamMessage(
                             PayloadType::kExperimentalTriggering, testing::_))
      .WillOnce(
          testing::Return(base::expected<void, SendUpstreamMessageError>()));
  EXPECT_CALL(*session_, Close()).Times(1);

  ExperimentalTriggeringResponse actions_response;
  actions_response.context_id = "test_session_id";
  actions_response.execute_actions_response = ExecuteActionsResponse{};
  captured_update_callback.Run(std::move(actions_response));
  testing::Mock::VerifyAndClearExpectations(session_.get());

  // Terminal declined device_opt_in_result closes the session.
  EXPECT_CALL(*session_, SendUpstreamMessage(
                             PayloadType::kExperimentalTriggering, testing::_))
      .WillOnce(
          testing::Return(base::expected<void, SendUpstreamMessageError>()));
  EXPECT_CALL(*session_, Close()).Times(1);

  ExperimentalTriggeringResponse declined_response;
  declined_response.context_id = "test_session_id";
  declined_response.device_opt_in_result = DeviceOptInResult::kDeclined;
  captured_update_callback.Run(std::move(declined_response));
}

// Pins `ExpectedTypeUrl` to the generated proto; this layer may include it.
TEST(GlicExperimentalTriggeringTypeUrlTest, MatchesProtoTypeName) {
  EXPECT_EQ(
      browser_actuator::transport::ExpectedTypeUrl(
          PayloadType::kExperimentalTriggering),
      base::StrCat({browser_actuator::transport::kTypeUrlPrefix,
                    components_sharing_message::GlicExperimentalTriggering()
                        .GetTypeName()}));
}

}  // namespace
}  // namespace glic
