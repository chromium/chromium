// Copyright 2026 The Chromium Authors
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
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace glic {
using actor::ActorKeyedServiceFake;
using actor::TaskId;
using testing::_;
using testing::AllOf;
using testing::Field;

class MockTaskNudgeStateChangeSubscriber {
 public:
  MOCK_METHOD(void,
              OnStateChanged,
              (bool show_bubble, GlicActivityNudgeState activity_nudge_state));
};

// TODO(crbug.com/502262218): Test suite has many missing expectations for this
// mock.
class MockTaskListBubbleChangeSubscriber {
 public:
  MOCK_METHOD(void, OnStateChanged, (ActivityInterruptionLevel level));
};

class GlicActivityManagerTest : public testing::Test {
 public:
  explicit GlicActivityManagerTest(
      const std::vector<base::test::FeatureRefAndParams>& additional_features =
          {})
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlicActor, {}},
        {features::kGlicActorPolicyControlExemption, {}},
        {features::kGlicActivityHub, {}}};
    for (const auto& feat : additional_features) {
      enabled_features.push_back(feat);
    }
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

    nudge_subscription_ = manager()->RegisterActivityNudgeStateChange(
        base::BindRepeating(&MockTaskNudgeStateChangeSubscriber::OnStateChanged,
                            base::Unretained(&mock_nudge_subscriber_)));

    bubble_subscription_ = manager()->RegisterActivityAlertCallback(
        base::BindRepeating(&MockTaskListBubbleChangeSubscriber::OnStateChanged,
                            base::Unretained(&mock_bubble_subscriber_)));
  }

  std::unique_ptr<KeyedService> BuildMockGlicKeyedService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto service = std::make_unique<testing::NiceMock<MockGlicKeyedService>>(
        profile, identity_test_env_.identity_manager(),
        testing_profile_manager_.profile_manager(), &glic_profile_manager_,
        /*contextual_cueing_service=*/nullptr,
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile));
    mock_glic_service_ = service.get();
    return service;
  }

  void TearDown() override {
    nudge_subscription_ = {};
    bubble_subscription_ = {};
    manager_ = nullptr;
    mock_glic_service_ = nullptr;
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
  void OnActorTaskStateUpdate(actor::TaskId task_id) {
    if (actor::ActorTask* task = actor_service_->GetTask(task_id)) {
      auto* ui_state_manager = actor::ui::ActorUiStateManager::Get(profile_);
      if (task->GetState() == actor::ActorTask::State::kCreated) {
        ui_state_manager->OnUiEvent(actor::ui::StartTask(task_id));
      } else {
        ui_state_manager->OnUiEvent(
            actor::ui::TaskStateChanged(task_id, task->GetState()));
      }
      task_environment_.FastForwardBy(
          actor::ui::kProfileScopedUiUpdateDebounceDelay);
    }
  }
  MockGlicKeyedService* mock_glic_service() { return mock_glic_service_; }
  MockGlicInstanceCoordinator& mock_coordinator() {
    return mock_glic_service_->mock_coordinator();
  }

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
  raw_ptr<MockGlicKeyedService> mock_glic_service_ = nullptr;
  base::CallbackListSubscription nudge_subscription_;
  base::CallbackListSubscription bubble_subscription_;
  MockTaskNudgeStateChangeSubscriber mock_nudge_subscriber_;
  MockTaskListBubbleChangeSubscriber mock_bubble_subscriber_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(GlicActivityManagerTest, DefaultState) {
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kDefault);
}

TEST_F(GlicActivityManagerTest, PausedTaskUpdatesNudgeAndBubbleSubscribers) {
  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(/*show_bubble=*/true,
                     Field(&GlicActivityNudgeState::text,
                           GlicActivityNudgeState::Text::kNeedsAttention)));
  EXPECT_CALL(mock_bubble_subscriber_,
              OnStateChanged(ActivityInterruptionLevel::kPassiveNudge));

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_1, /*from_actor=*/true);

  OnActorTaskStateUpdate(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->activities().size(), 1u);
  EXPECT_EQ(manager()->GetNumUnseenActivities(), 1u);
}

