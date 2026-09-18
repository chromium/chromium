// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_coordinator.h"

#include <memory>
#include <optional>
#include <variant>

#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/test_support/mock_actor_ui_tab_controller.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/actor/glic_actor_task_manager.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_manager.h"
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
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"
#include "components/sync_device_info/device_info.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace glic {

namespace {

constexpr char kTestContextId[] = "test_context";
constexpr char kTestConversationId[] = "conv_123";
constexpr char kTestToolName[] = "test_tool";

class TestExperimentalTriggeringManager
    : public GlicExperimentalTriggeringManager {
 public:
  TestExperimentalTriggeringManager()
      : GlicExperimentalTriggeringManager(nullptr, nullptr) {}
  ~TestExperimentalTriggeringManager() override = default;

  void GetExperimentalTriggeringUpdates(
      mojo::PendingRemote<mojom::ExperimentalTriggeringUpdatesHandler> handler,
      base::OnceCallback<void(bool)> success_status_callback) override {
    handler_.reset();
    handler_.Bind(std::move(handler));
    std::move(success_status_callback).Run(registration_success_);
  }

  void SendUpdate(mojom::ExperimentalTriggeringUpdatePtr update,
                  mojom::SubscriberObservationType observation) {
    if (handler_.is_bound()) {
      handler_->OnUpdate(std::move(update), observation);
    }
  }

  void FlushForTesting() {
    if (handler_.is_bound()) {
      handler_.FlushForTesting();
    }
  }

  void ResetHandler() { handler_.reset(); }
  void set_registration_success(bool success) {
    registration_success_ = success;
  }

  MOCK_METHOD(void,
              CaptureAndUploadEncryptedScreenshot,
              (const std::vector<uint8_t>&,
               const std::vector<uint8_t>&,
               base::OnceCallback<void(
                   base::expected<std::string, ScreenshotResult::Status>)>),
              (override));

 private:
  mojo::Remote<mojom::ExperimentalTriggeringUpdatesHandler> handler_;
  bool registration_success_ = true;
};

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

  // Whether the actor policy checker should be exempted from the policy and
  // account eligibility checks. Test profiles have no signed-in account, so
  // without this CanActOnWeb() is false and requests get rejected before they
  // reach the code under test. Subclasses override this to exercise the
  // rejection path itself.
  virtual bool ActorPolicyControlExemption() const { return true; }

