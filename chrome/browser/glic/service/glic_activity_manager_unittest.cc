// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/service/glic_activity_manager.h"

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"
#include "chrome/browser/actor/ui/states/actor_task_nudge_state.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
using actor::ActorKeyedServiceFake;
using actor::TaskId;
using ActorTaskNudgeState = actor::ui::ActorTaskNudgeState;
using testing::AllOf;
using testing::Field;
using testing::ReturnRef;
using testing::Values;

class MockTaskNudgeStateChangeSubscriber {
 public:
  MOCK_METHOD(void,
              OnStateChanged,
              (bool show_bubble, ActorTaskNudgeState actor_task_nudge_state));
};

// TODO(crbug.com/502262218): Test suite has many missing expectations for this
// mock.
class MockTaskListBubbleChangeSubscriber {
 public:
  MOCK_METHOD(void, OnStateChanged, (bool is_start_notification));
};

class GlicActivityManagerTest : public testing::Test,
                                public testing::WithParamInterface<bool> {
 public:
  GlicActivityManagerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlicActor,
         {{features::kGlicActorPolicyControlExemption.name, "true"}}}};
    feature_list_.InitWithFeaturesAndParameters(std::move(enabled_features),
                                                {});
  }

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    scoped_glic_bypass_.emplace();

    TestingProfile::TestingFactories factories;
    factories.emplace_back(
        actor::ActorKeyedServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<actor::ActorKeyedServiceFake>(
              Profile::FromBrowserContext(context));
        }));
    factories.emplace_back(
        glic::GlicKeyedServiceFactory::GetInstance(),
        base::BindRepeating(&GlicActivityManagerTest::BuildMockGlicKeyedService,
                            base::Unretained(this)));

    profile_ = testing_profile_manager_.CreateTestingProfile(
        "profile", std::move(factories));

#if !BUILDFLAG(IS_ANDROID)
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_);
#endif
    actor_service_ = static_cast<actor::ActorKeyedServiceFake*>(
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_));

    auto* glic_service = GlicKeyedService::Get(profile_);
    ASSERT_TRUE(glic_service);
    manager_ = &glic_service->activity_manager();

    nudge_subscription_ = manager()->RegisterTaskNudgeStateChange(
        base::BindRepeating(&MockTaskNudgeStateChangeSubscriber::OnStateChanged,
                            base::Unretained(&mock_nudge_subscriber_)));

    bubble_subscription_ = manager()->RegisterTaskListBubbleStateChange(
        base::BindRepeating(&MockTaskListBubbleChangeSubscriber::OnStateChanged,
                            base::Unretained(&mock_bubble_subscriber_)));
  }

  std::unique_ptr<KeyedService> BuildMockGlicKeyedService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<testing::NiceMock<MockGlicKeyedService>>(
        profile, identity_test_env_.identity_manager(),
        testing_profile_manager_.profile_manager(), &glic_profile_manager_,
        /*contextual_cueing_service=*/nullptr,
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile));
  }

  void TearDown() override {
    nudge_subscription_ = {};
    bubble_subscription_ = {};
    manager_ = nullptr;
    actor_service_ = nullptr;
#if !BUILDFLAG(IS_ANDROID)
    display_service_tester_.reset();
#endif
    profile_ = nullptr;
    testing_profile_manager_.DeleteAllTestingProfiles();
    scoped_glic_bypass_.reset();
    testing::Test::TearDown();
  }

  ActorKeyedServiceFake* actor_service() { return actor_service_; }

  GlicActivityManager* manager() { return manager_; }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

#if !BUILDFLAG(IS_ANDROID)
  NotificationDisplayServiceTester* display_service_tester() {
    return display_service_tester_.get();
  }
#endif

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  std::optional<glic::GlicEnabling::ScopedBypassEnablementChecksForTesting>
      scoped_glic_bypass_;
  signin::IdentityTestEnvironment identity_test_env_;
  glic::GlicProfileManager glic_profile_manager_;
  raw_ptr<TestingProfile> profile_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
#endif
  raw_ptr<actor::ActorKeyedServiceFake> actor_service_;
  raw_ptr<GlicActivityManager> manager_;
  base::CallbackListSubscription nudge_subscription_;
  base::CallbackListSubscription bubble_subscription_;
  MockTaskNudgeStateChangeSubscriber mock_nudge_subscriber_;
  MockTaskListBubbleChangeSubscriber mock_bubble_subscriber_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(GlicActivityManagerTest, DefaultState) {
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
}