TEST_F(GlicActivityManagerTest, ProcessingTaskInBubbleAlsoUpdatesTaskNudge) {
  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(/*show_bubble=*/true,
                     Field(&GlicActivityNudgeState::text,
                           GlicActivityNudgeState::Text::kNeedsAttention)));
  EXPECT_CALL(mock_bubble_subscriber_,
              OnStateChanged(ActivityInterruptionLevel::kPassiveNudge));
  EXPECT_CALL(mock_nudge_subscriber_,
              OnStateChanged(/*show_bubble=*/false,
                             Field(&GlicActivityNudgeState::text,
                                   GlicActivityNudgeState::Text::kDefault)));

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_1, /*from_actor=*/true);

  OnActorTaskStateUpdate(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->activities().size(), 1u);

  manager()->MarkActivitySeen(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kDefault);
  EXPECT_EQ(manager()->activities().size(), 1u);
}

TEST_F(GlicActivityManagerTest, MultipleTasksNeedAttention) {
  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  TaskId task_id_2 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_1, /*from_actor=*/true);
  OnActorTaskStateUpdate(task_id_1);
  actor_service()->PauseTaskForTesting(task_id_2, /*from_actor=*/true);
  OnActorTaskStateUpdate(task_id_2);

  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->activities().size(), 2u);
  EXPECT_EQ(manager()->GetNumUnseenActivities(), 2u);

  manager()->MarkActivitySeen(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->GetNumUnseenActivities(), 1u);

  manager()->MarkActivitySeen(task_id_2);
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kDefault);
  EXPECT_EQ(manager()->GetNumUnseenActivities(), 0u);
}

TEST_F(GlicActivityManagerTest,
       NeedsAttentionNudgePrioritizesCompleteTasksNudge) {
  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->StopTaskForTesting(
      task_id_1, actor::ActorTask::StoppedReason::kTaskComplete);
  OnActorTaskStateUpdate(task_id_1);
  TaskId task_id_2 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_2, /*from_actor=*/true);
  OnActorTaskStateUpdate(task_id_2);
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kNeedsAttention);
}

TEST_F(GlicActivityManagerTest, TransientTaskDoesNotShowBubble) {
  TaskId task_id = actor_service()->CreateTransientTaskForTesting();

  // Pausing a transient task should still show the bubble (standard behavior).
  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(/*show_bubble=*/true,
                     Field(&GlicActivityNudgeState::text,
                           GlicActivityNudgeState::Text::kNeedsAttention)));
  EXPECT_CALL(mock_bubble_subscriber_,
              OnStateChanged(ActivityInterruptionLevel::kPassiveNudge));
  actor_service()->PauseTaskForTesting(task_id, /*from_actor=*/true);
  OnActorTaskStateUpdate(task_id);

  // Stopping a transient task should show the nudge text but NOT the bubble.
  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(/*show_bubble=*/false,
                     Field(&GlicActivityNudgeState::text,
                           GlicActivityNudgeState::Text::kCompleteTasks)));
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(testing::_)).Times(0);
  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
  OnActorTaskStateUpdate(task_id);
}

TEST_F(GlicActivityManagerTest, ShouldShowBubble_FeatureModeRules) {
  // 1. Experimental Triggering task in kCreated and kActing state should NOT
  // show the bubble via nudge (startup bubble triggers are handled separately).
  TaskId exp_task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();
  EXPECT_CALL(mock_nudge_subscriber_, OnStateChanged(/*show_bubble=*/false, _))
      .Times(1);
  OnActorTaskStateUpdate(exp_task_id);
  testing::Mock::VerifyAndClearExpectations(&mock_nudge_subscriber_);

  actor_service()
      ->GetTask(exp_task_id)
      ->SetState(actor::ActorTask::State::kActing);
  EXPECT_CALL(mock_nudge_subscriber_, OnStateChanged(/*show_bubble=*/false, _))
      .Times(testing::AtMost(1));
  OnActorTaskStateUpdate(exp_task_id);
  testing::Mock::VerifyAndClearExpectations(&mock_nudge_subscriber_);

  // 2. Experimental Triggering task in kFinished state should NOT show the
  // bubble when done notification suppression is enabled.
  EXPECT_CALL(mock_nudge_subscriber_, OnStateChanged(/*show_bubble=*/true, _))
      .Times(0);
  actor_service()->StopTaskForTesting(
      exp_task_id, actor::ActorTask::StoppedReason::kTaskComplete);
  OnActorTaskStateUpdate(exp_task_id);
  testing::Mock::VerifyAndClearExpectations(&mock_nudge_subscriber_);

  // 3. Non-Experimental Triggering task in kActing state should NOT show the
  // bubble, but when paused by actor (needing attention) it SHOULD show the
  // bubble.
  TaskId standard_task_id = actor_service()->CreateTaskForTesting();
  actor_service()
      ->GetTask(standard_task_id)
      ->SetState(actor::ActorTask::State::kActing);
  EXPECT_CALL(mock_nudge_subscriber_, OnStateChanged(/*show_bubble=*/false, _))
      .Times(1);
  OnActorTaskStateUpdate(standard_task_id);
  testing::Mock::VerifyAndClearExpectations(&mock_nudge_subscriber_);

  EXPECT_CALL(mock_nudge_subscriber_, OnStateChanged(/*show_bubble=*/true, _))
      .Times(1);
  actor_service()->PauseTaskForTesting(standard_task_id, /*from_actor=*/true);
  OnActorTaskStateUpdate(standard_task_id);
}