  void SetUp() override {
    // glic_enabling.cc returns UNAVAILABLE for managed machines, so force
    // disable.
    scoped_platform_management_override_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetInstance()->GetForPlatform(),
            policy::EnterpriseManagementAuthority::NONE);

    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicExperimentalTriggering, {}},
         {features::kGlicExperimentalTriggeringScriptTools, {}},
         {features::kGlicActor,
          {{features::kGlicActorPolicyControlExemption.name,
            ActorPolicyControlExemption() ? "true" : "false"}}}},
        /*disabled_features=*/{});
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
    ON_CALL(mock_glic_instance_, GetExperimentalTriggeringManager())
        .WillByDefault(testing::Return(&test_triggering_manager_));
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
              if (options.on_panel_opened) {
                std::move(options.on_panel_opened).Run();
              }
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
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      scoped_platform_management_override_;
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;
  GlicProfileManager glic_profile_manager_;
  std::unique_ptr<TestGlicExperimentalTriggeringCoordinator> coordinator_;
  testing::NiceMock<TestExperimentalTriggeringManager> test_triggering_manager_;
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

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       GetScreenshotRequest_FeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      features::kGlicExperimentalTriggeringScreenshot);

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = GetScreenshotRequest{
      .public_key = {'k', 'e', 'y'},
      .auth_secret = {'s', 'e', 'c'},
      .request_token = {'t', 'o', 'k', 'e', 'n'},
  };

  auto response = coordinator_->OnRequest(
      kTestContextId, request,
      ScopedIncomingMessageResultLogger(
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);

  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data_type,
            TaskUpdate::DataType::kErrorMessage);
  EXPECT_EQ(response->task_update->data, "Screenshot feature is disabled.");

  // Should also notify the callback with kErrorDisabled.
  auto update = update_future.Take();
  ASSERT_TRUE(update.screenshot_result.has_value());
  EXPECT_EQ(update.screenshot_result->status,
            ScreenshotResult::Status::kErrorDisabled);
  EXPECT_EQ(update.screenshot_result->request_token,
            std::vector<uint8_t>({'t', 'o', 'k', 'e', 'n'}));
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       GetScreenshotRequest_FeatureEnabled_NoInstance) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      features::kGlicExperimentalTriggeringScreenshot);

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = GetScreenshotRequest{
      .public_key = {'k', 'e', 'y'},
      .auth_secret = {'s', 'e', 'c'},
      .request_token = {'t', 'o', 'k', 'e', 'n'},
  };

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data_type,
            TaskUpdate::DataType::kErrorMessage);
  EXPECT_EQ(response->task_update->data,
            "No active Glic instance available for screenshot.");
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       GetCapabilities_ReflectsStateAndFeatureFlag) {
  // 1. When state is kUnavailable, capabilities is always empty.
  EXPECT_TRUE(
      GlicExperimentalTriggeringCoordinator::GetCapabilities(
          syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable)
          .empty());

  // 2. When screenshot feature is disabled.
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(
        features::kGlicExperimentalTriggeringScreenshot);

    EXPECT_TRUE(GlicExperimentalTriggeringCoordinator::GetCapabilities(
                    syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady)
                    .empty());
    EXPECT_TRUE(
        GlicExperimentalTriggeringCoordinator::GetCapabilities(
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kNeedsOptIn)
            .empty());
  }

  // 3. When screenshot feature is enabled.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(
        features::kGlicExperimentalTriggeringScreenshot);

    EXPECT_EQ(GlicExperimentalTriggeringCoordinator::GetCapabilities(
                  syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady),
              base::flat_set<std::string>{kGlicCapabilityScreenshot});
    EXPECT_EQ(
        GlicExperimentalTriggeringCoordinator::GetCapabilities(
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kNeedsOptIn),
        base::flat_set<std::string>{kGlicCapabilityScreenshot});
  }
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
    ON_CALL(mock_tab_, GetProfile()).WillByDefault(testing::Return(profile_));
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
    // Destroy the coordinator, and with it any in-flight actor task, while the
    // tab and its WebContents are still around for the task to unwind against.
    coordinator_.reset();
    instance_helper_.reset();
    web_contents_.reset();
    GlicExperimentalTriggeringCoordinatorTest::TearDown();
  }

  void StartActuationSession(std::string_view conversation_id = "conv_123") {
    ExperimentalTriggeringRequest init_request;
    init_request.version = 1;
    init_request.context_id = kTestContextId;
    init_request.task_metadata =
        TaskMetadata{.conversation_id = std::string(conversation_id)};
    init_request.payload = TriggerActuationRequest{.initial_prompt = "hello"};
    auto response = SendRequest(init_request);
    ASSERT_TRUE(response.has_value());
    ASSERT_TRUE(response->task_update.has_value());
    EXPECT_EQ(response->task_update->state, TaskUpdate::State::kStarting);
  }

  ExperimentalTriggeringRequest CreateScreenshotRequest(
      std::vector<uint8_t> request_token = {'t', 'o', 'k', 'e', 'n'},
      std::string_view conversation_id = kTestConversationId) {
    ExperimentalTriggeringRequest screenshot_request;
    screenshot_request.version = 1;
    screenshot_request.context_id = kTestContextId;
    screenshot_request.task_metadata =
        TaskMetadata{.conversation_id = std::string(conversation_id)};
    screenshot_request.payload = GetScreenshotRequest{
        .public_key = {'k', 'e', 'y'},
        .auth_secret = {'s', 'e', 'c'},
        .request_token = std::move(request_token),
    };
    return screenshot_request;
  }

  ExperimentalTriggeringRequest CreateExecuteScriptToolRequest(
      std::optional<int32_t> tab_id = std::nullopt,
      std::optional<std::string> document_identifier = std::nullopt) {
    ExperimentalTriggeringRequest request;
    request.version = 1;
    request.context_id = kTestContextId;
    request.task_metadata =
        TaskMetadata{.conversation_id = kTestConversationId};
    ExecuteActionsRequest exec_req;
    optimization_guide::proto::Action* action = exec_req.actions.add_actions();
    optimization_guide::proto::ScriptToolAction* script_tool =
        action->mutable_script_tool();
    script_tool->set_tool_name(kTestToolName);
    if (tab_id.has_value()) {
      script_tool->set_tab_id(*tab_id);
    }
    if (document_identifier.has_value()) {
      script_tool->mutable_document_identifier()->set_serialized_token(
          *document_identifier);
    }
    request.payload = std::move(exec_req);
    return request;
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
  StartActuationSession();

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
  StartActuationSession();

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

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       GetScreenshotRequest_InvalidRequest_EmptyRequestToken) {
  // The invalid-request path is only reachable once the screenshot feature is
  // enabled; otherwise the request is rejected as disabled.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      features::kGlicExperimentalTriggeringScreenshot);

  StartActuationSession();

  EXPECT_CALL(test_triggering_manager_, CaptureAndUploadEncryptedScreenshot)
      .Times(0);

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  base::HistogramTester histogram_tester;
  auto response = coordinator_->OnRequest(
      kTestContextId, CreateScreenshotRequest(/*request_token=*/{}),
      ScopedIncomingMessageResultLogger(
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);

  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->screenshot_result.has_value());
  EXPECT_EQ(response->screenshot_result->status,
            ScreenshotResult::Status::kErrorInvalidRequest);
  EXPECT_TRUE(response->screenshot_result->file_token.empty());
  EXPECT_TRUE(response->screenshot_result->request_token.empty());
  EXPECT_FALSE(response->task_update.has_value());
  EXPECT_FALSE(update_future.IsReady());

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::
          kUnexpectedRequestPayload,
      1);
}

