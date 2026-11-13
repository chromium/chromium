// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_coordinator.h"

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/supports_user_data.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_surface_registry.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/ui/test_support/mock_actor_ui_tab_controller.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/actor/glic_actor_task_manager.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#include "chrome/browser/glic/experimental_triggering/actor_log.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_manager.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_metrics.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_types.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/provider_features.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/test_support/glic_api_test_passkeys.h"
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
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/journal_details_builder.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "chrome/browser/glic/experimental_triggering/glic_background_opt_in_notifier_android.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#endif

namespace glic {

namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsSupersetOf;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

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
  bool IsClientBound() const override { return is_client_bound_; }
  void set_is_client_bound(bool bound) { is_client_bound_ = bound; }
  void SimulateClientRebound() {
    is_client_bound_ = true;
    NotifyClientBound();
  }
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

  MOCK_METHOD(void,
              SubmitConfirmationResponse,
              (const std::vector<uint8_t>&, base::OnceCallback<void(bool)>),
              (override));

 private:
  mojo::Remote<mojom::ExperimentalTriggeringUpdatesHandler> handler_;
  bool registration_success_ = true;
  bool is_client_bound_ = true;
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
  bool IsChromeInBackground() const override {
    return is_chrome_in_background_;
  }
  void set_is_chrome_in_background(bool in_background) {
    is_chrome_in_background_ = in_background;
  }

 private:
  raw_ptr<BrowserWindowInterface> browser_window_ = nullptr;
  raw_ptr<tabs::TabInterface> active_tab_ = nullptr;
  bool is_chrome_in_background_ = false;
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

    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlicActor, {}},
        {features::kGlicExperimentalTriggering, {}},
        {features::kGlicExperimentalTriggeringScriptTools, {}}};
    std::vector<base::test::FeatureRef> disabled_features;
    if (ActorPolicyControlExemption()) {
      enabled_features.push_back(
          {features::kGlicActorPolicyControlExemption, {}});
    } else {
      disabled_features.push_back(features::kGlicActorPolicyControlExemption);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
    ASSERT_TRUE(profile_manager_.SetUp());

    TestingProfile::TestingFactories testing_factories;
    testing_factories.emplace_back(
        GlicKeyedServiceFactory::GetInstance(),
        base::BindRepeating(
            &GlicExperimentalTriggeringCoordinatorTest::CreateGlicKeyedService,
            base::Unretained(this)));

    profile_ = profile_manager_.CreateTestingProfile(
        "test_profile", std::move(testing_factories));

    auto* actor_service =
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_);
    ASSERT_TRUE(actor_service);
    journal_recorder_ = std::make_unique<actor::ActorJournalRecorder>(
        actor_service->GetJournal());

    coordinator_ =
        std::make_unique<TestGlicExperimentalTriggeringCoordinator>(profile_);
    OptIn();

    actor_policy_checker_ = std::make_unique<GlicActorPolicyChecker>(*profile_);
    actor_task_manager_ = std::make_unique<GlicActorTaskManager>(
        profile_, actor_service, *actor_policy_checker_,
        /*instance_metrics=*/nullptr,
        /*sharing_manager=*/nullptr,
        /*delegate=*/nullptr);

    ON_CALL(mock_glic_instance_, GetActorTaskManager())
        .WillByDefault(testing::Return(actor_task_manager_.get()));
    ON_CALL(mock_glic_instance_, GetExperimentalTriggeringManager())
        .WillByDefault(testing::Return(&test_triggering_manager_));
  }

  void TearDown() override {
    coordinator_.reset();
    actor_task_manager_.reset();
    actor_policy_checker_.reset();
    journal_recorder_.reset();
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
              if (auto_submit_options.show_panel && options.on_panel_opened) {
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
    auto* glic_service =
        GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/true);
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

  void OptOut() {
    auto* glic_service =
        GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/true);
    ASSERT_TRUE(glic_service);
    glic_service->enabling().SetCompletedFre(
        glic::prefs::FreStatus::kNotStarted);
    glic_service->enabling().SetUserEnabledActuationOnWeb(false);
    glic_service->enabling().SetExperimentalTriggeringEnabled(false);
  }

  std::optional<ExperimentalTriggeringResponse> SendRequest(
      const ExperimentalTriggeringRequest& request,
      tabs::TabInterface* prepared_tab = nullptr,
      const std::string& context_id = kTestContextId) {
    if (prepared_tab) {
      coordinator_->set_active_tab(prepared_tab);
    }
    return coordinator_->OnRequest(
        context_id, request,
        ScopedIncomingMessageResultLogger(
            GlicMetricReporter(profile_),
            ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
        base::DoNothing(), nullptr);
  }

  void ExpectHandlerDestroyed(
      const base::HistogramTester& tester,
      GlicExperimentalTriggeringHandlerDestroyReason reason,
      base::HistogramBase::Count32 count = 1,
      std::string_view context_id = kTestContextId) {
    for (const char* name :
         {"Glic.ExperimentalTriggering.UpdatesHandlerDestroyed",
          "Glic.Consumer.ExperimentalTriggering.UpdatesHandlerDestroyed"}) {
      tester.ExpectUniqueSample(name, reason, count);
    }
    if (!journal_recorder_) {
      return;
    }
    auto entries = journal_recorder_->GetEntries("UpdatesHandlerDestroyed");
    ASSERT_EQ(entries.size(), static_cast<size_t>(count));
    for (const auto& entry : entries) {
      EXPECT_EQ(entry.track_uuid,
                actor::MakeGlicExperimentalTriggeringTrackUUID(context_id));
      EXPECT_THAT(
          entry.details,
          testing::IsSupersetOf(
              {testing::Pair("context_id", std::string(context_id)),
               testing::Pair(
                   "reason",
                   std::string(
                       ExperimentalTriggeringHandlerDestroyReasonToString(
                           reason)))}));
    }
  }

  void ExpectSequenceAnomalyCount(
      const base::HistogramTester& tester,
      GlicExperimentalTriggeringSequenceAnomaly anomaly,
      base::HistogramBase::Count32 count) {
    for (const char* name :
         {"Glic.ExperimentalTriggering.SequenceAnomaly",
          "Glic.Consumer.ExperimentalTriggering.SequenceAnomaly"}) {
      tester.ExpectBucketCount(name, anomaly, count);
    }
  }

  void ExpectSequenceAnomalyTotalCount(const base::HistogramTester& tester,
                                       base::HistogramBase::Count32 count) {
    for (const char* name :
         {"Glic.ExperimentalTriggering.SequenceAnomaly",
          "Glic.Consumer.ExperimentalTriggering.SequenceAnomaly"}) {
      tester.ExpectTotalCount(name, count);
    }
    if (journal_recorder_) {
      EXPECT_EQ(journal_recorder_->GetEntries("SequenceAnomaly").size(),
                static_cast<size_t>(count));
    }
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
  std::unique_ptr<actor::ActorJournalRecorder> journal_recorder_;
  std::unique_ptr<TestGlicExperimentalTriggeringCoordinator> coordinator_;
  testing::NiceMock<TestExperimentalTriggeringManager> test_triggering_manager_;
  testing::NiceMock<MockGlicInstance> mock_glic_instance_;
  std::unique_ptr<GlicActorPolicyChecker> actor_policy_checker_;
  std::unique_ptr<GlicActorTaskManager> actor_task_manager_;
};

TEST_F(GlicExperimentalTriggeringCoordinatorTest, NoTaskMetadata) {
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_);
  ASSERT_TRUE(actor_service);
  actor_service->AddPendingTask(kTestContextId);
  EXPECT_TRUE(actor_service->HasPendingTask(kTestContextId));

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

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  EXPECT_FALSE(actor_service->HasPendingTask(kTestContextId));
  EXPECT_EQ(actor_service->GetPendingTasksCount(), 0u);
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
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);

  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->screenshot_result.has_value());
  EXPECT_EQ(response->screenshot_result->status,
            ScreenshotResult::Status::kErrorDisabled);
  EXPECT_EQ(response->screenshot_result->request_token,
            std::vector<uint8_t>({'t', 'o', 'k', 'e', 'n'}));
  EXPECT_FALSE(response->task_update.has_value());
  EXPECT_FALSE(update_future.IsReady());
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       GetScreenshotRequest_FeatureEnabled_NoInstance) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
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
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->screenshot_result.has_value());
  EXPECT_EQ(response->screenshot_result->status,
            ScreenshotResult::Status::kErrorCapture);
  EXPECT_EQ(response->screenshot_result->request_token,
            std::vector<uint8_t>({'t', 'o', 'k', 'e', 'n'}));
  EXPECT_FALSE(response->task_update.has_value());
  EXPECT_FALSE(update_future.IsReady());
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       GetScreenshotRequest_NotOptedIn) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      features::kGlicExperimentalTriggeringScreenshot);
  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/true);
  ASSERT_TRUE(glic_service);
  glic_service->enabling().SetCompletedFre(glic::prefs::FreStatus::kNotStarted);
  glic_service->enabling().SetUserEnabledActuationOnWeb(false);
  glic_service->enabling().SetExperimentalTriggeringEnabled(false);

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
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->screenshot_result.has_value());
  EXPECT_EQ(response->screenshot_result->status,
            ScreenshotResult::Status::kErrorDisabled);
  EXPECT_EQ(response->screenshot_result->request_token,
            std::vector<uint8_t>({'t', 'o', 'k', 'e', 'n'}));
  EXPECT_FALSE(response->task_update.has_value());
  EXPECT_FALSE(update_future.IsReady());
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
  EXPECT_THAT(response->task_update->metadata,
              testing::ElementsAre(
                  testing::Pair("chrome_error_code", "ET_NO_BROWSER_WINDOW")));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       ContinueActuationWithoutBrowserWindow_LiveSurface) {
  auto* glic_service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/false));
  ASSERT_TRUE(glic_service);
  EXPECT_CALL(glic_service->mock_coordinator(),
              GetInstanceForConversationId("conv_123"))
      .WillOnce(testing::Return(&mock_glic_instance_));
  EXPECT_CALL(mock_glic_instance_, HasLiveSurface())
      .WillOnce(testing::Return(true));

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = ContinueActuationRequest{.continuation_prompt = "continue"};

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kStarting);
}

// The conversation is still loaded but its last surface is gone (e.g. its
// tab was destroyed while the process stayed alive). The request must fail
// synchronously instead of returning kStarting and failing later with
// kTabClosed.
TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       ContinueActuationWithoutBrowserWindow_NoLiveSurface) {
  auto* glic_service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/false));
  ASSERT_TRUE(glic_service);
  EXPECT_CALL(glic_service->mock_coordinator(),
              GetInstanceForConversationId("conv_123"))
      .WillOnce(testing::Return(&mock_glic_instance_));
  EXPECT_CALL(mock_glic_instance_, HasLiveSurface())
      .WillOnce(testing::Return(false));

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = ContinueActuationRequest{.continuation_prompt = "continue"};

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data_type,
            TaskUpdate::DataType::kErrorMessage);
  EXPECT_EQ(response->task_update->data,
            "No browser window found for current profile.");
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       ContinueActuationWithoutBrowserWindow_ConversationNotPresent) {
  auto* glic_service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/false));
  ASSERT_TRUE(glic_service);
  EXPECT_CALL(glic_service->mock_coordinator(),
              GetInstanceForConversationId("conv_123"))
      .WillOnce(testing::Return(nullptr));

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = ContinueActuationRequest{.continuation_prompt = "continue"};

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data_type,
            TaskUpdate::DataType::kErrorMessage);
  EXPECT_EQ(response->task_update->data,
            "No browser window found for current profile.");
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       ContinueActuationWithoutBrowserWindow_EmptyConversationId) {
  auto* glic_service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/false));
  ASSERT_TRUE(glic_service);
  EXPECT_CALL(glic_service->mock_coordinator(),
              GetInstanceForConversationId(testing::_))
      .Times(0);

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = ""};
  request.payload = ContinueActuationRequest{.continuation_prompt = "continue"};

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data_type,
            TaskUpdate::DataType::kErrorMessage);
  EXPECT_EQ(response->task_update->data,
            "No browser window found for current profile.");
}
#else
TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       ContinueActuationWithoutBrowserWindow_Rejected) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      features::kGlicExperimentalTriggeringOpenWindowIfNone);
  auto* glic_service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/false));
  ASSERT_TRUE(glic_service);
  // Desktop doesn't check for an open conversation.
  EXPECT_CALL(glic_service->mock_coordinator(),
              GetInstanceForConversationId(testing::_))
      .Times(0);

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = ContinueActuationRequest{.continuation_prompt = "continue"};

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data_type,
            TaskUpdate::DataType::kErrorMessage);
  EXPECT_EQ(response->task_update->data,
            "No browser window found for current profile.");
}
#endif

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
  EXPECT_THAT(response->task_update->metadata,
              testing::ElementsAre(
                  testing::Pair("chrome_error_code", "ET_NO_BROWSER_WINDOW")));
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
  EXPECT_FALSE(response->task_update->metadata.contains("chrome_error_code"));
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
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      future1.GetRepeatingCallback(), nullptr);
  EXPECT_FALSE(response1.has_value());

  auto response2 = coordinator_->OnRequest(
      "context_2", request2,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
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
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      future1.GetRepeatingCallback(), nullptr);
  EXPECT_FALSE(response1.has_value());

  auto response2 = coordinator_->OnRequest(
      "context_2", request2,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
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
  base::test::TestFuture<std::optional<ExperimentalTriggeringResponse>>
      response_future;
  coordinator_->OnProtoMessage(
      kTestContextId, proto,
      ScopedIncomingMessageResultLogger(GlicMetricReporter(profile_),
                                        ScopedIncomingMessageResultLogger::
                                            Channel::kBrowserActuatorTransport),
      base::DoNothing(), response_future.GetCallback());
  std::optional<ExperimentalTriggeringResponse> response =
      response_future.Take();

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
  base::test::TestFuture<std::optional<ExperimentalTriggeringResponse>>
      response_future;
  coordinator_->OnProtoMessage(
      kTestContextId, proto,
      ScopedIncomingMessageResultLogger(GlicMetricReporter(profile_),
                                        ScopedIncomingMessageResultLogger::
                                            Channel::kBrowserActuatorTransport),
      base::DoNothing(), response_future.GetCallback());
  std::optional<ExperimentalTriggeringResponse> response =
      response_future.Take();

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
  base::test::TestFuture<std::optional<ExperimentalTriggeringResponse>>
      response_future;
  coordinator_->OnProtoMessage(
      kTestContextId, proto,
      ScopedIncomingMessageResultLogger(GlicMetricReporter(profile_),
                                        ScopedIncomingMessageResultLogger::
                                            Channel::kBrowserActuatorTransport),
      base::DoNothing(), response_future.GetCallback());
  std::optional<ExperimentalTriggeringResponse> response =
      response_future.Take();

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
    content::WebContents::CreateParams params(profile_);
    // Disable wake locks to avoid initializing device PowerSaveBlocker and
    // D-Bus thread runners during unit tests.
    params.enable_wake_locks = false;
    web_contents_ = content::WebContents::Create(params);
    ON_CALL(mock_tab_, GetProfile()).WillByDefault(testing::Return(profile_));
    ON_CALL(mock_tab_, GetContents())
        .WillByDefault(testing::Return(web_contents_.get()));
    ON_CALL(mock_tab_, GetWeakPtr())
        .WillByDefault(testing::Return(tab_weak_factory_.GetWeakPtr()));
    ON_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));
    instance_helper_ = std::make_unique<GlicInstanceHelper>(&mock_tab_);
    actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_)
        ->GetSurfaceRegistry()
        .OnTabCreated(mock_tab_);
    coordinator_->set_active_tab(&mock_tab_);
  }

  // Sends `request` with replies going to `update_callback`. Every request on
  // a context replaces the handler's update callback, so tests that wait for
  // a reply must pass the same callback to each request.
  std::optional<ExperimentalTriggeringResponse> SendRequestWithCallback(
      const ExperimentalTriggeringRequest& request,
      GlicExperimentalTriggeringUpdateCallback update_callback,
      tabs::TabInterface* prepared_tab = nullptr,
      const std::string& context_id = kTestContextId) {
    return coordinator_->OnRequest(
        context_id, request,
        ScopedIncomingMessageResultLogger(
            GlicMetricReporter(profile_),
            ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
        std::move(update_callback), prepared_tab);
  }

  MockGlicKeyedService* mock_glic_service() {
    return static_cast<MockGlicKeyedService*>(
        GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
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

  // Sends a GetScreenshotRequest on the active session and expects a
  // synchronous ScreenshotResult error that keeps the session's handler alive.
  void SendScreenshotRequestAndExpectSyncError(
      const std::vector<uint8_t>& request_token,
      ScreenshotResult::Status expected_status,
      GlicExperimentalTriggeringIncomingMessageResult expected_metric) {
    base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
    base::HistogramTester histogram_tester;
    auto response = coordinator_->OnRequest(
        kTestContextId, CreateScreenshotRequest(request_token),
        ScopedIncomingMessageResultLogger(
            GlicMetricReporter(profile_),
            ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
        update_future.GetRepeatingCallback(), nullptr);

    ASSERT_TRUE(response.has_value());
    ASSERT_TRUE(response->screenshot_result.has_value());
    EXPECT_EQ(response->screenshot_result->status, expected_status);
    EXPECT_TRUE(response->screenshot_result->file_token.empty());
    EXPECT_EQ(response->screenshot_result->request_token, request_token);
    EXPECT_FALSE(response->task_update.has_value());
    EXPECT_FALSE(update_future.IsReady());
    EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);
    histogram_tester.ExpectUniqueSample(
        "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
        expected_metric, 1);
    histogram_tester.ExpectUniqueSample(
        "Glic.Consumer.ExperimentalTriggering.IncomingMessageResult."
        "SharingMessage",
        expected_metric, 1);
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

  ExperimentalTriggeringRequest CreateConfirmationRequest(
      std::vector<uint8_t> confirmation_response = {'r', 'e', 's', 'p'},
      std::string_view conversation_id = "conv_123") {
    ExperimentalTriggeringRequest confirmation_request;
    confirmation_request.version = 1;
    confirmation_request.context_id = kTestContextId;
    confirmation_request.task_metadata =
        TaskMetadata{.conversation_id = std::string(conversation_id)};
    confirmation_request.payload = SubmitConfirmation{
        .confirmation_response = std::move(confirmation_response),
    };
    return confirmation_request;
  }

  // Builds a request for kTestContextId with the given payload.
  ExperimentalTriggeringRequest CreateRequest(
      ExperimentalTriggeringRequest::Payload payload) {
    ExperimentalTriggeringRequest request;
    request.version = 1;
    request.context_id = kTestContextId;
    request.task_metadata =
        TaskMetadata{.conversation_id = kTestConversationId};
    request.payload = std::move(payload);
    return request;
  }

  // Starts a direct action execution on kTestContextId that stays in flight
  // until the test stops it.
  void StartActionExecution() {
    auto* user_data = optimization_guide::DocumentIdentifierUserData::
        GetOrCreateForCurrentDocument(web_contents_->GetPrimaryMainFrame());
    EXPECT_FALSE(
        SendRequest(CreateExecuteScriptToolRequest(
                        /*tab_id=*/std::nullopt, user_data->serialized_token()))
            .has_value());
    ASSERT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);
  }

  // Sends `request` to kTestContextId, which must already own a live session,
  // and expects that the request does not tear down the session's handler.
  std::optional<ExperimentalTriggeringResponse> SendRequestToLiveSession(
      const ExperimentalTriggeringRequest& request) {
    EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);
    auto response = SendRequest(request);
    EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);
    return response;
  }

  // Expects `response` to be a synchronous failed task update.
  void ExpectFailedTaskUpdate(
      const std::optional<ExperimentalTriggeringResponse>& response,
      std::string_view expected_error) {
    ASSERT_TRUE(response.has_value());
    ASSERT_TRUE(response->task_update.has_value());
    EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
    EXPECT_EQ(response->task_update->data, expected_error);
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
  histogram_tester.ExpectUniqueSample(
      "Glic.Consumer.ExperimentalTriggering.IncomingMessageResult."
      "SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "Glic.Enterprise.ExperimentalTriggering.IncomingMessageResult."
      "SharingMessage",
      0);
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

  SendScreenshotRequestAndExpectSyncError(
      /*request_token=*/{}, ScreenshotResult::Status::kErrorInvalidRequest,
      GlicExperimentalTriggeringIncomingMessageResult::
          kUnexpectedRequestPayload);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       GetScreenshotRequest_FeatureDisabled_PreservesActiveSession) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      features::kGlicExperimentalTriggeringScreenshot);

  StartActuationSession();

  EXPECT_CALL(test_triggering_manager_, CaptureAndUploadEncryptedScreenshot)
      .Times(0);

  SendScreenshotRequestAndExpectSyncError(
      {'t', 'o', 'k', 'e', 'n'}, ScreenshotResult::Status::kErrorDisabled,
      GlicExperimentalTriggeringIncomingMessageResult::
          kUnexpectedRequestPayload);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       GetScreenshotRequest_NullTriggeringManager_PreservesActiveSession) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      features::kGlicExperimentalTriggeringScreenshot);

  StartActuationSession();

  ON_CALL(mock_glic_instance_, GetExperimentalTriggeringManager())
      .WillByDefault(testing::Return(nullptr));

  SendScreenshotRequestAndExpectSyncError(
      {'t', 'o', 'k', 'e', 'n'}, ScreenshotResult::Status::kErrorCapture,
      GlicExperimentalTriggeringIncomingMessageResult::
          kTriggeringManagerUnavailable);
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
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);

  // The asynchronous ScreenshotResult is the only reply; emitting a synchronous
  // task update would imply a state transition that didn't happen.
  EXPECT_FALSE(response.has_value());

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

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       SubmitConfirmation_InvalidRequest_EmptyPayload) {
  StartActuationSession();

  EXPECT_CALL(test_triggering_manager_, SubmitConfirmationResponse).Times(0);

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  base::HistogramTester histogram_tester;
  auto response = coordinator_->OnRequest(
      kTestContextId, CreateConfirmationRequest(/*confirmation_response=*/{}),
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);

  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->confirmation_result.has_value());
  EXPECT_EQ(response->confirmation_result->status,
            ConfirmationResult::Status::kErrorInvalidRequest);
  // Delivery failures must not masquerade as actuation task failures or tear
  // down the active updates handler.
  EXPECT_FALSE(response->task_update.has_value());
  EXPECT_FALSE(update_future.IsReady());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::
          kUnexpectedRequestPayload,
      1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Consumer.ExperimentalTriggering.IncomingMessageResult."
      "SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::
          kUnexpectedRequestPayload,
      1);
}

struct ConfirmationTestCase {
  const char* test_name;
  bool manager_accepted;
  ConfirmationResult::Status expected_status;
};

class GlicExperimentalTriggeringCoordinatorConfirmationTest
    : public GlicExperimentalTriggeringCoordinatorWithTabTest,
      public testing::WithParamInterface<ConfirmationTestCase> {};

TEST_P(GlicExperimentalTriggeringCoordinatorConfirmationTest,
       HandlesConfirmationResult) {
  StartActuationSession();

  const auto& test_case = GetParam();
  const std::vector<uint8_t> expected_payload = {'r', 'e', 's', 'p'};
  std::vector<uint8_t> received_payload;
  EXPECT_CALL(test_triggering_manager_, SubmitConfirmationResponse)
      .WillOnce([&](const std::vector<uint8_t>& response,
                    base::OnceCallback<void(bool)> callback) {
        received_payload = response;
        std::move(callback).Run(test_case.manager_accepted);
      });

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  auto response = coordinator_->OnRequest(
      kTestContextId, CreateConfirmationRequest(expected_payload),
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);

  // The asynchronous ConfirmationResult is the only reply; emitting a
  // synchronous task update would imply a state transition that didn't happen.
  EXPECT_FALSE(response.has_value());
  // The opaque payload must reach the manager byte-for-byte.
  EXPECT_EQ(received_payload, expected_payload);

  auto async_response = update_future.Take();
  ASSERT_TRUE(async_response.confirmation_result.has_value());
  EXPECT_EQ(async_response.confirmation_result->status,
            test_case.expected_status);
  EXPECT_FALSE(async_response.task_update.has_value());
}