TEST_F(GlicActivityManagerTest,
       ExperimentalTriggeringTaskDoesNotShowDoneNotification) {
  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();

  EXPECT_CALL(mock_nudge_subscriber_,
              OnStateChanged(/*show_bubble=*/false,
                             Field(&GlicActivityNudgeState::text,
                                   GlicActivityNudgeState::Text::kDefault)));
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(testing::_)).Times(0);
  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
  OnActorTaskStateUpdate(task_id);
}

TEST_F(GlicActivityManagerTest,
       ExperimentalTriggeringTaskShowsDoneNotificationWhenFlagDisabled) {
  base::test::ScopedFeatureList disabled_features;
  disabled_features.InitAndDisableFeature(
      features::kGlicExperimentalTriggeringSuppressDoneNotification);

  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();

  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(/*show_bubble=*/true,
                     Field(&GlicActivityNudgeState::text,
                           GlicActivityNudgeState::Text::kCompleteTasks)));
  EXPECT_CALL(mock_bubble_subscriber_,
              OnStateChanged(ActivityInterruptionLevel::kPassiveNudge))
      .Times(1);
  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(GlicActivityManagerTest,
       ShouldShowBubble_FeatureModeRulesWithFlagDisabled) {
  base::test::ScopedFeatureList disabled_features;
  disabled_features.InitAndDisableFeature(
      features::kGlicExperimentalTriggeringSuppressDoneNotification);

  // Experimental Triggering task in kFailed state should show the
  // bubble when suppression flag is disabled.
  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();
  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(/*show_bubble=*/true,
                     Field(&GlicActivityNudgeState::text,
                           GlicActivityNudgeState::Text::kCompleteTasks)));
  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kModelError);
}

TEST_F(GlicActivityManagerTest, RequiresTaskProcessing_FeatureModeRules) {
  // Experimental Triggering task in kCreated state should NOT require
  // processing (has_been_seen = true), whereas in kActing state it SHOULD
  // require processing (has_been_seen = false).
  TaskId exp_task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();
  OnActorTaskStateUpdate(exp_task_id);
  GlicActivityKey exp_key{GlicActivityType::kActorTask,
                          base::NumberToString(exp_task_id.value())};
  ASSERT_TRUE(manager()->activities().contains(exp_key));
  EXPECT_TRUE(manager()->activities().at(exp_key).has_been_seen);

  actor_service()
      ->GetTask(exp_task_id)
      ->SetState(actor::ActorTask::State::kActing);
  OnActorTaskStateUpdate(exp_task_id);
  EXPECT_FALSE(manager()->activities().at(exp_key).has_been_seen);

  // Non-Experimental Triggering task in kActing state should NOT require
  // processing (has_been_seen = true), whereas when needing attention it SHOULD
  // require processing (has_been_seen = false).
  TaskId std_task_id = actor_service()->CreateTaskForTesting();
  actor_service()
      ->GetTask(std_task_id)
      ->SetState(actor::ActorTask::State::kActing);
  OnActorTaskStateUpdate(std_task_id);
  GlicActivityKey std_key{GlicActivityType::kActorTask,
                          base::NumberToString(std_task_id.value())};
  ASSERT_TRUE(manager()->activities().contains(std_key));
  EXPECT_TRUE(manager()->activities().at(std_key).has_been_seen);

  actor_service()->PauseTaskForTesting(std_task_id, /*from_actor=*/true);
  OnActorTaskStateUpdate(std_task_id);
  EXPECT_FALSE(manager()->activities().at(std_key).has_been_seen);
}

