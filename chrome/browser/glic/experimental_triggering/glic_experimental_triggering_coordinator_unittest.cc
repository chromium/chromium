// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_coordinator.h"

#include <memory>
#include <optional>
#include <variant>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_metrics.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_types.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

namespace {

constexpr char kTestContextId[] = "test_context";

class TestGlicExperimentalTriggeringCoordinator
    : public GlicExperimentalTriggeringCoordinator {
 public:
  explicit TestGlicExperimentalTriggeringCoordinator(Profile* profile)
      : GlicExperimentalTriggeringCoordinator(profile) {}
  ~TestGlicExperimentalTriggeringCoordinator() override = default;

  tabs::TabInterface* GetActiveTab() const override { return nullptr; }
  BrowserWindowInterface* GetBrowserWindow() const override {
    return browser_window_;
  }
  void set_browser_window(BrowserWindowInterface* window) {
    browser_window_ = window;
  }

 private:
  raw_ptr<BrowserWindowInterface> browser_window_ = nullptr;
};

class GlicExperimentalTriggeringCoordinatorTest : public testing::Test {
 public:
  GlicExperimentalTriggeringCoordinatorTest() = default;
  ~GlicExperimentalTriggeringCoordinatorTest() override = default;

  void SetUp() override {
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

    GlicEnabling::SetBypassEnablementChecksForTesting(true);
    coordinator_ = std::make_unique<TestGlicExperimentalTriggeringCoordinator>(
        profile_);
    OptIn();
  }

  void TearDown() override {
    coordinator_.reset();
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
    auto* glic_service = GlicKeyedServiceFactory::GetGlicKeyedService(
        profile_, /*create=*/true);
    ASSERT_TRUE(glic_service);
    glic_service->enabling().SetCompletedFre(
        glic::prefs::FreStatus::kCompleted);
    glic_service->enabling().SetUserEnabledActuationOnWeb(true);
    glic_service->enabling().SetExperimentalTriggeringEnabled(true);
  }

  std::optional<ExperimentalTriggeringResponse> SendRequest(
      const ExperimentalTriggeringRequest& request) {
    return coordinator_->OnRequest(
        kTestContextId, request,
        ScopedIncomingMessageResultLogger(
            ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
        base::DoNothing(), nullptr);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;
  GlicProfileManager glic_profile_manager_;
  std::unique_ptr<TestGlicExperimentalTriggeringCoordinator> coordinator_;
};

TEST_F(GlicExperimentalTriggeringCoordinatorTest, NoTaskMetadata) {
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.payload = TriggerActuationRequest{.initial_prompt = "hello"};

  auto response = SendRequest(request);
  EXPECT_FALSE(response.has_value());
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
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = kTestContextId;
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = StopActuationRequest{.stop_reason = "STOPPED_BY_USER"};

  auto response = SendRequest(request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->task_update.has_value());
  EXPECT_EQ(response->task_update->state, TaskUpdate::State::kFailed);
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

}  // namespace
}  // namespace glic