INSTANTIATE_TEST_SUITE_P(
    SubmitConfirmation,
    GlicExperimentalTriggeringCoordinatorConfirmationTest,
    testing::Values(
        ConfirmationTestCase{
            .test_name = "Applied",
            .manager_accepted = true,
            .expected_status = ConfirmationResult::Status::kApplied,
        },
        ConfirmationTestCase{
            .test_name = "NotApplied",
            .manager_accepted = false,
            .expected_status = ConfirmationResult::Status::kNotApplied,
        }),
    [](const testing::TestParamInfo<ConfirmationTestCase>& info) {
      return info.param.test_name;
    });

// Without a live session, a request that fails early still cleans up the
// handler that it created.
TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       TriggerActuationNotOptedInWithoutSession_CleansUpHandler) {
  OptOut();

  ExpectFailedTaskUpdate(SendRequest(CreateRequest(
                             TriggerActuationRequest{.initial_prompt = "hi"})),
                         "User is not opted in to experimental triggering.");
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

// Handlers are shared per context id, so a request that fails early with a
// non-terminal reply must not tear down a live session on the same context
// (b/574916669). The tests below cover one such failure per request type, first
// on an actuation session and then on a direct action execution. A request
// that fails with a terminal reply ends the session; see
// GlicExperimentalTriggeringCoordinatorSyncTerminalTest.

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       DeviceOptInFailureDuringActuation_KeepsHandler) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      features::kGlicExperimentalTriggeringOptInTabFocus);
  StartActuationSession();
  base::HistogramTester histogram_tester;
  // Without a tab there is no WebContents to show the opt-in dialog in.
  coordinator_->set_active_tab(nullptr);

  auto response = SendRequestToLiveSession(CreateRequest(DeviceOptInRequest{}));
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->device_opt_in_result.has_value());
  EXPECT_EQ(*response->device_opt_in_result, DeviceOptInResult::kFailed);
  // The live session keeps its handler, so no outcome is recorded for it.
  histogram_tester.ExpectBucketCount(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kDestroyedBeforeCompletion,
      0);
  histogram_tester.ExpectBucketCount(
      "Glic.Consumer.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kDestroyedBeforeCompletion,
      0);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       SubmitConfirmationNotOptedInDuringActuation_KeepsHandler) {
  StartActuationSession();
  OptOut();

  auto response = SendRequestToLiveSession(CreateConfirmationRequest());
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->confirmation_result.has_value());
  EXPECT_EQ(response->confirmation_result->status,
            ConfirmationResult::Status::kErrorUnavailable);
}

// Once actuation starts, the invocation succeeds and is no longer pending, but
// the task keeps running. A request during the task must not tear down its
// handler: only the Glic instance shows that the task still runs.
TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       RequestAfterInvocationSucceeded_KeepsHandler) {
  base::OnceClosure on_success;
  EXPECT_CALL(*mock_glic_service(),
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        on_success = std::move(options.on_success);
        return mock_glic_instance_.GetWeakPtr();
      });
  StartActuationSession();
  // GlicInvokeHandler posts `on_success` when actuation starts.
  ASSERT_TRUE(on_success);
  std::move(on_success).Run();

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(SendRequestToLiveSession(CreateRequest(TaskMetadataUpdated{}))
                   .has_value());
  histogram_tester.ExpectBucketCount(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kDestroyedBeforeCompletion,
      0);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       StopActuationDuringActuation_TearsDownHandler) {
  StartActuationSession();

  auto response = SendRequest(
      CreateRequest(StopActuationRequest{.stop_reason = "STOPPED_BY_USER"}));
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kStopped);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       TaskMetadataUpdatedDuringActionExecution_KeepsHandler) {
  StartActionExecution();

  // There is no Glic instance to relay the metadata to, but the in-flight
  // execution still needs the handler to report its result.
  EXPECT_FALSE(SendRequestToLiveSession(CreateRequest(TaskMetadataUpdated{}))
                   .has_value());

  EXPECT_TRUE(SendRequest(CreateRequest(StopActuationRequest{
                              .stop_reason = "STOPPED_BY_USER"}))
                  .has_value());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       GetScreenshotFailureDuringActionExecution_KeepsHandler) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      features::kGlicExperimentalTriggeringScreenshot);
  StartActionExecution();

  auto response = SendRequestToLiveSession(CreateScreenshotRequest());
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->screenshot_result.has_value());
  EXPECT_EQ(response->screenshot_result->status,
            ScreenshotResult::Status::kErrorDisabled);

  EXPECT_TRUE(SendRequest(CreateRequest(StopActuationRequest{
                              .stop_reason = "STOPPED_BY_USER"}))
                  .has_value());
}

// A handler created for a request that fails early has no live work, so it
// must not leak.
TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       TaskMetadataUpdatedWithoutSession_CleansUpHandler) {
  EXPECT_FALSE(SendRequest(CreateRequest(TaskMetadataUpdated{})).has_value());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

// The tests below cover a pending device opt-in. The user is already opted
// in, so ShowDialog() posts its `accepted` answer instead of showing UI. Until
// that task runs, the opt-in is pending, as if the dialog were open.
class GlicExperimentalTriggeringCoordinatorOptInTest
    : public GlicExperimentalTriggeringCoordinatorWithTabTest {
 public:
  void SetUp() override {
    GlicExperimentalTriggeringCoordinatorWithTabTest::SetUp();
    // Show the opt-in on the test tab instead of looking for a Gemini tab.
    // Initialized after the base fixture's list, so it is destroyed first.
    opt_in_features_.InitAndDisableFeature(
        features::kGlicExperimentalTriggeringOptInTabFocus);
  }

 private:
  base::test::ScopedFeatureList opt_in_features_;
};

// A request that arrives while the opt-in is pending must not destroy the
// handler, or the user's answer is dropped.
TEST_F(GlicExperimentalTriggeringCoordinatorOptInTest,
       OptInPending_OtherRequestKeepsHandlerAndResult) {
  base::test::TestFuture<ExperimentalTriggeringResponse> result_future;
  EXPECT_FALSE(SendRequestWithCallback(CreateRequest(DeviceOptInRequest{}),
                                       result_future.GetRepeatingCallback())
                   .has_value());
  ASSERT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  // Fails without a Glic instance, before the posted answer runs.
  EXPECT_FALSE(SendRequestWithCallback(CreateRequest(TaskMetadataUpdated{}),
                                       result_future.GetRepeatingCallback())
                   .has_value());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  ASSERT_TRUE(result_future.Wait());
  auto result = result_future.Take();
  ASSERT_TRUE(result.device_opt_in_result.has_value());
  EXPECT_EQ(*result.device_opt_in_result, DeviceOptInResult::kAccepted);
  // Nothing else runs on this context, so the answer ends the handler.
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

// Answering an opt-in on a context that already has a live session must not
// destroy that session's handler.
TEST_F(GlicExperimentalTriggeringCoordinatorOptInTest,
       OptInAnsweredDuringActuation_KeepsHandler) {
  StartActuationSession();

  base::test::TestFuture<ExperimentalTriggeringResponse> result_future;
  EXPECT_FALSE(SendRequestWithCallback(CreateRequest(DeviceOptInRequest{}),
                                       result_future.GetRepeatingCallback())
                   .has_value());
  ASSERT_TRUE(result_future.Wait());
  auto result = result_future.Take();
  ASSERT_TRUE(result.device_opt_in_result.has_value());
  EXPECT_EQ(*result.device_opt_in_result, DeviceOptInResult::kAccepted);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);
}

// The server resends DeviceOptInRequest after it pauses and resumes. Both
// requests wait on one answer, and only one result is sent.
TEST_F(GlicExperimentalTriggeringCoordinatorOptInTest,
       ResentOptIn_SendsOneResult) {
  // Keep the handler alive after the first answer, so that a second result
  // would be sent if the handler did not drop it.
  StartActuationSession();

  base::HistogramTester histogram_tester;
  std::vector<ExperimentalTriggeringResponse> results;
  auto record =
      base::BindLambdaForTesting([&](ExperimentalTriggeringResponse response) {
        if (response.device_opt_in_result.has_value()) {
          results.push_back(std::move(response));
        }
      });
  for (int i = 0; i < 2; ++i) {
    EXPECT_FALSE(
        SendRequestWithCallback(CreateRequest(DeviceOptInRequest{}), record)
            .has_value());
  }
  histogram_tester.ExpectBucketCount(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::
          kOptInDialogAlreadyShowing,
      1);
  // The answer was posted already, so this task runs after it.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(*results[0].device_opt_in_result, DeviceOptInResult::kAccepted);
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
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      base::DoNothing());

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
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      base::DoNothing());

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
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      base::DoNothing());

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
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      base::DoNothing());
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
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      base::DoNothing());
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
      "Glic.Consumer.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.SidePanelOpened", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ClientConnected", true, 1);
  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.Latency.ToTerminalCompletion", 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       RemovesPendingTaskOnActuationStart) {
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_);
  ASSERT_TRUE(actor_service);
  actor_service->AddPendingTask(kTestContextId);
  EXPECT_TRUE(actor_service->HasPendingTask(kTestContextId));

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kStarting);

  // Still pending because the actor task hasn't started actuating yet.
  EXPECT_TRUE(actor_service->HasPendingTask(kTestContextId));
  EXPECT_EQ(actor_service->GetPendingTasksCount(), 1u);

  // A false notification alone does not resolve the pending task.
  actor_task_manager_->NotifyActuatingChangedForTesting(false);
  EXPECT_TRUE(actor_service->HasPendingTask(kTestContextId));
  EXPECT_EQ(actor_service->GetPendingTasksCount(), 1u);

  // Actuating starts: pending task is now removed.
  actor_task_manager_->NotifyActuatingChangedForTesting(true);
  EXPECT_FALSE(actor_service->HasPendingTask(kTestContextId));
  EXPECT_EQ(actor_service->GetPendingTasksCount(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ContinueActuationDoesNotObserveActorTaskCreation) {
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_);
  ASSERT_TRUE(actor_service);
  actor_service->AddPendingTask(kTestContextId);
  EXPECT_TRUE(actor_service->HasPendingTask(kTestContextId));

  ExperimentalTriggeringRequest continue_request;
  continue_request.version = 1;
  continue_request.context_id = kTestContextId;
  continue_request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  continue_request.payload =
      ContinueActuationRequest{.continuation_prompt = "keep going"};

  auto response = SendRequest(continue_request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kStarting);

  // Continuation requests do not observe actor task creation for pending tasks.
  actor_task_manager_->NotifyActuatingChangedForTesting(true);
  EXPECT_TRUE(actor_service->HasPendingTask(kTestContextId));
  EXPECT_EQ(actor_service->GetPendingTasksCount(), 1u);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       BackgroundContinuationRegistersAndClearsPendingTaskOnActuation) {
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_);
  ASSERT_TRUE(actor_service);
  EXPECT_EQ(actor_service->GetPendingTasksCount(), 0u);

  int added_callback_count = 0;
  auto added_sub = actor_service->AddPendingTaskAddedCallback(
      base::BindRepeating([](int* count, const std::string&) { (*count)++; },
                          base::Unretained(&added_callback_count)));

  coordinator_->set_is_chrome_in_background(true);
  EXPECT_CALL(mock_glic_instance_, IsActuating())
      .WillRepeatedly(testing::Return(false));

  ExperimentalTriggeringRequest continue_request;
  continue_request.version = 1;
  continue_request.context_id = kTestContextId;
  continue_request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  continue_request.payload =
      ContinueActuationRequest{.continuation_prompt = "keep going"};

  auto response = SendRequest(continue_request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kStarting);

  EXPECT_TRUE(actor_service->HasPendingTask(kTestContextId));
  EXPECT_EQ(actor_service->GetPendingTasksCount(), 1u);
  EXPECT_EQ(added_callback_count, 0);

  // Actuation starting clears the continuation pending entry.
  actor_task_manager_->NotifyActuatingChangedForTesting(true);
  EXPECT_FALSE(actor_service->HasPendingTask(kTestContextId));
  EXPECT_EQ(actor_service->GetPendingTasksCount(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       BackgroundContinuationClearsPendingTaskOnTerminalUpdate) {
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_);
  ASSERT_TRUE(actor_service);

  coordinator_->set_is_chrome_in_background(true);
  EXPECT_CALL(mock_glic_instance_, IsActuating())
      .WillRepeatedly(testing::Return(false));

  ExperimentalTriggeringRequest continue_request;
  continue_request.version = 1;
  continue_request.context_id = kTestContextId;
  continue_request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  continue_request.payload =
      ContinueActuationRequest{.continuation_prompt = "text reply only"};

  auto response = SendRequest(continue_request);
  ASSERT_TRUE(response.has_value());
  EXPECT_TRUE(actor_service->HasPendingTask(kTestContextId));

  // A terminal completion without creating a new actor task must clear the
  // pending entry so the foreground service does not linger.
  auto complete_update = mojom::ExperimentalTriggeringUpdate::New();
  complete_update->type =
      mojom::ExperimentalTriggeringUpdateType::kTerminalCompletion;
  complete_update->data = "done";
  test_triggering_manager_.SendUpdate(
      std::move(complete_update), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();

  EXPECT_FALSE(actor_service->HasPendingTask(kTestContextId));
  EXPECT_EQ(actor_service->GetPendingTasksCount(), 0u);
}

class GlicExperimentalTriggeringCoordinatorBackgroundContinuationYieldTest
    : public GlicExperimentalTriggeringCoordinatorWithTabTest,
      public testing::WithParamInterface<
          mojom::ExperimentalTriggeringUpdateType> {};

// A continuation turn can hand control back to the user (e.g. a clarifying
// question) before any actor task starts. The pending entry must be cleared,
// since pending entries never expire.
TEST_P(GlicExperimentalTriggeringCoordinatorBackgroundContinuationYieldTest,
       ClearsPendingTask) {
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_);
  ASSERT_TRUE(actor_service);

  coordinator_->set_is_chrome_in_background(true);
  EXPECT_CALL(mock_glic_instance_, IsActuating())
      .WillRepeatedly(testing::Return(false));

  ExperimentalTriggeringRequest continue_request;
  continue_request.version = 1;
  continue_request.context_id = kTestContextId;
  continue_request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  continue_request.payload =
      ContinueActuationRequest{.continuation_prompt = "ask me first"};

  auto response = SendRequest(continue_request);
  ASSERT_TRUE(response.has_value());
  EXPECT_TRUE(actor_service->HasPendingTask(kTestContextId));

  auto update = mojom::ExperimentalTriggeringUpdate::New();
  update->type = GetParam();
  update->data = "which size?";
  test_triggering_manager_.SendUpdate(
      std::move(update), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();

  EXPECT_FALSE(actor_service->HasPendingTask(kTestContextId));
  EXPECT_EQ(actor_service->GetPendingTasksCount(), 0u);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GlicExperimentalTriggeringCoordinatorBackgroundContinuationYieldTest,
    testing::Values(mojom::ExperimentalTriggeringUpdateType::kYieldToUser,
                    mojom::ExperimentalTriggeringUpdateType::kPaused));

// If the invocation fails synchronously (no instance is returned), no pending
// entry is added for a request that is about to fail.
TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       BackgroundContinuationWithoutInstanceDoesNotAddPendingTask) {
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_);
  ASSERT_TRUE(actor_service);

  coordinator_->set_is_chrome_in_background(true);
  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([](InvokeWithAutoSubmitPasskey passkey,
                   GlicInvokeOptions options,
                   GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        return base::WeakPtr<GlicInstance>();
      });

  ExperimentalTriggeringRequest continue_request;
  continue_request.version = 1;
  continue_request.context_id = kTestContextId;
  continue_request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  continue_request.payload =
      ContinueActuationRequest{.continuation_prompt = "keep going"};

  auto response = SendRequest(continue_request);
  ASSERT_TRUE(response.has_value());
  EXPECT_FALSE(actor_service->HasPendingTask(kTestContextId));
  EXPECT_EQ(actor_service->GetPendingTasksCount(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       BackgroundContinuationWhileAlreadyActuatingDoesNotAddPendingTask) {
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_);
  ASSERT_TRUE(actor_service);

  coordinator_->set_is_chrome_in_background(true);
  EXPECT_CALL(mock_glic_instance_, IsActuating())
      .WillRepeatedly(testing::Return(true));

  ExperimentalTriggeringRequest continue_request;
  continue_request.version = 1;
  continue_request.context_id = kTestContextId;
  continue_request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  continue_request.payload =
      ContinueActuationRequest{.continuation_prompt = "mid-task yield resume"};

  auto response = SendRequest(continue_request);
  ASSERT_TRUE(response.has_value());
  EXPECT_FALSE(actor_service->HasPendingTask(kTestContextId));
  EXPECT_EQ(actor_service->GetPendingTasksCount(), 0u);
}
#endif

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       RemovesPendingTaskOnInvokeError) {
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_);
  ASSERT_TRUE(actor_service);
  actor_service->AddPendingTask(kTestContextId);
  EXPECT_TRUE(actor_service->HasPendingTask(kTestContextId));

  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([this](InvokeWithAutoSubmitPasskey passkey,
                       GlicInvokeOptions options,
                       GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        options
            .TakeOnErrorWithDiagnostics(
                GlicApiTestPasskeys::GetErrorDiagnosticsPassKey())
            .Run(GlicInvokeError::kTimeout, "");
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

  EXPECT_FALSE(actor_service->HasPendingTask(kTestContextId));
  EXPECT_EQ(actor_service->GetPendingTasksCount(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       TriggerActuation_AttachesPaymentMetadataPayload) {
  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  std::optional<std::vector<uint8_t>> captured_metadata;
  std::optional<mojom::InvocationSource> captured_source;
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        captured_source = options.GetInvocationSource();
        auto* payload = std::get_if<mojom::InvocationPayloadPtr>(
            &options.source_or_payload);
        if (payload && *payload && (*payload)->is_experimental_triggering()) {
          captured_metadata =
              (*payload)->get_experimental_triggering()->payment_metadata;
        }
        return mock_glic_instance_.GetWeakPtr();
      });

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{
      .initial_prompt = "test", .payment_metadata = {0x01, 0x02, 0x03}};

  SendRequest(request);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return captured_metadata.has_value(); }));
  EXPECT_EQ(captured_source, mojom::InvocationSource::kExperimentalTriggering);
  EXPECT_EQ(*captured_metadata, (std::vector<uint8_t>{0x01, 0x02, 0x03}));
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       TriggerActuation_AttachesPayloadWithEmptyPaymentMetadata) {
  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  std::optional<std::vector<uint8_t>> captured_metadata;
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        auto* payload = std::get_if<mojom::InvocationPayloadPtr>(
            &options.source_or_payload);
        if (payload && *payload && (*payload)->is_experimental_triggering()) {
          captured_metadata =
              (*payload)->get_experimental_triggering()->payment_metadata;
        }
        return mock_glic_instance_.GetWeakPtr();
      });

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  SendRequest(request);

  // The payload is always attached for trigger requests, even when empty.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return captured_metadata.has_value(); }));
  EXPECT_TRUE(captured_metadata->empty());
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
        options
            .TakeOnErrorWithDiagnostics(
                GlicApiTestPasskeys::GetErrorDiagnosticsPassKey())
            .Run(GlicInvokeError::kTimeout, "");
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
      "Glic.Consumer.ExperimentalTriggering.ExecutionOutcome",
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
        options
            .TakeOnErrorWithDiagnostics(
                GlicApiTestPasskeys::GetErrorDiagnosticsPassKey())
            .Run(GlicInvokeError::kTimeout, "");
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
      "Glic.Consumer.ExperimentalTriggering.ExecutionOutcome",
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
        options
            .TakeOnErrorWithDiagnostics(
                GlicApiTestPasskeys::GetErrorDiagnosticsPassKey())
            .Run(GlicInvokeError::kInvokeInProgress, "");
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
  histogram_tester.ExpectUniqueSample(
      "Glic.Consumer.ExperimentalTriggering.ExecutionOutcome",
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
        options
            .TakeOnErrorWithDiagnostics(
                GlicApiTestPasskeys::GetErrorDiagnosticsPassKey())
            .Run(GlicInvokeError::kTabClosed, "");
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
  histogram_tester.ExpectUniqueSample(
      "Glic.Consumer.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kInvokeErrorTabClosed, 1);
}

// GlicInvokeHandler observes tab / panel / instance teardown (e.g. via
// TabInterface::WillDetach) before GlicInstanceImpl removes itself, so the
// terminal update produced from the invoke error must already carry the
// chrome_error_code that OnInstanceWillBeDestroyed() would otherwise add.
struct InvokeErrorChromeErrorCodeCase {
  GlicInvokeError error;
  const char* expected_code;
};

class GlicExperimentalTriggeringCoordinatorInvokeErrorCodeTest
    : public GlicExperimentalTriggeringCoordinatorWithTabTest,
      public testing::WithParamInterface<InvokeErrorChromeErrorCodeCase> {};

TEST_P(GlicExperimentalTriggeringCoordinatorInvokeErrorCodeTest,
       ActuationInvokeError_AttributesChromeErrorCode) {
  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  base::OnceCallback<void(GlicInvokeError, std::string)> on_error;
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        std::move(options.on_panel_opened).Run();
        std::move(options.on_client_connected)
            .Run(mock_glic_instance_.GetWeakPtr());
        on_error = options.TakeOnErrorWithDiagnostics(
            GlicApiTestPasskeys::GetErrorDiagnosticsPassKey());
        return mock_glic_instance_.GetWeakPtr();
      });

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};
  ASSERT_TRUE(
      coordinator_
          ->OnRequest(
              kTestContextId, request,
              ScopedIncomingMessageResultLogger(
                  GlicMetricReporter(profile_),
                  ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
              update_future.GetRepeatingCallback(), nullptr)
          .has_value());

  // The invoke error arrives first; the instance teardown notification that
  // follows must not produce a second terminal update.
  ASSERT_TRUE(on_error);
  std::move(on_error).Run(GetParam().error, "stage=WAIT_FOR_ACTUATION");
  test_triggering_manager_.NotifyInstanceWillBeDestroyed(
      GlicExperimentalTriggeringManager::InstanceTeardownReason::
          kInstanceDestroyed);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  ASSERT_TRUE(update_future.IsReady());
  auto failure = update_future.Take();
  ASSERT_TRUE(failure.task_update.has_value());
  EXPECT_EQ(failure.task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(failure.task_update->metadata["chrome_error_code"],
            GetParam().expected_code);
  EXPECT_EQ(failure.task_update->metadata["chrome_invoke_diagnostics"],
            "stage=WAIT_FOR_ACTUATION");
  EXPECT_FALSE(update_future.IsReady());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GlicExperimentalTriggeringCoordinatorInvokeErrorCodeTest,
    testing::Values(InvokeErrorChromeErrorCodeCase{GlicInvokeError::kTabClosed,
                                                   "GLIC_TAB_CLOSED"},
                    InvokeErrorChromeErrorCodeCase{
                        GlicInvokeError::kPanelClosed, "GLIC_PANEL_CLOSED"},
                    InvokeErrorChromeErrorCodeCase{
                        GlicInvokeError::kInstanceDestroyedBlankInstanceClosed,
                        "GLIC_PANEL_CLOSED"},
                    InvokeErrorChromeErrorCodeCase{
                        GlicInvokeError::kInstanceDestroyedUnbound,
                        "GLIC_DESTROYED_BEFORE_COMPLETION"},
                    InvokeErrorChromeErrorCodeCase{
                        GlicInvokeError::kInstanceDestroyedArchived,
                        "GLIC_DESTROYED_BEFORE_COMPLETION"},
                    InvokeErrorChromeErrorCodeCase{
                        GlicInvokeError::kInstanceDestroyedSignedOut,
                        "GLIC_DESTROYED_BEFORE_COMPLETION"},
                    InvokeErrorChromeErrorCodeCase{
                        GlicInvokeError::kInstanceDestroyedShutdown,
                        "GLIC_DESTROYED_BEFORE_COMPLETION"}));

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ActuationInvokeErrorOther_OmitsChromeErrorCode) {
  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([this](InvokeWithAutoSubmitPasskey passkey,
                       GlicInvokeOptions options,
                       GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        options
            .TakeOnErrorWithDiagnostics(
                GlicApiTestPasskeys::GetErrorDiagnosticsPassKey())
            .Run(GlicInvokeError::kInvalidConfiguration, "");
        return mock_glic_instance_.GetWeakPtr();
      });

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};
  ASSERT_TRUE(
      coordinator_
          ->OnRequest(
              kTestContextId, request,
              ScopedIncomingMessageResultLogger(
                  GlicMetricReporter(profile_),
                  ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
              update_future.GetRepeatingCallback(), nullptr)
          .has_value());

  auto failure = update_future.Take();
  ASSERT_TRUE(failure.task_update.has_value());
  EXPECT_EQ(failure.task_update->state, TaskUpdate::State::kFailed);
  EXPECT_FALSE(failure.task_update->metadata.contains("chrome_error_code"));
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
        options
            .TakeOnErrorWithDiagnostics(
                GlicApiTestPasskeys::GetErrorDiagnosticsPassKey())
            .Run(GlicInvokeError::kInvalidConfiguration, "");
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
  histogram_tester.ExpectUniqueSample(
      "Glic.Consumer.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kInvokeErrorOther, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ActuationSuperseded_EmitsFailedUpdateWithoutRecordingOutcome) {
  base::HistogramTester histogram_tester;

  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  base::OnceCallback<void(GlicInvokeError, std::string)> on_error;
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        on_error = options.TakeOnErrorWithDiagnostics(
            GlicApiTestPasskeys::GetErrorDiagnosticsPassKey());
        return mock_glic_instance_.GetWeakPtr();
      });

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  auto response = coordinator_->OnRequest(
      kTestContextId, request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kStarting);

  // Simulate the invocation being superseded by some other, unrelated
  // invocation (i.e. not from within this handler's own invoke call).
  ASSERT_TRUE(on_error);
  std::move(on_error).Run(
      GlicInvokeError::kSuperseded,
      "stage=WAIT_FOR_ACTUATION;load_state_at_start=LOADING");

  auto failure = update_future.Take();
  ASSERT_TRUE(failure.task_update.has_value());
  EXPECT_EQ(failure.task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(failure.task_update->data_type,
            TaskUpdate::DataType::kErrorMessage);
  EXPECT_EQ(failure.task_update->data,
            "Glic invocation failed: Invocation was superseded");
  EXPECT_THAT(
      failure.task_update->metadata,
      testing::UnorderedElementsAre(
          testing::Pair("chrome_error_code", "GLIC_SUPERSEDED"),
          testing::Pair("chrome_invoke_diagnostics",
                        "stage=WAIT_FOR_ACTUATION;load_state_at_start=LOADING"),
          testing::Pair("panel_opened", "0"),
          testing::Pair("client_connected", "0"),
          testing::Pair("t_to_terminal_ms", testing::_)));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));
  EXPECT_FALSE(update_future.IsReady());

  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.ExecutionOutcome", 0);
}