TEST_F(GlicActivityManagerTest, ActorTaskPopulatesActivityItemDetails) {
  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();
  actor_service()->GetTask(task_id)->SetState(actor::ActorTask::State::kActing);
  OnActorTaskStateUpdate(task_id);

  GlicActivityKey key{GlicActivityType::kActorTask,
                      base::NumberToString(task_id.value())};
  ASSERT_TRUE(manager()->activities().contains(key));
  const auto& item = manager()->activities().at(key);
  EXPECT_EQ(item.type(), GlicActivityType::kActorTask);
  EXPECT_EQ(item.task_id, task_id);
  EXPECT_EQ(item.state, GlicActivityState::kInProgress);
  EXPECT_EQ(item.title, u"Test Task");
  EXPECT_EQ(item.subtitle,
            l10n_util::GetStringUTF16(
                IDS_ACTOR_TASK_LIST_BUBBLE_ROW_ACTING_TASK_SUBTITLE));
  EXPECT_NE(item.icon, nullptr);
  EXPECT_TRUE(item.has_tab);
  EXPECT_TRUE(item.is_enabled);
}

TEST_F(GlicActivityManagerTest, PausedByUserTaskKeptWithPausedState) {
  TaskId task_id = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id, /*from_actor=*/false);
  OnActorTaskStateUpdate(task_id);

  GlicActivityKey key{GlicActivityType::kActorTask,
                      base::NumberToString(task_id.value())};
  ASSERT_TRUE(manager()->activities().contains(key));
  const auto& item = manager()->activities().at(key);
  EXPECT_EQ(item.state, GlicActivityState::kPaused);
  EXPECT_TRUE(item.has_been_seen);
}

TEST_F(GlicActivityManagerTest, SeenTaskNotResetOnDuplicateStateUpdate) {
  TaskId task_id = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id, /*from_actor=*/true);
  OnActorTaskStateUpdate(task_id);

  GlicActivityKey key{GlicActivityType::kActorTask,
                      base::NumberToString(task_id.value())};
  ASSERT_TRUE(manager()->activities().contains(key));
  EXPECT_FALSE(manager()->activities().at(key).has_been_seen);

  manager()->MarkActivitySeen(task_id);
  EXPECT_TRUE(manager()->activities().at(key).has_been_seen);

  // A subsequent update in the same state must not reset has_been_seen.
  OnActorTaskStateUpdate(task_id);
  EXPECT_TRUE(manager()->activities().at(key).has_been_seen);
}

TEST_F(GlicActivityManagerTest, HasActiveActivities) {
  EXPECT_FALSE(manager()->HasUnseenOrInProgressActivities());

  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();
  actor_service()->GetTask(task_id)->SetState(actor::ActorTask::State::kActing);
  OnActorTaskStateUpdate(task_id);
  EXPECT_TRUE(manager()->HasUnseenOrInProgressActivities());

  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
  OnActorTaskStateUpdate(task_id);
  manager()->MarkActivitySeen(task_id);
  EXPECT_FALSE(manager()->HasUnseenOrInProgressActivities());
}