struct ScreenshotCaptureTestCase {
  const char* test_name;
  base::expected<std::string, ScreenshotResult::Status> manager_result;
  ScreenshotResult::Status expected_status;
  std::string expected_file_token;
};

class GlicExperimentalTriggeringCoordinatorScreenshotCaptureTest
    : public GlicExperimentalTriggeringCoordinatorWithTabTest,
      public testing::WithParamInterface<ScreenshotCaptureTestCase> {};

TEST_P(GlicExperimentalTriggeringCoordinatorScreenshotCaptureTest,
       HandlesCaptureResult) {
  // The capture path is only reachable once the screenshot feature is enabled;
  // otherwise the request is rejected up front as disabled.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      features::kGlicExperimentalTriggeringScreenshot);

  StartActuationSession();

  const auto& test_case = GetParam();
  const std::vector<uint8_t> expected_token = {'t', 'o', 'k', 'e', 'n'};
  EXPECT_CALL(test_triggering_manager_, CaptureAndUploadEncryptedScreenshot)
      .WillOnce(testing::WithArg<2>(
          [&test_case](
              base::OnceCallback<void(
                  base::expected<std::string, ScreenshotResult::Status>)>
                  callback) {
            std::move(callback).Run(test_case.manager_result);
          }));

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  auto response = coordinator_->OnRequest(
      kTestContextId, CreateScreenshotRequest(expected_token),
      ScopedIncomingMessageResultLogger(
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);

  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kStarting);

  auto async_response = update_future.Take();
  ASSERT_TRUE(async_response.screenshot_result.has_value());
  EXPECT_EQ(async_response.screenshot_result->status,
            test_case.expected_status);
  EXPECT_EQ(async_response.screenshot_result->file_token,
            test_case.expected_file_token);
  EXPECT_EQ(async_response.screenshot_result->request_token, expected_token);
}

