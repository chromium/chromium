// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_coordinator.h"

#include <memory>
#include <optional>
#include <variant>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/actor/glic_actor_task_manager.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_metrics.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_types.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_instance.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"
#include "components/sync_device_info/device_info.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace glic {

namespace {

constexpr char kTestContextId[] = "test_context";

class TestGlicExperimentalTriggeringCoordinator
    : public GlicExperimentalTriggeringCoordinator {
 public:
  explicit TestGlicExperimentalTriggeringCoordinator(Profile* profile)
      : GlicExperimentalTriggeringCoordinator(profile) {}
  ~TestGlicExperimentalTriggeringCoordinator() override = default;

  tabs::TabInterface* GetActiveTab() const override { return active_tab_; }
  BrowserWindowInterface* GetBrowserWindow() const override {
    return browser_window_;
  }
  void set_browser_window(BrowserWindowInterface* window) {
    browser_window_ = window;
  }
  void set_active_tab(tabs::TabInterface* tab) { active_tab_ = tab; }

 private:
  raw_ptr<BrowserWindowInterface> browser_window_ = nullptr;
  raw_ptr<tabs::TabInterface> active_tab_ = nullptr;
};

class GlicExperimentalTriggeringCoordinatorTest : public testing::Test {
 public:
  GlicExperimentalTriggeringCoordinatorTest() = default;
  ~GlicExperimentalTriggeringCoordinatorTest() override = default;

  void SetUp() override {
    // glic_enabling.cc returns UNAVAILABLE for managed machines, so force
    // disable.
    scoped_platform_management_override_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetInstance()->GetForPlatform(),
            policy::EnterpriseManagementAuthority::NONE);

    feature_list_.InitAndEnableFeature(features::kGlicExperimentalTriggering);
    ASSERT_TRUE(profile_manager_.SetUp());

    TestingProfile::TestingFactories testing_factories;
    testing_factories.emplace_back(
        GlicKeyedServiceFactory::GetInstance(),
        base::BindRepeating(
            &GlicExperimentalTriggeringCoordinatorTest::CreateGlicKeyedService,
            base::Unretained(this)));

    profile_ = profile_manager_.CreateTestingProfile(
        "test_profile", std::move(testing_factories));

    coordinator_ = std::make_unique<TestGlicExperimentalTriggeringCoordinator>(
        profile_);
    OptIn();

    ON_CALL(mock_glic_instance_, GetActorTaskManager())
        .WillByDefault(testing::Return(
            reinterpret_cast<GlicActorTaskManager*>(dummy_task_manager_buf_)));
  }

