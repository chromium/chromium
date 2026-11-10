// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_transport_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/browser_actuator/transport/transport_service_factory.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_coordinator.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_metrics.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_types.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/provider_features.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/glic/test_support/scoped_glic_provider_override.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/transport/common.h"
#include "components/browser_actuator/public/transport/payload_type_mapping.h"
#include "components/browser_actuator/public/transport/transport_channel.h"
#include "components/browser_actuator/public/transport/transport_handler_factory_registry.h"
#include "components/browser_actuator/public/transport/transport_service.h"
#include "components/browser_actuator/public/transport/transport_session.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"
#include "components/sync_device_info/device_info.h"
#include "components/tabs/public/mock_tab_interface.h"
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

    feature_list_.InitWithFeatures(
        {features::kGlicActor, features::kGlicActorPolicyControlExemption,
         features::kGlicExperimentalTriggering},
        {});
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
    profile_->GetPrefs()->SetInteger(
        glic::prefs::kGlicSparkPolicySettings,
        static_cast<int>(glic::prefs::GlicSparkPolicyState::kEnabled));
    glic_service->enabling().SetCompletedFre(
        glic::prefs::FreStatus::kCompleted);
    glic_service->enabling().SetUserEnabledActuationOnWeb(true);
    glic_service->enabling().SetExperimentalTriggeringEnabled(true);
    ASSERT_EQ(glic_service->enabling().GetExperimentalTriggeringState(),
              syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady);
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

TEST_F(GlicExperimentalTriggeringTransportHandlerTest,
       GeneratesUuidWhenContextIdAndSessionIdAreEmpty) {
  OptIn();
  ON_CALL(*session_, GetSessionId()).WillByDefault(testing::Return(""));

  testing::NiceMock<MockGlicExperimentalTriggeringCoordinator> mock_coordinator(
      profile_);
  EXPECT_CALL(mock_coordinator,
              OnRequest(testing::Not(testing::IsEmpty()), testing::_,
                        testing::_, testing::_, nullptr))
      .WillOnce([](const std::string& context_id,
                   const ExperimentalTriggeringRequest& request,
                   ScopedIncomingMessageResultLogger result_logger,
                   GlicExperimentalTriggeringUpdateCallback update_callback,
                   tabs::TabInterface* prepared_tab)
                    -> std::optional<ExperimentalTriggeringResponse> {
        EXPECT_FALSE(context_id.empty());
        EXPECT_EQ(context_id, request.context_id);
        result_logger.set_result(
            GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
        return std::nullopt;
      });

  GlicExperimentalTriggeringTransportHandler handler(
      profile_, session_.get(), mock_coordinator.GetWeakPtr());

  components_sharing_message::GlicExperimentalTriggering triggering;
  triggering.mutable_request()->mutable_device_opt_in_request();
  handler.OnMessage(PayloadType::kExperimentalTriggering,
                    triggering.SerializeAsString());
}

// The coordinator drops a duplicate start during a running task with an empty
// reply. On this transport an empty reply sends nothing, and the session stays
// open for the running task.
TEST_F(GlicExperimentalTriggeringTransportHandlerTest,
       DuplicateStartMidTaskSendsNothingAndKeepsSession) {
  OptIn();
  testing::NiceMock<MockGlicExperimentalTriggeringCoordinator> mock_coordinator(
      profile_);
  EXPECT_CALL(mock_coordinator, OnRequest)
      .WillOnce([](const std::string& context_id,
                   const ExperimentalTriggeringRequest& request,
                   ScopedIncomingMessageResultLogger result_logger,
                   GlicExperimentalTriggeringUpdateCallback update_callback,
                   tabs::TabInterface* prepared_tab)
                    -> std::optional<ExperimentalTriggeringResponse> {
        result_logger.set_result(
            GlicExperimentalTriggeringIncomingMessageResult::
                kTaskAlreadyRunning);
        return std::nullopt;
      });
  EXPECT_CALL(*session_, SendUpstreamMessage).Times(0);
  EXPECT_CALL(*session_, Close()).Times(0);
  GlicExperimentalTriggeringTransportHandler handler(
      profile_, session_.get(), mock_coordinator.GetWeakPtr());

  components_sharing_message::GlicExperimentalTriggering triggering;
  triggering.set_context_id("test_session_id");
  triggering.mutable_task_metadata()->set_conversation_id("conv_123");
  triggering.mutable_request()->mutable_trigger_actuation_request();
  base::HistogramTester histogram_tester;
  handler.OnMessage(PayloadType::kExperimentalTriggering,
                    triggering.SerializeAsString());

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult."
      "BrowserActuatorTransport",
      GlicExperimentalTriggeringIncomingMessageResult::kTaskAlreadyRunning, 1);
}