TEST_F(GlicActivityManagerTest, CancelledTaskClearsTracking) {
  TaskId task_id =
      actor_service()->CreateExperimentalTriggeringTaskForTesting();
  actor::ActorTask* task = actor_service()->GetTask(task_id);
  task->SetState(actor::ActorTask::State::kActing);
  OnActorTaskStateUpdate(task_id);

  GlicActivityKey key{GlicActivityType::kActorTask,
                      base::NumberToString(task_id.value())};
  EXPECT_TRUE(manager()->activities().contains(key));

  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kStoppedByUser);
  OnActorTaskStateUpdate(task_id);
  EXPECT_FALSE(manager()->activities().contains(key));
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

  OnActorTaskStateUpdate(task_id);

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

  OnActorTaskStateUpdate(task_id);

  std::string notification_id =
      "actor_task_start_" + base::NumberToString(task_id.value());
  ASSERT_TRUE(
      display_service_tester()->GetNotification(notification_id).has_value());
  GlicActivityKey key{GlicActivityType::kActorTask,
                      base::NumberToString(task_id.value())};
  ASSERT_TRUE(manager()->activities().contains(key));
  EXPECT_FALSE(manager()->activities().at(key).has_been_seen);

  display_service_tester()->SimulateClick(
      NotificationHandler::Type::GLIC_ACTOR_TASK, notification_id,
      /*action_index=*/std::nullopt, /*reply=*/std::nullopt);

  ASSERT_TRUE(manager()->activities().contains(key));
  EXPECT_TRUE(manager()->activities().at(key).has_been_seen);
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

  OnActorTaskStateUpdate(task_id);

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

  OnActorTaskStateUpdate(task_id);

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

  OnActorTaskStateUpdate(task_id);

  std::string notification_id =
      "actor_task_start_" + base::NumberToString(task_id.value());
  ASSERT_TRUE(
      display_service_tester()->GetNotification(notification_id).has_value());

  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
  OnActorTaskStateUpdate(task_id);

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

  OnActorTaskStateUpdate(task_id);

  std::string notification_id =
      "actor_task_start_" + base::NumberToString(task_id.value());
  ASSERT_TRUE(
      display_service_tester()->GetNotification(notification_id).has_value());

  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kStoppedByUser);
  OnActorTaskStateUpdate(task_id);

  EXPECT_FALSE(
      display_service_tester()->GetNotification(notification_id).has_value());
}
#endif

class MockActivityNudgeStateChangeSubscriber {
 public:
  MOCK_METHOD(void,
              OnStateChanged,
              (bool show_bubble, GlicActivityNudgeState activity_nudge_state));
};

class MockActivityBubbleChangeSubscriber {
 public:
  MOCK_METHOD(void, OnStateChanged, (ActivityInterruptionLevel level));
};

class GlicUnifiedActivityManagerTest : public GlicActivityManagerTest {
 public:
  GlicUnifiedActivityManagerTest()
      : GlicActivityManagerTest({{features::kGlicActivityHub, {}}}) {}

  void SetUp() override {
    GlicActivityManagerTest::SetUp();

    nudge_subscription_ =
        manager()->RegisterActivityNudgeStateChange(base::BindRepeating(
            &MockActivityNudgeStateChangeSubscriber::OnStateChanged,
            base::Unretained(&mock_nudge_subscriber_)));

    bubble_subscription_ = manager()->RegisterActivityAlertCallback(
        base::BindRepeating(&MockActivityBubbleChangeSubscriber::OnStateChanged,
                            base::Unretained(&mock_bubble_subscriber_)));
  }

  void TearDown() override {
    nudge_subscription_ = {};
    bubble_subscription_ = {};
    GlicActivityManagerTest::TearDown();
  }

 protected:
  base::CallbackListSubscription nudge_subscription_;
  base::CallbackListSubscription bubble_subscription_;
  MockActivityNudgeStateChangeSubscriber mock_nudge_subscriber_;
  MockActivityBubbleChangeSubscriber mock_bubble_subscriber_;
};

TEST_F(GlicUnifiedActivityManagerTest, DefaultState) {
  EXPECT_FALSE(manager()->HasUnseenOrInProgressActivities());
  EXPECT_EQ(manager()->GetNumUnseenActivities(), 0u);
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kDefault);
}

TEST_F(GlicUnifiedActivityManagerTest, ConversationGenerating_State) {
  EXPECT_CALL(mock_nudge_subscriber_,
              OnStateChanged(/*show_bubble=*/false,
                             Field(&GlicActivityNudgeState::text,
                                   GlicActivityNudgeState::Text::kDefault)))
      .Times(1);

  manager()->OnConversationActivityStateChanged(
      "conv_1", {.state = GlicActivityState::kInProgress,
                 .title = "Document Search",
                 .preview_snippet = "Searching for documents..."});

  EXPECT_TRUE(manager()->HasUnseenOrInProgressActivities());
  EXPECT_EQ(manager()->GetNumUnseenActivities(), 0u);
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kDefault);
}