  void TearDown() override {
    coordinator_.reset();
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

    ON_CALL(*service, InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
        .WillByDefault(
            [this](InvokeWithAutoSubmitPasskey passkey,
                   GlicInvokeOptions options,
                   GlicInvokeWithAutoSubmitOptions auto_submit_options) {
              if (options.on_client_connected) {
                std::move(options.on_client_connected)
                    .Run(mock_glic_instance_.GetWeakPtr());
              }
              return mock_glic_instance_.GetWeakPtr();
            });

    return service;
  }

  void OptIn() {
    auto* glic_service = GlicKeyedServiceFactory::GetGlicKeyedService(
        profile_, /*create=*/true);
    ASSERT_TRUE(glic_service);
    // This allows this test to run on managed (dev) machines.
    profile_->GetPrefs()->SetInteger(
        glic::prefs::kGlicSparkPolicySettings,
        static_cast<int>(glic::prefs::GlicSparkPolicyState::kEnabled));
    glic_service->enabling().SetCompletedFre(
        glic::prefs::FreStatus::kCompleted);
    glic_service->enabling().SetUserEnabledActuationOnWeb(true);
    glic_service->enabling().SetExperimentalTriggeringEnabled(true);
  }

  std::optional<ExperimentalTriggeringResponse> SendRequest(
      const ExperimentalTriggeringRequest& request,
      tabs::TabInterface* prepared_tab = nullptr) {
    if (prepared_tab) {
      coordinator_->set_active_tab(prepared_tab);
    }
    return coordinator_->OnRequest(
        kTestContextId, request,
        ScopedIncomingMessageResultLogger(
            ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
        base::DoNothing(), nullptr);
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
  std::unique_ptr<TestGlicExperimentalTriggeringCoordinator> coordinator_;
  testing::NiceMock<MockGlicInstance> mock_glic_instance_;
  alignas(GlicActorTaskManager) char dummy_task_manager_buf_[sizeof(
      GlicActorTaskManager)] = {0};
};

TEST_F(GlicExperimentalTriggeringCoordinatorTest, NoTaskMetadata) {
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.payload = TriggerActuationRequest{.initial_prompt = "hello"};

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data_type,
            TaskUpdate::DataType::kErrorMessage);
  EXPECT_EQ(response->task_update->data,
            "Received GlicExperimentalTriggering message with missing task "
            "metadata.");
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest, NoServerChannelConfig) {
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = std::monostate();

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data_type,
            TaskUpdate::DataType::kErrorMessage);
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest, NoRequestPayload) {
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = std::monostate();

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest, GetScreenshotRequest) {
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = GetScreenshotRequest{
      .public_key = {'k', 'e', 'y'},
      .auth_secret = {'s', 'e', 'c'},
  };

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data_type,
            TaskUpdate::DataType::kErrorMessage);
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest, UnrecognizedStopActuation) {
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_);
  base::test::TestFuture<std::string> cancel_future;
  auto subscription =
      actor_service->AddMessageTriggerTaskStoppedCallback(base::BindRepeating(
          [](base::test::TestFuture<std::string>* future,
             const std::string& context_id) { future->SetValue(context_id); },
          base::Unretained(&cancel_future)));

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = StopActuationRequest{.stop_reason = "STOPPED_BY_USER"};

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data,
            "Failed to stop task due to missing glic instance.");

  EXPECT_TRUE(cancel_future.Wait());
  EXPECT_EQ(cancel_future.Take(), kTestContextId);
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest, NoBrowserWindow) {
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "hello"};

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data_type,
            TaskUpdate::DataType::kErrorMessage);
  EXPECT_EQ(response->task_update->data,
            "No browser window found for current profile.");
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest, SameVersionNoBrowserWindow) {
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "hello"};

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data_type,
            TaskUpdate::DataType::kErrorMessage);
  EXPECT_EQ(response->task_update->data,
            "No browser window found for current profile.");
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest, RejectRequestWhenNotOptedIn) {
  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/true);
  ASSERT_TRUE(glic_service);
  glic_service->enabling().SetCompletedFre(glic::prefs::FreStatus::kNotStarted);
  glic_service->enabling().SetUserEnabledActuationOnWeb(false);
  glic_service->enabling().SetExperimentalTriggeringEnabled(false);

  base::HistogramTester histogram_tester;

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{
      .conversation_id = "conv_123",
      .sender_sequence_number = 42,
  };
  request.payload = TriggerActuationRequest{.initial_prompt = "hello"};

  auto response = SendRequest(request);

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.StateOnActuationRequest",
      syncer::DeviceInfo::GlicExperimentalTriggeringState::kNeedsOptIn, 1);

  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data_type,
            TaskUpdate::DataType::kErrorMessage);
  EXPECT_EQ(response->task_update->data,
            "User is not opted in to experimental triggering.");
  EXPECT_EQ(response->task_metadata->sender_sequence_number, 0);
  EXPECT_EQ(response->task_metadata->last_seen_sequence_number, 42);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       CleansUpUpdatesHandlerOnOptInRejection) {
  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/true);
  ASSERT_TRUE(glic_service);
  glic_service->enabling().SetCompletedFre(glic::prefs::FreStatus::kNotStarted);
  glic_service->enabling().SetUserEnabledActuationOnWeb(false);
  glic_service->enabling().SetExperimentalTriggeringEnabled(false);

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{
      .conversation_id = "conv_123",
      .sender_sequence_number = 42,
  };
  request.payload = TriggerActuationRequest{.initial_prompt = "hello"};

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_metadata->sender_sequence_number, 0);
  EXPECT_EQ(response->task_metadata->last_seen_sequence_number, 42);

  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       CleansUpUpdatesHandlerOnStopActuation) {
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{
      .conversation_id = "conv_123",
      .sender_sequence_number = 42,
  };
  request.payload = StopActuationRequest{.stop_reason = "STOPPED_BY_USER"};

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data,
            "Failed to stop task due to missing glic instance.");

  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       HandlesMultipleConcurrentDeviceOptInRequests) {
  auto web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile_));
  tabs::MockTabInterface mock_tab;
  ON_CALL(mock_tab, GetContents())
      .WillByDefault(testing::Return(web_contents.get()));
  coordinator_->set_active_tab(&mock_tab);

  base::test::TestFuture<ExperimentalTriggeringResponse> future1;
  base::test::TestFuture<ExperimentalTriggeringResponse> future2;

  ExperimentalTriggeringRequest request1;
  request1.version = 1;
  request1.context_id = "context_1";
  request1.task_metadata = TaskMetadata{.sender_sequence_number = 42};
  request1.payload = DeviceOptInRequest{.triggering_source = "ChromeOS"};

  ExperimentalTriggeringRequest request2;
  request2.version = 1;
  request2.context_id = "context_2";
  request2.task_metadata = TaskMetadata{.sender_sequence_number = 43};
  request2.payload = DeviceOptInRequest{.triggering_source = "ChromeOS"};

  auto response1 = coordinator_->OnRequest(
      "context_1", request1,
      ScopedIncomingMessageResultLogger(
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      future1.GetRepeatingCallback(), nullptr);
  EXPECT_FALSE(response1.has_value());

  auto response2 = coordinator_->OnRequest(
      "context_2", request2,
      ScopedIncomingMessageResultLogger(
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      future2.GetRepeatingCallback(), nullptr);
  EXPECT_FALSE(response2.has_value());

  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/true);
  ASSERT_TRUE(glic_service);
  glic_service->opt_in_controller().CloseDialog(/*accepted=*/true);

  auto result1 = future1.Take();
  ASSERT_TRUE(result1.device_opt_in_result.has_value());
  EXPECT_EQ(*result1.device_opt_in_result, DeviceOptInResult::kAccepted);
  EXPECT_EQ(result1.task_metadata->sender_sequence_number, 0);
  EXPECT_EQ(result1.task_metadata->last_seen_sequence_number, 42);

  auto result2 = future2.Take();
  ASSERT_TRUE(result2.device_opt_in_result.has_value());
  EXPECT_EQ(*result2.device_opt_in_result, DeviceOptInResult::kAccepted);
  EXPECT_EQ(result2.task_metadata->sender_sequence_number, 0);
  EXPECT_EQ(result2.task_metadata->last_seen_sequence_number, 43);
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       HandlesMultipleConcurrentDeviceOptInRequestsDeclinedOnTeardown) {
  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/true);
  ASSERT_TRUE(glic_service);
  glic_service->enabling().SetExperimentalTriggeringEnabled(false);

  auto web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile_));
  tabs::MockTabInterface mock_tab;
  ON_CALL(mock_tab, GetContents())
      .WillByDefault(testing::Return(web_contents.get()));
  coordinator_->set_active_tab(&mock_tab);

  base::test::TestFuture<ExperimentalTriggeringResponse> future1;
  base::test::TestFuture<ExperimentalTriggeringResponse> future2;

  ExperimentalTriggeringRequest request1;
  request1.version = 1;
  request1.context_id = "context_1";
  request1.task_metadata = TaskMetadata{.sender_sequence_number = 42};
  request1.payload = DeviceOptInRequest{.triggering_source = "ChromeOS"};

  ExperimentalTriggeringRequest request2;
  request2.version = 1;
  request2.context_id = "context_2";
  request2.task_metadata = TaskMetadata{.sender_sequence_number = 43};
  request2.payload = DeviceOptInRequest{.triggering_source = "ChromeOS"};

  auto response1 = coordinator_->OnRequest(
      "context_1", request1,
      ScopedIncomingMessageResultLogger(
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      future1.GetRepeatingCallback(), nullptr);
  EXPECT_FALSE(response1.has_value());

  auto response2 = coordinator_->OnRequest(
      "context_2", request2,
      ScopedIncomingMessageResultLogger(
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      future2.GetRepeatingCallback(), nullptr);
  EXPECT_FALSE(response2.has_value());

  glic_service->opt_in_controller().CloseDialog(/*accepted=*/false);

  auto result1 = future1.Take();
  ASSERT_TRUE(result1.device_opt_in_result.has_value());
  EXPECT_EQ(*result1.device_opt_in_result, DeviceOptInResult::kDeclined);
  EXPECT_EQ(result1.task_metadata->sender_sequence_number, 0);
  EXPECT_EQ(result1.task_metadata->last_seen_sequence_number, 42);

  auto result2 = future2.Take();
  ASSERT_TRUE(result2.device_opt_in_result.has_value());
  EXPECT_EQ(*result2.device_opt_in_result, DeviceOptInResult::kDeclined);
  EXPECT_EQ(result2.task_metadata->sender_sequence_number, 0);
  EXPECT_EQ(result2.task_metadata->last_seen_sequence_number, 43);
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       OnProtoMessage_MissingPayload) {
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.set_context_id(kTestContextId);
  proto.mutable_task_metadata()->set_conversation_id("conv_123");
  proto.mutable_task_metadata()->set_task_id("task_456");
  proto.mutable_task_metadata()->set_sender_sequence_number(42);

  base::HistogramTester histogram_tester;
  auto response = coordinator_->OnProtoMessage(
      kTestContextId, proto,
      ScopedIncomingMessageResultLogger(ScopedIncomingMessageResultLogger::
                                            Channel::kBrowserActuatorTransport),
      base::DoNothing(), nullptr);

  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(
      response->task_update->data,
      "Received GlicExperimentalTriggering message with no request payload.");
  ASSERT_TRUE(response->task_metadata.has_value());
  EXPECT_EQ(response->task_metadata->conversation_id, "conv_123");
  EXPECT_EQ(response->task_metadata->task_id, "task_456");
  EXPECT_EQ(response->task_metadata->last_seen_sequence_number, 42);
  EXPECT_EQ(response->task_metadata->sender_sequence_number, 0);

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult."
      "BrowserActuatorTransport",
      GlicExperimentalTriggeringIncomingMessageResult::kMissingPayload, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       OnProtoMessage_EmptyRequestPayload) {
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.set_context_id(kTestContextId);
  // Create an empty request submessage where payload_case() == PAYLOAD_NOT_SET.
  proto.mutable_request();

  base::HistogramTester histogram_tester;
  auto response = coordinator_->OnProtoMessage(
      kTestContextId, proto,
      ScopedIncomingMessageResultLogger(ScopedIncomingMessageResultLogger::
                                            Channel::kBrowserActuatorTransport),
      base::DoNothing(), nullptr);

  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(
      response->task_update->data,
      "Received GlicExperimentalTriggering message with no request payload.");

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult."
      "BrowserActuatorTransport",
      GlicExperimentalTriggeringIncomingMessageResult::kMissingPayload, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       OnProtoMessage_VersionMismatch) {
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.set_context_id(kTestContextId);
  proto.mutable_task_metadata()->set_sender_sequence_number(42);
  proto.set_glic_experimental_triggering_version(9999);
  proto.mutable_request()->mutable_device_opt_in_request();

  base::HistogramTester histogram_tester;
  auto response = coordinator_->OnProtoMessage(
      kTestContextId, proto,
      ScopedIncomingMessageResultLogger(ScopedIncomingMessageResultLogger::
                                            Channel::kBrowserActuatorTransport),
      base::DoNothing(), nullptr);

  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data,
            "Rejected: version mismatch or unavailable.");
  ASSERT_TRUE(response->task_metadata.has_value());
  EXPECT_EQ(response->task_metadata->last_seen_sequence_number, 42);
  EXPECT_EQ(response->task_metadata->sender_sequence_number, 0);

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult."
      "BrowserActuatorTransport",
      GlicExperimentalTriggeringIncomingMessageResult::
          kVersionMismatchOrUnavailable,
      1);
}