// The coordinator replies FAILED to a request that it refuses during a running
// task, for example a Continue after the user opted out, which ends the task.
// On this transport the terminal reply also closes the session.
TEST_F(GlicExperimentalTriggeringTransportHandlerTest,
       RefusedRequestMidTaskSendsFailedAndClosesSession) {
  OptIn();
  testing::NiceMock<MockGlicExperimentalTriggeringCoordinator> mock_coordinator(
      profile_);
  EXPECT_CALL(mock_coordinator, OnRequest)
      .WillOnce([](const std::string& context_id,
                   const ExperimentalTriggeringRequest& request,
                   ScopedIncomingMessageResultLogger result_logger,
                   GlicExperimentalTriggeringUpdateCallback update_callback,
                   tabs::TabInterface* prepared_tab)
                    -> std::optional<ExperimentalTriggeringResponse> {
        result_logger.set_result(
            GlicExperimentalTriggeringIncomingMessageResult::kUserNotOptedIn);
        ExperimentalTriggeringResponse response;
        response.context_id = context_id;
        response.task_update = TaskUpdate{
            .state = TaskUpdate::State::kFailed,
            .data_type = TaskUpdate::DataType::kErrorMessage,
            .data = "User is not opted in to experimental triggering."};
        return response;
      });
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
  EXPECT_CALL(*session_, Close()).Times(1);
  GlicExperimentalTriggeringTransportHandler handler(
      profile_, session_.get(), mock_coordinator.GetWeakPtr());

  components_sharing_message::GlicExperimentalTriggering triggering;
  triggering.set_context_id("test_session_id");
  triggering.mutable_task_metadata()->set_conversation_id("conv_123");
  triggering.mutable_request()->mutable_continue_actuation_request();
  handler.OnMessage(PayloadType::kExperimentalTriggering,
                    triggering.SerializeAsString());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(GlicExperimentalTriggeringTransportHandlerTest,
       QueuesTriggerActuationUntilBackgroundTabPreparedOnAndroid) {
  base::test::ScopedFeatureList bg_feature;
  bg_feature.InitAndEnableFeature(features::kGlicBackgroundTriggering);
  OptIn();

  testing::NiceMock<MockGlicExperimentalTriggeringCoordinator> mock_coordinator(
      profile_);
  GlicExperimentalTriggeringTransportHandler handler(
      profile_, session_.get(), mock_coordinator.GetWeakPtr());

  components_sharing_message::GlicExperimentalTriggering triggering;
  triggering.set_context_id("bg_context_id");
  triggering.mutable_request()->mutable_trigger_actuation_request();

  components_sharing_message::GlicExperimentalTriggering follow_up;
  follow_up.set_context_id("bg_context_id");
  follow_up.mutable_request()->mutable_continue_actuation_request();

  base::HistogramTester histogram_tester;
  EXPECT_CALL(mock_coordinator, OnRequest(testing::_, testing::_, testing::_,
                                          testing::_, testing::_))
      .Times(0);
  handler.OnMessage(PayloadType::kExperimentalTriggering,
                    triggering.SerializeAsString());
  handler.OnMessage(PayloadType::kExperimentalTriggering,
                    follow_up.SerializeAsString());
  testing::Mock::VerifyAndClearExpectations(&mock_coordinator);

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  ASSERT_TRUE(actor_service);
  EXPECT_TRUE(actor_service->HasPendingTask("bg_context_id"));

  tabs::MockTabInterface mock_tab;
  base::WeakPtrFactory<tabs::TabInterface> tab_weak_factory(&mock_tab);
  ON_CALL(mock_tab, GetWeakPtr())
      .WillByDefault(testing::Return(tab_weak_factory.GetWeakPtr()));
  auto starting_response =
      [](const std::string& context_id,
         const ExperimentalTriggeringRequest& request,
         ScopedIncomingMessageResultLogger result_logger,
         GlicExperimentalTriggeringUpdateCallback update_callback,
         tabs::TabInterface* prepared_tab)
      -> std::optional<ExperimentalTriggeringResponse> {
    result_logger.set_result(
        GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
    ExperimentalTriggeringResponse response;
    response.context_id = context_id;
    response.task_update = TaskUpdate{.state = TaskUpdate::State::kStarting};
    return response;
  };
  {
    testing::InSequence seq;
    EXPECT_CALL(mock_coordinator, OnRequest("bg_context_id", testing::_,
                                            testing::_, testing::_, &mock_tab))
        .WillOnce(starting_response);
    EXPECT_CALL(mock_coordinator, OnRequest("bg_context_id", testing::_,
                                            testing::_, testing::_, nullptr))
        .WillOnce(starting_response);
  }
  EXPECT_CALL(*session_, SendUpstreamMessage(
                             PayloadType::kExperimentalTriggering, testing::_))
      .Times(2)
      .WillRepeatedly(
          testing::Return(base::expected<void, SendUpstreamMessageError>()));
  EXPECT_CALL(*session_, Close()).Times(0);

  actor_service->NotifyBackgroundTabReady(&mock_tab, "bg_context_id");
  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.Android.BackgroundSetupLatency.Success", 2);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult."
      "BrowserActuatorTransport",
      GlicExperimentalTriggeringIncomingMessageResult::kSuccess, 2);
}

TEST_F(GlicExperimentalTriggeringTransportHandlerTest,
       BackgroundSetupFailedSendsErrorResponseOnAndroid) {
  base::test::ScopedFeatureList bg_feature;
  bg_feature.InitAndEnableFeature(features::kGlicBackgroundTriggering);
  OptIn();

  testing::NiceMock<MockGlicExperimentalTriggeringCoordinator> mock_coordinator(
      profile_);
  GlicExperimentalTriggeringTransportHandler handler(
      profile_, session_.get(), mock_coordinator.GetWeakPtr());

  components_sharing_message::GlicExperimentalTriggering triggering;
  triggering.set_context_id("bg_fail_context_id");
  triggering.mutable_task_metadata()->set_sender_sequence_number(11);
  triggering.mutable_request()->mutable_trigger_actuation_request();

  base::HistogramTester histogram_tester;
  handler.OnMessage(PayloadType::kExperimentalTriggering,
                    triggering.SerializeAsString());

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  ASSERT_TRUE(actor_service);
  EXPECT_TRUE(actor_service->HasPendingTask("bg_fail_context_id"));

  EXPECT_CALL(*session_, SendUpstreamMessage(
                             PayloadType::kExperimentalTriggering, testing::_))
      .WillOnce([](PayloadType type, const google::protobuf::MessageLite& msg)
                    -> base::expected<void, SendUpstreamMessageError> {
        const auto& response = static_cast<
            const components_sharing_message::GlicExperimentalTriggering&>(msg);
        EXPECT_EQ("bg_fail_context_id", response.context_id());
        EXPECT_EQ(11, response.task_metadata().last_seen_sequence_number());
        EXPECT_EQ(components_sharing_message::GlicExperimentalTriggering::
                      ExperimentalTriggeringResponse::TaskUpdate::FAILED,
                  response.response().task_update().state());
        return {};
      });
  EXPECT_CALL(*session_, Close()).Times(1);

  actor_service->NotifyBackgroundSetupFailed("bg_fail_context_id");
  EXPECT_FALSE(actor_service->HasPendingTask("bg_fail_context_id"));
  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.Android.BackgroundSetupLatency.Failed", 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult."
      "BrowserActuatorTransport",
      GlicExperimentalTriggeringIncomingMessageResult::kNoBrowserWindow, 1);
}