INSTANTIATE_TEST_SUITE_P(
    GetScreenshotRequest,
    GlicExperimentalTriggeringCoordinatorScreenshotCaptureTest,
    testing::Values(
        ScreenshotCaptureTestCase{
            .test_name = "Success",
            .manager_result = base::ok("file_token_123"),
            .expected_status = ScreenshotResult::Status::kSuccess,
            .expected_file_token = "file_token_123",
        },
        ScreenshotCaptureTestCase{
            .test_name = "EmptyFileToken_ReturnsErrorServer",
            .manager_result = base::ok(""),
            .expected_status = ScreenshotResult::Status::kErrorServer,
            .expected_file_token = "",
        },
        ScreenshotCaptureTestCase{
            .test_name = "ErrorCapture",
            .manager_result =
                base::unexpected(ScreenshotResult::Status::kErrorCapture),
            .expected_status = ScreenshotResult::Status::kErrorCapture,
            .expected_file_token = "",
        },
        ScreenshotCaptureTestCase{
            .test_name = "ErrorDisabled",
            .manager_result =
                base::unexpected(ScreenshotResult::Status::kErrorDisabled),
            .expected_status = ScreenshotResult::Status::kErrorDisabled,
            .expected_file_token = "",
        },
        ScreenshotCaptureTestCase{
            .test_name = "ErrorServer",
            .manager_result =
                base::unexpected(ScreenshotResult::Status::kErrorServer),
            .expected_status = ScreenshotResult::Status::kErrorServer,
            .expected_file_token = "",
        }),
    [](const testing::TestParamInfo<ScreenshotCaptureTestCase>& info) {
      return info.param.test_name;
    });

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

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ActuationSuccess_RecordsAllMetrics) {
  base::HistogramTester histogram_tester;

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kStarting);

  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.Latency.ToSidePanelOpened", 1);
  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.Latency.ToClientConnected", 1);

  auto update = mojom::ExperimentalTriggeringUpdate::New();
  update->type = mojom::ExperimentalTriggeringUpdateType::kWorklog;
  update->data = "working";
  test_triggering_manager_.SendUpdate(
      std::move(update), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();
  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.Latency.ToFirstResponse", 1);

  auto complete_update = mojom::ExperimentalTriggeringUpdate::New();
  complete_update->type =
      mojom::ExperimentalTriggeringUpdateType::kTerminalCompletion;
  complete_update->data = "done";
  test_triggering_manager_.SendUpdate(
      std::move(complete_update), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.SidePanelOpened", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ClientConnected", true, 1);
  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.Latency.ToTerminalCompletion", 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ActuationTimeoutWaitingForClient_RecordsFalseMilestones) {
  base::HistogramTester histogram_tester;

  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([this](InvokeWithAutoSubmitPasskey passkey,
                       GlicInvokeOptions options,
                       GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        std::move(options.on_error).Run(GlicInvokeError::kTimeout);
        return mock_glic_instance_.GetWeakPtr();
      });

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kTimeoutWaitingForClient, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.SidePanelOpened", false, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ClientConnected", false, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ActuationTimeoutWaitingForActuation_RecordsOutcome) {
  base::HistogramTester histogram_tester;

  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([this](InvokeWithAutoSubmitPasskey passkey,
                       GlicInvokeOptions options,
                       GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        if (options.on_panel_opened) {
          std::move(options.on_panel_opened).Run();
        }
        if (options.on_client_connected) {
          std::move(options.on_client_connected)
              .Run(mock_glic_instance_.GetWeakPtr());
        }
        std::move(options.on_error).Run(GlicInvokeError::kTimeout);
        return mock_glic_instance_.GetWeakPtr();
      });

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  SendRequest(request);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kTimeoutWaitingForActuation,
      1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.SidePanelOpened", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ClientConnected", true, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ActuationInvokeError_RecordsSpecificOutcome) {
  base::HistogramTester histogram_tester;

  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([this](InvokeWithAutoSubmitPasskey passkey,
                       GlicInvokeOptions options,
                       GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        std::move(options.on_error).Run(GlicInvokeError::kInvokeInProgress);
        return mock_glic_instance_.GetWeakPtr();
      });

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  SendRequest(request);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kInvokeErrorInvokeInProgress,
      1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ActuationInvokeErrorTabClosed_RecordsOutcome) {
  base::HistogramTester histogram_tester;

  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([this](InvokeWithAutoSubmitPasskey passkey,
                       GlicInvokeOptions options,
                       GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        std::move(options.on_error).Run(GlicInvokeError::kTabClosed);
        return mock_glic_instance_.GetWeakPtr();
      });

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  SendRequest(request);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kInvokeErrorTabClosed, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ActuationInvokeErrorOther_RecordsOutcome) {
  base::HistogramTester histogram_tester;

  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([this](InvokeWithAutoSubmitPasskey passkey,
                       GlicInvokeOptions options,
                       GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        std::move(options.on_error).Run(GlicInvokeError::kInvalidConfiguration);
        return mock_glic_instance_.GetWeakPtr();
      });

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  SendRequest(request);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kInvokeErrorOther, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ActuationSuperseded_DoesNotRecordDestroyedOutcome) {
  base::HistogramTester histogram_tester;

  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([this](InvokeWithAutoSubmitPasskey passkey,
                       GlicInvokeOptions options,
                       GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        std::move(options.on_error).Run(GlicInvokeError::kSuperseded);
        return mock_glic_instance_.GetWeakPtr();
      });

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  SendRequest(request);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.ExecutionOutcome", 0);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ActuationUpdatesRegistrationFailed_RecordsOutcome) {
  base::HistogramTester histogram_tester;

  test_triggering_manager_.set_registration_success(false);

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  SendRequest(request);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kUpdatesRegistrationFailed,
      1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ActuationTerminalFailed_RecordsOutcome) {
  base::HistogramTester histogram_tester;

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  SendRequest(request);

  auto fail_update = mojom::ExperimentalTriggeringUpdate::New();
  fail_update->type = mojom::ExperimentalTriggeringUpdateType::kTerminalFailed;
  fail_update->data = "error";
  test_triggering_manager_.SendUpdate(
      std::move(fail_update), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kTerminalFailed, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ActuationTerminalStopped_RecordsOutcome) {
  base::HistogramTester histogram_tester;

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  SendRequest(request);

  auto stop_update = mojom::ExperimentalTriggeringUpdate::New();
  stop_update->type = mojom::ExperimentalTriggeringUpdateType::kTerminalStopped;
  stop_update->data = "stopped";
  test_triggering_manager_.SendUpdate(
      std::move(stop_update), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kTerminalStopped, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ActuationMojoDisconnect_RecordsOutcome) {
  base::HistogramTester histogram_tester;

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  SendRequest(request);

  // Simulate web client Mojo disconnection before terminal completion.
  test_triggering_manager_.ResetHandler();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::
          kClientDisconnectedBeforeResponse,
      1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ActuationTeardownBeforeCompletion_RecordsOutcome) {
  base::HistogramTester histogram_tester;

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  SendRequest(request);

  // Destroying coordinator triggers handler destructor.
  coordinator_.reset();

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kDestroyedBeforeCompletion,
      1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ExecuteActions_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndDisableFeature(
      features::kGlicExperimentalTriggeringScriptTools);

  auto response = SendRequest(CreateExecuteScriptToolRequest());
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data,
            "Script tool execution is not enabled.");
  histogram_tester_.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kScriptToolsDisabled, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ExecuteActions_NonScriptToolAction) {
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = kTestConversationId};
  ExecuteActionsRequest exec_req;
  optimization_guide::proto::Action* action = exec_req.actions.add_actions();
  action->mutable_click();
  request.payload = std::move(exec_req);

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data,
            "ExecuteActions contained non-ScriptTool actions.");
  histogram_tester_.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kNonScriptToolAction, 1);
}