// Regression test: a continuation request supersedes the in-flight invocation
// started by the trigger request on the same context. Since both requests share
// one updates handler, the superseded invocation's error must not tear down the
// handler, otherwise updates for the continuation are dropped.
TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ContinuationSupersedingOwnInvocation_KeepsReceivingUpdates) {
  base::HistogramTester histogram_tester;

  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  base::OnceCallback<void(GlicInvokeError, std::string)> trigger_on_error;
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        EXPECT_FALSE(options.supersede_if_in_progress);
        std::move(options.on_panel_opened).Run();
        std::move(options.on_client_connected)
            .Run(mock_glic_instance_.GetWeakPtr());
        // The trigger invocation stays in progress.
        trigger_on_error = options.TakeOnErrorWithDiagnostics(
            GlicApiTestPasskeys::GetErrorDiagnosticsPassKey());
        return mock_glic_instance_.GetWeakPtr();
      })
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        EXPECT_TRUE(options.supersede_if_in_progress);
        // Mirrors GlicInstanceCoordinatorImpl, which cancels the in-progress
        // invocation before starting the superseding one. Like
        // GlicInvokeHandler::OnError(), the error callback is posted.
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(trigger_on_error),
                                      GlicInvokeError::kSuperseded, ""));
        std::move(options.on_panel_opened).Run();
        std::move(options.on_client_connected)
            .Run(mock_glic_instance_.GetWeakPtr());
        return mock_glic_instance_.GetWeakPtr();
      });

  StartActuationSession();

  ExperimentalTriggeringRequest continue_request;
  continue_request.version = 1;
  continue_request.context_id = kTestContextId;
  continue_request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  continue_request.payload =
      ContinueActuationRequest{.continuation_prompt = "keep going"};

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  auto response = coordinator_->OnRequest(
      kTestContextId, continue_request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kStarting);

  // Updates for the continuation must still be delivered. If the handler had
  // been torn down, its receiver would already be disconnected and this update
  // would never arrive.
  auto update = mojom::ExperimentalTriggeringUpdate::New();
  update->type = mojom::ExperimentalTriggeringUpdateType::kWorklog;
  update->data = "continuing";
  test_triggering_manager_.SendUpdate(
      std::move(update), mojom::SubscriberObservationType::kUpdate);

  // Flushing runs the loop until the update is dispatched, so any posted
  // cleanup has also run by now.
  test_triggering_manager_.FlushForTesting();
  ASSERT_TRUE(update_future.IsReady());
  auto async_response = update_future.Take();
  ASSERT_TRUE(async_response.task_update.has_value());
  EXPECT_EQ(async_response.task_update->state, TaskUpdate::State::kRunning);
  EXPECT_EQ(async_response.task_update->data_type,
            TaskUpdate::DataType::kWorklog);
  EXPECT_EQ(async_response.task_update->data, "continuing");

  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  // The self-supersede must not have been recorded as an outcome.
  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.ExecutionOutcome", 0);
}

// Once the user has closed the panel mid-session, a continuation must not
// reopen it: the turn keeps running in the background on the kept-alive
// instance. The initial trigger still shows the panel.
TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ContinuationWithPanelClosed_DoesNotReopenPanel) {
  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  std::vector<bool> show_panel_values;
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .Times(2)
      .WillRepeatedly([&](InvokeWithAutoSubmitPasskey passkey,
                          GlicInvokeOptions options,
                          GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        show_panel_values.push_back(auto_submit_options.show_panel);
        std::move(options.on_panel_opened).Run();
        std::move(options.on_client_connected)
            .Run(mock_glic_instance_.GetWeakPtr());
        return mock_glic_instance_.GetWeakPtr();
      });

  ON_CALL(mock_glic_instance_, IsShowing())
      .WillByDefault(testing::Return(true));
  StartActuationSession();
  ASSERT_THAT(show_panel_values, testing::ElementsAre(true));

  // The user closes the panel; the instance stays alive for the session.
  ON_CALL(mock_glic_instance_, IsShowing())
      .WillByDefault(testing::Return(false));

  ExperimentalTriggeringRequest continue_request;
  continue_request.version = 1;
  continue_request.context_id = kTestContextId;
  continue_request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  continue_request.payload =
      ContinueActuationRequest{.continuation_prompt = "keep going"};
  auto response = SendRequest(continue_request);
  ASSERT_TRUE(response.has_value());
  EXPECT_THAT(show_panel_values, testing::ElementsAre(true, false));
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
  histogram_tester.ExpectUniqueSample(
      "Glic.Consumer.ExperimentalTriggering.ExecutionOutcome",
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
  histogram_tester.ExpectUniqueSample(
      "Glic.Consumer.ExperimentalTriggering.ExecutionOutcome",
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
  histogram_tester.ExpectUniqueSample(
      "Glic.Consumer.ExperimentalTriggering.ExecutionOutcome",
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
  histogram_tester.ExpectUniqueSample(
      "Glic.Consumer.ExperimentalTriggering.ExecutionOutcome",
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

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  auto response = coordinator_->OnRequest(
      kTestContextId, request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kStarting);
  EXPECT_FALSE(update_future.IsReady());

  // Destroying coordinator mid-flight triggers the handler destructor, which
  // emits a best-effort terminal kFailed update and records the outcome.
  coordinator_.reset();

  ASSERT_TRUE(update_future.IsReady());
  auto failure = update_future.Take();
  ASSERT_TRUE(failure.task_update.has_value());
  EXPECT_EQ(failure.task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(failure.task_update->data_type,
            TaskUpdate::DataType::kErrorMessage);
  EXPECT_EQ(failure.task_update->data,
            "Experimental triggering handler destroyed before completion.");
  EXPECT_THAT(failure.task_update->metadata,
              testing::UnorderedElementsAre(
                  testing::Pair("chrome_error_code",
                                "GLIC_DESTROYED_BEFORE_COMPLETION"),
                  testing::Pair("panel_opened", "1"),
                  testing::Pair("client_connected", "1"),
                  testing::Pair("t_to_panel_open_ms", testing::_),
                  testing::Pair("t_to_client_connected_ms", testing::_),
                  testing::Pair("t_to_terminal_ms", testing::_)));
  EXPECT_FALSE(update_future.IsReady());

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kDestroyedBeforeCompletion,
      1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Consumer.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kDestroyedBeforeCompletion,
      1);
}

// A terminal update (COMPLETE, STOPPED or FAILED) ends the task on both sides:
// after one, nothing more arrives for the task, so the handler must stop the
// work it still owns and end the session itself.
class GlicExperimentalTriggeringCoordinatorTerminalUpdateTest
    : public GlicExperimentalTriggeringCoordinatorWithTabTest {
 protected:
  // Sends a TriggerActuationRequest whose updates go to `updates_`, and
  // returns its synchronous reply.
  std::optional<ExperimentalTriggeringResponse> SendTrigger() {
    return SendRequestWithCallback(
        CreateRequest(TriggerActuationRequest{.initial_prompt = "hello"}),
        updates_.GetRepeatingCallback());
  }

  void StartSession() {
    auto response = SendTrigger();
    ASSERT_TRUE(response.has_value());
    ASSERT_TRUE(response->task_update.has_value());
    EXPECT_EQ(response->task_update->state, TaskUpdate::State::kStarting);
    EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);
  }

  void SendWebClientUpdate(mojom::ExperimentalTriggeringUpdateType type,
                           mojom::SubscriberObservationType observation =
                               mojom::SubscriberObservationType::kUpdate) {
    auto update = mojom::ExperimentalTriggeringUpdate::New();
    update->type = type;
    update->data = "data";
    test_triggering_manager_.SendUpdate(std::move(update), observation);
    test_triggering_manager_.FlushForTesting();
  }

  // Expects the next update sent to the server to be in `state`.
  void ExpectNextUpdate(TaskUpdate::State state) {
    ExperimentalTriggeringResponse response = updates_.Take();
    ASSERT_TRUE(response.task_update.has_value());
    EXPECT_EQ(response.task_update->state, state);
  }

  void ExpectSessionEnded() {
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
    }));
  }

  // Reports that an actor task was created on the instance after this
  // session's trigger.
  void CreateActorTask() {
    actor_task_manager_->NotifyActuatingChangedForTesting(true);
  }

  // Makes the next invocation keep running and hand its error callback to
  // `on_error`. If `client_connects`, it first reports that the web client
  // connected.
  void ExpectInvocation(
      base::OnceCallback<void(GlicInvokeError, std::string)>& on_error,
      bool client_connects) {
    EXPECT_CALL(*mock_glic_service(),
                InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
        .WillOnce([&on_error, client_connects, this](
                      InvokeWithAutoSubmitPasskey passkey,
                      GlicInvokeOptions options,
                      GlicInvokeWithAutoSubmitOptions auto_submit_options) {
          if (client_connects) {
            std::move(options.on_client_connected)
                .Run(mock_glic_instance_.GetWeakPtr());
          }
          on_error = options.TakeOnErrorWithDiagnostics(
              GlicApiTestPasskeys::GetErrorDiagnosticsPassKey());
          return mock_glic_instance_.GetWeakPtr();
        });
  }

  base::test::TestFuture<ExperimentalTriggeringResponse> updates_;
};

struct SessionEndTestCase {
  enum class End { kUpdate, kErrorObservation, kDisconnect, kDestroy };
  std::string test_name;
  End end;
  // For End::kUpdate.
  mojom::ExperimentalTriggeringUpdateType type =
      mojom::ExperimentalTriggeringUpdateType::kUnknown;
  TaskUpdate::State expected_state;
  bool expect_cancel_task;
  GlicExperimentalTriggeringHandlerDestroyReason expected_destroy_reason;
};

class GlicExperimentalTriggeringCoordinatorSessionEndTest
    : public GlicExperimentalTriggeringCoordinatorTerminalUpdateTest,
      public testing::WithParamInterface<SessionEndTestCase> {};

TEST_P(GlicExperimentalTriggeringCoordinatorSessionEndTest,
       StopsOwnedWorkAndEndsSession) {
  const SessionEndTestCase& test_case = GetParam();
  base::HistogramTester destroy_tester;
  // An invocation is in progress on the instance. GlicInstance can't tell
  // whether it is this handler's, so it is left alone.
  ON_CALL(mock_glic_instance_, IsInvoking())
      .WillByDefault(testing::Return(true));
  EXPECT_CALL(mock_glic_instance_, CancelInvoke()).Times(0);
  EXPECT_CALL(mock_glic_instance_, CancelTask())
      .Times(test_case.expect_cancel_task ? 1 : 0);
  StartSession();
  CreateActorTask();

  switch (test_case.end) {
    case SessionEndTestCase::End::kUpdate:
      SendWebClientUpdate(test_case.type);
      break;
    case SessionEndTestCase::End::kErrorObservation:
      SendWebClientUpdate(mojom::ExperimentalTriggeringUpdateType::kUnknown,
                          mojom::SubscriberObservationType::kError);
      break;
    case SessionEndTestCase::End::kDisconnect:
      test_triggering_manager_.ResetHandler();
      break;
    case SessionEndTestCase::End::kDestroy:
      coordinator_.reset();
      break;
  }
  ExpectNextUpdate(test_case.expected_state);
  if (test_case.end != SessionEndTestCase::End::kDestroy) {
    ExpectSessionEnded();
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
    }));
  }
  ExpectHandlerDestroyed(destroy_tester, test_case.expected_destroy_reason);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GlicExperimentalTriggeringCoordinatorSessionEndTest,
    testing::Values(
        // After COMPLETE the web client stops its actor task itself.
        SessionEndTestCase{
            .test_name = "TerminalCompletion",
            .end = SessionEndTestCase::End::kUpdate,
            .type =
                mojom::ExperimentalTriggeringUpdateType::kTerminalCompletion,
            .expected_state = TaskUpdate::State::kComplete,
            .expect_cancel_task = false,
            .expected_destroy_reason =
                GlicExperimentalTriggeringHandlerDestroyReason::
                    kSessionComplete},
        SessionEndTestCase{
            .test_name = "TerminalStopped",
            .end = SessionEndTestCase::End::kUpdate,
            .type = mojom::ExperimentalTriggeringUpdateType::kTerminalStopped,
            .expected_state = TaskUpdate::State::kStopped,
            .expect_cancel_task = true,
            .expected_destroy_reason =
                GlicExperimentalTriggeringHandlerDestroyReason::
                    kSessionStopped},
        SessionEndTestCase{
            .test_name = "TerminalFailed",
            .end = SessionEndTestCase::End::kUpdate,
            .type = mojom::ExperimentalTriggeringUpdateType::kTerminalFailed,
            .expected_state = TaskUpdate::State::kFailed,
            .expect_cancel_task = true,
            .expected_destroy_reason =
                GlicExperimentalTriggeringHandlerDestroyReason::kSessionFailed},
        SessionEndTestCase{
            .test_name = "ErrorObservation",
            .end = SessionEndTestCase::End::kErrorObservation,
            .expected_state = TaskUpdate::State::kFailed,
            .expect_cancel_task = true,
            .expected_destroy_reason =
                GlicExperimentalTriggeringHandlerDestroyReason::kSessionFailed},
        SessionEndTestCase{.test_name = "Disconnect",
                           .end = SessionEndTestCase::End::kDisconnect,
                           .expected_state = TaskUpdate::State::kFailed,
                           .expect_cancel_task = true,
                           .expected_destroy_reason =
                               GlicExperimentalTriggeringHandlerDestroyReason::
                                   kClientDisconnected},
        SessionEndTestCase{.test_name = "DestroyedMidTask",
                           .end = SessionEndTestCase::End::kDestroy,
                           .expected_state = TaskUpdate::State::kFailed,
                           .expect_cancel_task = true,
                           .expected_destroy_reason =
                               GlicExperimentalTriggeringHandlerDestroyReason::
                                   kCoordinatorShutdown}),
    [](const testing::TestParamInfo<SessionEndTestCase>& info) {
      return info.param.test_name;
    });

struct InvokeErrorTestCase {
  std::string test_name;
  bool create_actor_task;
  GlicInvokeError error;
  bool expect_cancel_task;
  GlicExperimentalTriggeringHandlerDestroyReason expected_destroy_reason;
};

class GlicExperimentalTriggeringCoordinatorInvokeErrorTest
    : public GlicExperimentalTriggeringCoordinatorTerminalUpdateTest,
      public testing::WithParamInterface<InvokeErrorTestCase> {};

TEST_P(GlicExperimentalTriggeringCoordinatorInvokeErrorTest,
       SendsFailedAndEndsSession) {
  const InvokeErrorTestCase& test_case = GetParam();
  base::HistogramTester destroy_tester;
  base::OnceCallback<void(GlicInvokeError, std::string)> on_error;
  ExpectInvocation(on_error, /*client_connects=*/true);
  EXPECT_CALL(mock_glic_instance_, CancelTask())
      .Times(test_case.expect_cancel_task ? 1 : 0);
  StartSession();
  if (test_case.create_actor_task) {
    CreateActorTask();
  }

  ASSERT_TRUE(on_error);
  std::move(on_error).Run(test_case.error, std::string());
  ExpectNextUpdate(TaskUpdate::State::kFailed);
  ExpectSessionEnded();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));
  ExpectHandlerDestroyed(destroy_tester, test_case.expected_destroy_reason);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GlicExperimentalTriggeringCoordinatorInvokeErrorTest,
    testing::Values(
        InvokeErrorTestCase{
            .test_name = "AfterActorTask",
            .create_actor_task = true,
            .error = GlicInvokeError::kTabClosed,
            .expect_cancel_task = true,
            .expected_destroy_reason =
                GlicExperimentalTriggeringHandlerDestroyReason::kTabClosed},
        // The client connected, but the invocation failed before an actor
        // task was created, for example before the prompt was sent. An actor
        // task that is active on the instance belongs to another caller.
        InvokeErrorTestCase{
            .test_name = "BeforeActorTask",
            .create_actor_task = false,
            .error = GlicInvokeError::kTimeout,
            .expect_cancel_task = false,
            .expected_destroy_reason =
                GlicExperimentalTriggeringHandlerDestroyReason::kInvokeError},
        InvokeErrorTestCase{
            .test_name = "InvokeInProgress",
            .create_actor_task = false,
            .error = GlicInvokeError::kInvokeInProgress,
            .expect_cancel_task = false,
            .expected_destroy_reason =
                GlicExperimentalTriggeringHandlerDestroyReason::kInvokeError},
        InvokeErrorTestCase{
            .test_name = "InstanceDestroyedArchived",
            .create_actor_task = false,
            .error = GlicInvokeError::kInstanceDestroyedArchived,
            .expect_cancel_task = false,
            .expected_destroy_reason =
                GlicExperimentalTriggeringHandlerDestroyReason::
                    kInstanceDestroyed},
        // The superseding invocation doesn't start an actor task for this
        // handler.
        InvokeErrorTestCase{
            .test_name = "Superseded",
            .create_actor_task = false,
            .error = GlicInvokeError::kSuperseded,
            .expect_cancel_task = false,
            .expected_destroy_reason =
                GlicExperimentalTriggeringHandlerDestroyReason::kSuperseded}),
    [](const testing::TestParamInfo<InvokeErrorTestCase>& info) {
      return info.param.test_name;
    });

class GlicExperimentalTriggeringCoordinatorRegistrationFailureTest
    : public GlicExperimentalTriggeringCoordinatorTerminalUpdateTest,
      public testing::WithParamInterface<bool> {};

TEST_P(GlicExperimentalTriggeringCoordinatorRegistrationFailureTest,
       EndsSessionAndRemovesPendingTask) {
  const bool no_triggering_manager = GetParam();
  if (no_triggering_manager) {
    ON_CALL(mock_glic_instance_, GetExperimentalTriggeringManager())
        .WillByDefault(testing::Return(nullptr));
  } else {
    test_triggering_manager_.set_registration_success(false);
  }
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_);
  actor_service->AddPendingTask(kTestContextId);
  // No actor task was created yet, so there is none to cancel.
  EXPECT_CALL(mock_glic_instance_, CancelTask()).Times(0);

  // The failure is reported synchronously, so the terminal update has been
  // sent, but the posted cleanup has not run yet.
  ASSERT_TRUE(SendTrigger().has_value());
  EXPECT_FALSE(actor_service->HasPendingTask(kTestContextId));
  ExpectNextUpdate(TaskUpdate::State::kFailed);
  ExpectSessionEnded();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GlicExperimentalTriggeringCoordinatorRegistrationFailureTest,
    testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "NoTriggeringManager" : "RegistrationRejected";
    });

class GlicExperimentalTriggeringCoordinatorFailedContinueTest
    : public GlicExperimentalTriggeringCoordinatorTerminalUpdateTest,
      public testing::WithParamInterface<bool> {};

// A yielded actor task is still this handler's, so a Continue that fails
// before acting must cancel it.
TEST_P(GlicExperimentalTriggeringCoordinatorFailedContinueTest,
       CancelsYieldedTask) {
  const bool fails_synchronously = GetParam();
  base::OnceCallback<void(GlicInvokeError, std::string)> continue_on_error;
  EXPECT_CALL(*mock_glic_service(),
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce(testing::DoDefault())
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options)
                    -> base::WeakPtr<GlicInstance> {
        auto on_error = options.TakeOnErrorWithDiagnostics(
            GlicApiTestPasskeys::GetErrorDiagnosticsPassKey());
        if (fails_synchronously) {
          // Like GlicInstanceCoordinatorImpl, a synchronous failure posts
          // on_error and returns no instance.
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(on_error),
                             GlicInvokeError::kLiveModeActive, std::string()));
          return base::WeakPtr<GlicInstance>();
        }
        // The continuation never reaches the web client.
        continue_on_error = std::move(on_error);
        return mock_glic_instance_.GetWeakPtr();
      });
  StartSession();
  CreateActorTask();
  SendWebClientUpdate(mojom::ExperimentalTriggeringUpdateType::kYieldToUser);
  ExpectNextUpdate(TaskUpdate::State::kYield);

  EXPECT_CALL(mock_glic_instance_, CancelTask());
  auto response = SendRequestWithCallback(
      CreateRequest(
          ContinueActuationRequest{.continuation_prompt = "keep going"}),
      updates_.GetRepeatingCallback());
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kStarting);
  if (!fails_synchronously) {
    ASSERT_TRUE(continue_on_error);
    std::move(continue_on_error).Run(GlicInvokeError::kTimeout, std::string());
  }
  ExpectNextUpdate(TaskUpdate::State::kFailed);
  ExpectSessionEnded();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GlicExperimentalTriggeringCoordinatorFailedContinueTest,
    testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "Synchronously" : "Asynchronously";
    });