class GlicExperimentalTriggeringCoordinatorWithTabTest
    : public GlicExperimentalTriggeringCoordinatorTest {
 public:
  GlicExperimentalTriggeringCoordinatorWithTabTest() = default;
  ~GlicExperimentalTriggeringCoordinatorWithTabTest() override = default;

  void SetUp() override {
    GlicExperimentalTriggeringCoordinatorTest::SetUp();
    coordinator_->set_browser_window(&mock_browser_window_);
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    ON_CALL(mock_tab_, GetContents())
        .WillByDefault(testing::Return(web_contents_.get()));
    ON_CALL(mock_tab_, GetWeakPtr())
        .WillByDefault(testing::Return(tab_weak_factory_.GetWeakPtr()));
    ON_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));
    instance_helper_ = std::make_unique<GlicInstanceHelper>(&mock_tab_);
    coordinator_->set_active_tab(&mock_tab_);
  }

  void TearDown() override {
    instance_helper_.reset();
    web_contents_.reset();
    GlicExperimentalTriggeringCoordinatorTest::TearDown();
  }

 protected:
  MockBrowserWindowInterface mock_browser_window_;
  std::unique_ptr<content::WebContents> web_contents_;
  tabs::MockTabInterface mock_tab_;
  base::WeakPtrFactory<tabs::TabInterface> tab_weak_factory_{&mock_tab_};
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<GlicInstanceHelper> instance_helper_;
};

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       RelaysParentConversationMetadataInitial) {
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{
      .conversation_id = "conv_123",
      .parent_conversation_metadata =
          ParentConversationMetadata{
              .conversation_id = "test_init_id",
              .conversation_title = "test_init_title",
          },
  };
  request.payload = TriggerActuationRequest{.initial_prompt = "hello"};

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kStarting);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       RelaysParentConversationMetadataUpdated) {
  ExperimentalTriggeringRequest init_request;
  init_request.version = 1;
  init_request.context_id = kTestContextId;
  init_request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  init_request.payload = TriggerActuationRequest{.initial_prompt = "hello"};
  auto init_response = SendRequest(init_request);
  ASSERT_TRUE(init_response.has_value());
  ASSERT_TRUE(init_response->task_update.has_value());
  EXPECT_EQ(init_response->task_update->state, TaskUpdate::State::kStarting);

  ExperimentalTriggeringRequest update_request;
  update_request.version = 1;
  update_request.context_id = kTestContextId;
  update_request.task_metadata = TaskMetadata{
      .parent_conversation_metadata =
          ParentConversationMetadata{
              .conversation_id = "test_conv_id",
              .conversation_title = "test_title",
          },
  };
  update_request.payload = TaskMetadataUpdated{};

  base::HistogramTester histogram_tester;
  auto response = SendRequest(update_request);
  EXPECT_FALSE(response.has_value());
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kSuccess, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       RespectsLastSeenSequenceNumber) {
  ExperimentalTriggeringRequest init_request;
  init_request.version = 1;
  init_request.context_id = kTestContextId;
  init_request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  init_request.payload = TriggerActuationRequest{.initial_prompt = "hello"};
  auto init_response = SendRequest(init_request);
  ASSERT_TRUE(init_response.has_value());
  ASSERT_TRUE(init_response->task_update.has_value());
  EXPECT_EQ(init_response->task_update->state, TaskUpdate::State::kStarting);

  ExperimentalTriggeringRequest stop_request;
  stop_request.version = 1;
  stop_request.context_id = kTestContextId;
  stop_request.task_metadata = TaskMetadata{
      .conversation_id = "conv_123",
      .sender_sequence_number = 42,
  };
  stop_request.payload = StopActuationRequest{.stop_reason = "STOPPED_BY_USER"};

  auto response = SendRequest(stop_request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kStopped);
  EXPECT_EQ(response->task_metadata->last_seen_sequence_number, 42);
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       RecordsInitialMessageDeliveryLatency) {
  base::HistogramTester histogram_tester;
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.set_context_id(kTestContextId);
  proto.set_glic_experimental_triggering_version(1);
  proto.mutable_request()->mutable_device_opt_in_request();

  base::Time sent_time = base::Time::Now() - base::Seconds(5);
  auto* timestamp = proto.mutable_task_metadata()->mutable_server_time_stamp();
  timestamp->set_seconds((sent_time - base::Time::UnixEpoch()).InSeconds());
  timestamp->set_nanos(0);

  coordinator_->OnProtoMessage(
      kTestContextId, proto,
      ScopedIncomingMessageResultLogger(
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      base::DoNothing(), nullptr);

  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.FirstFCMMessageLatency", 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       DoesNotRecordLatencyForInvalidServerTimestamp) {
  base::HistogramTester histogram_tester;
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.set_context_id(kTestContextId);
  proto.set_glic_experimental_triggering_version(1);
  proto.mutable_request()->mutable_device_opt_in_request();

  // Default/empty server timestamp (seconds = 0, nanos = 0).
  proto.mutable_task_metadata()->mutable_server_time_stamp();

  coordinator_->OnProtoMessage(
      kTestContextId, proto,
      ScopedIncomingMessageResultLogger(
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      base::DoNothing(), nullptr);

  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.FirstFCMMessageLatency", 0);
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       ClampsNegativeDeliveryLatencyToZero) {
  base::HistogramTester histogram_tester;
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.set_context_id(kTestContextId);
  proto.set_glic_experimental_triggering_version(1);
  proto.mutable_request()->mutable_device_opt_in_request();

  // Server timestamp is 10 seconds in the future due to clock skew.
  base::Time sent_time = base::Time::Now() + base::Seconds(10);
  auto* timestamp = proto.mutable_task_metadata()->mutable_server_time_stamp();
  timestamp->set_seconds((sent_time - base::Time::UnixEpoch()).InSeconds());
  timestamp->set_nanos(0);

  coordinator_->OnProtoMessage(
      kTestContextId, proto,
      ScopedIncomingMessageResultLogger(
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      base::DoNothing(), nullptr);

  histogram_tester.ExpectBucketCount(
      "Glic.ExperimentalTriggering.FirstFCMMessageLatency", 0, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       DoesNotRecordLatencyForSubsequentMessagesInSameContext) {
  base::HistogramTester histogram_tester;
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.set_context_id(kTestContextId);
  proto.set_glic_experimental_triggering_version(1);
  proto.mutable_request()->mutable_trigger_actuation_request();

  base::Time sent_time = base::Time::Now() - base::Seconds(5);
  auto* timestamp = proto.mutable_task_metadata()->mutable_server_time_stamp();
  timestamp->set_seconds((sent_time - base::Time::UnixEpoch()).InSeconds());
  timestamp->set_nanos(0);

  // 1. First message initializes the updates handler and logs latency.
  coordinator_->OnProtoMessage(
      kTestContextId, proto,
      ScopedIncomingMessageResultLogger(
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      base::DoNothing(), nullptr);
  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.FirstFCMMessageLatency", 1);

  // 2. Second message for the same context_id must not log latency again.
  components_sharing_message::GlicExperimentalTriggering proto2;
  proto2.set_context_id(kTestContextId);
  proto2.set_glic_experimental_triggering_version(1);
  proto2.mutable_request()->mutable_continue_actuation_request();
  auto* timestamp2 =
      proto2.mutable_task_metadata()->mutable_server_time_stamp();
  timestamp2->set_seconds((sent_time - base::Time::UnixEpoch()).InSeconds());
  timestamp2->set_nanos(0);

  coordinator_->OnProtoMessage(
      kTestContextId, proto2,
      ScopedIncomingMessageResultLogger(
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      base::DoNothing(), nullptr);
  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.FirstFCMMessageLatency", 1);
}

}  // namespace
}  // namespace glic