TEST_F(GlicUnifiedActivityManagerTest, ConversationResponse_TriggersNudge) {
  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(/*show_bubble=*/false,
                     Field(&GlicActivityNudgeState::text,
                           GlicActivityNudgeState::Text::kResponseReady)))
      .Times(1);
  EXPECT_CALL(mock_bubble_subscriber_,
              OnStateChanged(ActivityInterruptionLevel::kPassiveNudge))
      .Times(1);

  manager()->OnConversationActivityStateChanged(
      "conv_1", {.state = GlicActivityState::kCompleted,
                 .title = "Trip Planning",
                 .preview_snippet = "Here is your itinerary."});

  EXPECT_TRUE(manager()->HasUnseenOrInProgressActivities());
  EXPECT_EQ(manager()->GetNumUnseenActivities(), 1u);
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kResponseReady);
}

TEST_F(GlicUnifiedActivityManagerTest,
       VisibleConversation_MarksSeenAndSuppressesNudge) {
  EXPECT_CALL(mock_nudge_subscriber_,
              OnStateChanged(/*show_bubble=*/false,
                             Field(&GlicActivityNudgeState::text,
                                   GlicActivityNudgeState::Text::kDefault)))
      .Times(1);
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(_)).Times(0);

  manager()->OnConversationActivityStateChanged(
      "conv_1", {.state = GlicActivityState::kCompleted,
                 .title = "Trip Planning",
                 .preview_snippet = "Here is your itinerary.",
                 .is_currently_visible = true});

  GlicActivityKey key{GlicActivityType::kConversationResponse, "conv_1"};
  ASSERT_TRUE(manager()->activities().contains(key));
  EXPECT_TRUE(manager()->activities().at(key).has_been_seen);
  EXPECT_TRUE(manager()->activities().at(key).subtitle.empty());
  EXPECT_FALSE(manager()->HasUnseenOrInProgressActivities());
  EXPECT_EQ(manager()->GetNumUnseenActivities(), 0u);
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kDefault);
}

TEST_F(GlicUnifiedActivityManagerTest,
       ConversationNeedsInput_TriggersAttentionNudge) {
  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(/*show_bubble=*/false,
                     Field(&GlicActivityNudgeState::text,
                           GlicActivityNudgeState::Text::kNeedsAttention)))
      .Times(1);
  EXPECT_CALL(mock_bubble_subscriber_,
              OnStateChanged(ActivityInterruptionLevel::kPassiveNudge))
      .Times(1);

  manager()->OnConversationActivityStateChanged(
      "conv_1", {.state = GlicActivityState::kNeedsAttention,
                 .title = "Trip Planning",
                 .preview_snippet = "Please confirm the date range."});

  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kNeedsAttention);
}

TEST_F(GlicUnifiedActivityManagerTest,
       ConversationFailed_TriggersResponseReadyNudge) {
  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(/*show_bubble=*/false,
                     Field(&GlicActivityNudgeState::text,
                           GlicActivityNudgeState::Text::kResponseReady)))
      .Times(1);
  EXPECT_CALL(mock_bubble_subscriber_,
              OnStateChanged(ActivityInterruptionLevel::kPassiveNudge))
      .Times(1);

  manager()->OnConversationActivityStateChanged(
      "conv_1", {.state = GlicActivityState::kFailed,
                 .title = "Trip Planning",
                 .preview_snippet = "Generation failed."});

  EXPECT_TRUE(manager()->HasUnseenOrInProgressActivities());
  EXPECT_EQ(manager()->GetNumUnseenActivities(), 1u);
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kResponseReady);
  EXPECT_FALSE(manager()->GetCurrentActivityNudgeState().has_attention_badge);
}

TEST_F(GlicUnifiedActivityManagerTest,
       PriorityArbitration_NeedsAttentionOverResponseReady) {
  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(/*show_bubble=*/false,
                     Field(&GlicActivityNudgeState::text,
                           GlicActivityNudgeState::Text::kResponseReady)))
      .Times(1);

  manager()->OnConversationActivityStateChanged(
      "conv_1", {.state = GlicActivityState::kCompleted,
                 .title = "Conversation 1",
                 .preview_snippet = "Response 1 ready"});

  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(/*show_bubble=*/false,
                     Field(&GlicActivityNudgeState::text,
                           GlicActivityNudgeState::Text::kNeedsAttention)))
      .Times(1);

  manager()->OnConversationActivityStateChanged(
      "conv_2", {.state = GlicActivityState::kNeedsAttention,
                 .title = "Conversation 2",
                 .preview_snippet = "Confirmation required"});

  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kNeedsAttention);
}