TEST_F(GlicExperimentalTriggeringCoordinatorTerminalUpdateTest,
       StaleCleanupDoesNotDeleteNewHandler) {
  // The invocation succeeds without a Glic instance, so the handler has no
  // live work left, but has not sent a terminal update. So the Trigger deletes
  // it right away: its destructor sends FAILED and posts a cleanup for the same
  // context id.
  EXPECT_CALL(*mock_glic_service(),
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([](InvokeWithAutoSubmitPasskey passkey,
                   GlicInvokeOptions options,
                   GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        std::move(options.on_success).Run();
        return base::WeakPtr<GlicInstance>();
      })
      .WillRepeatedly(testing::DoDefault());
  auto first = SendTrigger();
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(first->task_update.has_value());
  EXPECT_EQ(first->task_update->state, TaskUpdate::State::kStarting);
  ExpectNextUpdate(TaskUpdate::State::kFailed);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);

  // A new session starts on the same context id before that cleanup runs, on
  // a new handler whose sequence numbers restart at 0.
  auto response = SendTrigger();
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_metadata.has_value());
  EXPECT_EQ(response->task_metadata->sender_sequence_number, 0);
  // Tasks run in order, so the stale cleanup has run once this task runs.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);
}

// A synchronous terminal reply ends the task for the server, so Chrome must
// also stop the task and end the session. A duplicate start request gets an
// empty reply and must leave the running task untouched.
class GlicExperimentalTriggeringCoordinatorSyncTerminalTest
    : public GlicExperimentalTriggeringCoordinatorTerminalUpdateTest {
 protected:
  std::optional<ExperimentalTriggeringResponse> SendProto(
      const components_sharing_message::GlicExperimentalTriggering& proto) {
    base::test::TestFuture<std::optional<ExperimentalTriggeringResponse>>
        response_future;
    coordinator_->OnProtoMessage(
        kTestContextId, proto,
        ScopedIncomingMessageResultLogger(
            GlicMetricReporter(profile_),
            ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
        updates_.GetRepeatingCallback(), response_future.GetCallback());
    return response_future.Take();
  }

  components_sharing_message::GlicExperimentalTriggering
  CreateVersionMismatchProto() {
    components_sharing_message::GlicExperimentalTriggering proto;
    proto.set_context_id(kTestContextId);
    proto.set_glic_experimental_triggering_version(9999);
    proto.mutable_request()->mutable_device_opt_in_request();
    return proto;
  }

  // Starts a session whose actor task this handler owns, and expects that
  // the task is cancelled exactly once.
  void StartSessionWithActorTask() {
    EXPECT_CALL(mock_glic_instance_, CancelTask());
    StartSession();
    CreateActorTask();
  }

  // Starts a session whose actor task yielded to the user.
  void StartYieldedSession() {
    StartSessionWithActorTask();
    SendWebClientUpdate(mojom::ExperimentalTriggeringUpdateType::kYieldToUser);
    ExpectNextUpdate(TaskUpdate::State::kYield);
  }

  // Expects `response` to be FAILED with `expected_error`, and the session to
  // be over.
  void ExpectFailedAndSessionEnded(
      const std::optional<ExperimentalTriggeringResponse>& response,
      std::string_view expected_error) {
    ExpectFailedTaskUpdate(response, expected_error);
    ExpectSessionOver();
  }

  // Expects `response` to be STOPPED, and the session to be over.
  void ExpectStoppedAndSessionEnded(
      const std::optional<ExperimentalTriggeringResponse>& response) {
    ASSERT_TRUE(response.has_value());
    ASSERT_TRUE(response->task_update.has_value());
    EXPECT_EQ(response->task_update->state, TaskUpdate::State::kStopped);
    ExpectSessionOver();
  }

  // Expects that the running task was not touched: it is not cancelled, and
  // its web client updates still reach the original update callback.
  void ExpectTaskKeepsRunning() {
    EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);
    histogram_tester_.ExpectBucketCount(
        "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
        GlicExperimentalTriggeringIncomingMessageResult::kTaskAlreadyRunning,
        1);
    testing::Mock::VerifyAndClearExpectations(&mock_glic_instance_);

    SendWebClientUpdate(mojom::ExperimentalTriggeringUpdateType::kWorklog);
    ExpectNextUpdate(TaskUpdate::State::kRunning);

    // After COMPLETE the web client ends its own actor task.
    EXPECT_CALL(mock_glic_instance_, CancelTask()).Times(0);
    SendWebClientUpdate(
        mojom::ExperimentalTriggeringUpdateType::kTerminalCompletion);
    ExpectNextUpdate(TaskUpdate::State::kComplete);
    ExpectSessionEnded();
  }

 private:
  // Expects that the session is over: the task expectations were met, the
  // handler is gone, later web client updates are dropped, and the next
  // trigger starts a new handler.
  void ExpectSessionOver() {
    EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
    testing::Mock::VerifyAndClearExpectations(&mock_glic_instance_);

    SendWebClientUpdate(mojom::ExperimentalTriggeringUpdateType::kWorklog);
    EXPECT_FALSE(updates_.IsReady());

    auto next_response = SendTrigger();
    ASSERT_TRUE(next_response.has_value());
    ASSERT_TRUE(next_response->task_metadata.has_value());
    EXPECT_EQ(next_response->task_metadata->sender_sequence_number, 0);
  }
};

TEST_F(GlicExperimentalTriggeringCoordinatorSyncTerminalTest,
       StopAfterActorTask_CancelsTaskOnce) {
  StartSessionWithActorTask();

  auto response = SendRequestWithCallback(
      CreateRequest(StopActuationRequest{.stop_reason = "STOPPED_BY_USER"}),
      updates_.GetRepeatingCallback());
  histogram_tester_.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kTerminalStopped, 1);
  ExpectStoppedAndSessionEnded(response);
}

// An actor task on a shared Glic instance that was not created after this
// handler's trigger belongs to another caller, so Stop must not cancel it.
TEST_F(GlicExperimentalTriggeringCoordinatorSyncTerminalTest,
       StopBeforeActorTask_DoesNotCancelTask) {
  EXPECT_CALL(mock_glic_instance_, CancelTask()).Times(0);
  StartSession();

  ExpectStoppedAndSessionEnded(SendRequestWithCallback(
      CreateRequest(StopActuationRequest{.stop_reason = "STOPPED_BY_USER"}),
      updates_.GetRepeatingCallback()));
}

// After COMPLETE, the handler stays in the map until its posted cleanup runs.
// A failing request in that window must not cancel the actor task, which the
// web client ends itself after COMPLETE. The parameter is true for a request
// that the coordinator rejects before OnRequest().
class GlicExperimentalTriggeringCoordinatorFailureAfterCompletionTest
    : public GlicExperimentalTriggeringCoordinatorSyncTerminalTest,
      public testing::WithParamInterface<bool> {};

TEST_P(GlicExperimentalTriggeringCoordinatorFailureAfterCompletionTest,
       DoesNotCancelTask) {
  const bool rejected_proto = GetParam();
  EXPECT_CALL(mock_glic_instance_, CancelTask()).Times(0);
  StartSession();
  CreateActorTask();
  SendWebClientUpdate(
      mojom::ExperimentalTriggeringUpdateType::kTerminalCompletion);
  ExpectNextUpdate(TaskUpdate::State::kComplete);
  ASSERT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  if (rejected_proto) {
    ExpectFailedTaskUpdate(SendProto(CreateVersionMismatchProto()),
                           "Rejected: version mismatch or unavailable.");
  } else {
    ExpectFailedTaskUpdate(
        SendRequestWithCallback(CreateRequest(RequestPayloadNotSet{}),
                                updates_.GetRepeatingCallback()),
        "Received GlicExperimentalTriggering message with no actionable "
        "request.");
  }
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
  // The outcome stays the one that COMPLETE recorded.
  histogram_tester_.ExpectBucketCount(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kTerminalFailed, 0);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GlicExperimentalTriggeringCoordinatorFailureAfterCompletionTest,
    testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "RejectedProto" : "OnRequest";
    });

// A refused first request started no actuation, so it records no outcome.
TEST_F(GlicExperimentalTriggeringCoordinatorSyncTerminalTest,
       RefusedFirstTrigger_RecordsNoOutcome) {
  OptOut();

  ExpectFailedTaskUpdate(SendTrigger(),
                         "User is not opted in to experimental triggering.");
  histogram_tester_.ExpectTotalCount(
      "Glic.ExperimentalTriggering.ExecutionOutcome", 0);
}

struct SyncFailureTestCase {
  enum class Setup {
    // A running session with an actor task.
    kMidTask,
    // A yielded session, then the user opts out.
    kYieldedThenOptOut,
    // A yielded session, then the browser window goes away.
    kYieldedThenNoBrowserWindow,
  };
  std::string test_name;
  Setup setup;
  ExperimentalTriggeringRequest::Payload payload;
  bool clear_task_metadata = false;
  std::string expected_error;
};

class GlicExperimentalTriggeringCoordinatorSyncFailureTest
    : public GlicExperimentalTriggeringCoordinatorSyncTerminalTest,
      public testing::WithParamInterface<SyncFailureTestCase> {};

TEST_P(GlicExperimentalTriggeringCoordinatorSyncFailureTest,
       StopsTaskAndEndsSession) {
  const SyncFailureTestCase& test_case = GetParam();
  switch (test_case.setup) {
    case SyncFailureTestCase::Setup::kMidTask:
      StartSessionWithActorTask();
      break;
    case SyncFailureTestCase::Setup::kYieldedThenOptOut:
      StartYieldedSession();
      OptOut();
      break;
    case SyncFailureTestCase::Setup::kYieldedThenNoBrowserWindow:
      StartYieldedSession();
      coordinator_->set_browser_window(nullptr);
      break;
  }

  ExperimentalTriggeringRequest request = CreateRequest(test_case.payload);
  if (test_case.clear_task_metadata) {
    request.task_metadata.reset();
  }
  auto response =
      SendRequestWithCallback(request, updates_.GetRepeatingCallback());
  // The server was told FAILED, so the outcome is a terminal failure, not a
  // teardown.
  histogram_tester_.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kTerminalFailed, 1);
  ExpectFailedAndSessionEnded(response, test_case.expected_error);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GlicExperimentalTriggeringCoordinatorSyncFailureTest,
    testing::Values(
        SyncFailureTestCase{
            .test_name = "RequestPayloadNotSetMidTask",
            .setup = SyncFailureTestCase::Setup::kMidTask,
            .payload = RequestPayloadNotSet{},
            .expected_error = "Received GlicExperimentalTriggering message "
                              "with no actionable request."},
        SyncFailureTestCase{
            .test_name = "MonostatePayloadMidTask",
            .setup = SyncFailureTestCase::Setup::kMidTask,
            .payload = std::monostate(),
            .expected_error = "Received GlicExperimentalTriggering message "
                              "with no actionable request."},
        // A start request without task metadata is malformed, not a
        // duplicate.
        SyncFailureTestCase{
            .test_name = "MissingTaskMetadataMidTask",
            .setup = SyncFailureTestCase::Setup::kMidTask,
            .payload = TriggerActuationRequest{.initial_prompt = "hello"},
            .clear_task_metadata = true,
            .expected_error = "Received GlicExperimentalTriggering message "
                              "with missing task metadata."},
        SyncFailureTestCase{
            .test_name = "ContinueNotOptedInAfterYield",
            .setup = SyncFailureTestCase::Setup::kYieldedThenOptOut,
            .payload =
                ContinueActuationRequest{.continuation_prompt = "keep going"},
            .expected_error =
                "User is not opted in to experimental triggering."},
        SyncFailureTestCase{
            .test_name = "ContinueNoBrowserWindowAfterYield",
            .setup = SyncFailureTestCase::Setup::kYieldedThenNoBrowserWindow,
            .payload =
                ContinueActuationRequest{.continuation_prompt = "keep going"},
            .expected_error = "No browser window found for current profile."}),
    [](const testing::TestParamInfo<SyncFailureTestCase>& info) {
      return info.param.test_name;
    });

// Malformed protos that OnProtoMessage() rejects before it converts them.
class GlicExperimentalTriggeringCoordinatorRejectedProtoTest
    : public GlicExperimentalTriggeringCoordinatorSyncTerminalTest,
      public testing::WithParamInterface<bool> {};

TEST_P(GlicExperimentalTriggeringCoordinatorRejectedProtoTest,
       StopsTaskAndEndsSession) {
  const bool version_mismatch = GetParam();
  StartSessionWithActorTask();

  components_sharing_message::GlicExperimentalTriggering proto;
  if (version_mismatch) {
    proto = CreateVersionMismatchProto();
  } else {
    proto.set_context_id(kTestContextId);
    proto.mutable_task_metadata()->set_conversation_id(kTestConversationId);
  }
  auto response = SendProto(proto);
  histogram_tester_.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kTerminalFailed, 1);
  // The reply continues the session's sequence numbers. Reply 0 was STARTING,
  // so a reply numbered 0 would be dropped as stale.
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_metadata.has_value());
  EXPECT_EQ(response->task_metadata->sender_sequence_number, 1);
  ExpectFailedAndSessionEnded(
      response, version_mismatch ? "Rejected: version mismatch or unavailable."
                                 : "Received GlicExperimentalTriggering "
                                   "message with no request payload.");
}

INSTANTIATE_TEST_SUITE_P(,
                         GlicExperimentalTriggeringCoordinatorRejectedProtoTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "VersionMismatch"
                                             : "MissingPayload";
                         });

// Known gap: before an actor task exists, there is nothing that Chrome can
// cancel, so the invocation keeps running after the session ends
// (b/582416328).
TEST_F(GlicExperimentalTriggeringCoordinatorSyncTerminalTest,
       FailureBeforeActorTask_EndsSessionWithoutCancellingTask) {
  EXPECT_CALL(mock_glic_instance_, CancelTask()).Times(0);
  StartSession();

  ExpectFailedTaskUpdate(
      SendRequestWithCallback(CreateRequest(RequestPayloadNotSet{}),
                              updates_.GetRepeatingCallback()),
      "Received GlicExperimentalTriggering message with no actionable "
      "request.");
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

// A duplicate start while the session runs is dropped with an empty reply,
// whether or not the invocation has created an actor task yet.
class GlicExperimentalTriggeringCoordinatorDuplicateStartTest
    : public GlicExperimentalTriggeringCoordinatorSyncTerminalTest,
      public testing::WithParamInterface<bool> {
 protected:
  void StartRunningSession() {
    EXPECT_CALL(mock_glic_instance_, CancelTask()).Times(0);
    StartSession();
    if (GetParam()) {
      CreateActorTask();
    }
  }
};

TEST_P(GlicExperimentalTriggeringCoordinatorDuplicateStartTest,
       DuplicateTrigger_DroppedAndKeepsTask) {
  StartRunningSession();

  // The duplicate brings its own update callback, which must not replace the
  // running task's.
  base::test::TestFuture<ExperimentalTriggeringResponse> duplicate_updates;
  EXPECT_FALSE(SendRequestWithCallback(CreateRequest(TriggerActuationRequest{
                                           .initial_prompt = "again"}),
                                       duplicate_updates.GetRepeatingCallback())
                   .has_value());

  ExpectTaskKeepsRunning();
  EXPECT_FALSE(duplicate_updates.IsReady());
}

TEST_P(GlicExperimentalTriggeringCoordinatorDuplicateStartTest,
       ExecuteActions_DroppedAndKeepsTask) {
  StartRunningSession();
  // With script tools off, ProcessExecuteActionsRequest() would reply FAILED,
  // and that terminal reply would end the running session. The duplicate
  // check must run first.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      features::kGlicExperimentalTriggeringScriptTools);

  base::test::TestFuture<ExperimentalTriggeringResponse> duplicate_updates;
  EXPECT_FALSE(SendRequestWithCallback(CreateExecuteScriptToolRequest(),
                                       duplicate_updates.GetRepeatingCallback())
                   .has_value());

  ExpectTaskKeepsRunning();
  EXPECT_FALSE(duplicate_updates.IsReady());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GlicExperimentalTriggeringCoordinatorDuplicateStartTest,
    testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "AfterActorTask" : "BeforeActorTask";
    });

// An invocation can still be pending without an instance:
// InvokeWithAutoSubmit() returned none, and neither callback has run yet. A
// start request in that window is a duplicate too.
TEST_F(GlicExperimentalTriggeringCoordinatorSyncTerminalTest,
       DuplicateStartWhileInvocationPendingWithoutInstance_Dropped) {
  base::OnceCallback<void(GlicInvokeError, std::string)> on_error;
  EXPECT_CALL(*mock_glic_service(),
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        on_error = options.TakeOnErrorWithDiagnostics(
            GlicApiTestPasskeys::GetErrorDiagnosticsPassKey());
        return base::WeakPtr<GlicInstance>();
      });
  StartSession();

  base::test::TestFuture<ExperimentalTriggeringResponse> duplicate_updates;
  EXPECT_FALSE(SendRequestWithCallback(CreateRequest(TriggerActuationRequest{
                                           .initial_prompt = "again"}),
                                       duplicate_updates.GetRepeatingCallback())
                   .has_value());
  EXPECT_FALSE(SendRequestWithCallback(CreateExecuteScriptToolRequest(),
                                       duplicate_updates.GetRepeatingCallback())
                   .has_value());
  histogram_tester_.ExpectBucketCount(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kTaskAlreadyRunning, 2);

  // The pending invocation still reports to the original update callback.
  ASSERT_TRUE(on_error);
  std::move(on_error).Run(GlicInvokeError::kTimeout, "");
  ExpectNextUpdate(TaskUpdate::State::kFailed);
  ExpectSessionEnded();
  EXPECT_FALSE(duplicate_updates.IsReady());
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ActuationTeardownAfterTerminalCompletion_DoesNotEmitDuplicateUpdate) {
  base::HistogramTester histogram_tester;

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  auto response = coordinator_->OnRequest(
      kTestContextId, request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);
  ASSERT_TRUE(response.has_value());

  auto complete_update = mojom::ExperimentalTriggeringUpdate::New();
  complete_update->type =
      mojom::ExperimentalTriggeringUpdateType::kTerminalCompletion;
  complete_update->data = "done";
  test_triggering_manager_.SendUpdate(
      std::move(complete_update), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();

  ASSERT_TRUE(update_future.IsReady());
  auto completion = update_future.Take();
  ASSERT_TRUE(completion.task_update.has_value());
  EXPECT_EQ(completion.task_update->state, TaskUpdate::State::kComplete);

  // Destroying coordinator after a terminal update was already sent must not
  // emit an extra kFailed update or overwrite the recorded outcome.
  coordinator_.reset();
  EXPECT_FALSE(update_future.IsReady());

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kSuccess, 1);
}

TEST_F(
    GlicExperimentalTriggeringCoordinatorWithTabTest,
    StopActuationDuringActiveSession_DoesNotEmitDuplicateFailedUpdateOnTeardown) {
  ExperimentalTriggeringRequest trigger_request;
  trigger_request.version = 1;
  trigger_request.context_id = kTestContextId;
  trigger_request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  trigger_request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  auto start_response = coordinator_->OnRequest(
      kTestContextId, trigger_request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);
  ASSERT_TRUE(start_response.has_value());
  ASSERT_TRUE(start_response->task_update.has_value());
  EXPECT_EQ(start_response->task_update->state, TaskUpdate::State::kStarting);

  ExperimentalTriggeringRequest stop_request;
  stop_request.version = 1;
  stop_request.context_id = kTestContextId;
  stop_request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  stop_request.payload = StopActuationRequest{.stop_reason = "user cancelled"};

  auto stop_response = coordinator_->OnRequest(
      kTestContextId, stop_request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);
  ASSERT_TRUE(stop_response.has_value());
  ASSERT_TRUE(stop_response->task_update.has_value());
  EXPECT_EQ(stop_response->task_update->state, TaskUpdate::State::kStopped);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
  EXPECT_FALSE(update_future.IsReady());
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
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.ExperimentalTriggering.IncomingMessageResult."
      "SharingMessage",
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
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.ExperimentalTriggering.IncomingMessageResult."
      "SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kNonScriptToolAction, 1);
}

// Fixture without the actor policy control exemption, so that the profile is
// not allowed to act on the web.
class GlicExperimentalTriggeringCoordinatorCannotActOnWebTest
    : public GlicExperimentalTriggeringCoordinatorWithTabTest {
 public:
  bool ActorPolicyControlExemption() const override { return false; }

 protected:
  std::optional<ExperimentalTriggeringResponse> SendProtoMessage(
      const components_sharing_message::GlicExperimentalTriggering& proto) {
    base::test::TestFuture<std::optional<ExperimentalTriggeringResponse>>
        response_future;
    coordinator_->OnProtoMessage(kTestContextId, proto,
                                 ScopedIncomingMessageResultLogger(
                                     GlicMetricReporter(profile_),
                                     ScopedIncomingMessageResultLogger::
                                         Channel::kBrowserActuatorTransport),
                                 base::DoNothing(),
                                 response_future.GetCallback());
    return response_future.Take();
  }
};

TEST_F(GlicExperimentalTriggeringCoordinatorCannotActOnWebTest,
       ExecuteActions_CannotActOnWeb) {
  // Experimental triggering is unavailable for profiles that cannot act on the
  // web, so the request is rejected before any action-specific checks.
  auto response = SendRequest(CreateExecuteScriptToolRequest());
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data,
            "User is not opted in to experimental triggering.");
  EXPECT_THAT(response->task_update->metadata,
              ElementsAre(Pair("chrome_error_code", "ET_CANNOT_ACT_ON_WEB")));
  histogram_tester_.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.StateOnActuationRequest",
      syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kUserNotOptedIn, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.ExperimentalTriggering.IncomingMessageResult."
      "SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kUserNotOptedIn, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorCannotActOnWebTest,
       TriggerActuation_CannotActOnWeb) {
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "hello"};

  // The request must fail immediately rather than invoking Glic, which would
  // be unable to actuate.
  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(response->task_update->data,
            "User is not opted in to experimental triggering.");
  EXPECT_THAT(response->task_update->metadata,
              ElementsAre(Pair("chrome_error_code", "ET_CANNOT_ACT_ON_WEB")));
  histogram_tester_.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.StateOnActuationRequest",
      syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable, 1);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorCannotActOnWebTest,
       OnProtoMessage_CannotActOnWeb_PopulatesErrorCodeForSupportedVersion) {
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.set_context_id(kTestContextId);
  proto.mutable_task_metadata()->set_sender_sequence_number(7);
  proto.set_glic_experimental_triggering_version(
      GlicEnabling::kExperimentalTriggeringProtocolVersion);
  proto.mutable_request()
      ->mutable_trigger_actuation_request()
      ->set_initial_prompt("hello");

  auto supported_version_response = SendProtoMessage(proto);
  ASSERT_TRUE(supported_version_response.has_value());
  ASSERT_TRUE(supported_version_response->task_update.has_value());
  EXPECT_EQ(supported_version_response->task_update->state,
            TaskUpdate::State::kFailed);
  EXPECT_EQ(supported_version_response->task_update->data,
            "Rejected: version mismatch or unavailable.");
  EXPECT_THAT(supported_version_response->task_update->metadata,
              ElementsAre(Pair("chrome_error_code", "ET_CANNOT_ACT_ON_WEB")));

  // An unsupported protocol version must still be treated as a version mismatch
  // and omit `ET_CANNOT_ACT_ON_WEB`.
  proto.set_glic_experimental_triggering_version(9999);
  auto unsupported_version_response = SendProtoMessage(proto);
  ASSERT_TRUE(unsupported_version_response.has_value());
  ASSERT_TRUE(unsupported_version_response->task_update.has_value());
  EXPECT_EQ(unsupported_version_response->task_update->state,
            TaskUpdate::State::kFailed);
  EXPECT_TRUE(unsupported_version_response->task_update->metadata.empty());
}