TEST_F(GlicActivityManagerTest, NoActiveTasks_ReturnDefaultState) {
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
}

TEST_F(GlicActivityManagerTest, NoDuplicatedTaskNudgeStateUpdates) {
  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(/*show_bubble=*/true,
                     AllOf(Field(&ActorTaskNudgeState::text,
                                 ActorTaskNudgeState::Text::kNeedsAttention))));
  // Should only be one call for default.
  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(/*show_bubble=*/false,
                     AllOf(Field(&ActorTaskNudgeState::text,
                                 ActorTaskNudgeState::Text::kDefault))));

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_1, /*from_actor=*/true);
  manager()->UpdateTaskIconComponents(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);

  actor_service()->StopTask(task_id_1,
                            actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->UpdateTaskIconComponents(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);

  TaskId task_id_2 = actor_service()->CreateTaskForTesting();
  actor_service()->StopTask(task_id_2,
                            actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->UpdateTaskIconComponents(task_id_2);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
}

TEST_F(GlicActivityManagerTest, NudgeShowsDefaultTextOnComplete) {
  EXPECT_CALL(mock_nudge_subscriber_, OnStateChanged(testing::_, testing::_))
      .Times(0);

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->StopTask(task_id_1,
                            actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->UpdateTaskIconComponents(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
}

TEST_F(GlicActivityManagerTest, PausedTaskUpdatesNudgeAndBubbleSubscribers) {
  EXPECT_CALL(mock_nudge_subscriber_,
              OnStateChanged(
                  /*show_bubble=*/true,
                  ActorTaskNudgeState{
                      .text = ActorTaskNudgeState::Text::kNeedsAttention}));
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(false));

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_1, /*from_actor=*/true);

  manager()->UpdateTaskIconComponents(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().size(), 1u);
  EXPECT_EQ(manager()->GetNumActorTasksNeedProcessing(), 1u);
}

TEST_F(GlicActivityManagerTest, ProcessingTaskInBubbleAlsoUpdatesTaskNudge) {
  EXPECT_CALL(mock_nudge_subscriber_,
              OnStateChanged(
                  /*show_bubble=*/true,
                  ActorTaskNudgeState{
                      .text = ActorTaskNudgeState::Text::kNeedsAttention}));
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(false));
  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(
          /*show_bubble=*/false,
          ActorTaskNudgeState{.text = ActorTaskNudgeState::Text::kDefault}));

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_1, /*from_actor=*/true);

  manager()->UpdateTaskIconComponents(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().size(), 1u);

  manager()->ProcessRowInTaskListBubble(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().size(), 1u);
}

TEST_F(GlicActivityManagerTest,
       MultipleTasksNeedAttentionNudgeShowsMultipleTasksText) {
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(false)).Times(2);

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  TaskId task_id_2 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_1, /*from_actor=*/true);
  manager()->OnActorTaskStateUpdate(task_id_1);
  actor_service()->PauseTaskForTesting(task_id_2, /*from_actor=*/false);
  manager()->OnActorTaskStateUpdate(task_id_2);

  manager()->UpdateTaskIconComponents(task_id_1);
  manager()->UpdateTaskIconComponents(task_id_2);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().size(), 2u);
  EXPECT_EQ(manager()->GetNumActorTasksNeedProcessing(), 1u);
}

TEST_F(GlicActivityManagerTest,
       MultipleTasksNeedAttentionRemainsInPopoverUntilAllClicked) {
  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  TaskId task_id_2 = actor_service()->CreateTaskForTesting();

  actor_service()->PauseTaskForTesting(task_id_1, /*from_actor=*/true);
  manager()->OnActorTaskStateUpdate(task_id_1);
  actor_service()->PauseTaskForTesting(task_id_2, /*from_actor=*/true);
  manager()->OnActorTaskStateUpdate(task_id_2);

  manager()->UpdateTaskIconComponents(task_id_1);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_1), true);

  manager()->UpdateTaskIconComponents(task_id_2);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_2), true);

  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().size(), 2u);

  // Process one task, the text should remain the same and all bubbles should
  // still exist.
  manager()->ProcessRowInTaskListBubble(task_id_1);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_1), false);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().size(), 2u);

  // Process the other task, the text should change to default and all bubbles
  // should still exist.
  manager()->ProcessRowInTaskListBubble(task_id_2);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_2), false);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().size(), 2u);
}