TEST_F(GlicUnifiedActivityManagerTest,
       PluralityCalculation_MultipleConversations) {
  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(/*show_bubble=*/false,
                     Field(&GlicActivityNudgeState::text,
                           GlicActivityNudgeState::Text::kResponseReady)))
      .Times(1);

  manager()->OnConversationActivityStateChanged(
      "conv_1", {.state = GlicActivityState::kCompleted,
                 .title = "Conversation 1",
                 .preview_snippet = "Done 1"});

  EXPECT_CALL(mock_nudge_subscriber_,
              OnStateChanged(
                  /*show_bubble=*/false,
                  AllOf(Field(&GlicActivityNudgeState::text,
                              GlicActivityNudgeState::Text::kResponseReady),
                        Field(&GlicActivityNudgeState::count, 2u))))
      .Times(1);

  manager()->OnConversationActivityStateChanged(
      "conv_2", {.state = GlicActivityState::kCompleted,
                 .title = "Conversation 2",
                 .preview_snippet = "Done 2"});

  EXPECT_EQ(manager()->GetNumUnseenActivities(), 2u);
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kResponseReady);
}

TEST_F(GlicUnifiedActivityManagerTest, DismissalLifecycle_MarkSeen) {
  manager()->OnConversationActivityStateChanged(
      "conv_1", {.state = GlicActivityState::kCompleted,
                 .title = "Conversation 1",
                 .preview_snippet = "Done"});

  EXPECT_EQ(manager()->GetNumUnseenActivities(), 1u);

  GlicActivityKey key{GlicActivityType::kConversationResponse, "conv_1"};
  EXPECT_EQ(manager()->activities().at(key).subtitle, u"Done");
  manager()->MarkActivitySeen(key);

  EXPECT_EQ(manager()->GetNumUnseenActivities(), 0u);
  EXPECT_TRUE(manager()->activities().at(key).subtitle.empty());
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kDefault);
}

TEST_F(GlicUnifiedActivityManagerTest, ConversationRemoved_RemovesActivity) {
  manager()->OnConversationActivityStateChanged(
      "conv_1", {.state = GlicActivityState::kCompleted,
                 .title = "Conversation 1",
                 .preview_snippet = "Done"});
  EXPECT_TRUE(manager()->HasUnseenOrInProgressActivities());

  manager()->OnConversationDestroyed("conv_1");
  EXPECT_FALSE(manager()->HasUnseenOrInProgressActivities());
  EXPECT_EQ(manager()->GetNumUnseenActivities(), 0u);
}

TEST_F(GlicUnifiedActivityManagerTest, ActorTaskStateTransitions) {
  TaskId task_id = actor_service()->CreateTaskForTesting();
  OnActorTaskStateUpdate(task_id);

  EXPECT_TRUE(manager()->HasUnseenOrInProgressActivities());
  EXPECT_EQ(manager()->GetNumUnseenActivities(), 0u);

  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(/*show_bubble=*/true,
                     Field(&GlicActivityNudgeState::text,
                           GlicActivityNudgeState::Text::kNeedsAttention)))
      .Times(1);

  actor_service()->PauseTaskForTesting(task_id, /*from_actor=*/true);
  OnActorTaskStateUpdate(task_id);

  EXPECT_EQ(manager()->GetNumUnseenActivities(), 1u);
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kNeedsAttention);

  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(/*show_bubble=*/true,
                     Field(&GlicActivityNudgeState::text,
                           GlicActivityNudgeState::Text::kCompleteTasks)))
      .Times(1);

  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
  OnActorTaskStateUpdate(task_id);

  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kCompleteTasks);
}