TEST_F(GlicExperimentalTriggeringCoordinatorCannotActOnWebTest,
       UnavailableForHigherPrecedenceGates_OmitsCannotActOnWebErrorCode) {
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.set_context_id(kTestContextId);
  proto.mutable_task_metadata()->set_sender_sequence_number(9);
  proto.set_glic_experimental_triggering_version(
      GlicEnabling::kExperimentalTriggeringProtocolVersion);
  proto.mutable_request()
      ->mutable_trigger_actuation_request()
      ->set_initial_prompt("hello");

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "hello"};

  // 1. Disabled by Spark policy on a managed profile (`kDisabledBySparkPolicy`)
  // while `CanActOnWeb` is also false must not misattribute the rejection as
  // `ET_CANNOT_ACT_ON_WEB`.
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(profile_),
      policy::EnterpriseManagementAuthority::CLOUD);
  profile_->GetPrefs()->SetInteger(
      glic::prefs::kGlicSparkPolicySettings,
      static_cast<int>(glic::prefs::GlicSparkPolicyState::kDisabled));

  auto spark_proto_response = SendProtoMessage(proto);
  ASSERT_TRUE(spark_proto_response.has_value());
  ASSERT_TRUE(spark_proto_response->task_update.has_value());
  EXPECT_TRUE(spark_proto_response->task_update->metadata.empty());

  auto spark_request_response = SendRequest(request);
  ASSERT_TRUE(spark_request_response.has_value());
  ASSERT_TRUE(spark_request_response->task_update.has_value());
  EXPECT_TRUE(spark_request_response->task_update->metadata.empty());

  // 2. Restore Spark policy, then disable `kGlicExperimentalTriggering`
  // (`kFeatureDisabled`).
  profile_->GetPrefs()->SetInteger(
      glic::prefs::kGlicSparkPolicySettings,
      static_cast<int>(glic::prefs::GlicSparkPolicyState::kEnabled));
  base::test::ScopedFeatureList disable_triggering;
  disable_triggering.InitAndDisableFeature(
      features::kGlicExperimentalTriggering);

  auto feature_proto_response = SendProtoMessage(proto);
  ASSERT_TRUE(feature_proto_response.has_value());
  ASSERT_TRUE(feature_proto_response->task_update.has_value());
  EXPECT_TRUE(feature_proto_response->task_update->metadata.empty());

  auto feature_request_response = SendRequest(request);
  ASSERT_TRUE(feature_request_response.has_value());
  ASSERT_TRUE(feature_request_response->task_update.has_value());
  EXPECT_TRUE(feature_request_response->task_update->metadata.empty());
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
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.ExperimentalTriggering.IncomingMessageResult."
      "SharingMessage",
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
       DuplicateStartWhileExecutingActions_DroppedAndKeepsExecution) {
  auto* user_data = optimization_guide::DocumentIdentifierUserData::
      GetOrCreateForCurrentDocument(web_contents_->GetPrimaryMainFrame());
  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .Times(0);
  const ExperimentalTriggeringRequest execute_request =
      CreateExecuteScriptToolRequest(mock_tab_.GetHandle().raw_value(),
                                     user_data->serialized_token());

  const struct {
    const char* name;
    ExperimentalTriggeringRequest request;
  } kDuplicates[] = {
      {"Trigger",
       CreateRequest(TriggerActuationRequest{.initial_prompt = "test"})},
      {"Continue",
       CreateRequest(ContinueActuationRequest{.continuation_prompt = "test"})},
      {"ExecuteActions", execute_request},
  };
  for (const auto& duplicate : kDuplicates) {
    SCOPED_TRACE(duplicate.name);
    base::HistogramTester histogram_tester;
    base::test::TestFuture<ExperimentalTriggeringResponse> first_updates;
    EXPECT_FALSE(SendRequestWithCallback(execute_request,
                                         first_updates.GetRepeatingCallback(),
                                         &mock_tab_)
                     .has_value());
    EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

    // The duplicate is dropped with an empty reply, before the execution
    // finishes.
    base::test::TestFuture<ExperimentalTriggeringResponse> duplicate_updates;
    EXPECT_FALSE(
        SendRequestWithCallback(duplicate.request,
                                duplicate_updates.GetRepeatingCallback())
            .has_value());
    histogram_tester.ExpectBucketCount(
        "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
        GlicExperimentalTriggeringIncomingMessageResult::kTaskAlreadyRunning,
        1);
    histogram_tester.ExpectBucketCount(
        "Glic.Consumer.ExperimentalTriggering.IncomingMessageResult."
        "SharingMessage",
        GlicExperimentalTriggeringIncomingMessageResult::kTaskAlreadyRunning,
        1);

    // The execution continues and reports its result to its own callback.
    ExperimentalTriggeringResponse result = first_updates.Take();
    EXPECT_TRUE(result.execute_actions_response.has_value());
    EXPECT_FALSE(duplicate_updates.IsReady());
    EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
  }
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ExecuteActions_Complete) {
  auto* user_data = optimization_guide::DocumentIdentifierUserData::
      GetOrCreateForCurrentDocument(web_contents_->GetPrimaryMainFrame());

  ExperimentalTriggeringRequest request = CreateExecuteScriptToolRequest(
      mock_tab_.GetHandle().raw_value(), user_data->serialized_token());

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  coordinator_->OnRequest(
      kTestContextId, request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
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
          GlicMetricReporter(profile_),
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
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.ExperimentalTriggering.IncomingMessageResult."
      "SharingMessage",
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
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.ExperimentalTriggering.IncomingMessageResult."
      "SharingMessage",
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
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.ExperimentalTriggering.IncomingMessageResult."
      "SharingMessage",
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
          GlicMetricReporter(profile_),
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

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       InvokeTimeout_PopulatesStageStartStateAndPhaseTimingsInMetadata) {
  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  base::OnceClosure on_panel_opened;
  base::OnceCallback<void(GlicInvokeError, std::string)>
      on_error_with_diagnostics;
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        on_panel_opened = std::move(options.on_panel_opened);
        on_error_with_diagnostics = options.TakeOnErrorWithDiagnostics(
            GlicApiTestPasskeys::GetErrorDiagnosticsPassKey());
        return mock_glic_instance_.GetWeakPtr();
      });

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  auto start_response = coordinator_->OnRequest(
      kTestContextId, request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);
  ASSERT_TRUE(start_response.has_value());
  ASSERT_TRUE(start_response->task_update.has_value());
  EXPECT_EQ(start_response->task_update->state, TaskUpdate::State::kStarting);
  EXPECT_TRUE(start_response->task_update->metadata.empty());

  task_environment_.FastForwardBy(base::Milliseconds(120));
  ASSERT_TRUE(on_panel_opened);
  std::move(on_panel_opened).Run();

  task_environment_.FastForwardBy(base::Milliseconds(380));
  ASSERT_TRUE(on_error_with_diagnostics);
  std::move(on_error_with_diagnostics)
      .Run(GlicInvokeError::kTimeout,
           "stage=WAIT_FOR_CLIENT_READY;load_state_at_start=LOADING");

  auto failure = update_future.Take();
  ASSERT_TRUE(failure.task_update.has_value());
  EXPECT_EQ(failure.task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(failure.task_update->data,
            "Glic invocation failed: Invocation timed out");
  EXPECT_THAT(
      failure.task_update->metadata,
      UnorderedElementsAre(
          Pair("chrome_error_code", "GLIC_INVOKE_TIMEOUT"),
          Pair("chrome_invoke_diagnostics",
               "stage=WAIT_FOR_CLIENT_READY;load_state_at_start=LOADING"),
          Pair("panel_opened", "1"), Pair("client_connected", "0"),
          Pair("t_to_panel_open_ms", "120"), Pair("t_to_terminal_ms", "500")));
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       InvokeClientLoadError_PopulatesLoadErrorReasonInMetadata) {
  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  base::OnceCallback<void(GlicInvokeError, std::string)>
      on_error_with_diagnostics;
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        on_error_with_diagnostics = options.TakeOnErrorWithDiagnostics(
            GlicApiTestPasskeys::GetErrorDiagnosticsPassKey());
        return mock_glic_instance_.GetWeakPtr();
      });

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  ASSERT_TRUE(
      coordinator_
          ->OnRequest(
              kTestContextId, request,
              ScopedIncomingMessageResultLogger(
                  GlicMetricReporter(profile_),
                  ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
              update_future.GetRepeatingCallback(), nullptr)
          .has_value());

  task_environment_.FastForwardBy(base::Milliseconds(75));
  ASSERT_TRUE(on_error_with_diagnostics);
  std::move(on_error_with_diagnostics)
      .Run(GlicInvokeError::kClientLoadError,
           "stage=WAIT_FOR_CLIENT_READY;load_state_at_start=ERROR;"
           "load_error=OFFLINE");

  auto failure = update_future.Take();
  ASSERT_TRUE(failure.task_update.has_value());
  EXPECT_EQ(failure.task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(failure.task_update->data,
            "Glic invocation failed: Client load error");
  EXPECT_THAT(failure.task_update->metadata,
              UnorderedElementsAre(
                  Pair("chrome_error_code", "GLIC_CLIENT_LOAD_ERROR"),
                  Pair("chrome_invoke_diagnostics",
                       "stage=WAIT_FOR_CLIENT_READY;load_state_at_start=ERROR;"
                       "load_error=OFFLINE"),
                  Pair("panel_opened", "0"), Pair("client_connected", "0"),
                  Pair("t_to_terminal_ms", "75")));
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       InvokeTabClosedPanelClosedAndUnlistedErrors_PopulateExpectedErrorCodes) {
  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));

  struct InvokeErrorResult {
    std::optional<ExperimentalTriggeringResponse> start_response;
    ExperimentalTriggeringResponse terminal_update;
  };
  auto run_invoke_error = [&](const std::string& context_id,
                              GlicInvokeError error,
                              const std::string& diagnostic_info) {
    EXPECT_CALL(*service,
                InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
        .WillOnce([this, error, diagnostic_info](
                      InvokeWithAutoSubmitPasskey passkey,
                      GlicInvokeOptions options,
                      GlicInvokeWithAutoSubmitOptions auto_submit_options) {
          options
              .TakeOnErrorWithDiagnostics(
                  GlicApiTestPasskeys::GetErrorDiagnosticsPassKey())
              .Run(error, diagnostic_info);
          return mock_glic_instance_.GetWeakPtr();
        });

    ExperimentalTriggeringRequest request;
    request.version = 1;
    request.context_id = context_id;
    request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
    request.payload = TriggerActuationRequest{.initial_prompt = "test"};

    base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
    auto start_response = coordinator_->OnRequest(
        context_id, request,
        ScopedIncomingMessageResultLogger(
            GlicMetricReporter(profile_),
            ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
        update_future.GetRepeatingCallback(), nullptr);
    return InvokeErrorResult{.start_response = std::move(start_response),
                             .terminal_update = update_future.Take()};
  };

  auto tab_closed =
      run_invoke_error("context_tab_closed", GlicInvokeError::kTabClosed,
                       "stage=WAIT_FOR_NAVIGATION");
  EXPECT_TRUE(tab_closed.start_response.has_value());
  ASSERT_TRUE(tab_closed.terminal_update.task_update.has_value());
  EXPECT_THAT(
      tab_closed.terminal_update.task_update->metadata,
      IsSupersetOf({
          Pair("chrome_error_code", "GLIC_TAB_CLOSED"),
          Pair("chrome_invoke_diagnostics", "stage=WAIT_FOR_NAVIGATION"),
      }));

  auto panel_closed =
      run_invoke_error("context_panel_closed", GlicInvokeError::kPanelClosed,
                       "stage=SHOW_INSTANCE");
  EXPECT_TRUE(panel_closed.start_response.has_value());
  ASSERT_TRUE(panel_closed.terminal_update.task_update.has_value());
  EXPECT_THAT(panel_closed.terminal_update.task_update->metadata,
              IsSupersetOf({
                  Pair("chrome_error_code", "GLIC_PANEL_CLOSED"),
                  Pair("chrome_invoke_diagnostics", "stage=SHOW_INSTANCE"),
              }));

  auto unlisted = run_invoke_error("context_unlisted",
                                   GlicInvokeError::kInvalidConfiguration,
                                   "stage=SEND_TO_CLIENT");
  EXPECT_TRUE(unlisted.start_response.has_value());
  ASSERT_TRUE(unlisted.terminal_update.task_update.has_value());
  EXPECT_FALSE(unlisted.terminal_update.task_update->metadata.contains(
      "chrome_error_code"));
  EXPECT_THAT(
      unlisted.terminal_update.task_update->metadata,
      Contains(Pair("chrome_invoke_diagnostics", "stage=SEND_TO_CLIENT")));

  auto unlisted_empty_diagnostics = run_invoke_error(
      "context_unlisted_empty", GlicInvokeError::kInvalidConfiguration, "");
  EXPECT_TRUE(unlisted_empty_diagnostics.start_response.has_value());
  ASSERT_TRUE(
      unlisted_empty_diagnostics.terminal_update.task_update.has_value());
  EXPECT_FALSE(
      unlisted_empty_diagnostics.terminal_update.task_update->metadata.contains(
          "chrome_error_code"));
  EXPECT_FALSE(
      unlisted_empty_diagnostics.terminal_update.task_update->metadata.contains(
          "chrome_invoke_diagnostics"));
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ActuationMojoDisconnect_PopulatesChromeErrorCodeAndTimings) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndDisableFeature(
      features::kGlicExperimentalTriggeringNavigationReconnectGrace);

  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  base::OnceClosure on_panel_opened;
  base::OnceCallback<void(base::WeakPtr<GlicInstance>)> on_client_connected;
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        on_panel_opened = std::move(options.on_panel_opened);
        on_client_connected = std::move(options.on_client_connected);
        return mock_glic_instance_.GetWeakPtr();
      });

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  ASSERT_TRUE(
      coordinator_
          ->OnRequest(
              kTestContextId, request,
              ScopedIncomingMessageResultLogger(
                  GlicMetricReporter(profile_),
                  ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
              update_future.GetRepeatingCallback(), nullptr)
          .has_value());

  task_environment_.FastForwardBy(base::Milliseconds(50));
  std::move(on_panel_opened).Run();

  task_environment_.FastForwardBy(base::Milliseconds(150));
  std::move(on_client_connected).Run(mock_glic_instance_.GetWeakPtr());

  task_environment_.FastForwardBy(base::Milliseconds(300));
  test_triggering_manager_.ResetHandler();

  auto failure = update_future.Take();
  ASSERT_TRUE(failure.task_update.has_value());
  EXPECT_EQ(failure.task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(failure.task_update->data, "Client disconnected unexpectedly.");
  EXPECT_THAT(failure.task_update->metadata,
              UnorderedElementsAre(
                  Pair("chrome_error_code", "GLIC_CLIENT_DISCONNECTED"),
                  Pair("panel_opened", "1"), Pair("client_connected", "1"),
                  Pair("t_to_panel_open_ms", "50"),
                  Pair("t_to_client_connected_ms", "200"),
                  Pair("t_to_terminal_ms", "500")));
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       WebClientTerminalUpdates_PopulatePhaseTimingsWithoutOverwritingData) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(features::kGlicStructuredYieldMetadata);

  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  base::OnceClosure fail_on_panel_opened;
  base::OnceCallback<void(base::WeakPtr<GlicInstance>)>
      fail_on_client_connected;
  base::OnceClosure complete_on_panel_opened;
  base::OnceCallback<void(base::WeakPtr<GlicInstance>)>
      complete_on_client_connected;
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        fail_on_panel_opened = std::move(options.on_panel_opened);
        fail_on_client_connected = std::move(options.on_client_connected);
        return mock_glic_instance_.GetWeakPtr();
      })
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        complete_on_panel_opened = std::move(options.on_panel_opened);
        complete_on_client_connected = std::move(options.on_client_connected);
        return mock_glic_instance_.GetWeakPtr();
      });

  // 1. WebUI terminal FAILED update preserves WebUI `data` and omits
  // `chrome_error_code` while populating phase timings.
  ExperimentalTriggeringRequest fail_request;
  fail_request.version = 1;
  fail_request.context_id = "context_webui_fail";
  fail_request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  fail_request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  base::test::TestFuture<ExperimentalTriggeringResponse> fail_future;
  ASSERT_TRUE(
      coordinator_
          ->OnRequest(
              "context_webui_fail", fail_request,
              ScopedIncomingMessageResultLogger(
                  GlicMetricReporter(profile_),
                  ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
              fail_future.GetRepeatingCallback(), nullptr)
          .has_value());

  task_environment_.FastForwardBy(base::Milliseconds(40));
  ASSERT_TRUE(fail_on_panel_opened);
  std::move(fail_on_panel_opened).Run();

  task_environment_.FastForwardBy(base::Milliseconds(160));
  ASSERT_TRUE(fail_on_client_connected);
  std::move(fail_on_client_connected).Run(mock_glic_instance_.GetWeakPtr());

  task_environment_.FastForwardBy(base::Milliseconds(250));
  auto worklog = mojom::ExperimentalTriggeringUpdate::New();
  worklog->type = mojom::ExperimentalTriggeringUpdateType::kWorklog;
  worklog->data = "step 1";
  test_triggering_manager_.SendUpdate(
      std::move(worklog), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();

  auto worklog_response = fail_future.Take();
  ASSERT_TRUE(worklog_response.task_update.has_value());
  EXPECT_EQ(worklog_response.task_update->state, TaskUpdate::State::kRunning);
  EXPECT_TRUE(worklog_response.task_update->metadata.empty());

  task_environment_.FastForwardBy(base::Milliseconds(350));
  auto terminal_fail = mojom::ExperimentalTriggeringUpdate::New();
  terminal_fail->type =
      mojom::ExperimentalTriggeringUpdateType::kTerminalFailed;
  terminal_fail->data = "RPC_DEADLINE_EXCEEDED";
  terminal_fail->metadata = base::flat_map<std::string, std::string>{
      {"gic_custom_key", "custom_val"}};
  test_triggering_manager_.SendUpdate(
      std::move(terminal_fail), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();

  auto fail_response = fail_future.Take();
  ASSERT_TRUE(fail_response.task_update.has_value());
  EXPECT_EQ(fail_response.task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(fail_response.task_update->data, "RPC_DEADLINE_EXCEEDED");
  EXPECT_FALSE(
      fail_response.task_update->metadata.contains("chrome_error_code"));
  EXPECT_THAT(fail_response.task_update->metadata,
              UnorderedElementsAre(Pair("gic_custom_key", "custom_val"),
                                   Pair("panel_opened", "1"),
                                   Pair("client_connected", "1"),
                                   Pair("t_to_panel_open_ms", "40"),
                                   Pair("t_to_client_connected_ms", "200"),
                                   Pair("t_to_first_response_ms", "450"),
                                   Pair("t_to_terminal_ms", "800")));

  // 2. WebUI terminal COMPLETE update also populates phase timings without
  // `chrome_error_code`.
  ExperimentalTriggeringRequest complete_request;
  complete_request.version = 1;
  complete_request.context_id = "context_webui_complete";
  complete_request.task_metadata = TaskMetadata{.conversation_id = "conv_456"};
  complete_request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  base::test::TestFuture<ExperimentalTriggeringResponse> complete_future;
  ASSERT_TRUE(
      coordinator_
          ->OnRequest(
              "context_webui_complete", complete_request,
              ScopedIncomingMessageResultLogger(
                  GlicMetricReporter(profile_),
                  ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
              complete_future.GetRepeatingCallback(), nullptr)
          .has_value());

  task_environment_.FastForwardBy(base::Milliseconds(30));
  ASSERT_TRUE(complete_on_panel_opened);
  std::move(complete_on_panel_opened).Run();
  task_environment_.FastForwardBy(base::Milliseconds(70));
  ASSERT_TRUE(complete_on_client_connected);
  std::move(complete_on_client_connected).Run(mock_glic_instance_.GetWeakPtr());
  task_environment_.FastForwardBy(base::Milliseconds(400));

  auto terminal_complete = mojom::ExperimentalTriggeringUpdate::New();
  terminal_complete->type =
      mojom::ExperimentalTriggeringUpdateType::kTerminalCompletion;
  terminal_complete->data = "final answer";
  test_triggering_manager_.SendUpdate(
      std::move(terminal_complete), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();

  auto complete_response = complete_future.Take();
  ASSERT_TRUE(complete_response.task_update.has_value());
  EXPECT_EQ(complete_response.task_update->state, TaskUpdate::State::kComplete);
  EXPECT_EQ(complete_response.task_update->data, "final answer");
  EXPECT_THAT(complete_response.task_update->metadata,
              UnorderedElementsAre(Pair("panel_opened", "1"),
                                   Pair("client_connected", "1"),
                                   Pair("t_to_panel_open_ms", "30"),
                                   Pair("t_to_client_connected_ms", "100"),
                                   Pair("t_to_first_response_ms", "500"),
                                   Pair("t_to_terminal_ms", "500")));
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       WebClientUpdates_StripReservedChromeDiagnosticMetadataKeys) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(features::kGlicStructuredYieldMetadata);

  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  base::OnceCallback<void(base::WeakPtr<GlicInstance>)> on_client_connected;
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        // Leave `on_panel_opened` uninvoked so `panel_opened` stays false and
        // `t_to_panel_open_ms` is unreached.
        on_client_connected = std::move(options.on_client_connected);
        return mock_glic_instance_.GetWeakPtr();
      });

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  ASSERT_TRUE(
      coordinator_
          ->OnRequest(
              kTestContextId, request,
              ScopedIncomingMessageResultLogger(
                  GlicMetricReporter(profile_),
                  ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
              update_future.GetRepeatingCallback(), nullptr)
          .has_value());

  task_environment_.FastForwardBy(base::Milliseconds(100));
  ASSERT_TRUE(on_client_connected);
  std::move(on_client_connected).Run(mock_glic_instance_.GetWeakPtr());

  task_environment_.FastForwardBy(base::Milliseconds(150));
  auto worklog = mojom::ExperimentalTriggeringUpdate::New();
  worklog->type = mojom::ExperimentalTriggeringUpdateType::kWorklog;
  worklog->data = "step 1";
  worklog->metadata = base::flat_map<std::string, std::string>{
      {"chrome_error_code", "FORGED_ERROR"},
      {"chrome_invoke_diagnostics", "forged_diag"},
      {"panel_opened", "1"},
      {"client_connected", "0"},
      {"t_to_panel_open_ms", "9999"},
      {"t_to_custom_ms", "8888"},
      {"gic_custom_key", "worklog_val"},
  };
  test_triggering_manager_.SendUpdate(
      std::move(worklog), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();

  auto worklog_response = update_future.Take();
  ASSERT_TRUE(worklog_response.task_update.has_value());
  EXPECT_EQ(worklog_response.task_update->state, TaskUpdate::State::kRunning);
  EXPECT_THAT(worklog_response.task_update->metadata,
              UnorderedElementsAre(Pair("gic_custom_key", "worklog_val")));

  task_environment_.FastForwardBy(base::Milliseconds(50));
  auto terminal_fail = mojom::ExperimentalTriggeringUpdate::New();
  terminal_fail->type =
      mojom::ExperimentalTriggeringUpdateType::kTerminalFailed;
  terminal_fail->data = "WEB_CLIENT_ERROR";
  terminal_fail->metadata = base::flat_map<std::string, std::string>{
      {"chrome_error_code", "FORGED_ERROR"},
      {"chrome_invoke_diagnostics", "forged_diag"},
      {"panel_opened", "1"},
      {"client_connected", "0"},
      {"t_to_panel_open_ms", "9999"},
      {"t_to_client_connected_ms", "9999"},
      {"t_to_first_response_ms", "9999"},
      {"t_to_terminal_ms", "9999"},
      {"gic_custom_key", "terminal_val"},
  };
  test_triggering_manager_.SendUpdate(
      std::move(terminal_fail), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();

  auto fail_response = update_future.Take();
  ASSERT_TRUE(fail_response.task_update.has_value());
  EXPECT_EQ(fail_response.task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(fail_response.task_update->data, "WEB_CLIENT_ERROR");
  EXPECT_THAT(fail_response.task_update->metadata,
              UnorderedElementsAre(Pair("gic_custom_key", "terminal_val"),
                                   Pair("panel_opened", "0"),
                                   Pair("client_connected", "1"),
                                   Pair("t_to_client_connected_ms", "100"),
                                   Pair("t_to_first_response_ms", "250"),
                                   Pair("t_to_terminal_ms", "300")));
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       StopActuationWithoutInstanceAfterStart_PopulatesDiagnosticMetadata) {
  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  base::OnceCallback<void(GlicInvokeError, std::string)> pending_on_error;
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        pending_on_error = options.TakeOnErrorWithDiagnostics(
            GlicApiTestPasskeys::GetErrorDiagnosticsPassKey());
        return nullptr;
      });

  ExperimentalTriggeringRequest trigger_request;
  trigger_request.version = 1;
  trigger_request.context_id = kTestContextId;
  trigger_request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  trigger_request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  ASSERT_TRUE(SendRequest(trigger_request).has_value());
  ASSERT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  task_environment_.FastForwardBy(base::Milliseconds(90));

  ExperimentalTriggeringRequest stop_request;
  stop_request.version = 1;
  stop_request.context_id = kTestContextId;
  stop_request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  stop_request.payload = StopActuationRequest{.stop_reason = "STOPPED_BY_USER"};

  auto stop_response = SendRequest(stop_request);
  ASSERT_TRUE(stop_response.has_value());
  ASSERT_TRUE(stop_response->task_update.has_value());
  EXPECT_EQ(stop_response->task_update->state, TaskUpdate::State::kFailed);
  EXPECT_THAT(stop_response->task_update->metadata,
              UnorderedElementsAre(Pair("panel_opened", "0"),
                                   Pair("client_connected", "0"),
                                   Pair("t_to_terminal_ms", "90")));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       PreparedTabShowsPanelOnlyWhenChromeInForeground) {
  auto* glic_service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/false));
  ASSERT_TRUE(glic_service);

  std::optional<bool> captured_show_panel;
  EXPECT_CALL(*glic_service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillRepeatedly([this, &captured_show_panel](
                          InvokeWithAutoSubmitPasskey passkey,
                          GlicInvokeOptions options,
                          GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        captured_show_panel = auto_submit_options.show_panel;
        if (auto_submit_options.show_panel && options.on_panel_opened) {
          std::move(options.on_panel_opened).Run();
        }
        if (options.on_client_connected) {
          std::move(options.on_client_connected)
              .Run(mock_glic_instance_.GetWeakPtr());
        }
        return mock_glic_instance_.GetWeakPtr();
      });

  // Background: show_panel should be false for prepared tab.
  coordinator_->set_is_chrome_in_background(true);
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_1"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  auto response = coordinator_->OnRequest(
      kTestContextId, request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), &mock_tab_);
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(captured_show_panel, false);

  // Foreground: show_panel should be true for prepared tab.
  coordinator_->set_is_chrome_in_background(false);
  request.context_id = "context_2";
  request.task_metadata = TaskMetadata{.conversation_id = "conv_2"};
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future2;
  auto response2 = coordinator_->OnRequest(
      "context_2", request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future2.GetRepeatingCallback(), &mock_tab_);
  ASSERT_TRUE(response2.has_value());
  EXPECT_EQ(captured_show_panel, true);

  // Continuations without a prepared tab follow the same rule.
  request.payload = ContinueActuationRequest{.continuation_prompt = "continue"};

  // Background with a browser window: show_panel should be false.
  coordinator_->set_is_chrome_in_background(true);
  captured_show_panel.reset();
  request.context_id = "context_5";
  request.task_metadata = TaskMetadata{.conversation_id = "conv_5"};
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future5;
  auto response5 = coordinator_->OnRequest(
      "context_5", request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future5.GetRepeatingCallback(), /*prepared_tab=*/nullptr);
  ASSERT_TRUE(response5.has_value());
  ASSERT_TRUE(response5->task_update.has_value());
  EXPECT_EQ(response5->task_update->state, TaskUpdate::State::kStarting);
  EXPECT_EQ(captured_show_panel, false);

  // Without a browser window, for a conversation that still has a surface.
  coordinator_->set_browser_window(nullptr);
  EXPECT_CALL(glic_service->mock_coordinator(),
              GetInstanceForConversationId(testing::_))
      .WillRepeatedly(testing::Return(&mock_glic_instance_));
  EXPECT_CALL(mock_glic_instance_, HasLiveSurface())
      .WillRepeatedly(testing::Return(true));

  // Background: show_panel should be false for a continuation.
  coordinator_->set_is_chrome_in_background(true);
  captured_show_panel.reset();
  request.context_id = "context_3";
  request.task_metadata = TaskMetadata{.conversation_id = "conv_3"};
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future3;
  auto response3 = coordinator_->OnRequest(
      "context_3", request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future3.GetRepeatingCallback(), /*prepared_tab=*/nullptr);
  ASSERT_TRUE(response3.has_value());
  ASSERT_TRUE(response3->task_update.has_value());
  EXPECT_EQ(response3->task_update->state, TaskUpdate::State::kStarting);
  EXPECT_EQ(captured_show_panel, false);

  // Foreground: show_panel should be true for a continuation.
  coordinator_->set_is_chrome_in_background(false);
  captured_show_panel.reset();
  request.context_id = "context_4";
  request.task_metadata = TaskMetadata{.conversation_id = "conv_4"};
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future4;
  auto response4 = coordinator_->OnRequest(
      "context_4", request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future4.GetRepeatingCallback(), /*prepared_tab=*/nullptr);
  ASSERT_TRUE(response4.has_value());
  ASSERT_TRUE(response4->task_update.has_value());
  EXPECT_EQ(response4->task_update->state, TaskUpdate::State::kStarting);
  EXPECT_EQ(captured_show_panel, true);
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       DeviceOptInRequestFailsWhenChromeInBackground) {
  OptOut();
  coordinator_->set_is_chrome_in_background(true);

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.sender_sequence_number = 42};
  request.payload = DeviceOptInRequest{.triggering_source = "Android"};

  base::HistogramTester histogram_tester;
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  auto response = coordinator_->OnRequest(
      kTestContextId, request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);

  // The opt-in UI can't be shown, so the reply is synchronous.
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->context_id, kTestContextId);
  ASSERT_TRUE(response->device_opt_in_result.has_value());
  EXPECT_EQ(*response->device_opt_in_result, DeviceOptInResult::kFailed);
  EXPECT_FALSE(response->task_update.has_value());
  ASSERT_TRUE(response->task_metadata.has_value());
  EXPECT_EQ(response->task_metadata->last_seen_sequence_number, 42);
  EXPECT_FALSE(update_future.IsReady());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kAndroidOptInUnsupported,
      1);
}