TEST_F(GlicExperimentalTriggeringTransportHandlerTest,
       StopActuationCancelsQueuedPendingMessageOnAndroid) {
  base::test::ScopedFeatureList bg_feature;
  bg_feature.InitAndEnableFeature(features::kGlicBackgroundTriggering);
  OptIn();

  testing::NiceMock<MockGlicExperimentalTriggeringCoordinator> mock_coordinator(
      profile_);
  GlicExperimentalTriggeringTransportHandler handler(
      profile_, session_.get(), mock_coordinator.GetWeakPtr());

  base::HistogramTester histogram_tester;
  components_sharing_message::GlicExperimentalTriggering start_msg;
  start_msg.set_context_id("bg_stop_context_id");
  start_msg.mutable_task_metadata()->set_sender_sequence_number(5);
  start_msg.mutable_request()->mutable_trigger_actuation_request();
  handler.OnMessage(PayloadType::kExperimentalTriggering,
                    start_msg.SerializeAsString());

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  ASSERT_TRUE(actor_service);
  EXPECT_TRUE(actor_service->HasPendingTask("bg_stop_context_id"));

  components_sharing_message::GlicExperimentalTriggering stop_msg;
  stop_msg.set_context_id("bg_stop_context_id");
  stop_msg.mutable_request()->mutable_stop_actuation_request();

  EXPECT_CALL(*session_, SendUpstreamMessage(
                             PayloadType::kExperimentalTriggering, testing::_))
      .WillOnce([](PayloadType type, const google::protobuf::MessageLite& msg)
                    -> base::expected<void, SendUpstreamMessageError> {
        const auto& response = static_cast<
            const components_sharing_message::GlicExperimentalTriggering&>(msg);
        EXPECT_EQ("bg_stop_context_id", response.context_id());
        EXPECT_EQ(5, response.task_metadata().last_seen_sequence_number());
        EXPECT_EQ(components_sharing_message::GlicExperimentalTriggering::
                      ExperimentalTriggeringResponse::TaskUpdate::FAILED,
                  response.response().task_update().state());
        return {};
      });
  EXPECT_CALL(*session_, Close()).Times(1);
  EXPECT_CALL(mock_coordinator, OnRequest("bg_stop_context_id", testing::_,
                                          testing::_, testing::_, nullptr))
      .WillOnce([](const std::string& context_id,
                   const ExperimentalTriggeringRequest& request,
                   ScopedIncomingMessageResultLogger result_logger,
                   GlicExperimentalTriggeringUpdateCallback update_callback,
                   tabs::TabInterface* prepared_tab)
                    -> std::optional<ExperimentalTriggeringResponse> {
        result_logger.set_result(
            GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
        return std::nullopt;
      });

  handler.OnMessage(PayloadType::kExperimentalTriggering,
                    stop_msg.SerializeAsString());
  EXPECT_FALSE(actor_service->HasPendingTask("bg_stop_context_id"));
  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.Android.BackgroundSetupLatency.Cancelled",
      1);
  histogram_tester.ExpectBucketCount(
      "Glic.ExperimentalTriggering.IncomingMessageResult."
      "BrowserActuatorTransport",
      GlicExperimentalTriggeringIncomingMessageResult::kNoBrowserWindow, 1);
  histogram_tester.ExpectBucketCount(
      "Glic.ExperimentalTriggering.IncomingMessageResult."
      "BrowserActuatorTransport",
      GlicExperimentalTriggeringIncomingMessageResult::kSuccess, 1);
}