// Fixture without the actor policy control exemption, so that the profile is
// not allowed to act on the web.
class GlicExperimentalTriggeringCoordinatorCannotActOnWebTest
    : public GlicExperimentalTriggeringCoordinatorWithTabTest {
 public:
  bool ActorPolicyControlExemption() const override { return false; }
};

TEST_F(GlicExperimentalTriggeringCoordinatorCannotActOnWebTest,
       ExecuteActions_CannotActOnWeb) {
  auto response = SendRequest(CreateExecuteScriptToolRequest());
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data,
            "Acting on the web is not allowed for this profile.");
  histogram_tester_.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kCannotActOnWeb, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ExecuteActions_SuccessAndStop) {
  auto* user_data = optimization_guide::DocumentIdentifierUserData::
      GetOrCreateForCurrentDocument(web_contents_->GetPrimaryMainFrame());

  ExperimentalTriggeringRequest request = CreateExecuteScriptToolRequest(
      /*tab_id=*/std::nullopt, user_data->serialized_token());

  auto response = SendRequest(request);
  EXPECT_FALSE(response.has_value());
  histogram_tester_.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kSuccess, 1);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  ExperimentalTriggeringRequest stop_request;
  stop_request.version = 1;
  stop_request.context_id = kTestContextId;
  stop_request.task_metadata =
      TaskMetadata{.conversation_id = kTestConversationId};
  stop_request.payload = StopActuationRequest{.stop_reason = "STOPPED_BY_USER"};

  auto stop_response = SendRequest(stop_request);
  ASSERT_TRUE(stop_response.has_value());
  ASSERT_TRUE(stop_response->task_update.has_value());
  EXPECT_EQ(stop_response->task_update->state, TaskUpdate::State::kStopped);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ExecuteActions_AlreadyExecuting) {
  auto* user_data = optimization_guide::DocumentIdentifierUserData::
      GetOrCreateForCurrentDocument(web_contents_->GetPrimaryMainFrame());

  ExperimentalTriggeringRequest request = CreateExecuteScriptToolRequest(
      /*tab_id=*/std::nullopt, user_data->serialized_token());

  auto response1 = SendRequest(request);
  EXPECT_FALSE(response1.has_value());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  auto response2 = SendRequest(request);
  ASSERT_TRUE(response2.has_value());
  ASSERT_TRUE(response2->task_update.has_value());
  EXPECT_EQ(response2->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response2->task_update->data,
            "Another task is already executing for this context.");
  histogram_tester_.ExpectBucketCount(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kTaskAlreadyRunning, 1);
  // Rejecting the second request must not tear down the first execution.
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       TriggerActuationWhileExecutingActions_Rejected) {
  auto* user_data = optimization_guide::DocumentIdentifierUserData::
      GetOrCreateForCurrentDocument(web_contents_->GetPrimaryMainFrame());

  ExperimentalTriggeringRequest request = CreateExecuteScriptToolRequest(
      /*tab_id=*/std::nullopt, user_data->serialized_token());

  EXPECT_FALSE(SendRequest(request).has_value());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .Times(0);

  ExperimentalTriggeringRequest actuation_request;
  actuation_request.version = 1;
  actuation_request.context_id = kTestContextId;
  actuation_request.task_metadata =
      TaskMetadata{.conversation_id = kTestConversationId};
  actuation_request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  auto response = SendRequest(actuation_request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data,
            "Direct action execution is already in progress for this "
            "context.");
  histogram_tester_.ExpectBucketCount(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kTaskAlreadyRunning, 1);
  // The in-flight execution and its handler must survive the rejection.
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ExecuteActions_Complete) {
  ExperimentalTriggeringRequest request =
      CreateExecuteScriptToolRequest(mock_tab_.GetHandle().raw_value());

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  coordinator_->OnRequest(
      kTestContextId, request,
      ScopedIncomingMessageResultLogger(
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), &mock_tab_);

  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  auto response = update_future.Take();
  EXPECT_EQ(response.context_id, kTestContextId);
  EXPECT_TRUE(response.task_metadata.has_value());
  EXPECT_EQ(response.task_metadata->conversation_id, kTestConversationId);
  ASSERT_TRUE(response.execute_actions_response.has_value());
  // In unit tests, the mock WebContents has no active RenderFrameHost, so tool
  // time-of-use validation returns kTabWentAway.
  EXPECT_EQ(response.execute_actions_response->actions_result.action_result(),
            static_cast<int32_t>(actor::mojom::ActionResultCode::kTabWentAway));
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ExecuteActions_WithDocumentIdentifier_Success) {
  auto* user_data = optimization_guide::DocumentIdentifierUserData::
      GetOrCreateForCurrentDocument(web_contents_->GetPrimaryMainFrame());

  ExperimentalTriggeringRequest request = CreateExecuteScriptToolRequest(
      /*tab_id=*/std::nullopt, user_data->serialized_token());

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  coordinator_->OnRequest(
      kTestContextId, request,
      ScopedIncomingMessageResultLogger(
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), &mock_tab_);

  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  auto response = update_future.Take();
  EXPECT_EQ(response.context_id, kTestContextId);
  EXPECT_TRUE(response.task_metadata.has_value());
  EXPECT_EQ(response.task_metadata->conversation_id, kTestConversationId);
  ASSERT_TRUE(response.execute_actions_response.has_value());
  // In unit tests, the mock WebContents has no active RenderFrameHost, so tool
  // time-of-use validation returns kTabWentAway.
  EXPECT_EQ(response.execute_actions_response->actions_result.action_result(),
            static_cast<int32_t>(actor::mojom::ActionResultCode::kTabWentAway));
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
  histogram_tester_.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kSuccess, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ExecuteActions_MissingTabIdAndDocumentIdentifier_Fails) {
  auto response = SendRequest(CreateExecuteScriptToolRequest());
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data,
            "Target tab for document identifier could not be found.");
  histogram_tester_.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kTabNotFound, 1);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ExecuteActions_WithUnknownDocumentIdentifier_Fails) {
  ExperimentalTriggeringRequest request = CreateExecuteScriptToolRequest(
      /*tab_id=*/std::nullopt, base::UnguessableToken::Create().ToString());

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data,
            "Target tab for document identifier could not be found.");
  histogram_tester_.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kTabNotFound, 1);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ExecuteActions_Timeout) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      features::kGlicExperimentalTriggeringScriptTools, {{"timeout", "5s"}});

  // NiceMock suppresses warnings for ActorUiStateManager calling GetWeakPtr()
  // on the controller during tab registration (configured via ON_CALL in
  // MockActorUiTabController's constructor).
  testing::NiceMock<actor::ui::MockActorUiTabController> mock_tab_controller(
      mock_tab_);
  // Never complete the async UI event so that the actions hang and trigger
  // the coordinator timeout.
  EXPECT_CALL(mock_tab_controller, OnUiTabStateChange)
      .WillRepeatedly(
          [](const actor::ui::UiTabState&, actor::ui::UiResultCallback) {});

  ExperimentalTriggeringRequest request = CreateExecuteScriptToolRequest(
      mock_tab_.GetHandle().raw_value(),
      base::UnguessableToken::Create().ToString());

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  coordinator_->OnRequest(
      kTestContextId, request,
      ScopedIncomingMessageResultLogger(
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), &mock_tab_);

  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);
  EXPECT_FALSE(update_future.IsReady());

  task_environment_.FastForwardBy(base::Seconds(6));

  auto response = update_future.Take();
  EXPECT_EQ(response.context_id, kTestContextId);
  EXPECT_TRUE(response.task_metadata.has_value());
  EXPECT_EQ(response.task_metadata->conversation_id, kTestConversationId);
  ASSERT_TRUE(response.execute_actions_response.has_value());
  EXPECT_EQ(response.execute_actions_response->actions_result.action_result(),
            static_cast<int32_t>(actor::mojom::ActionResultCode::kToolTimeout));
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

}  // namespace
}  // namespace glic