class GlicExperimentalTriggeringCoordinatorBackgroundOptInTest
    : public GlicExperimentalTriggeringCoordinatorWithTabTest {
 public:
  void SetUp() override {
    GlicExperimentalTriggeringCoordinatorWithTabTest::SetUp();
    background_opt_in_feature_.InitAndEnableFeature(
        features::kGlicExperimentalTriggeringBackgroundOptInNotification);
    BackgroundOptInNotifier::Get().SetCallbacksForTesting(
        base::BindLambdaForTesting([this]() {
          if (!can_show_background_opt_in_notification_) {
            return false;
          }
          is_background_opt_in_notification_showing_ = true;
          return true;
        }),
        base::BindLambdaForTesting(
            [this]() { is_background_opt_in_notification_showing_ = false; }));
    OptOut();
    coordinator_->set_is_chrome_in_background(true);
  }

  void TearDown() override {
    GlicExperimentalTriggeringCoordinatorWithTabTest::TearDown();
    BackgroundOptInNotifier::Get().SetCallbacksForTesting({}, {});
  }

  std::optional<ExperimentalTriggeringResponse> SendOptInRequest(
      base::test::TestFuture<ExperimentalTriggeringResponse>& update_future,
      std::string_view context_id = kTestContextId) {
    ExperimentalTriggeringRequest request =
        CreateRequest(DeviceOptInRequest{.triggering_source = "Android"});
    request.context_id = std::string(context_id);
    request.task_metadata = TaskMetadata{.sender_sequence_number = 42};
    return coordinator_->OnRequest(
        std::string(context_id), request,
        ScopedIncomingMessageResultLogger(
            GlicMetricReporter(profile_),
            ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
        update_future.GetRepeatingCallback(), nullptr);
  }

  void ForegroundChrome() {
    coordinator_->set_is_chrome_in_background(false);
    BackgroundOptInNotifier::Get().ScheduleForegroundNotification();
  }

  void set_can_show_background_opt_in_notification(bool can_show) {
    can_show_background_opt_in_notification_ = can_show;
  }

  bool is_background_opt_in_notification_showing() const {
    return is_background_opt_in_notification_showing_;
  }

 private:
  base::test::ScopedFeatureList background_opt_in_feature_;
  bool can_show_background_opt_in_notification_ = true;
  bool is_background_opt_in_notification_showing_ = false;
};

TEST_F(GlicExperimentalTriggeringCoordinatorBackgroundOptInTest,
       ShowsNotificationAndDefersDialogUntilForegrounded) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  EXPECT_FALSE(SendOptInRequest(update_future).has_value());
  EXPECT_TRUE(is_background_opt_in_notification_showing());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::
          kDeferredForBackgroundOptIn,
      1);

  ForegroundChrome();

  // In unit tests without a WindowAndroid, GlicExperimentalOptInUIHostAndroid
  // dismisses the dialog when ShowDialog() is reached.
  auto result = update_future.Take();
  EXPECT_FALSE(is_background_opt_in_notification_showing());
  EXPECT_EQ(result.device_opt_in_result, DeviceOptInResult::kDeclined);
  EXPECT_EQ(result.task_metadata->last_seen_sequence_number, 42);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorBackgroundOptInTest,
       AlreadyOptedInReturnsAcceptedImmediatelyWithoutShowingNotification) {
  OptIn();

  base::HistogramTester histogram_tester;
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  auto response = SendOptInRequest(update_future);

  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->device_opt_in_result, DeviceOptInResult::kAccepted);
  EXPECT_EQ(response->task_metadata->last_seen_sequence_number, 42);
  EXPECT_FALSE(is_background_opt_in_notification_showing());
  EXPECT_FALSE(update_future.IsReady());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::kSuccess, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorBackgroundOptInTest,
       TimesOutWhenUserNeverForegroundsChrome) {
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  EXPECT_FALSE(SendOptInRequest(update_future).has_value());
  EXPECT_TRUE(is_background_opt_in_notification_showing());

  task_environment_.FastForwardBy(
      features::kGlicExperimentalTriggeringBackgroundOptInTimeout.Get());

  EXPECT_FALSE(is_background_opt_in_notification_showing());
  EXPECT_EQ(update_future.Take().device_opt_in_result,
            DeviceOptInResult::kFailed);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorBackgroundOptInTest,
       CancelledStopsTimerAndFailsImmediately) {
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  EXPECT_FALSE(SendOptInRequest(update_future).has_value());
  EXPECT_TRUE(is_background_opt_in_notification_showing());

  BackgroundOptInNotifier::Get().Notify(
      BackgroundOptInNotifier::Event::kCancelled);

  EXPECT_FALSE(is_background_opt_in_notification_showing());
  auto result = update_future.Take();
  EXPECT_EQ(result.device_opt_in_result, DeviceOptInResult::kFailed);
  EXPECT_EQ(result.task_metadata->last_seen_sequence_number, 42);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorBackgroundOptInTest,
       FailsImmediatelyWhenNotificationCannotBeShown) {
  set_can_show_background_opt_in_notification(false);

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  auto response = SendOptInRequest(update_future);

  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->device_opt_in_result, DeviceOptInResult::kFailed);
  EXPECT_FALSE(update_future.IsReady());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorBackgroundOptInTest,
       MultipleRequestsKeepNotificationUntilAllResolved) {
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future1;
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future2;
  EXPECT_FALSE(SendOptInRequest(update_future1, "context_1").has_value());
  EXPECT_FALSE(SendOptInRequest(update_future2, "context_2").has_value());
  EXPECT_TRUE(is_background_opt_in_notification_showing());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 2u);

  ExperimentalTriggeringRequest stop_request =
      CreateRequest(StopActuationRequest{});
  stop_request.context_id = "context_1";
  coordinator_->OnRequest(
      "context_1", stop_request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future1.GetRepeatingCallback(), nullptr);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);
  EXPECT_TRUE(is_background_opt_in_notification_showing());

  ForegroundChrome();

  EXPECT_EQ(update_future2.Take().device_opt_in_result,
            DeviceOptInResult::kDeclined);
  EXPECT_FALSE(is_background_opt_in_notification_showing());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorBackgroundOptInTest,
       MultipleCoordinatorsShareProcessWideNotificationState) {
  auto second_coordinator =
      std::make_unique<TestGlicExperimentalTriggeringCoordinator>(profile_);
  second_coordinator->set_is_chrome_in_background(true);

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future1;
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future2;
  EXPECT_FALSE(SendOptInRequest(update_future1, "context_1").has_value());

  ExperimentalTriggeringRequest request2 =
      CreateRequest(DeviceOptInRequest{.triggering_source = "Android"});
  request2.context_id = "context_2";
  request2.task_metadata = TaskMetadata{.sender_sequence_number = 42};
  EXPECT_FALSE(
      second_coordinator
          ->OnRequest(
              "context_2", request2,
              ScopedIncomingMessageResultLogger(
                  GlicMetricReporter(profile_),
                  ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
              update_future2.GetRepeatingCallback(), nullptr)
          .has_value());
  EXPECT_TRUE(is_background_opt_in_notification_showing());

  second_coordinator.reset();
  EXPECT_TRUE(is_background_opt_in_notification_showing());

  coordinator_.reset();
  EXPECT_FALSE(is_background_opt_in_notification_showing());
}

TEST_F(GlicExperimentalTriggeringCoordinatorBackgroundOptInTest,
       AutoAcceptsWithoutCreatingTabWhenOptInBecomesNotNeeded) {
  coordinator_->set_active_tab(nullptr);

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  EXPECT_FALSE(SendOptInRequest(update_future).has_value());
  EXPECT_TRUE(is_background_opt_in_notification_showing());

  OptIn();
  ForegroundChrome();

  EXPECT_EQ(update_future.Take().device_opt_in_result,
            DeviceOptInResult::kAccepted);
  EXPECT_FALSE(is_background_opt_in_notification_showing());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorBackgroundOptInTest,
       FailsWhenNoWebContentsAvailableUponForegrounding) {
  base::test::ScopedFeatureList disable_tab_focus;
  disable_tab_focus.InitAndDisableFeature(
      features::kGlicExperimentalTriggeringOptInTabFocus);
  coordinator_->set_active_tab(nullptr);

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  EXPECT_FALSE(SendOptInRequest(update_future).has_value());
  EXPECT_TRUE(is_background_opt_in_notification_showing());

  ForegroundChrome();

  EXPECT_EQ(update_future.Take().device_opt_in_result,
            DeviceOptInResult::kFailed);
  EXPECT_FALSE(is_background_opt_in_notification_showing());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorBackgroundOptInTest,
       ResentRequestInForegroundStopsBackgroundWaitAndTimer) {
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  EXPECT_FALSE(SendOptInRequest(update_future).has_value());
  EXPECT_TRUE(is_background_opt_in_notification_showing());

  coordinator_->set_is_chrome_in_background(false);
  EXPECT_FALSE(SendOptInRequest(update_future).has_value());
  EXPECT_FALSE(is_background_opt_in_notification_showing());
  EXPECT_EQ(update_future.Take().device_opt_in_result,
            DeviceOptInResult::kDeclined);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);

  task_environment_.FastForwardBy(
      features::kGlicExperimentalTriggeringBackgroundOptInTimeout.Get());
  EXPECT_FALSE(update_future.IsReady());
}

TEST_F(GlicExperimentalTriggeringCoordinatorBackgroundOptInTest,
       ResentRequestWhenOptInNotNeededStopsBackgroundWaitAndTimer) {
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  EXPECT_FALSE(SendOptInRequest(update_future).has_value());
  EXPECT_TRUE(is_background_opt_in_notification_showing());

  OptIn();
  auto response = SendOptInRequest(update_future);
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->device_opt_in_result, DeviceOptInResult::kAccepted);
  EXPECT_FALSE(is_background_opt_in_notification_showing());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);

  task_environment_.FastForwardBy(
      features::kGlicExperimentalTriggeringBackgroundOptInTimeout.Get());
  EXPECT_FALSE(update_future.IsReady());
}

TEST_F(GlicExperimentalTriggeringCoordinatorBackgroundOptInTest,
       ResentRequestWhileForegroundDialogPendingAndBackgroundedIsCoalesced) {
  base::test::ScopedFeatureList disable_tab_focus;
  disable_tab_focus.InitAndDisableFeature(
      features::kGlicExperimentalTriggeringOptInTabFocus);
  OptIn();
  coordinator_->set_is_chrome_in_background(false);

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  EXPECT_FALSE(SendOptInRequest(update_future).has_value());
  EXPECT_FALSE(is_background_opt_in_notification_showing());
  ASSERT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  coordinator_->set_is_chrome_in_background(true);
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(SendOptInRequest(update_future).has_value());
  EXPECT_FALSE(is_background_opt_in_notification_showing());
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.IncomingMessageResult.SharingMessage",
      GlicExperimentalTriggeringIncomingMessageResult::
          kOptInDialogAlreadyShowing,
      1);

  ASSERT_TRUE(update_future.Wait());
  EXPECT_EQ(update_future.Take().device_opt_in_result,
            DeviceOptInResult::kAccepted);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorBackgroundOptInTest,
       ActuationRequestStopsBackgroundOptInWaitAndTimer) {
  auto* glic_service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/false));
  ASSERT_TRUE(glic_service);
  EXPECT_CALL(*glic_service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(mock_glic_instance_.GetWeakPtr()));

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  EXPECT_FALSE(SendOptInRequest(update_future).has_value());
  EXPECT_TRUE(is_background_opt_in_notification_showing());

  OptIn();
  ExperimentalTriggeringRequest trigger_request =
      CreateRequest(TriggerActuationRequest{.initial_prompt = "act"});
  auto response = coordinator_->OnRequest(
      kTestContextId, trigger_request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), &mock_tab_);
  ASSERT_TRUE(response.has_value());
  EXPECT_FALSE(is_background_opt_in_notification_showing());

  task_environment_.FastForwardBy(
      features::kGlicExperimentalTriggeringBackgroundOptInTimeout.Get());
  EXPECT_FALSE(update_future.IsReady());
}

TEST_F(GlicExperimentalTriggeringCoordinatorBackgroundOptInTest,
       IgnoresNonBrowserActivityUntilBrowserWindowBecomesVisible) {
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  EXPECT_FALSE(SendOptInRequest(update_future).has_value());
  EXPECT_TRUE(is_background_opt_in_notification_showing());

  // A translucent notification trampoline activity starts while the browser
  // window is still in the background.
  BackgroundOptInNotifier::Get().Notify(
      BackgroundOptInNotifier::Event::kForegrounded);
  EXPECT_TRUE(is_background_opt_in_notification_showing());
  EXPECT_FALSE(update_future.IsReady());

  // The browser activity now becomes visible and starts.
  ForegroundChrome();

  auto result = update_future.Take();
  EXPECT_FALSE(is_background_opt_in_notification_showing());
  EXPECT_EQ(result.device_opt_in_result, DeviceOptInResult::kDeclined);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorBackgroundOptInTest,
       ColdStartBrowserWindowCreatedAfterActivityResumeShowsDeferredDialog) {
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  EXPECT_FALSE(SendOptInRequest(update_future).has_value());
  EXPECT_TRUE(is_background_opt_in_notification_showing());

  // Activity.onResume() fires during cold start before post-native startup
  // creates the browser window and initializes the active TabModel.
  BackgroundOptInNotifier::Get().Notify(
      BackgroundOptInNotifier::Event::kForegrounded);
  EXPECT_TRUE(is_background_opt_in_notification_showing());
  EXPECT_FALSE(update_future.IsReady());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  // Post-native startup finishes creating the browser window and active tab
  // without firing another ActivityState.RESUMED transition.
  coordinator_->set_is_chrome_in_background(false);
  JNIEnv* env = base::android::AttachCurrentThread();
  const int64_t browser_ptr = reinterpret_cast<int64_t>(&mock_browser_window_);
  GlobalBrowserCollection::GetInstance()
      ->GetPlatformDelegate()
      ->OnBrowserCreated(env, browser_ptr);
  base::ScopedClosureRunner close_browser(base::BindOnce(
      [](JNIEnv* env, int64_t browser_ptr) {
        GlobalBrowserCollection::GetInstance()
            ->GetPlatformDelegate()
            ->OnBrowserClosed(env, browser_ptr);
      },
      env, browser_ptr));

  auto result = update_future.Take();
  EXPECT_FALSE(is_background_opt_in_notification_showing());
  EXPECT_EQ(result.device_opt_in_result, DeviceOptInResult::kDeclined);
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
}

TEST_F(GlicExperimentalTriggeringCoordinatorBackgroundOptInTest,
       ResentRequestSyncFailureStopsBackgroundWaitAndCleansUpHandler) {
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  EXPECT_FALSE(SendOptInRequest(update_future).has_value());
  EXPECT_TRUE(is_background_opt_in_notification_showing());
  ASSERT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
      profile_, BrowserContextKeyedServiceFactory::TestingFactory());

  auto response = SendOptInRequest(update_future);
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->device_opt_in_result, DeviceOptInResult::kFailed);
  EXPECT_FALSE(is_background_opt_in_notification_showing());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);

  task_environment_.FastForwardBy(
      features::kGlicExperimentalTriggeringBackgroundOptInTimeout.Get());
  EXPECT_FALSE(update_future.IsReady());
}
#endif