// A request that is rejected before any handler (here a version mismatch)
// replies FAILED, which ends the task for the server. A Trigger that waits for
// a background tab on the same context must not start the task later.
TEST_F(GlicExperimentalTriggeringTransportHandlerTest,
       VersionMismatchCancelsQueuedPendingMessageOnAndroid) {
  base::test::ScopedFeatureList bg_feature;
  bg_feature.InitAndEnableFeature(features::kGlicBackgroundTriggering);
  OptIn();

  testing::NiceMock<MockGlicExperimentalTriggeringCoordinator> mock_coordinator(
      profile_);
  GlicExperimentalTriggeringTransportHandler handler(
      profile_, session_.get(), mock_coordinator.GetWeakPtr());

  base::HistogramTester histogram_tester;
  components_sharing_message::GlicExperimentalTriggering start_msg;
  start_msg.set_context_id("bg_reject_context_id");
  start_msg.mutable_task_metadata()->set_sender_sequence_number(5);
  start_msg.mutable_request()->mutable_trigger_actuation_request();
  handler.OnMessage(PayloadType::kExperimentalTriggering,
                    start_msg.SerializeAsString());

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  ASSERT_TRUE(actor_service);
  EXPECT_TRUE(actor_service->HasPendingTask("bg_reject_context_id"));

  components_sharing_message::GlicExperimentalTriggering bad_msg;
  bad_msg.set_context_id("bg_reject_context_id");
  bad_msg.set_glic_experimental_triggering_version(9999);
  bad_msg.mutable_request()->mutable_device_opt_in_request();

  EXPECT_CALL(*session_, SendUpstreamMessage(
                             PayloadType::kExperimentalTriggering, testing::_))
      .WillRepeatedly([](PayloadType type,
                         const google::protobuf::MessageLite& msg)
                          -> base::expected<void, SendUpstreamMessageError> {
        const auto& response = static_cast<
            const components_sharing_message::GlicExperimentalTriggering&>(msg);
        EXPECT_EQ("bg_reject_context_id", response.context_id());
        EXPECT_EQ(components_sharing_message::GlicExperimentalTriggering::
                      ExperimentalTriggeringResponse::TaskUpdate::FAILED,
                  response.response().task_update().state());
        return {};
      });
  EXPECT_CALL(*session_, Close()).Times(testing::AtLeast(1));
  // The queued Trigger is dropped, so it never reaches OnRequest().
  EXPECT_CALL(mock_coordinator, OnRequest).Times(0);

  handler.OnMessage(PayloadType::kExperimentalTriggering,
                    bad_msg.SerializeAsString());
  EXPECT_FALSE(actor_service->HasPendingTask("bg_reject_context_id"));
  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.Android.BackgroundSetupLatency.Cancelled",
      1);
  histogram_tester.ExpectBucketCount(
      "Glic.ExperimentalTriggering.IncomingMessageResult."
      "BrowserActuatorTransport",
      GlicExperimentalTriggeringIncomingMessageResult::
          kVersionMismatchOrUnavailable,
      2);
}