TEST_F(GlicActivityManagerTest,
       OnActorTaskRemoved_RemovesTaskAndUpdatesBubbleAndNudge) {
  // Create a task.
  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_1, /*from_actor=*/true);
  manager()->OnActorTaskStateUpdate(task_id_1);

  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_1), true);

  // Stop task.
  actor_service()->StopTask(task_id_1,
                            actor::ActorTask::StoppedReason::kTaskComplete);
  actor::ui::ActorUiStateManager::Get(profile_.get())
      ->OnUiEvent(actor::ui::StopTask(
          task_id_1, actor::ActorTask::State::kFinished, "Test Task",
          /*last_acted_on_tab_handle=*/tabs::TabHandle(),
          actor::ActorTask::TaskDuration::kDefault));
  task_environment().FastForwardBy(base::Seconds(
      features::kGlicActorUiCompletedTaskExpiryDelaySeconds.Get()));

  manager()->UpdateTaskIconComponents(task_id_1);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().size(), 0u);
}

TEST_F(GlicActivityManagerTest,
       OnActorTaskStopped_ProcessStoppedTasksAndUpdatesBubbleAndNudge) {
  // Create tasks.
  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_1, /*from_actor=*/false);
  manager()->OnActorTaskStateUpdate(task_id_1);
  TaskId task_id_2 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_2, /*from_actor=*/true);
  manager()->OnActorTaskStateUpdate(task_id_2);
  TaskId task_id_3 = actor_service()->CreateTaskForTesting();
  actor::ui::ActorUiStateManager::Get(profile_.get())
      ->OnUiEvent(actor::ui::TaskStateChanged(
          task_id_3, actor::ActorTask::State::kActing));
  manager()->OnActorTaskStateUpdate(task_id_3);

  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_1), false);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_2), true);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_3), false);

  actor_service()->StopTaskForTesting(
      task_id_1, actor::ActorTask::StoppedReason::kStoppedByUser);
  manager()->UpdateTaskIconComponents(task_id_1);
  actor_service()->StopTaskForTesting(
      task_id_2, actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->UpdateTaskIconComponents(task_id_2);
  actor_service()->StopTaskForTesting(
      task_id_3, actor::ActorTask::StoppedReason::kChromeFailure);
  manager()->UpdateTaskIconComponents(task_id_3);

  EXPECT_FALSE(manager()->actor_task_list_bubble_rows().contains(task_id_1));
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_2), true);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_3), true);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kCompleteTasks);
}

TEST_F(GlicActivityManagerTest,
       NeedsAttentionNudgePrioritizesCompleteTasksNudge) {
  base::test::ScopedFeatureList scoped_features;
  // Create tasks.
  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->StopTaskForTesting(
      task_id_1, actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->UpdateTaskIconComponents(task_id_1);
  TaskId task_id_2 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_2, /*from_actor=*/true);
  manager()->UpdateTaskIconComponents(task_id_2);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
}

TEST_F(GlicActivityManagerTest, TransientTaskDoesNotShowBubble) {
  TaskId task_id = actor_service()->CreateTransientTaskForTesting();

  // Pausing a transient task should still show the bubble (standard behavior).
  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(/*show_bubble=*/true,
                     Field(&ActorTaskNudgeState::text,
                           ActorTaskNudgeState::Text::kNeedsAttention)));
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(false));
  actor_service()->PauseTaskForTesting(task_id, /*from_actor=*/true);
  manager()->UpdateTaskIconComponents(task_id);

  // Stopping a transient task should show the nudge text but NOT the bubble.
  EXPECT_CALL(mock_nudge_subscriber_,
              OnStateChanged(/*show_bubble=*/false,
                             Field(&ActorTaskNudgeState::text,
                                   ActorTaskNudgeState::Text::kCompleteTasks)));
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(testing::_)).Times(0);
  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->UpdateTaskIconComponents(task_id);
}