class FakeExperimentalTriggeringClient
    : public mojom::ExperimentalTriggeringClient {
 public:
  FakeExperimentalTriggeringClient() = default;
  ~FakeExperimentalTriggeringClient() override = default;

  mojo::PendingRemote<mojom::ExperimentalTriggeringClient> BindNewPipe() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void DisconnectWithoutReplying() {
    receiver_.reset();
    pending_updates_callback_.Reset();
  }

  void GetExperimentalTriggeringUpdates(
      mojo::PendingRemote<mojom::ExperimentalTriggeringUpdatesHandler> handler,
      GetExperimentalTriggeringUpdatesCallback callback) override {
    if (hold_updates_reply_) {
      pending_handler_ = std::move(handler);
      pending_updates_callback_ = std::move(callback);
      return;
    }
    std::move(callback).Run(true);
  }

  void UploadEncryptedScreenshot(
      mojom::ScreenshotPtr screenshot,
      UploadEncryptedScreenshotCallback callback) override {
    std::move(callback).Run(std::nullopt);
  }

  void SubmitExperimentalTriggeringConfirmationResponse(
      const std::vector<uint8_t>& response,
      SubmitExperimentalTriggeringConfirmationResponseCallback callback)
      override {
    std::move(callback).Run(false);
  }

  void set_hold_updates_reply(bool hold) { hold_updates_reply_ = hold; }
  bool has_held_updates_reply() const {
    return !pending_updates_callback_.is_null();
  }

 private:
  bool hold_updates_reply_ = false;
  mojo::PendingRemote<mojom::ExperimentalTriggeringUpdatesHandler>
      pending_handler_;
  GetExperimentalTriggeringUpdatesCallback pending_updates_callback_;
  mojo::Receiver<mojom::ExperimentalTriggeringClient> receiver_{this};
};

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       ManagerClientDisconnect_ResetsRemoteAndRunsPendingUpdatesCallbackFalse) {
  GlicExperimentalTriggeringManager manager(nullptr, nullptr);
  EXPECT_FALSE(manager.IsClientBound());

  int bound_notifications = 0;
  auto subscription = manager.AddClientBoundCallback(
      base::BindLambdaForTesting([&]() { ++bound_notifications; }));

  FakeExperimentalTriggeringClient fake_client;
  fake_client.set_hold_updates_reply(true);
  manager.Bind(fake_client.BindNewPipe());
  EXPECT_TRUE(manager.IsClientBound());
  EXPECT_EQ(bound_notifications, 1);

  mojo::PendingRemote<mojom::ExperimentalTriggeringUpdatesHandler> remote;
  std::ignore = remote.InitWithNewPipeAndPassReceiver();
  base::test::TestFuture<bool> registration_future;
  manager.GetExperimentalTriggeringUpdates(std::move(remote),
                                           registration_future.GetCallback());

  // Wait until the Mojo request arrives at fake_client, then drop the pipe
  // without replying (simulating a mid-handshake <webview> reload).
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_client.has_held_updates_reply(); }));
  fake_client.DisconnectWithoutReplying();

  // The pending reply is failed and the remote reset within the same
  // disconnect notification, so once the callback has run the remote is gone.
  EXPECT_FALSE(registration_future.Get());
  EXPECT_FALSE(manager.IsClientBound());
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       EarlyDisconnectBeforeFirstResponse_RecoversOnClientReconnect) {
  base::HistogramTester histogram_tester;

  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  base::OnceCallback<void(GlicInvokeError, std::string)> initial_on_error;
  std::vector<std::string> reconnected_prompts;
  std::optional<InstanceId> reconnected_target_instance;
  bool reconnected_superseded = false;

  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        EXPECT_FALSE(options.supersede_if_in_progress);
        if (options.on_panel_opened) {
          std::move(options.on_panel_opened).Run();
        }
        if (options.on_client_connected) {
          std::move(options.on_client_connected)
              .Run(mock_glic_instance_.GetWeakPtr());
        }
        initial_on_error = options.TakeOnErrorWithDiagnostics(
            GlicApiTestPasskeys::GetErrorDiagnosticsPassKey());
        return mock_glic_instance_.GetWeakPtr();
      })
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
                    GlicInvokeOptions options,
                    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        reconnected_superseded = options.supersede_if_in_progress;
        reconnected_prompts = options.prompts;
        if (const auto* id =
                std::get_if<InstanceId>(&options.target.conversation)) {
          reconnected_target_instance = *id;
        }
        if (initial_on_error) {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(initial_on_error),
                             GlicInvokeError::kSuperseded, std::string()));
        }
        if (options.on_panel_opened) {
          std::move(options.on_panel_opened).Run();
        }
        if (options.on_client_connected) {
          std::move(options.on_client_connected)
              .Run(mock_glic_instance_.GetWeakPtr());
        }
        if (options.on_success) {
          std::move(options.on_success).Run();
        }
        return mock_glic_instance_.GetWeakPtr();
      });

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload =
      TriggerActuationRequest{.initial_prompt = "book flight to nyc"};

  auto start_response = coordinator_->OnRequest(
      kTestContextId, request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);
  ASSERT_TRUE(start_response.has_value());
  ASSERT_TRUE(start_response->task_update.has_value());
  EXPECT_EQ(start_response->task_update->state, TaskUpdate::State::kStarting);

  // Guest reloads 300ms after initial connection, before any response arrives.
  task_environment_.FastForwardBy(base::Milliseconds(300));
  test_triggering_manager_.set_is_client_bound(false);
  test_triggering_manager_.ResetHandler();
  task_environment_.FastForwardBy(base::Milliseconds(50));

  // Must remain in reconnect grace window without emitting a failure update.
  EXPECT_FALSE(update_future.IsReady());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  // Reloaded guest binds 250ms later within the 10s grace window.
  task_environment_.FastForwardBy(base::Milliseconds(250));
  test_triggering_manager_.SimulateClientRebound();
  task_environment_.FastForwardBy(base::Milliseconds(50));

  EXPECT_TRUE(reconnected_superseded);
  EXPECT_EQ(reconnected_target_instance, mock_glic_instance_.id());
  ASSERT_EQ(reconnected_prompts.size(), 1u);
  EXPECT_EQ(reconnected_prompts[0], "book flight to nyc");

  // Reloaded guest emits worklog and terminal completion.
  auto worklog = mojom::ExperimentalTriggeringUpdate::New();
  worklog->type = mojom::ExperimentalTriggeringUpdateType::kWorklog;
  worklog->data = "searching flights";
  test_triggering_manager_.SendUpdate(
      std::move(worklog), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();

  auto worklog_resp = update_future.Take();
  ASSERT_TRUE(worklog_resp.task_update.has_value());
  EXPECT_EQ(worklog_resp.task_update->state, TaskUpdate::State::kRunning);
  EXPECT_EQ(worklog_resp.task_update->data, "searching flights");

  auto complete = mojom::ExperimentalTriggeringUpdate::New();
  complete->type = mojom::ExperimentalTriggeringUpdateType::kTerminalCompletion;
  complete->data = "done";
  test_triggering_manager_.SendUpdate(
      std::move(complete), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();

  auto complete_resp = update_future.Take();
  ASSERT_TRUE(complete_resp.task_update.has_value());
  EXPECT_EQ(complete_resp.task_update->state, TaskUpdate::State::kComplete);
  EXPECT_EQ(complete_resp.task_update->data, "done");

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kSuccess, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       EarlyDisconnectBeforeFirstResponse_FailsAfterGracePeriodExpires) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  auto start_response = coordinator_->OnRequest(
      kTestContextId, request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);
  ASSERT_TRUE(start_response.has_value());

  test_triggering_manager_.set_is_client_bound(false);
  test_triggering_manager_.ResetHandler();

  // Before the 10s grace period expires, the session waits for reconnect.
  task_environment_.FastForwardBy(base::Seconds(9));
  EXPECT_FALSE(update_future.IsReady());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  // Once the 10s grace period expires without a reconnect, it fails cleanly.
  task_environment_.FastForwardBy(base::Seconds(2));
  ASSERT_TRUE(update_future.IsReady());
  auto failure = update_future.Take();
  ASSERT_TRUE(failure.task_update.has_value());
  EXPECT_EQ(failure.task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(failure.task_update->data, "Client disconnected unexpectedly.");
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::
          kClientDisconnectedBeforeResponse,
      1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       DisconnectAfterFirstResponse_FailsImmediatelyWithoutReconnectGrace) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  ASSERT_TRUE(
      coordinator_
          ->OnRequest(
              kTestContextId, request,
              ScopedIncomingMessageResultLogger(
                  GlicMetricReporter(profile_),
                  ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
              update_future.GetRepeatingCallback(), nullptr)
          .has_value());

  // First response arrives from the guest.
  auto worklog = mojom::ExperimentalTriggeringUpdate::New();
  worklog->type = mojom::ExperimentalTriggeringUpdateType::kWorklog;
  worklog->data = "step 1";
  test_triggering_manager_.SendUpdate(
      std::move(worklog), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();
  ASSERT_TRUE(update_future.IsReady());
  EXPECT_EQ(update_future.Take().task_update->state,
            TaskUpdate::State::kRunning);

  // Subsequent disconnect mid-turn must fail immediately without waiting for
  // the 10s grace timer (avoiding duplicate prompt submission).
  test_triggering_manager_.ResetHandler();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  ASSERT_TRUE(update_future.IsReady());
  auto failure = update_future.Take();
  ASSERT_TRUE(failure.task_update.has_value());
  EXPECT_EQ(failure.task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(failure.task_update->data, "Client disconnected unexpectedly.");
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::
          kClientDisconnectedBeforeResponse,
      1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       EarlyDisconnectWithPreparedTab_ReportsTabClosedWhenPreparedTabCloses) {
  struct ScopedPreparedTab {
    tabs::MockTabInterface tab;
    base::WeakPtrFactory<tabs::TabInterface> weak_factory{&tab};
  };
  auto prepared_tab = std::make_unique<ScopedPreparedTab>();
  ON_CALL(prepared_tab->tab, GetWeakPtr())
      .WillByDefault(testing::Return(prepared_tab->weak_factory.GetWeakPtr()));

  auto* service = static_cast<MockGlicKeyedService*>(
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_, false));
  // InvokeWithAutoSubmit must only be called once (initial invoke) and must not
  // fall back to GetBrowserWindow() when the prepared tab closes before rebind.
  EXPECT_CALL(*service,
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([&](InvokeWithAutoSubmitPasskey passkey,
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

  base::HistogramTester histogram_tester;
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  ASSERT_TRUE(
      coordinator_
          ->OnRequest(
              kTestContextId, request,
              ScopedIncomingMessageResultLogger(
                  GlicMetricReporter(profile_),
                  ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
              update_future.GetRepeatingCallback(), &prepared_tab->tab)
          .has_value());

  // Enter reconnect grace, then close the prepared tab before client rebinds.
  test_triggering_manager_.set_is_client_bound(false);
  test_triggering_manager_.ResetHandler();
  task_environment_.FastForwardBy(base::Milliseconds(50));
  EXPECT_FALSE(update_future.IsReady());

  prepared_tab.reset();
  test_triggering_manager_.SimulateClientRebound();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  ASSERT_TRUE(update_future.IsReady());
  auto failure = update_future.Take();
  ASSERT_TRUE(failure.task_update.has_value());
  EXPECT_EQ(failure.task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(failure.task_update->data, "Target tab closed before completion.");
  EXPECT_EQ(failure.task_update->metadata["chrome_error_code"],
            "GLIC_TAB_CLOSED");
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kInvokeErrorTabClosed, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       ActiveActuationSessionTracking_LifecycleAcrossCompletionAndStop) {
  std::vector<bool> session_states;
  auto sub = test_triggering_manager_.AddActiveActuationSessionChangedCallback(
      base::BindLambdaForTesting(
          [&](bool active) { session_states.push_back(active); }));
  EXPECT_FALSE(test_triggering_manager_.HasActiveActuationSession());

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "turn 1"};

  ASSERT_TRUE(
      coordinator_
          ->OnRequest(
              kTestContextId, request,
              ScopedIncomingMessageResultLogger(
                  GlicMetricReporter(profile_),
                  ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
              update_future.GetRepeatingCallback(), nullptr)
          .has_value());
  EXPECT_TRUE(test_triggering_manager_.HasActiveActuationSession());
  EXPECT_THAT(session_states, testing::ElementsAre(true));

  // Worklog updates keep the session active.
  auto worklog = mojom::ExperimentalTriggeringUpdate::New();
  worklog->type = mojom::ExperimentalTriggeringUpdateType::kWorklog;
  worklog->data = "working";
  test_triggering_manager_.SendUpdate(
      std::move(worklog), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();
  EXPECT_EQ(update_future.Take().task_update->state,
            TaskUpdate::State::kRunning);
  EXPECT_TRUE(test_triggering_manager_.HasActiveActuationSession());

  // Yielding to the user and continuing on a follow-up turn also keeps the
  // session continuously active so an unbound instance stays alive across
  // turns.
  auto yield_update = mojom::ExperimentalTriggeringUpdate::New();
  yield_update->type = mojom::ExperimentalTriggeringUpdateType::kYieldToUser;
  yield_update->data = "need input";
  test_triggering_manager_.SendUpdate(
      std::move(yield_update), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();
  EXPECT_EQ(update_future.Take().task_update->state, TaskUpdate::State::kYield);
  EXPECT_TRUE(test_triggering_manager_.HasActiveActuationSession());

  ExperimentalTriggeringRequest cont_request;
  cont_request.version = 1;
  cont_request.context_id = kTestContextId;
  cont_request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  cont_request.payload =
      ContinueActuationRequest{.continuation_prompt = "turn 2"};
  ASSERT_TRUE(
      coordinator_
          ->OnRequest(
              kTestContextId, cont_request,
              ScopedIncomingMessageResultLogger(
                  GlicMetricReporter(profile_),
                  ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
              update_future.GetRepeatingCallback(), nullptr)
          .has_value());
  EXPECT_TRUE(test_triggering_manager_.HasActiveActuationSession());
  EXPECT_THAT(session_states, testing::ElementsAre(true));

  // Terminal completion releases the active session so an unbound instance can
  // clean up, and ends the session.
  auto complete = mojom::ExperimentalTriggeringUpdate::New();
  complete->type = mojom::ExperimentalTriggeringUpdateType::kTerminalCompletion;
  complete->data = "done";
  test_triggering_manager_.SendUpdate(
      std::move(complete), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();
  EXPECT_EQ(update_future.Take().task_update->state,
            TaskUpdate::State::kComplete);
  EXPECT_FALSE(test_triggering_manager_.HasActiveActuationSession());
  EXPECT_THAT(session_states, testing::ElementsAre(true, false));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  // A subsequent session re-registers the active session, and
  // StopActuationRequest releases it immediately.
  ASSERT_TRUE(
      coordinator_
          ->OnRequest(
              kTestContextId, request,
              ScopedIncomingMessageResultLogger(
                  GlicMetricReporter(profile_),
                  ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
              update_future.GetRepeatingCallback(), nullptr)
          .has_value());
  EXPECT_TRUE(test_triggering_manager_.HasActiveActuationSession());
  EXPECT_THAT(session_states, testing::ElementsAre(true, false, true));

  // StopActuationRequest releases the active session immediately.
  ExperimentalTriggeringRequest stop_request;
  stop_request.version = 1;
  stop_request.context_id = kTestContextId;
  stop_request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  stop_request.payload = StopActuationRequest{};
  auto stop_resp = coordinator_->OnRequest(
      kTestContextId, stop_request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(profile_),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), nullptr);
  ASSERT_TRUE(stop_resp.has_value());
  EXPECT_EQ(stop_resp->task_update->state, TaskUpdate::State::kStopped);
  EXPECT_FALSE(test_triggering_manager_.HasActiveActuationSession());
  EXPECT_THAT(session_states, testing::ElementsAre(true, false, true, false));
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       InstanceWillBeDestroyed_EmitsAttributedTabClosedBeforePipeDisconnect) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  ASSERT_TRUE(
      coordinator_
          ->OnRequest(
              kTestContextId, request,
              ScopedIncomingMessageResultLogger(
                  GlicMetricReporter(profile_),
                  ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
              update_future.GetRepeatingCallback(), nullptr)
          .has_value());

  auto worklog = mojom::ExperimentalTriggeringUpdate::New();
  worklog->type = mojom::ExperimentalTriggeringUpdateType::kWorklog;
  worklog->data = "step 1";
  test_triggering_manager_.SendUpdate(
      std::move(worklog), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();
  EXPECT_EQ(update_future.Take().task_update->state,
            TaskUpdate::State::kRunning);

  // Simulate GlicInstanceImpl::CloseInstanceAndShutdown notifying synchronously
  // right before <webview> teardown drops the Mojo updates pipe.
  test_triggering_manager_.NotifyInstanceWillBeDestroyed(
      GlicExperimentalTriggeringManager::InstanceTeardownReason::kTabClosed);
  test_triggering_manager_.ResetHandler();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  ASSERT_TRUE(update_future.IsReady());
  auto failure = update_future.Take();
  ASSERT_TRUE(failure.task_update.has_value());
  EXPECT_EQ(failure.task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(failure.task_update->data, "Target tab closed before completion.");
  auto code_it = failure.task_update->metadata.find("chrome_error_code");
  ASSERT_NE(code_it, failure.task_update->metadata.end());
  EXPECT_EQ(code_it->second, "GLIC_TAB_CLOSED");
  EXPECT_FALSE(update_future.IsReady());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);

  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kDestroyedBeforeCompletion,
      1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       InstanceWillBeDestroyed_PreemptsReconnectGraceWithDestroyedCode) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = "ctx_panel_closed";
  request.task_metadata = TaskMetadata{.conversation_id = "conv_1"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  ASSERT_TRUE(
      coordinator_
          ->OnRequest(
              "ctx_panel_closed", request,
              ScopedIncomingMessageResultLogger(
                  GlicMetricReporter(profile_),
                  ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
              update_future.GetRepeatingCallback(), nullptr)
          .has_value());

  // Enter reconnect grace first, then tear down the instance: teardown must
  // preempt the grace timer and immediately report the attributed code.
  test_triggering_manager_.set_is_client_bound(false);
  test_triggering_manager_.ResetHandler();
  task_environment_.FastForwardBy(base::Milliseconds(50));
  EXPECT_FALSE(update_future.IsReady());

  test_triggering_manager_.NotifyInstanceWillBeDestroyed(
      GlicExperimentalTriggeringManager::InstanceTeardownReason::
          kInstanceDestroyed);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  ASSERT_TRUE(update_future.IsReady());
  auto failure = update_future.Take();
  ASSERT_TRUE(failure.task_update.has_value());
  EXPECT_EQ(failure.task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(failure.task_update->metadata["chrome_error_code"],
            "GLIC_DESTROYED_BEFORE_COMPLETION");
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kDestroyedBeforeCompletion,
      1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       InstanceWillBeDestroyed_EmitsDestroyedCodeAndNotifiesAtMostOnce) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;

  int destroy_notifications = 0;
  auto sub = test_triggering_manager_.AddInstanceWillBeDestroyedCallback(
      base::BindLambdaForTesting(
          [&](GlicExperimentalTriggeringManager::InstanceTeardownReason) {
            ++destroy_notifications;
          }));

  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = "ctx_instance_destroyed";
  request.task_metadata = TaskMetadata{.conversation_id = "conv_2"};
  request.payload = TriggerActuationRequest{.initial_prompt = "test"};

  ASSERT_TRUE(
      coordinator_
          ->OnRequest(
              "ctx_instance_destroyed", request,
              ScopedIncomingMessageResultLogger(
                  GlicMetricReporter(profile_),
                  ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
              update_future.GetRepeatingCallback(), nullptr)
          .has_value());

  // Simulate Shutdown() followed by CloseInstanceAndShutdown(): only the first
  // notification should fire.
  test_triggering_manager_.NotifyInstanceWillBeDestroyed(
      GlicExperimentalTriggeringManager::InstanceTeardownReason::
          kInstanceDestroyed);
  test_triggering_manager_.NotifyInstanceWillBeDestroyed(
      GlicExperimentalTriggeringManager::InstanceTeardownReason::
          kInstanceDestroyed);
  EXPECT_EQ(destroy_notifications, 1);

  test_triggering_manager_.ResetHandler();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  ASSERT_TRUE(update_future.IsReady());
  auto failure = update_future.Take();
  ASSERT_TRUE(failure.task_update.has_value());
  EXPECT_EQ(failure.task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(failure.task_update->metadata["chrome_error_code"],
            "GLIC_DESTROYED_BEFORE_COMPLETION");
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.ExecutionOutcome",
      GlicExperimentalTriggeringExecutionOutcome::kDestroyedBeforeCompletion,
      1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorOptInTest,
       UpdatesHandlerDestroyed_RecordsIdleAfterOptInAndWithoutSession) {
  base::HistogramTester histogram_tester;

  // 1. Completed DeviceOptInRequest destroys the handler with kIdle.
  base::test::TestFuture<ExperimentalTriggeringResponse> opt_in_future;
  EXPECT_FALSE(SendRequestWithCallback(CreateRequest(DeviceOptInRequest{}),
                                       opt_in_future.GetRepeatingCallback())
                   .has_value());
  ASSERT_TRUE(opt_in_future.Wait());
  EXPECT_EQ(*opt_in_future.Take().device_opt_in_result,
            DeviceOptInResult::kAccepted);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  // 2. Stateless helper request with no active session also destroys with
  // kIdle.
  EXPECT_FALSE(SendRequest(CreateRequest(TaskMetadataUpdated{})).has_value());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);

  ExpectHandlerDestroyed(histogram_tester,
                         GlicExperimentalTriggeringHandlerDestroyReason::kIdle,
                         /*count=*/2);
  for (const auto& entry :
       journal_recorder_->GetEntries("UpdatesHandlerDestroyed")) {
    EXPECT_THAT(
        entry.details,
        UnorderedElementsAre(
            Pair("context_id", std::string(kTestContextId)),
            Pair("reason", "Idle"), Pair("has_instance", "false"),
            Pair("has_actions_runner", "false"),
            Pair("awaiting_opt_in", "false"), Pair("pending_invoke_count", "0"),
            Pair("is_actuation_started", "false"),
            Pair("terminal_update_sent", "false")));
  }
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       UpdatesHandlerDestroyed_RecordsIdleAfterExecuteActions) {
  base::HistogramTester histogram_tester;
  auto* user_data = optimization_guide::DocumentIdentifierUserData::
      GetOrCreateForCurrentDocument(web_contents_->GetPrimaryMainFrame());
  ExperimentalTriggeringRequest request = CreateExecuteScriptToolRequest(
      mock_tab_.GetHandle().raw_value(), user_data->serialized_token());

  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  SendRequestWithCallback(request, update_future.GetRepeatingCallback(),
                          &mock_tab_);
  ASSERT_TRUE(update_future.Take().execute_actions_response.has_value());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  ExpectHandlerDestroyed(histogram_tester,
                         GlicExperimentalTriggeringHandlerDestroyReason::kIdle);
}

struct InstanceTeardownDestroyReasonTestCase {
  const char* test_name;
  GlicExperimentalTriggeringManager::InstanceTeardownReason teardown_reason;
  GlicExperimentalTriggeringHandlerDestroyReason expected_destroy_reason;
};

class GlicExperimentalTriggeringCoordinatorInstanceTeardownTest
    : public GlicExperimentalTriggeringCoordinatorWithTabTest,
      public testing::WithParamInterface<
          InstanceTeardownDestroyReasonTestCase> {};

TEST_P(GlicExperimentalTriggeringCoordinatorInstanceTeardownTest,
       UpdatesHandlerDestroyed_RecordsReason) {
  const InstanceTeardownDestroyReasonTestCase& test_case = GetParam();
  base::HistogramTester tester;

  StartActuationSession();
  test_triggering_manager_.NotifyInstanceWillBeDestroyed(
      test_case.teardown_reason);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));

  ExpectHandlerDestroyed(tester, test_case.expected_destroy_reason);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GlicExperimentalTriggeringCoordinatorInstanceTeardownTest,
    testing::Values(
        InstanceTeardownDestroyReasonTestCase{
            .test_name = "TabClosed",
            .teardown_reason = GlicExperimentalTriggeringManager::
                InstanceTeardownReason::kTabClosed,
            .expected_destroy_reason =
                GlicExperimentalTriggeringHandlerDestroyReason::kTabClosed},
        InstanceTeardownDestroyReasonTestCase{
            .test_name = "InstanceDestroyed",
            .teardown_reason = GlicExperimentalTriggeringManager::
                InstanceTeardownReason::kInstanceDestroyed,
            .expected_destroy_reason =
                GlicExperimentalTriggeringHandlerDestroyReason::
                    kInstanceDestroyed}),
    [](const testing::TestParamInfo<InstanceTeardownDestroyReasonTestCase>&
           info) { return info.param.test_name; });

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       UpdatesHandlerDestroyed_RecordsSessionStoppedOnSyncStop) {
  base::HistogramTester tester;
  StartActuationSession();
  ASSERT_TRUE(
      SendRequest(CreateRequest(StopActuationRequest{.stop_reason = "stop"}))
          .has_value());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
  ExpectHandlerDestroyed(
      tester, GlicExperimentalTriggeringHandlerDestroyReason::kSessionStopped);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       UpdatesHandlerDestroyed_RecordsRejectedSynchronouslyOnOptedOutTrigger) {
  base::HistogramTester tester;
  OptOut();
  ASSERT_TRUE(SendRequest(CreateRequest(
                              TriggerActuationRequest{.initial_prompt = "no"}))
                  .has_value());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
  ExpectHandlerDestroyed(
      tester,
      GlicExperimentalTriggeringHandlerDestroyReason::kRejectedSynchronously);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       UpdatesHandlerDestroyed_RecordsRegistrationFailed) {
  base::HistogramTester tester;
  test_triggering_manager_.set_registration_success(false);
  StartActuationSession();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));
  ExpectHandlerDestroyed(
      tester,
      GlicExperimentalTriggeringHandlerDestroyReason::kRegistrationFailed);
}

TEST_F(
    GlicExperimentalTriggeringCoordinatorWithTabTest,
    UpdatesHandlerDestroyed_RecordsUnexpectedLiveWithoutInstanceOrTerminalUpdate) {
  base::HistogramTester tester;
  base::OnceClosure invoke_on_success;
  EXPECT_CALL(*mock_glic_service(),
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([&invoke_on_success](InvokeWithAutoSubmitPasskey,
                                     GlicInvokeOptions options,
                                     GlicInvokeWithAutoSubmitOptions) {
        invoke_on_success = std::move(options.on_success);
        return base::WeakPtr<GlicInstance>();
      });

  // Invocation completes asynchronously without binding a GlicInstance
  // (`is_actuation_started_=true`, `!terminal_update_sent_`, `!HasLiveWork()`),
  // and a subsequent `TaskMetadataUpdated` triggers cleanup of the un-ended
  // actuation session.
  StartActuationSession();
  ASSERT_TRUE(invoke_on_success);
  std::move(invoke_on_success).Run();
  EXPECT_FALSE(SendRequest(CreateRequest(TaskMetadataUpdated{})).has_value());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);
  ExpectHandlerDestroyed(tester,
                         GlicExperimentalTriggeringHandlerDestroyReason::
                             kUnexpectedLiveDestruction);
  EXPECT_THAT(
      journal_recorder_->GetEntries("UpdatesHandlerDestroyed")[0].details,
      UnorderedElementsAre(
          Pair("context_id", std::string(kTestContextId)),
          Pair("reason", "UnexpectedLiveDestruction"),
          Pair("has_instance", "false"), Pair("has_actions_runner", "false"),
          Pair("awaiting_opt_in", "false"), Pair("pending_invoke_count", "0"),
          Pair("is_actuation_started", "true"),
          Pair("terminal_update_sent", "false")));
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       UpdatesHandlerDestroyed_PreservesSessionEndReasonOnCoordinatorShutdown) {
  base::HistogramTester tester;
  StartActuationSession();
  auto update = mojom::ExperimentalTriggeringUpdate::New();
  update->type = mojom::ExperimentalTriggeringUpdateType::kTerminalCompletion;
  test_triggering_manager_.SendUpdate(
      std::move(update), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();
  ASSERT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  coordinator_.reset();
  ExpectHandlerDestroyed(
      tester, GlicExperimentalTriggeringHandlerDestroyReason::kSessionComplete);
  EXPECT_THAT(
      journal_recorder_->GetEntries("UpdatesHandlerDestroyed")[0].details,
      IsSupersetOf({Pair("terminal_update_sent", "true")}));
}

// Owns a coordinator as `Profile` `SupportsUserData::Data` so it is torn down
// inside `~SupportsUserData()` during `TestingProfile` destruction—after
// `PerformInterlockedTwoPhaseShutdown()` has run `MarkContextDead()` and
// destroyed `ActorKeyedService`—without running a second shutdown pass.
class PostContextDeadCoordinatorHolder : public base::SupportsUserData::Data {
 public:
  PostContextDeadCoordinatorHolder(
      std::unique_ptr<TestGlicExperimentalTriggeringCoordinator> coordinator,
      base::OnceClosure on_before_destroy)
      : coordinator_(std::move(coordinator)),
        on_before_destroy_(std::move(on_before_destroy)) {}

  ~PostContextDeadCoordinatorHolder() override {
    if (on_before_destroy_) {
      std::move(on_before_destroy_).Run();
    }
    coordinator_.reset();
  }

 private:
  std::unique_ptr<TestGlicExperimentalTriggeringCoordinator> coordinator_;
  base::OnceClosure on_before_destroy_;
};

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       UpdatesHandlerDestroyed_LateWorkBeforeCleanupRecordsUnexpectedLive) {
  base::HistogramTester tester;
  base::OnceCallback<void(GlicInvokeError, std::string)> on_error;
  EXPECT_CALL(*mock_glic_service(),
              InvokeWithAutoSubmit(testing::_, testing::_, testing::_))
      .WillOnce([&on_error](InvokeWithAutoSubmitPasskey,
                            GlicInvokeOptions options,
                            GlicInvokeWithAutoSubmitOptions) {
        on_error = options.TakeOnErrorWithDiagnostics(
            GlicApiTestPasskeys::GetErrorDiagnosticsPassKey());
        return base::WeakPtr<GlicInstance>();
      });

  StartActuationSession();
  ASSERT_TRUE(on_error);
  std::move(on_error).Run(GlicInvokeError::kInvokeInProgress, std::string());
  ASSERT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  // Start async ExecuteActions work on the same context before the posted
  // `CleanupAsync()` task from `OnInvokeError` runs. When `CleanupAsync()`
  // executes first on the task queue while `actions_runner_` is still live, it
  // must record `kUnexpectedLiveDestruction` rather than `kInvokeError`.
  auto* user_data = optimization_guide::DocumentIdentifierUserData::
      GetOrCreateForCurrentDocument(web_contents_->GetPrimaryMainFrame());
  ExperimentalTriggeringRequest exec_req = CreateExecuteScriptToolRequest(
      mock_tab_.GetHandle().raw_value(), user_data->serialized_token());
  SendRequestWithCallback(exec_req, base::DoNothing(), &mock_tab_);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator_->GetUpdatesHandlerMapSizeForTesting() == 0;
  }));
  ExpectHandlerDestroyed(tester,
                         GlicExperimentalTriggeringHandlerDestroyReason::
                             kUnexpectedLiveDestruction);
  EXPECT_THAT(
      journal_recorder_->GetEntries("UpdatesHandlerDestroyed")[0].details,
      IsSupersetOf({Pair("has_actions_runner", "true"),
                    Pair("terminal_update_sent", "true")}));
}

TEST_F(GlicExperimentalTriggeringCoordinatorTest,
       UpdatesHandlerDestroyed_SafeDuringPostContextDeadShutdown) {
  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_);
  ASSERT_TRUE(actor_service);
  base::WeakPtr<actor::ActorKeyedService> actor_service_weak =
      actor_service->GetWeakPtr();

  MockBrowserWindowInterface shutdown_window;
  ON_CALL(shutdown_window, GetProfile())
      .WillByDefault(testing::Return(profile_.get()));
  coordinator_->set_browser_window(&shutdown_window);

  ExperimentalTriggeringRequest req;
  req.version = 1;
  req.context_id = "ctx_phase2";
  req.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  req.payload = TriggerActuationRequest{.initial_prompt = "go"};
  ASSERT_TRUE(
      SendRequest(req, /*prepared_tab=*/nullptr, "ctx_phase2").has_value());
  ASSERT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);
  coordinator_->set_browser_window(nullptr);

  // Tear down the fixture's `"test_profile"` rather than creating a second
  // `TestingProfile`: on Android, `GCMDriver` enforces a process-wide Java
  // singleton (`sInstance == null`) and throws `IllegalStateException` if two
  // profiles instantiate `GCMDriverAndroid` concurrently.
  bool destroyed_after_actor_service = false;
  TestingProfile* profile_to_delete = profile_;
  profile_ = nullptr;
  actor_task_manager_.reset();
  actor_policy_checker_.reset();
  journal_recorder_.reset();

  static const int kCoordinatorHolderKey = 0;
  profile_to_delete->SetUserData(
      &kCoordinatorHolderKey,
      std::make_unique<PostContextDeadCoordinatorHolder>(
          std::move(coordinator_),
          base::BindLambdaForTesting(
              [&destroyed_after_actor_service, actor_service_weak]() {
                destroyed_after_actor_service = !actor_service_weak;
              })));
  profile_to_delete = nullptr;

  // Deleting `"test_profile"` through `TestingProfileManager` runs
  // `PerformInterlockedTwoPhaseShutdown()` once and destroys
  // `PostContextDeadCoordinatorHolder`—and with it `coordinator_`—after both
  // `MarkContextDead()` and `~ActorKeyedService()`.
  base::HistogramTester tester;
  profile_manager_.DeleteTestingProfile("test_profile");
  EXPECT_TRUE(destroyed_after_actor_service);
  ExpectHandlerDestroyed(
      tester,
      GlicExperimentalTriggeringHandlerDestroyReason::kCoordinatorShutdown);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       SequenceAnomaly_NormalUnsetNegativeAndDroppedAdvanceRecordNothing) {
  base::HistogramTester histogram_tester;

  // 1. Legacy/unset and negative sentinel sequence numbers record no anomalies.
  ExperimentalTriggeringRequest unset_trigger =
      CreateRequest(TriggerActuationRequest{.initial_prompt = "legacy"});
  unset_trigger.context_id = "ctx_legacy";
  unset_trigger.task_metadata = TaskMetadata{
      .conversation_id = "conv_legacy",
      .sender_sequence_number = -1,
      .last_seen_sequence_number = -1,
  };
  ASSERT_TRUE(SendRequest(unset_trigger, /*prepared_tab=*/nullptr, "ctx_legacy")
                  .has_value());

  // 2. Well-ordered multi-turn sequence + dropped duplicate-start with
  // advancing sequence number records no anomalies.
  base::test::TestFuture<ExperimentalTriggeringResponse> updates;
  ExperimentalTriggeringRequest trigger =
      CreateRequest(TriggerActuationRequest{.initial_prompt = "turn 1"});
  trigger.task_metadata = TaskMetadata{
      .conversation_id = "conv_123",
      .sender_sequence_number = 0,
      .last_seen_sequence_number = std::nullopt,
  };
  auto start_resp =
      SendRequestWithCallback(trigger, updates.GetRepeatingCallback());
  ASSERT_TRUE(start_resp.has_value());
  EXPECT_EQ(start_resp->task_metadata->sender_sequence_number, 0);

  // Dropped duplicate Trigger with seq=1 advances the anomaly high-water mark
  // to 1 without mutating the running session's last_seen_sequence_number_.
  ExperimentalTriggeringRequest dup_trigger =
      CreateRequest(TriggerActuationRequest{.initial_prompt = "dup"});
  dup_trigger.task_metadata = TaskMetadata{
      .conversation_id = "conv_123",
      .sender_sequence_number = 1,
      .last_seen_sequence_number = 0,
  };
  EXPECT_FALSE(SendRequest(dup_trigger).has_value());

  auto yield_update = mojom::ExperimentalTriggeringUpdate::New();
  yield_update->type = mojom::ExperimentalTriggeringUpdateType::kYieldToUser;
  yield_update->data = "question";
  test_triggering_manager_.SendUpdate(
      std::move(yield_update), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();
  auto yield_resp = updates.Take();
  EXPECT_EQ(yield_resp.task_metadata->sender_sequence_number, 1);
  // Running task kept its last_seen_sequence_number = 0 despite dropped seq=1.
  EXPECT_EQ(yield_resp.task_metadata->last_seen_sequence_number, 0);

  ExperimentalTriggeringRequest cont =
      CreateRequest(ContinueActuationRequest{.continuation_prompt = "turn 2"});
  cont.task_metadata = TaskMetadata{
      .conversation_id = "conv_123",
      .sender_sequence_number = 2,
      .last_seen_sequence_number = 1,
  };
  ASSERT_TRUE(SendRequestWithCallback(cont, updates.GetRepeatingCallback())
                  .has_value());

  ExpectSequenceAnomalyTotalCount(histogram_tester, 0);
}

TEST_F(
    GlicExperimentalTriggeringCoordinatorWithTabTest,
    SequenceAnomaly_RecreatedHandlerRecordsSingleMostSpecificStreamRestartedBucket) {
  base::HistogramTester histogram_tester;

  // Continuation arriving at a brand-new handler (sent_reply_count=0,
  // prev_max_seen=none) with sender_sequence_number=1 and
  // last_seen_sequence_number=0 matches both StreamRestarted and
  // MissingHistory, but records only the single most specific bucket
  // (kStreamRestarted) while still advancing max_seen_request_sequence_number_
  // to 1.
  ExperimentalTriggeringRequest cont =
      CreateRequest(ContinueActuationRequest{.continuation_prompt = "resume"});
  cont.task_metadata = TaskMetadata{
      .conversation_id = "conv_123",
      .sender_sequence_number = 1,
      .last_seen_sequence_number = 0,
  };
  ASSERT_TRUE(SendRequest(cont).has_value());

  ExpectSequenceAnomalyCount(
      histogram_tester,
      GlicExperimentalTriggeringSequenceAnomaly::kStreamRestarted, 1);
  ExpectSequenceAnomalyCount(
      histogram_tester,
      GlicExperimentalTriggeringSequenceAnomaly::kMissingHistory, 0);
  ExpectSequenceAnomalyTotalCount(histogram_tester, 1);

  auto events = journal_recorder_->GetEntries("SequenceAnomaly");
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].track_uuid,
            actor::MakeGlicExperimentalTriggeringTrackUUID(kTestContextId));
  EXPECT_THAT(events[0].details,
              UnorderedElementsAre(
                  Pair("context_id", std::string(kTestContextId)),
                  Pair("anomaly", "StreamRestarted"),
                  Pair("request_type", "ContinueActuationRequest"),
                  Pair("request_seq", "1"), Pair("request_last_seen", "0"),
                  Pair("handler_max_seen_request_seq", "none"),
                  Pair("handler_sent_count", "0")));

  // Follow-up in-order request with seq=2 and last_seen=0 (<
  // sent_reply_count=1) confirms max_seen_request_sequence_number_ advanced to
  // 1 on the first request (records 0 additional anomalies).
  ExperimentalTriggeringRequest meta = CreateRequest(TaskMetadataUpdated{});
  meta.task_metadata = TaskMetadata{
      .conversation_id = "conv_123",
      .sender_sequence_number = 2,
      .last_seen_sequence_number = 0,
  };
  EXPECT_FALSE(SendRequest(meta).has_value());
  ExpectSequenceAnomalyTotalCount(histogram_tester, 1);
}

TEST_F(GlicExperimentalTriggeringCoordinatorOptInTest,
       SequenceAnomaly_DeviceOptInRetryExemptFromDuplicateOrReplayedRequest) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ExperimentalTriggeringResponse> opt_in_future;

  ExperimentalTriggeringRequest opt_in_req =
      CreateRequest(DeviceOptInRequest{});
  opt_in_req.task_metadata = TaskMetadata{
      .conversation_id = "conv_opt_in",
      .sender_sequence_number = 0,
      .last_seen_sequence_number = std::nullopt,
  };

  // First Sync opt-in opens the pending dialog (`awaiting_opt_in_ == true`).
  EXPECT_FALSE(
      SendRequestWithCallback(opt_in_req, opt_in_future.GetRepeatingCallback())
          .has_value());
  ASSERT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 1u);

  // Resumed Sync opt-in retry stamps sender_sequence_number = 0 again while the
  // dialog handler is still open; exempted from kDuplicateOrReplayedRequest.
  EXPECT_FALSE(
      SendRequestWithCallback(opt_in_req, opt_in_future.GetRepeatingCallback())
          .has_value());
  ExpectSequenceAnomalyTotalCount(histogram_tester, 0);

  ASSERT_TRUE(opt_in_future.Wait());
  EXPECT_EQ(*opt_in_future.Take().device_opt_in_result,
            DeviceOptInResult::kAccepted);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       SequenceAnomaly_StopAndTaskMetadataUpdatedExemptOnFreshHandler) {
  base::HistogramTester histogram_tester;

  ExperimentalTriggeringRequest metadata_req =
      CreateRequest(TaskMetadataUpdated{});
  metadata_req.context_id = "ctx_meta";
  metadata_req.task_metadata = TaskMetadata{
      .conversation_id = "conv_1",
      .sender_sequence_number = 3,
      .last_seen_sequence_number = 2,
  };
  EXPECT_FALSE(SendRequest(metadata_req, /*prepared_tab=*/nullptr, "ctx_meta")
                   .has_value());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);

  ExperimentalTriggeringRequest stop_req =
      CreateRequest(StopActuationRequest{.stop_reason = "cancel"});
  stop_req.context_id = "ctx_stop";
  stop_req.task_metadata = TaskMetadata{
      .conversation_id = "conv_1",
      .sender_sequence_number = 4,
      .last_seen_sequence_number = 2,
  };
  EXPECT_TRUE(
      SendRequest(stop_req, /*prepared_tab=*/nullptr, "ctx_stop").has_value());
  EXPECT_EQ(coordinator_->GetUpdatesHandlerMapSizeForTesting(), 0u);

  ExpectSequenceAnomalyTotalCount(histogram_tester, 0);
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       SequenceAnomaly_OutOfOrderAndDuplicateReplayUseMonotonicHighWaterMark) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ExperimentalTriggeringResponse> updates;

  // Initial trigger: seq=0, sends STARTING at reply seq=0 (sent_reply_count=1).
  ExperimentalTriggeringRequest trigger =
      CreateRequest(TriggerActuationRequest{.initial_prompt = "start"});
  trigger.task_metadata = TaskMetadata{
      .conversation_id = "conv_123",
      .sender_sequence_number = 0,
      .last_seen_sequence_number = std::nullopt,
  };
  ASSERT_TRUE(SendRequestWithCallback(trigger, updates.GetRepeatingCallback())
                  .has_value());

  // 1. Replayed TriggerActuationRequest with seq=0: evaluated before
  // IsDuplicateStart(), records kDuplicateOrReplayedRequest while dropped.
  EXPECT_FALSE(SendRequest(trigger).has_value());
  ExpectSequenceAnomalyCount(
      histogram_tester,
      GlicExperimentalTriggeringSequenceAnomaly::kDuplicateOrReplayedRequest,
      1);
  ExpectSequenceAnomalyTotalCount(histogram_tester, 1);

  // 2. Non-start TaskMetadataUpdated with seq=2 and last_seen=1 (>= 1 sent):
  // LastSeenAheadOfSent takes precedence over RequestGap (1 bucket per request)
  // while still advancing max_seen to 2.
  ExperimentalTriggeringRequest ahead_with_gap =
      CreateRequest(TaskMetadataUpdated{});
  ahead_with_gap.task_metadata = TaskMetadata{
      .conversation_id = "conv_123",
      .sender_sequence_number = 2,
      .last_seen_sequence_number = 1,
  };
  EXPECT_FALSE(
      SendRequestWithCallback(ahead_with_gap, updates.GetRepeatingCallback())
          .has_value());

  // 3. Out-of-order late non-start TaskMetadataUpdated with seq=1 (<
  // max_seen=2): records kDuplicateOrReplayedRequest and MUST NOT regress
  // max_seen or last_seen_sequence_number_ from 2 back to 1.
  ExperimentalTriggeringRequest late_seq_1 =
      CreateRequest(TaskMetadataUpdated{});
  late_seq_1.task_metadata = TaskMetadata{
      .conversation_id = "conv_123",
      .sender_sequence_number = 1,
      .last_seen_sequence_number = 0,
  };
  EXPECT_FALSE(
      SendRequestWithCallback(late_seq_1, updates.GetRepeatingCallback())
          .has_value());

  // Verify outgoing async update still echoes last_seen_sequence_number = 2
  // (not regressed to 1).
  auto worklog = mojom::ExperimentalTriggeringUpdate::New();
  worklog->type = mojom::ExperimentalTriggeringUpdateType::kWorklog;
  worklog->data = "step";
  test_triggering_manager_.SendUpdate(
      std::move(worklog), mojom::SubscriberObservationType::kUpdate);
  test_triggering_manager_.FlushForTesting();
  EXPECT_EQ(updates.Take().task_metadata->last_seen_sequence_number, 2);

  // 4. Next in-order TaskMetadataUpdated with seq=3 and valid last_seen=1
  // (< sent_reply_count=2): records ZERO anomalies (no spurious gap!).
  ExperimentalTriggeringRequest in_order_3 =
      CreateRequest(TaskMetadataUpdated{});
  in_order_3.task_metadata = TaskMetadata{
      .conversation_id = "conv_123",
      .sender_sequence_number = 3,
      .last_seen_sequence_number = 1,
  };
  EXPECT_FALSE(SendRequest(in_order_3).has_value());

  // 5. TaskMetadataUpdated jumping from max_seen=3 to seq=5 with valid
  // last_seen=1 (< sent_reply_count=2): records kRequestGap.
  ExperimentalTriggeringRequest gap_to_5 = CreateRequest(TaskMetadataUpdated{});
  gap_to_5.task_metadata = TaskMetadata{
      .conversation_id = "conv_123",
      .sender_sequence_number = 5,
      .last_seen_sequence_number = 1,
  };
  EXPECT_FALSE(SendRequest(gap_to_5).has_value());

  ExpectSequenceAnomalyCount(
      histogram_tester,
      GlicExperimentalTriggeringSequenceAnomaly::kDuplicateOrReplayedRequest,
      2);
  ExpectSequenceAnomalyCount(
      histogram_tester,
      GlicExperimentalTriggeringSequenceAnomaly::kLastSeenAheadOfSent, 1);
  ExpectSequenceAnomalyCount(
      histogram_tester, GlicExperimentalTriggeringSequenceAnomaly::kRequestGap,
      1);
  ExpectSequenceAnomalyTotalCount(histogram_tester, 4);

  auto events = journal_recorder_->GetEntries("SequenceAnomaly");
  ASSERT_EQ(events.size(), 4u);
  EXPECT_THAT(events[0].details,
              UnorderedElementsAre(
                  Pair("context_id", std::string(kTestContextId)),
                  Pair("anomaly", "DuplicateOrReplayedRequest"),
                  Pair("request_type", "TriggerActuationRequest"),
                  Pair("request_seq", "0"), Pair("request_last_seen", "none"),
                  Pair("handler_max_seen_request_seq", "0"),
                  Pair("handler_sent_count", "1")));
  EXPECT_THAT(events[1].details,
              UnorderedElementsAre(
                  Pair("context_id", std::string(kTestContextId)),
                  Pair("anomaly", "LastSeenAheadOfSent"),
                  Pair("request_type", "TaskMetadataUpdated"),
                  Pair("request_seq", "2"), Pair("request_last_seen", "1"),
                  Pair("handler_max_seen_request_seq", "0"),
                  Pair("handler_sent_count", "1")));
  EXPECT_THAT(events[2].details,
              UnorderedElementsAre(
                  Pair("context_id", std::string(kTestContextId)),
                  Pair("anomaly", "DuplicateOrReplayedRequest"),
                  Pair("request_type", "TaskMetadataUpdated"),
                  Pair("request_seq", "1"), Pair("request_last_seen", "0"),
                  Pair("handler_max_seen_request_seq", "2"),
                  Pair("handler_sent_count", "1")));
  EXPECT_THAT(events[3].details,
              UnorderedElementsAre(
                  Pair("context_id", std::string(kTestContextId)),
                  Pair("anomaly", "RequestGap"),
                  Pair("request_type", "TaskMetadataUpdated"),
                  Pair("request_seq", "5"), Pair("request_last_seen", "1"),
                  Pair("handler_max_seen_request_seq", "3"),
                  Pair("handler_sent_count", "2")));
}