TEST_F(GlicExperimentalTriggeringTransportHandlerTest,
       DestroyingCoordinatorCancelsPendingMessagesOnAndroid) {
  base::test::ScopedFeatureList bg_feature;
  bg_feature.InitAndEnableFeature(features::kGlicBackgroundTriggering);
  OptIn();

  auto mock_coordinator = std::make_unique<
      testing::NiceMock<MockGlicExperimentalTriggeringCoordinator>>(profile_);
  auto handler = std::make_unique<GlicExperimentalTriggeringTransportHandler>(
      profile_, session_.get(), mock_coordinator->GetWeakPtr());

  components_sharing_message::GlicExperimentalTriggering start_msg;
  start_msg.set_context_id("bg_destroy_context_id");
  start_msg.mutable_task_metadata()->set_sender_sequence_number(7);
  start_msg.mutable_request()->mutable_trigger_actuation_request();

  base::HistogramTester histogram_tester;
  handler->OnMessage(PayloadType::kExperimentalTriggering,
                     start_msg.SerializeAsString());

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  ASSERT_TRUE(actor_service);
  EXPECT_TRUE(actor_service->HasPendingTask("bg_destroy_context_id"));

  EXPECT_CALL(*session_, SendUpstreamMessage(
                             PayloadType::kExperimentalTriggering, testing::_))
      .WillOnce([](PayloadType type, const google::protobuf::MessageLite& msg)
                    -> base::expected<void, SendUpstreamMessageError> {
        const auto& response = static_cast<
            const components_sharing_message::GlicExperimentalTriggering&>(msg);
        EXPECT_EQ("bg_destroy_context_id", response.context_id());
        EXPECT_EQ(7, response.task_metadata().last_seen_sequence_number());
        EXPECT_EQ(components_sharing_message::GlicExperimentalTriggering::
                      ExperimentalTriggeringResponse::TaskUpdate::FAILED,
                  response.response().task_update().state());
        return {};
      });
  EXPECT_CALL(*session_, Close()).Times(1);

  mock_coordinator.reset();
  EXPECT_FALSE(actor_service->HasPendingTask("bg_destroy_context_id"));
  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.Android.BackgroundSetupLatency.Shutdown", 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult."
      "BrowserActuatorTransport",
      GlicExperimentalTriggeringIncomingMessageResult::kCoordinatorUnavailable,
      1);
}
#endif  // BUILDFLAG(IS_ANDROID)