TEST_F(GlicActivityManagerTest, ShouldShowBubble_FeatureModeRules) {
  // 1. Experimental Triggering task in kActing state should NOT show the
  // bubble via nudge (startup bubble triggers are handled separately).
  EXPECT_FALSE(GlicActivityManager::ShouldShowBubble(
      actor::ActorTask::State::kActing,
      actor::ActorTask::TaskDuration::kDefault,
      glic::mojom::FeatureMode::kExperimentalTriggering));

  // 2. Experimental Triggering task in kCreated state should NOT show the
  // bubble (it hasn't started acting yet).
  EXPECT_FALSE(GlicActivityManager::ShouldShowBubble(
      actor::ActorTask::State::kCreated,
      actor::ActorTask::TaskDuration::kDefault,
      glic::mojom::FeatureMode::kExperimentalTriggering));

  // 3. Non-Experimental Triggering task in kActing state should NOT show the
  // bubble (prevent UI noise).
  EXPECT_FALSE(GlicActivityManager::ShouldShowBubble(
      actor::ActorTask::State::kActing,
      actor::ActorTask::TaskDuration::kDefault,
      glic::mojom::FeatureMode::kUnspecified));

  // 4. Non-Experimental Triggering task that needs attention should still show
  // the bubble (fallback logic).
  EXPECT_TRUE(GlicActivityManager::ShouldShowBubble(
      actor::ActorTask::State::kWaitingOnUser,
      actor::ActorTask::TaskDuration::kDefault,
      glic::mojom::FeatureMode::kUnspecified));

  // 5. Experimental Triggering task in kFinished state should NOT show the
  // bubble.
  EXPECT_FALSE(GlicActivityManager::ShouldShowBubble(
      actor::ActorTask::State::kFinished,
      actor::ActorTask::TaskDuration::kDefault,
      glic::mojom::FeatureMode::kExperimentalTriggering));

  // 6. Experimental Triggering task in kFailed state should NOT show the
  // bubble.
  EXPECT_FALSE(GlicActivityManager::ShouldShowBubble(
      actor::ActorTask::State::kFailed,
      actor::ActorTask::TaskDuration::kDefault,
      glic::mojom::FeatureMode::kExperimentalTriggering));
}

TEST_F(GlicActivityManagerTest,
       ExperimentalTriggeringTaskDoesNotShowDoneNotification) {
  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();

  EXPECT_CALL(mock_nudge_subscriber_,
              OnStateChanged(/*show_bubble=*/false,
                             Field(&ActorTaskNudgeState::text,
                                   ActorTaskNudgeState::Text::kDefault)));
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(testing::_)).Times(0);
  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->UpdateTaskIconComponents(task_id);
}

TEST_F(GlicActivityManagerTest,
       ExperimentalTriggeringTaskShowsDoneNotificationWhenFlagDisabled) {
  base::test::ScopedFeatureList disabled_features;
  disabled_features.InitAndDisableFeature(
      features::kGlicExperimentalTriggeringSuppressDoneNotification);

  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();

  EXPECT_CALL(mock_nudge_subscriber_,
              OnStateChanged(/*show_bubble=*/true,
                             Field(&ActorTaskNudgeState::text,
                                   ActorTaskNudgeState::Text::kCompleteTasks)));
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(false)).Times(2);
  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->UpdateTaskIconComponents(task_id);
}

TEST_F(GlicActivityManagerTest,
       ShouldShowBubble_FeatureModeRulesWithFlagDisabled) {
  base::test::ScopedFeatureList disabled_features;
  disabled_features.InitAndDisableFeature(
      features::kGlicExperimentalTriggeringSuppressDoneNotification);

  // Experimental Triggering task in kFinished state should show the
  // bubble when suppression flag is disabled.
  EXPECT_TRUE(GlicActivityManager::ShouldShowBubble(
      actor::ActorTask::State::kFinished,
      actor::ActorTask::TaskDuration::kDefault,
      glic::mojom::FeatureMode::kExperimentalTriggering));

  // Experimental Triggering task in kFailed state should show the
  // bubble when suppression flag is disabled.
  EXPECT_TRUE(GlicActivityManager::ShouldShowBubble(
      actor::ActorTask::State::kFailed,
      actor::ActorTask::TaskDuration::kDefault,
      glic::mojom::FeatureMode::kExperimentalTriggering));
}

TEST_F(GlicActivityManagerTest, HasActiveExperimentalTask) {
  EXPECT_FALSE(manager()->HasActiveExperimentalTask());

  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();
  actor_service()->GetTask(task_id)->SetState(actor::ActorTask::State::kActing);
  actor::ui::ActorUiStateManager::Get(profile_.get())
      ->OnUiEvent(actor::ui::TaskStateChanged(
          task_id, actor::ActorTask::State::kActing));
  manager()->UpdateTaskIconComponents(task_id);
  EXPECT_TRUE(manager()->HasActiveExperimentalTask());

  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->UpdateTaskIconComponents(task_id);
  EXPECT_FALSE(manager()->HasActiveExperimentalTask());
}