TEST_F(GlicExperimentalTriggeringCoordinatorWithTabTest,
       SequenceAnomaly_Int64MaxDoesNotOverflow) {
  base::HistogramTester histogram_tester;
  constexpr int64_t kMaxSeq = std::numeric_limits<int64_t>::max();

  ExperimentalTriggeringRequest trigger =
      CreateRequest(TriggerActuationRequest{.initial_prompt = "max"});
  trigger.task_metadata = TaskMetadata{
      .conversation_id = "conv_max",
      .sender_sequence_number = kMaxSeq,
      .last_seen_sequence_number = std::nullopt,
  };
  ASSERT_TRUE(SendRequest(trigger).has_value());
  ExpectSequenceAnomalyCount(
      histogram_tester,
      GlicExperimentalTriggeringSequenceAnomaly::kMissingHistory, 1);
  ExpectSequenceAnomalyTotalCount(histogram_tester, 1);

  // Duplicate INT64_MAX must classify as kDuplicateOrReplayedRequest without
  // signed integer overflow on +1.
  ExperimentalTriggeringRequest dup_max = CreateRequest(TaskMetadataUpdated{});
  dup_max.task_metadata = TaskMetadata{
      .conversation_id = "conv_max",
      .sender_sequence_number = kMaxSeq,
      .last_seen_sequence_number = 0,
  };
  EXPECT_FALSE(SendRequest(dup_max).has_value());
  ExpectSequenceAnomalyCount(
      histogram_tester,
      GlicExperimentalTriggeringSequenceAnomaly::kDuplicateOrReplayedRequest,
      1);
  ExpectSequenceAnomalyCount(
      histogram_tester, GlicExperimentalTriggeringSequenceAnomaly::kRequestGap,
      0);
  ExpectSequenceAnomalyTotalCount(histogram_tester, 2);
}

}  // namespace
}  // namespace glic