TEST_F(GlicUnifiedActivityManagerTest,
       SimultaneousActorTaskAndConversationResponse) {
  // Start with a completed ActorTask -> kCompleteTasks with count 1.
  TaskId task_id = actor_service()->CreateTaskForTesting();
  actor_service()->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
  OnActorTaskStateUpdate(task_id);

  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kCompleteTasks);
  EXPECT_EQ(manager()->GetNumUnseenActivities(), 1u);

  // Add a completed ConversationResponse -> ActorTask (kCompleteTasks) remains
  // prioritized over non-actuation conversations with combined unseen count 2.
  manager()->OnConversationActivityStateChanged(
      "conv_1", {.state = GlicActivityState::kCompleted,
                 .title = "Conversation 1",
                 .preview_snippet = "Response ready"});
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kCompleteTasks);
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().count, 2u);
  EXPECT_EQ(manager()->GetNumUnseenActivities(), 2u);

  // Marking the ActorTask seen leaves the unseen ConversationResponse
  // (kResponseReady).
  manager()->MarkActivitySeen(task_id);
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().text,
            GlicActivityNudgeState::Text::kResponseReady);
  EXPECT_EQ(manager()->GetCurrentActivityNudgeState().count, 1u);
}

TEST_F(GlicUnifiedActivityManagerTest, ConversationTitleUpdated_UpdatesTitle) {
  manager()->OnConversationActivityStateChanged(
      "conv_1",
      {.state = GlicActivityState::kCompleted, .preview_snippet = "Done"});

  GlicActivityKey key{GlicActivityType::kConversationResponse, "conv_1"};
  ASSERT_TRUE(manager()->activities().contains(key));
  EXPECT_TRUE(manager()->activities().at(key).title.empty());

  EXPECT_CALL(mock_bubble_subscriber_,
              OnStateChanged(ActivityInterruptionLevel::kSilent))
      .Times(1);
  manager()->OnConversationTitleUpdated("conv_1", "Updated Title");
  EXPECT_EQ(manager()->activities().at(key).title, u"Updated Title");

  // Updating title of a non-existent conversation should not crash or insert.
  manager()->OnConversationTitleUpdated("unknown_conv", "Unknown Title");
  GlicActivityKey unknown_key{GlicActivityType::kConversationResponse,
                              "unknown_conv"};
  EXPECT_FALSE(manager()->activities().contains(unknown_key));
}

TEST_F(GlicUnifiedActivityManagerTest,
       ConversationGenerating_ClearsPreviousTurnSnippet) {
  manager()->OnConversationActivityStateChanged(
      "conv_1", {.state = GlicActivityState::kCompleted,
                 .title = "Conversation 1",
                 .preview_snippet = "Previous turn response"});

  GlicActivityKey key{GlicActivityType::kConversationResponse, "conv_1"};
  ASSERT_TRUE(manager()->activities().contains(key));
  EXPECT_EQ(manager()->activities().at(key).subtitle,
            u"Previous turn response");

  // Starting a new turn without a snippet clears the previous turn's snippet.
  manager()->OnConversationActivityStateChanged(
      "conv_1", {.state = GlicActivityState::kInProgress});
  EXPECT_TRUE(manager()->activities().at(key).subtitle.empty());

  // If a preview snippet is provided while generating, it updates the subtitle.
  manager()->OnConversationActivityStateChanged(
      "conv_1", {.state = GlicActivityState::kInProgress,
                 .preview_snippet = "Streaming partial..."});
  EXPECT_EQ(manager()->activities().at(key).subtitle, u"Streaming partial...");

  // Completing a turn without a snippet also clears intermediate partial text.
  manager()->OnConversationActivityStateChanged(
      "conv_1", {.state = GlicActivityState::kCompleted});
  EXPECT_TRUE(manager()->activities().at(key).subtitle.empty());
}

TEST_F(GlicUnifiedActivityManagerTest, ConversationDestroyed_ClearsActivity) {
  manager()->OnConversationActivityStateChanged(
      "conv_1", {.state = GlicActivityState::kCompleted,
                 .title = "Conversation 1",
                 .preview_snippet = "Response preview"});

  EXPECT_TRUE(manager()->HasUnseenOrInProgressActivities());
  EXPECT_EQ(manager()->GetNumUnseenActivities(), 1u);

  EXPECT_CALL(mock_nudge_subscriber_,
              OnStateChanged(/*show_bubble=*/false,
                             Field(&GlicActivityNudgeState::text,
                                   GlicActivityNudgeState::Text::kDefault)))
      .Times(1);

  manager()->OnConversationDestroyed("conv_1");

  EXPECT_FALSE(manager()->HasUnseenOrInProgressActivities());
  EXPECT_EQ(manager()->GetNumUnseenActivities(), 0u);
}
}  // namespace glic