TEST_F(GlicActivityManagerTest, RequiresTaskProcessing_FeatureModeRules) {
  // Experimental Triggering task in kActing state should return true.
  EXPECT_TRUE(GlicActivityManager::RequiresTaskProcessing(
      actor::ActorTask::State::kActing,
      glic::mojom::FeatureMode::kExperimentalTriggering));

  // Non-Experimental Triggering task in kActing state should return false.
  EXPECT_FALSE(GlicActivityManager::RequiresTaskProcessing(
      actor::ActorTask::State::kActing,
      glic::mojom::FeatureMode::kUnspecified));

  // Experimental Triggering task in kCreated state should return false.
  EXPECT_FALSE(GlicActivityManager::RequiresTaskProcessing(
      actor::ActorTask::State::kCreated,
      glic::mojom::FeatureMode::kExperimentalTriggering));

  // Non-Experimental Triggering task that needs attention should return true.
  EXPECT_TRUE(GlicActivityManager::RequiresTaskProcessing(
      actor::ActorTask::State::kWaitingOnUser,
      glic::mojom::FeatureMode::kUnspecified));
}

TEST_F(GlicActivityManagerTest,
       ExperimentalTriggeringTaskShowsNudgeOnlyOnFirstTurn) {
  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();
  actor::ActorTask* task = actor_service()->GetTask(task_id);

  // Turn 1 starts: state transitions to kActing.
  task->SetState(actor::ActorTask::State::kActing);

  EXPECT_CALL(mock_nudge_subscriber_,
              OnStateChanged(/*show_bubble=*/false,
                             Field(&ActorTaskNudgeState::text,
                                   ActorTaskNudgeState::Text::kDefault)));
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(true)).Times(1);

  manager()->UpdateTaskIconComponents(task_id);

  EXPECT_TRUE(manager()->actor_task_list_bubble_rows().at(task_id));

  // Turn 1 ends: state transitions to kReflecting.
  testing::Mock::VerifyAndClearExpectations(&mock_nudge_subscriber_);
  testing::Mock::VerifyAndClearExpectations(&mock_bubble_subscriber_);

  task->SetState(actor::ActorTask::State::kReflecting);

  EXPECT_CALL(mock_nudge_subscriber_, OnStateChanged(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(testing::_)).Times(0);

  manager()->UpdateTaskIconComponents(task_id);

  EXPECT_TRUE(manager()->actor_task_list_bubble_rows().at(task_id));

  // Turn 2 starts: state transitions to kActing again.
  testing::Mock::VerifyAndClearExpectations(&mock_nudge_subscriber_);
  testing::Mock::VerifyAndClearExpectations(&mock_bubble_subscriber_);

  task->SetState(actor::ActorTask::State::kActing);

  EXPECT_CALL(mock_nudge_subscriber_, OnStateChanged(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(testing::_)).Times(0);

  manager()->UpdateTaskIconComponents(task_id);
}

TEST_F(GlicActivityManagerTest, ShowsStartNotificationOnFirstActing) {
  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();
  actor::ActorTask* task = actor_service()->GetTask(task_id);

  // In kCreated state, task is updated before acting. No notification should
  // fire.
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(testing::_)).Times(0);
  manager()->UpdateTaskIconComponents(task_id);
  testing::Mock::VerifyAndClearExpectations(&mock_bubble_subscriber_);
  EXPECT_FALSE(manager()->tasks_notified_of_start().contains(task_id));

  // Transition to kActing: start notification should fire.
  task->SetState(actor::ActorTask::State::kActing);
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(true)).Times(1);
  manager()->UpdateTaskIconComponents(task_id);
  EXPECT_TRUE(manager()->actor_task_list_bubble_rows().at(task_id));
  EXPECT_TRUE(manager()->tasks_notified_of_start().contains(task_id));
}

