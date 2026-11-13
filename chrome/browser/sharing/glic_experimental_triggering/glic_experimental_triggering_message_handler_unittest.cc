// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/glic_experimental_triggering/glic_experimental_triggering_message_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_coordinator.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_types.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/provider_features.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/sharing_message/mock_sharing_message_sender.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_send_message_result.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

namespace {

constexpr char kDefaultServerConfig[] = "test_config";
constexpr char kDefaultP256dh[] = "test_p256dh";
constexpr char kDefaultAuthSecret[] = "test_auth_secret";

components_sharing_message::SharingMessage CreateTriggeringMessage(
    int64_t sequence_number = 42,
    const std::string& server_config = kDefaultServerConfig) {
  components_sharing_message::SharingMessage message;
  auto* channel = message.mutable_server_channel_configuration();
  channel->set_configuration(server_config);
  channel->set_p256dh(kDefaultP256dh);
  channel->set_auth_secret(kDefaultAuthSecret);
  auto* triggering = message.mutable_glic_experimental_triggering();
  triggering->mutable_task_metadata()->set_sender_sequence_number(
      sequence_number);
  triggering->set_glic_experimental_triggering_version(1);
  return message;
}

class MockGlicExperimentalTriggeringCoordinator
    : public GlicExperimentalTriggeringCoordinator {
 public:
  explicit MockGlicExperimentalTriggeringCoordinator(Profile* profile)
      : GlicExperimentalTriggeringCoordinator(profile) {}
  ~MockGlicExperimentalTriggeringCoordinator() override = default;

  MOCK_METHOD(std::optional<ExperimentalTriggeringResponse>,
              OnRequest,
              (const std::string&,
               const ExperimentalTriggeringRequest&,
               ScopedIncomingMessageResultLogger,
               GlicExperimentalTriggeringUpdateCallback,
               tabs::TabInterface*),
              (override));
};

class TestGlicExperimentalTriggeringMessageHandler
    : public GlicExperimentalTriggeringMessageHandler {
 public:
  TestGlicExperimentalTriggeringMessageHandler(Profile* profile,
                                               SharingMessageSender* sender)
      : GlicExperimentalTriggeringMessageHandler(profile, sender) {}
  TestGlicExperimentalTriggeringMessageHandler(
      Profile* profile,
      SharingMessageSender* sender,
      std::unique_ptr<GlicExperimentalTriggeringCoordinator> coordinator)
      : GlicExperimentalTriggeringMessageHandler(profile,
                                                 sender,
                                                 std::move(coordinator)) {}
  ~TestGlicExperimentalTriggeringMessageHandler() override = default;
};

class GlicExperimentalTriggeringMessageHandlerTest : public testing::Test {
 public:
  GlicExperimentalTriggeringMessageHandlerTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicExperimentalTriggering,
                              features::kGlicActor,
                              features::kGlicActorPolicyControlExemption},
        /*disabled_features=*/{});
  }
  ~GlicExperimentalTriggeringMessageHandlerTest() override = default;

  void SetUp() override {
    platform_management_override_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetInstance()->GetForPlatform(),
            policy::EnterpriseManagementAuthority::NONE);
    ASSERT_TRUE(profile_manager_.SetUp());

    TestingProfile::TestingFactories testing_factories;
    testing_factories.emplace_back(
        GlicKeyedServiceFactory::GetInstance(),
        base::BindRepeating(&GlicExperimentalTriggeringMessageHandlerTest::
                                CreateGlicKeyedService,
                            base::Unretained(this)));

    profile_ = profile_manager_.CreateTestingProfile(
        "test_profile", std::move(testing_factories));

    profile_management_override_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetInstance()->GetForProfile(
                profile_),
            policy::EnterpriseManagementAuthority::NONE);

    handler_ = std::make_unique<TestGlicExperimentalTriggeringMessageHandler>(
        profile_, &mock_sharing_message_sender_);

    OptIn();
  }

  void TearDown() override {
    handler_.reset();
    profile_management_override_.reset();
    platform_management_override_.reset();
    profile_ = nullptr;
  }

  std::unique_ptr<KeyedService> CreateGlicKeyedService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<GlicKeyedService>(
        profile, IdentityManagerFactory::GetForProfile(profile),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_,
        ContextualCueingServiceFactory::GetForProfile(profile),
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile));
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
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass_;
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      platform_management_override_;
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      profile_management_override_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;
  GlicProfileManager glic_profile_manager_;
  testing::NiceMock<MockSharingMessageSender> mock_sharing_message_sender_;
  std::unique_ptr<TestGlicExperimentalTriggeringMessageHandler> handler_;
};