// Checks which profiles' GlicKeyedService registers the experimental
// triggering factory on the BrowserActuator transport.
class GlicExperimentalTriggeringTransportRegistrationTest
    : public testing::Test {
 public:
  GlicExperimentalTriggeringTransportRegistrationTest() {
    feature_list_.InitWithFeatures(
        {features::kGlicExperimentalTriggering,
         browser_actuator::kBrowserActuator,
         browser_actuator::kEnableBrowserActuatorForGlicExperimentalTriggering},
        {});
  }

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  // Creates a profile latched to `provider` and returns the factories
  // registered for experimental triggering payloads on its transport channel.
  std::vector<browser_actuator::transport::TransportHandlerFactory*>
  GetRegisteredFactories(GlicProvider provider) {
    ScopedGlicProviderOverride provider_override(provider);
    TestingProfile::TestingFactories testing_factories;
    testing_factories.emplace_back(
        GlicKeyedServiceFactory::GetInstance(),
        base::BindLambdaForTesting([&](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(context);
          return std::make_unique<GlicKeyedService>(
              profile, IdentityManagerFactory::GetForProfile(profile),
              profile_manager_.profile_manager(), &glic_profile_manager_,
              /*contextual_cueing_service=*/nullptr,
              /*actor_keyed_service=*/nullptr);
        }));
    TestingProfile* profile = profile_manager_.CreateTestingProfile(
        "profile", std::move(testing_factories));
    EXPECT_TRUE(GlicKeyedService::Get(profile));
    browser_actuator::transport::TransportService* transport_service =
        browser_actuator::transport::TransportServiceFactory::GetForProfile(
            profile);
    if (!transport_service || !transport_service->GetChannel()) {
      ADD_FAILURE() << "No BrowserActuator transport channel";
      return {};
    }
    return transport_service->GetChannel()
        ->GetHandlerFactoryRegistry()
        ->GetFactories(PayloadType::kExperimentalTriggering);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  GlicProfileManager glic_profile_manager_;
};

TEST_F(GlicExperimentalTriggeringTransportRegistrationTest,
       RegisteredForGemini) {
  EXPECT_EQ(GetRegisteredFactories(GlicProvider::kGemini).size(), 1u);
}

TEST_F(GlicExperimentalTriggeringTransportRegistrationTest,
       NotRegisteredForGeminiEnterprise) {
  EXPECT_TRUE(GetRegisteredFactories(GlicProvider::kGeminiEnterprise).empty());
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