TEST_F(GlicActivityManagerTest, StartNotificationNotRepeatedAfterRowClicked) {
  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();
  actor::ActorTask* task = actor_service()->GetTask(task_id);
  task->SetState(actor::ActorTask::State::kActing);
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(true)).Times(1);
  manager()->UpdateTaskIconComponents(task_id);
  testing::Mock::VerifyAndClearExpectations(&mock_bubble_subscriber_);
  EXPECT_TRUE(manager()->actor_task_list_bubble_rows().at(task_id));
  EXPECT_TRUE(manager()->tasks_notified_of_start().contains(task_id));

  // Process row in task list bubble (resets requires_processing).
  manager()->ProcessRowInTaskListBubble(task_id);
  EXPECT_FALSE(manager()->actor_task_list_bubble_rows().at(task_id));

  // Transition to kReflecting: start notification should not repeat, and
  // requires_processing should be refreshed to true.
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(testing::_)).Times(0);
  task->SetState(actor::ActorTask::State::kReflecting);
  manager()->UpdateTaskIconComponents(task_id);
  EXPECT_TRUE(manager()->actor_task_list_bubble_rows().at(task_id));
  EXPECT_TRUE(manager()->tasks_notified_of_start().contains(task_id));

  // Transition back to kActing: start notification should still not repeat.
  task->SetState(actor::ActorTask::State::kActing);
  manager()->UpdateTaskIconComponents(task_id);
  EXPECT_TRUE(manager()->actor_task_list_bubble_rows().at(task_id));
}

TEST_F(GlicActivityManagerTest, CancelledTaskClearsStartNotificationTracking) {
  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();
  actor::ActorTask* task = actor_service()->GetTask(task_id);
  task->SetState(actor::ActorTask::State::kActing);
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(true)).Times(1);
  manager()->UpdateTaskIconComponents(task_id);
  testing::Mock::VerifyAndClearExpectations(&mock_bubble_subscriber_);
  EXPECT_TRUE(manager()->tasks_notified_of_start().contains(task_id));

  // Cancel the task. Both row tracking and start notification tracking
  // should be erased.
  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kStoppedByUser);
  manager()->UpdateTaskIconComponents(task_id);
  EXPECT_FALSE(manager()->actor_task_list_bubble_rows().contains(task_id));
  EXPECT_FALSE(manager()->tasks_notified_of_start().contains(task_id));

  // A new active task will trigger its start notification.
  TaskId task_id_2 =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();
  actor::ActorTask* task_2 = actor_service()->GetTask(task_id_2);
  task_2->SetState(actor::ActorTask::State::kActing);
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(true)).Times(1);
  manager()->UpdateTaskIconComponents(task_id_2);
  EXPECT_TRUE(manager()->tasks_notified_of_start().contains(task_id_2));
}

#if !BUILDFLAG(IS_ANDROID)
class GlicActivityManagerOsNotificationTest : public GlicActivityManagerTest {
 public:
  GlicActivityManagerOsNotificationTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kGlicExperimentalTriggeringOsNotification);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that an OS notification is displayed when an experimental triggering
// task starts and no browser window is active.
TEST_F(GlicActivityManagerOsNotificationTest,
       ExperimentalTriggeringTask_NoActiveBrowser_ShowsNotification) {
  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();
  actor::ActorTask* task = actor_service()->GetTask(task_id);
  task->SetState(actor::ActorTask::State::kActing);
  actor::ui::ActorUiStateManager::Get(profile_.get())
      ->OnUiEvent(actor::ui::TaskStateChanged(
          task_id, actor::ActorTask::State::kActing));

  manager()->UpdateTaskIconComponents(task_id);

  std::string notification_id =
      "actor_task_start_" + base::NumberToString(task_id.value());
  auto notification =
      display_service_tester()->GetNotification(notification_id);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->id(), notification_id);
  EXPECT_EQ(display_service_tester()
                ->GetDisplayedNotificationsForType(
                    NotificationHandler::Type::GLIC_ACTOR_TASK)
                .size(),
            1u);
}

// Verifies that clicking the OS notification marks the task list row as
// processed and dismisses the notification.
TEST_F(GlicActivityManagerOsNotificationTest,
       ExperimentalTriggeringTask_NotificationClicked_ProcessesRow) {
  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();
  actor::ActorTask* task = actor_service()->GetTask(task_id);
  task->SetState(actor::ActorTask::State::kActing);
  actor::ui::ActorUiStateManager::Get(profile_.get())
      ->OnUiEvent(actor::ui::TaskStateChanged(
          task_id, actor::ActorTask::State::kActing));

  manager()->UpdateTaskIconComponents(task_id);

  std::string notification_id =
      "actor_task_start_" + base::NumberToString(task_id.value());
  ASSERT_TRUE(
      display_service_tester()->GetNotification(notification_id).has_value());
  EXPECT_TRUE(manager()->actor_task_list_bubble_rows().at(task_id));

  display_service_tester()->SimulateClick(
      NotificationHandler::Type::GLIC_ACTOR_TASK, notification_id,
      /*action_index=*/std::nullopt, /*reply=*/std::nullopt);

  EXPECT_FALSE(manager()->actor_task_list_bubble_rows().at(task_id));
  EXPECT_FALSE(
      display_service_tester()->GetNotification(notification_id).has_value());
}