TEST_F(GlicExperimentalTriggeringMessageHandlerTest, NewerVersion) {
  components_sharing_message::SharingMessage message =
      CreateTriggeringMessage();
  auto* triggering = message.mutable_glic_experimental_triggering();
  triggering->set_glic_experimental_triggering_version(2);
  triggering->mutable_request()->mutable_trigger_actuation_request();

  base::test::TestFuture<
      std::unique_ptr<components_sharing_message::ResponseMessage>>
      done_future;
  handler_->OnMessage(std::move(message), done_future.GetCallback());

  EXPECT_TRUE(done_future.Wait());
  auto response = done_future.Take();
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->has_glic_experimental_triggering());
  const auto& triggering_resp = response->glic_experimental_triggering();
  ASSERT_TRUE(triggering_resp.has_response());
  ASSERT_TRUE(triggering_resp.response().has_task_update());
  const auto& task_update = triggering_resp.response().task_update();
  EXPECT_EQ(task_update.state(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::FAILED);
  EXPECT_EQ(task_update.data_type(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::ERROR_MESSAGE);
  EXPECT_EQ(task_update.data(), "Rejected: version mismatch or unavailable.");
}

TEST_F(GlicExperimentalTriggeringMessageHandlerTest,
       AutoGeneratesContextIdWhenAbsent) {
  auto mock_coordinator =
      std::make_unique<MockGlicExperimentalTriggeringCoordinator>(profile_);
  raw_ptr<MockGlicExperimentalTriggeringCoordinator> raw_coordinator =
      mock_coordinator.get();
  handler_ = std::make_unique<TestGlicExperimentalTriggeringMessageHandler>(
      profile_, &mock_sharing_message_sender_, std::move(mock_coordinator));

  components_sharing_message::SharingMessage message =
      CreateTriggeringMessage();
  message.mutable_glic_experimental_triggering()->clear_context_id();
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_device_opt_in_request()
      ->set_triggering_source("ChromeOS");

  base::test::TestFuture<components_sharing_message::ServerChannelConfiguration,
                         components_sharing_message::SharingMessage>
      future;
  EXPECT_CALL(
      mock_sharing_message_sender_,
      SendMessageToServerTarget(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(
          [&](const components_sharing_message::ServerChannelConfiguration&
                  server_channel,
              base::TimeDelta, components_sharing_message::SharingMessage msg,
              SharingMessageSender::ResponseCallback) {
            future.SetValue(server_channel, std::move(msg));
            return base::OnceClosure();
          });

  EXPECT_CALL(*raw_coordinator, OnRequest(testing::_, testing::_, testing::_,
                                          testing::_, testing::_))
      .WillOnce([&](const std::string& context_id,
                    const ExperimentalTriggeringRequest& request,
                    ScopedIncomingMessageResultLogger result_logger,
                    GlicExperimentalTriggeringUpdateCallback update_callback,
                    tabs::TabInterface* prepared_tab) {
        EXPECT_FALSE(context_id.empty());
        result_logger.set_result(
            GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
        ExperimentalTriggeringResponse update_response;
        update_response.context_id = context_id;
        update_callback.Run(std::move(update_response));
        return std::nullopt;
      });

  handler_->OnMessage(std::move(message), base::DoNothing());

  auto [server_channel, received_message] = future.Take();
  EXPECT_FALSE(
      received_message.glic_experimental_triggering().context_id().empty());
}

TEST_F(GlicExperimentalTriggeringMessageHandlerTest,
       AutoGeneratesContextIdWhenEmpty) {
  auto mock_coordinator =
      std::make_unique<MockGlicExperimentalTriggeringCoordinator>(profile_);
  raw_ptr<MockGlicExperimentalTriggeringCoordinator> raw_coordinator =
      mock_coordinator.get();
  handler_ = std::make_unique<TestGlicExperimentalTriggeringMessageHandler>(
      profile_, &mock_sharing_message_sender_, std::move(mock_coordinator));

  components_sharing_message::SharingMessage message =
      CreateTriggeringMessage();
  message.mutable_glic_experimental_triggering()->set_context_id("");
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_device_opt_in_request()
      ->set_triggering_source("ChromeOS");

  base::test::TestFuture<components_sharing_message::ServerChannelConfiguration,
                         components_sharing_message::SharingMessage>
      future;
  EXPECT_CALL(
      mock_sharing_message_sender_,
      SendMessageToServerTarget(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(
          [&](const components_sharing_message::ServerChannelConfiguration&
                  server_channel,
              base::TimeDelta, components_sharing_message::SharingMessage msg,
              SharingMessageSender::ResponseCallback) {
            future.SetValue(server_channel, std::move(msg));
            return base::OnceClosure();
          });

  EXPECT_CALL(*raw_coordinator, OnRequest(testing::_, testing::_, testing::_,
                                          testing::_, testing::_))
      .WillOnce([&](const std::string& context_id,
                    const ExperimentalTriggeringRequest& request,
                    ScopedIncomingMessageResultLogger result_logger,
                    GlicExperimentalTriggeringUpdateCallback update_callback,
                    tabs::TabInterface* prepared_tab) {
        EXPECT_FALSE(context_id.empty());
        result_logger.set_result(
            GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
        ExperimentalTriggeringResponse update_response;
        update_response.context_id = context_id;
        update_callback.Run(std::move(update_response));
        return std::nullopt;
      });

  handler_->OnMessage(std::move(message), base::DoNothing());

  auto [server_channel, received_message] = future.Take();
  EXPECT_FALSE(
      received_message.glic_experimental_triggering().context_id().empty());
}

TEST_F(GlicExperimentalTriggeringMessageHandlerTest,
       LatestServerChannelConfigurationWinsForUpdates) {
  auto mock_coordinator =
      std::make_unique<MockGlicExperimentalTriggeringCoordinator>(profile_);
  raw_ptr<MockGlicExperimentalTriggeringCoordinator> raw_coordinator =
      mock_coordinator.get();
  handler_ = std::make_unique<TestGlicExperimentalTriggeringMessageHandler>(
      profile_, &mock_sharing_message_sender_, std::move(mock_coordinator));

  auto message1 = CreateTriggeringMessage(42, "config_1");
  auto* triggering1 = message1.mutable_glic_experimental_triggering();
  triggering1->set_context_id("test-context-id");
  triggering1->mutable_request()
      ->mutable_device_opt_in_request()
      ->set_triggering_source("ChromeOS");

  GlicExperimentalTriggeringUpdateCallback update_callback;

  EXPECT_CALL(*raw_coordinator, OnRequest(testing::_, testing::_, testing::_,
                                          testing::_, testing::_))
      .WillOnce([&](const std::string& context_id,
                    const ExperimentalTriggeringRequest& request,
                    ScopedIncomingMessageResultLogger result_logger,
                    GlicExperimentalTriggeringUpdateCallback cb,
                    tabs::TabInterface* prepared_tab) {
        result_logger.set_result(
            GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
        update_callback = std::move(cb);
        return std::nullopt;
      })
      .WillOnce([&](const std::string& context_id,
                    const ExperimentalTriggeringRequest& request,
                    ScopedIncomingMessageResultLogger result_logger,
                    GlicExperimentalTriggeringUpdateCallback cb,
                    tabs::TabInterface* prepared_tab) {
        result_logger.set_result(
            GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
        update_callback = std::move(cb);
        return std::nullopt;
      });

  handler_->OnMessage(std::move(message1), base::DoNothing());

  auto message2 = CreateTriggeringMessage(43, "config_2");
  auto* triggering2 = message2.mutable_glic_experimental_triggering();
  triggering2->set_context_id("test-context-id");
  triggering2->mutable_request()
      ->mutable_device_opt_in_request()
      ->set_triggering_source("ChromeOS");

  handler_->OnMessage(std::move(message2), base::DoNothing());

  base::test::TestFuture<components_sharing_message::ServerChannelConfiguration,
                         components_sharing_message::SharingMessage>
      update_future;

  EXPECT_CALL(
      mock_sharing_message_sender_,
      SendMessageToServerTarget(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(
          [&](const components_sharing_message::ServerChannelConfiguration&
                  server_channel,
              base::TimeDelta,
              components_sharing_message::SharingMessage message,
              SharingMessageSender::ResponseCallback) {
            update_future.SetValue(server_channel, std::move(message));
            return base::OnceClosure();
          });

  ASSERT_TRUE(update_callback);
  ExperimentalTriggeringResponse update_response;
  update_response.context_id = "test-context-id";
  update_callback.Run(std::move(update_response));

  auto [server_channel, received_message] = update_future.Take();
  EXPECT_EQ(server_channel.configuration(), "config_2");
}

TEST_F(GlicExperimentalTriggeringMessageHandlerTest,
       TerminalUpdateSendResult_RecordsOnlyForTerminalTaskUpdates) {
  auto mock_coordinator =
      std::make_unique<MockGlicExperimentalTriggeringCoordinator>(profile_);
  raw_ptr<MockGlicExperimentalTriggeringCoordinator> raw_coordinator =
      mock_coordinator.get();
  handler_ = std::make_unique<TestGlicExperimentalTriggeringMessageHandler>(
      profile_, &mock_sharing_message_sender_, std::move(mock_coordinator));

  GlicExperimentalTriggeringUpdateCallback update_callback;
  EXPECT_CALL(*raw_coordinator, OnRequest(testing::_, testing::_, testing::_,
                                          testing::_, testing::_))
      .WillOnce([&](const std::string& context_id,
                    const ExperimentalTriggeringRequest& request,
                    ScopedIncomingMessageResultLogger result_logger,
                    GlicExperimentalTriggeringUpdateCallback cb,
                    tabs::TabInterface* prepared_tab) {
        result_logger.set_result(
            GlicExperimentalTriggeringIncomingMessageResult::kSuccess);
        update_callback = std::move(cb);
        return std::nullopt;
      });

  auto message = CreateTriggeringMessage(1);
  message.mutable_glic_experimental_triggering()->set_context_id("ctx_send");
  message.mutable_glic_experimental_triggering()
      ->mutable_request()
      ->mutable_trigger_actuation_request();
  handler_->OnMessage(std::move(message), base::DoNothing());
  ASSERT_TRUE(update_callback);

  std::vector<SharingMessageSender::ResponseCallback> pending_send_callbacks;
  EXPECT_CALL(
      mock_sharing_message_sender_,
      SendMessageToServerTarget(testing::_, testing::_, testing::_, testing::_))
      .WillRepeatedly(
          [&](const components_sharing_message::ServerChannelConfiguration&,
              base::TimeDelta, components_sharing_message::SharingMessage,
              SharingMessageSender::ResponseCallback callback) {
            pending_send_callbacks.push_back(std::move(callback));
            return base::OnceClosure();
          });

  base::HistogramTester histogram_tester;

  // 1. Non-terminal TaskUpdates (RUNNING, YIELD, PAUSED, RESUMED) and
  // non-TaskUpdate responses do not record TerminalUpdateSendResult.
  for (TaskUpdate::State non_terminal_state :
       {TaskUpdate::State::kStarting, TaskUpdate::State::kRunning,
        TaskUpdate::State::kYield, TaskUpdate::State::kPaused,
        TaskUpdate::State::kResumed}) {
    ExperimentalTriggeringResponse resp;
    resp.context_id = "ctx_send";
    resp.task_update = TaskUpdate{.state = non_terminal_state};
    update_callback.Run(std::move(resp));
  }
  ExperimentalTriggeringResponse opt_in_resp;
  opt_in_resp.context_id = "ctx_send";
  opt_in_resp.device_opt_in_result = DeviceOptInResult::kAccepted;
  update_callback.Run(std::move(opt_in_resp));

  ExperimentalTriggeringResponse screenshot_resp;
  screenshot_resp.context_id = "ctx_send";
  screenshot_resp.screenshot_result =
      ScreenshotResult{.status = ScreenshotResult::Status::kSuccess};
  update_callback.Run(std::move(screenshot_resp));

  for (auto& cb : pending_send_callbacks) {
    std::move(cb).Run(SharingSendMessageResult::kSuccessful, nullptr);
  }
  pending_send_callbacks.clear();

  for (const char* name :
       {"Glic.ExperimentalTriggering.TerminalUpdateSendResult",
        "Glic.Consumer.ExperimentalTriggering.TerminalUpdateSendResult"}) {
    histogram_tester.ExpectTotalCount(name, 0);
  }

  // 2. Terminal TaskUpdates (COMPLETE, STOPPED, FAILED) record the send result,
  // including when the callback runs after the message handler is destroyed.
  for (TaskUpdate::State terminal_state :
       {TaskUpdate::State::kComplete, TaskUpdate::State::kStopped,
        TaskUpdate::State::kFailed}) {
    ExperimentalTriggeringResponse resp;
    resp.context_id = "ctx_send";
    resp.task_update = TaskUpdate{.state = terminal_state};
    update_callback.Run(std::move(resp));
  }
  ASSERT_EQ(pending_send_callbacks.size(), 3u);

  std::move(pending_send_callbacks[0])
      .Run(SharingSendMessageResult::kSuccessful, nullptr);
  std::move(pending_send_callbacks[1])
      .Run(SharingSendMessageResult::kNetworkError, nullptr);

  // Destroy handler before the third in-flight send callback completes.
  raw_coordinator = nullptr;
  handler_.reset();
  std::move(pending_send_callbacks[2])
      .Run(SharingSendMessageResult::kAckTimeout, nullptr);

  for (const char* name :
       {"Glic.ExperimentalTriggering.TerminalUpdateSendResult",
        "Glic.Consumer.ExperimentalTriggering.TerminalUpdateSendResult"}) {
    histogram_tester.ExpectBucketCount(
        name, SharingSendMessageResult::kSuccessful, 1);
    histogram_tester.ExpectBucketCount(
        name, SharingSendMessageResult::kNetworkError, 1);
    histogram_tester.ExpectBucketCount(
        name, SharingSendMessageResult::kAckTimeout, 1);
    histogram_tester.ExpectTotalCount(name, 3);
  }
}

}  // namespace
}  // namespace glic