// Verifies that no OS notification is displayed when the feature flag is
// disabled.
TEST_F(GlicActivityManagerTest,
       ExperimentalTriggeringTask_FeatureDisabled_DoesNotShowNotification) {
#if !BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kGlicExperimentalTriggeringOsNotification);
  NotificationDisplayServiceTester display_service_tester(profile_.get());

  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();
  actor::ActorTask* task = actor_service()->GetTask(task_id);
  task->SetState(actor::ActorTask::State::kActing);
  actor::ui::ActorUiStateManager::Get(profile_.get())
      ->OnUiEvent(actor::ui::TaskStateChanged(
          task_id, actor::ActorTask::State::kActing));

  manager()->UpdateTaskIconComponents(task_id);

  std::string notification_id =
      "actor_task_start_" + base::NumberToString(task_id.value());
  EXPECT_FALSE(
      display_service_tester.GetNotification(notification_id).has_value());
#endif
}

// Verifies that non-experimental actor tasks do not trigger an OS notification.
TEST_F(GlicActivityManagerOsNotificationTest,
       NonExperimentalTask_DoesNotShowNotification) {
  TaskId task_id = actor_service()->CreateTaskForTesting();
  actor::ActorTask* task = actor_service()->GetTask(task_id);
  task->SetState(actor::ActorTask::State::kActing);
  actor::ui::ActorUiStateManager::Get(profile_.get())
      ->OnUiEvent(actor::ui::TaskStateChanged(
          task_id, actor::ActorTask::State::kActing));

  manager()->UpdateTaskIconComponents(task_id);

  std::string notification_id =
      "actor_task_start_" + base::NumberToString(task_id.value());
  EXPECT_FALSE(
      display_service_tester()->GetNotification(notification_id).has_value());
}

// Verifies that completing an experimental triggering task automatically
// dismisses its OS notification.
TEST_F(GlicActivityManagerOsNotificationTest,
       ExperimentalTriggeringTask_TaskComplete_ClosesNotification) {
  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();
  actor::ActorTask* task = actor_service()->GetTask(task_id);
  task->SetState(actor::ActorTask::State::kActing);
  actor::ui::ActorUiStateManager::Get(profile_.get())
      ->OnUiEvent(actor::ui::TaskStateChanged(
          task_id, actor::ActorTask::State::kActing));

  manager()->UpdateTaskIconComponents(task_id);

  std::string notification_id =
      "actor_task_start_" + base::NumberToString(task_id.value());
  ASSERT_TRUE(
      display_service_tester()->GetNotification(notification_id).has_value());

  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->UpdateTaskIconComponents(task_id);

  EXPECT_FALSE(
      display_service_tester()->GetNotification(notification_id).has_value());
}

// Verifies that stopping/cancelling an experimental triggering task
// automatically dismisses its OS notification.
TEST_F(GlicActivityManagerOsNotificationTest,
       ExperimentalTriggeringTask_TaskCancelled_ClosesNotification) {
  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();
  actor::ActorTask* task = actor_service()->GetTask(task_id);
  task->SetState(actor::ActorTask::State::kActing);
  actor::ui::ActorUiStateManager::Get(profile_.get())
      ->OnUiEvent(actor::ui::TaskStateChanged(
          task_id, actor::ActorTask::State::kActing));

  manager()->UpdateTaskIconComponents(task_id);

  std::string notification_id =
      "actor_task_start_" + base::NumberToString(task_id.value());
  ASSERT_TRUE(
      display_service_tester()->GetNotification(notification_id).has_value());

  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kStoppedByUser);
  manager()->UpdateTaskIconComponents(task_id);

  EXPECT_FALSE(
      display_service_tester()->GetNotification(notification_id).has_value());
}
#endif

}  // namespace glic
