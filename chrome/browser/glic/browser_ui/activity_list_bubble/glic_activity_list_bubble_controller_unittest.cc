// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_list_bubble_controller.h"

#include <memory>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_list_bubble.h"
#include "chrome/browser/glic/browser_ui/glic_actor_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_controller.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_activity_manager.h"
#include "chrome/browser/glic/public/service/legacy_glic_activity_manager.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace glic {

namespace {

class MockGlicActorNudgeController : public GlicActorNudgeController {
 public:
  MockGlicActorNudgeController(
      BrowserWindowInterface* browser,
      GlicSplitButtonController* split_button_controller)
      : GlicActorNudgeController(browser, split_button_controller) {}
  ~MockGlicActorNudgeController() override = default;

  void ShowBubble() override {}
  void CloseBubble() override {}
};

class MockGlicSplitButtonController : public GlicSplitButtonController {
 public:
  MockGlicSplitButtonController(BrowserWindowInterface* browser,
                                GlicKeyedService* glic_service)
      : GlicSplitButtonController(browser, glic_service) {
    if (!base::FeatureList::IsEnabled(features::kGlicActivityHub)) {
      SetActorNudgeControllerForTesting(nullptr);
      SetActorNudgeControllerForTesting(
          std::make_unique<MockGlicActorNudgeController>(browser, this));
    }
  }
  ~MockGlicSplitButtonController() override = default;

  MOCK_METHOD(glic::GlicSplitButtonDelegate*,
              GetActiveDelegate,
              (),
              (override));
};

class TestGlicSplitButtonDelegate : public glic::GlicSplitButtonDelegate {
 public:
  TestGlicSplitButtonDelegate(views::View* anchor_view,
                              GlicActivityListBubbleController* controller)
      : anchor_view_(anchor_view), controller_(controller) {}

  void ShowActivityListBubble() override {
    if (bubble_) {
      bubble_->Close();
      bubble_.reset();
    }
    if (controller_) {
      bubble_ = std::make_unique<GlicActivityListBubble>(
          controller_->CreateRowViewModels(),
          base::BindRepeating(
              &GlicActivityListBubbleController::OnBubbleDestroyed,
              controller_->GetWeakPtr()));
      bubble_->Show(anchor_view_);
    }
  }

  void CloseActivityListBubble() override {
    if (bubble_) {
      bubble_->Close();
    }
  }

  bool IsActivityListBubbleShowing() override {
    return bubble_ && bubble_->IsShowing();
  }

  views::Widget* bubble_widget() {
    return bubble_ ? bubble_->widget() : nullptr;
  }

 private:
  raw_ptr<views::View> anchor_view_;
  raw_ptr<GlicActivityListBubbleController> controller_;
  std::unique_ptr<GlicActivityListBubble> bubble_;
};

}  // namespace

class GlicActivityListBubbleControllerTest : public ChromeViewsTestBase {
 public:
  GlicActivityListBubbleControllerTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlicActor, {}},
        {features::kGlicActorPolicyControlExemption, {}}};
    feature_list_.InitWithFeaturesAndParameters(std::move(enabled_features),
                                                {});
  }

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();

    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());

    profile_ = testing_profile_manager_->CreateTestingProfile(
        "profile",
        TestingProfile::TestingFactories{
            TestingProfile::TestingFactory{
                actor::ActorKeyedServiceFactory::GetInstance(),
                base::BindRepeating(&GlicActivityListBubbleControllerTest::
                                        BuildActorKeyedService,
                                    base::Unretained(this))},
            TestingProfile::TestingFactory{
                GlicKeyedServiceFactory::GetInstance(),
                base::BindRepeating(&GlicActivityListBubbleControllerTest::
                                        BuildMockGlicKeyedService,
                                    base::Unretained(this))}});

    glic_test_env_.SetupProfile(profile_);

    browser_window_interface_ = std::make_unique<MockBrowserWindowInterface>();
    ON_CALL(*browser_window_interface_, GetUnownedUserDataHost)
        .WillByDefault(::testing::ReturnRef(user_data_host_));
    ON_CALL(*browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile_));
    ON_CALL(*browser_window_interface_, IsActive())
        .WillByDefault(testing::Return(true));
    mock_glic_split_button_controller_ =
        std::make_unique<testing::NiceMock<MockGlicSplitButtonController>>(
            browser_window_interface_.get(), mock_glic_keyed_service_);
    controller_ =
        GlicActivityListBubbleController::From(browser_window_interface_.get());
    ASSERT_TRUE(controller_);
    test_delegate_ = std::make_unique<TestGlicSplitButtonDelegate>(
        anchor_widget_->GetContentsView(), controller_);
    ON_CALL(*mock_glic_split_button_controller_, GetActiveDelegate())
        .WillByDefault(testing::Return(test_delegate_.get()));
  }

  std::unique_ptr<KeyedService> BuildActorKeyedService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto actor_keyed_service =
        std::make_unique<actor::ActorKeyedServiceFake>(profile);

    return std::move(actor_keyed_service);
  }

  std::unique_ptr<KeyedService> BuildMockGlicKeyedService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto mock_service =
        std::make_unique<testing::NiceMock<MockGlicKeyedService>>(
            profile, identity_test_env_.identity_manager(),
            testing_profile_manager_->profile_manager(), &glic_profile_manager_,
            /*contextual_cueing_service=*/nullptr,
            actor::ActorKeyedServiceFactory::GetActorKeyedService(profile));
    mock_glic_keyed_service_ = mock_service.get();
    return mock_service;
  }

  void TearDown() override {
    test_delegate_.reset();
    controller_ = nullptr;
    mock_glic_split_button_controller_.reset();
    browser_window_interface_.reset();
    mock_glic_keyed_service_ = nullptr;
    profile_ = nullptr;
    testing_profile_manager_.reset();
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void Click(views::Button* button) {
    button->OnMousePressed(
        ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(1, 1),
                       gfx::Point(0, 0), base::TimeTicks::Now(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
    button->OnMouseReleased(
        ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(1, 1),
                       gfx::Point(0, 0), base::TimeTicks::Now(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  }

 protected:
  views::View* GetContentViewInActorTaskListBubble(
      views::Widget* actor_task_list_bubble) {
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForView(
            actor_task_list_bubble->widget_delegate()
                ->AsBubbleDialogDelegate()
                ->GetAnchorView());
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
        kActorTaskListBubbleView, context);
  }

  GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  signin::IdentityTestEnvironment identity_test_env_;
  GlicProfileManager glic_profile_manager_;
  GlicUnitTestEnvironment glic_test_env_;
  raw_ptr<MockGlicKeyedService> mock_glic_keyed_service_ = nullptr;
  std::unique_ptr<MockGlicSplitButtonController>
      mock_glic_split_button_controller_;
  std::unique_ptr<TestGlicSplitButtonDelegate> test_delegate_;
  raw_ptr<GlicActivityListBubbleController> controller_ = nullptr;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  ui::UnownedUserDataHost user_data_host_;
  views::UniqueWidgetPtr anchor_widget_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(GlicActivityListBubbleControllerTest, ShowBubbleRecordsHistogram) {
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  glic::LegacyGlicActivityManager* manager =
      &mock_glic_keyed_service_->legacy_activity_manager();
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor_service->GetTask(task_id)->Pause(true);
  manager->UpdateTaskIconComponents(task_id);

  base::HistogramTester histogram_tester;

  controller_->ShowBubble();

  histogram_tester.ExpectBucketCount("Actor.Ui.TaskListBubble.Rows", 1, 1);

  // Close the bubble so IsActorTaskListBubbleShowing() becomes false. Now that
  // LegacyGlicActivityManager is a real instance wired to GlicKeyedService,
  // subsequent task mutations in the loop below would otherwise trigger
  // OnStateUpdate() -> ShowBubble() on each intermediate task while the
  // bubble is open, recording premature intermediate histogram samples.
  controller_->CloseBubble();

  // Stop previous task, Add and Pause 3 more tasks, and ensure histogram bucket
  // for 1 row stays the same while the bucket for 3 rows is incremented.
  actor_service->StopTask(task_id,
                          actor::ActorTask::StoppedReason::kTaskComplete);
  manager->UpdateTaskIconComponents(task_id);

  for (int i = 0; i < 3; i++) {
    actor::TaskId new_task_id = actor_service->CreateTask(
        actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
    actor_service->GetTask(new_task_id)->Pause(true);
    manager->UpdateTaskIconComponents(new_task_id);
  }

  controller_->ShowBubble();

  histogram_tester.ExpectBucketCount("Actor.Ui.TaskListBubble.Rows", 1, 1);
  histogram_tester.ExpectBucketCount("Actor.Ui.TaskListBubble.Rows", 4, 1);

  EXPECT_EQ(
      2u,
      histogram_tester.GetAllSamples("Actor.Ui.TaskListBubble.Rows").size());
}

TEST_F(GlicActivityListBubbleControllerTest,
       ShowBubble_InactiveBrowserWindow_NoStartNotification) {
  // If the browser window is inactive and it is NOT a start notification,
  // ShowBubble should return early and NOT show the bubble.
  EXPECT_CALL(*browser_window_interface_, IsActive())
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(*mock_glic_keyed_service_, IsPanelShowingForBrowser(testing::_))
      .WillRepeatedly(testing::Return(true));

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  glic::LegacyGlicActivityManager* manager =
      &mock_glic_keyed_service_->legacy_activity_manager();
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor_service->GetTask(task_id)->Pause(true);
  manager->UpdateTaskIconComponents(task_id);

  controller_->ShowBubble(
      /*is_start_notification=*/false);

  // Bubble widget should not be created.
  EXPECT_FALSE(controller_->IsBubbleShowing());
}

TEST_F(GlicActivityListBubbleControllerTest,
       ShowBubble_InactiveBrowserWindow_StartNotification_PanelNotShowing) {
  // If the browser window is inactive, and it IS a start notification,
  // but Glic panel is NOT showing, ShowBubble should return early and NOT show
  // the bubble.
  EXPECT_CALL(*browser_window_interface_, IsActive())
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(*mock_glic_keyed_service_, IsPanelShowingForBrowser(testing::_))
      .WillRepeatedly(testing::Return(false));

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  glic::LegacyGlicActivityManager* manager =
      &mock_glic_keyed_service_->legacy_activity_manager();
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor_service->GetTask(task_id)->Pause(true);
  manager->UpdateTaskIconComponents(task_id);

  controller_->ShowBubble(
      /*is_start_notification=*/true);

  // Bubble widget should not be created.
  EXPECT_FALSE(controller_->IsBubbleShowing());
}

TEST_F(GlicActivityListBubbleControllerTest,
       ShowBubble_InactiveBrowserWindow_StartNotification_PanelShowing) {
  // If the browser window is inactive, and it IS a start notification,
  // and Glic panel IS showing, ShowBubble should show the bubble.
  EXPECT_CALL(*browser_window_interface_, IsActive())
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(*mock_glic_keyed_service_, IsPanelShowingForBrowser(testing::_))
      .WillRepeatedly(testing::Return(true));

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  glic::LegacyGlicActivityManager* manager =
      &mock_glic_keyed_service_->legacy_activity_manager();
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor_service->GetTask(task_id)->Pause(true);
  manager->UpdateTaskIconComponents(task_id);

  controller_->ShowBubble(
      /*is_start_notification=*/true);

  // Fast forward for delayed show.
  task_environment()->FastForwardBy(
      base::Milliseconds(features::kGlicActorUiTaskListBubbleDelayMs.Get()));

  // Bubble widget should be created and visible.
  EXPECT_TRUE(controller_->IsBubbleShowing());
}

TEST_F(GlicActivityListBubbleControllerTest, ShowBubble_DelayedWhenIconHidden) {
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  glic::LegacyGlicActivityManager* manager =
      &mock_glic_keyed_service_->legacy_activity_manager();
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor_service->GetTask(task_id)->Pause(true);
  manager->UpdateTaskIconComponents(task_id);

  // Set anchor view to hidden.
  views::View* anchor_view = anchor_widget_->GetContentsView();
  anchor_view->SetVisible(false);

  controller_->ShowBubble(
      /*is_start_notification=*/true);

  // Bubble widget should NOT be created immediately.
  EXPECT_FALSE(controller_->IsBubbleShowing());

  // Fast forward by delay ms.
  task_environment()->FastForwardBy(
      base::Milliseconds(features::kGlicActorUiTaskListBubbleDelayMs.Get()));

  // Bubble widget should now be created and visible.
  EXPECT_TRUE(controller_->IsBubbleShowing());
}

TEST_F(GlicActivityListBubbleControllerTest,
       ShowBubble_NotDelayedWhenIconVisible) {
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  glic::LegacyGlicActivityManager* manager =
      &mock_glic_keyed_service_->legacy_activity_manager();
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor_service->GetTask(task_id)->Pause(true);
  manager->UpdateTaskIconComponents(task_id);

  // Set anchor view to visible.
  views::View* anchor_view = anchor_widget_->GetContentsView();
  anchor_view->SetVisible(true);

  controller_->ShowBubble(
      /*is_start_notification=*/false);

  // Bubble widget should be created immediately.
  EXPECT_TRUE(controller_->IsBubbleShowing());
}

TEST_F(GlicActivityListBubbleControllerTest,
       OnActivityAlert_InterruptionLevels) {
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  glic::LegacyGlicActivityManager* manager =
      &mock_glic_keyed_service_->legacy_activity_manager();
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor_service->GetTask(task_id)->Pause(true);
  manager->UpdateTaskIconComponents(task_id);

  // 1. Silent alert does not pop open the bubble when closed.
  controller_->OnActivityAlert(ActivityInterruptionLevel::kSilent);
  EXPECT_FALSE(controller_->IsBubbleShowing());

  // 2. Passive nudge alert does not pop open the bubble when closed.
  controller_->OnActivityAlert(ActivityInterruptionLevel::kPassiveNudge);
  EXPECT_FALSE(controller_->IsBubbleShowing());

  // 3. Delayed task start alert shows bubble with delay.
  controller_->OnActivityAlert(ActivityInterruptionLevel::kDelayedTaskStart);
  EXPECT_FALSE(controller_->IsBubbleShowing());

  task_environment()->FastForwardBy(
      base::Milliseconds(features::kGlicActorUiTaskListBubbleDelayMs.Get()));
  EXPECT_TRUE(controller_->IsBubbleShowing());

  // When bubble is already showing, a passive nudge updates it without closing
  // it.
  controller_->OnActivityAlert(ActivityInterruptionLevel::kPassiveNudge);
  EXPECT_TRUE(controller_->IsBubbleShowing());
}

TEST_F(GlicActivityListBubbleControllerTest,
       GetActorTaskRowViewModels_PriorityOrder) {
  auto* fake_actor_service = static_cast<actor::ActorKeyedServiceFake*>(
      actor::ActorKeyedService::Get(profile_));

  actor::TaskId id_1 = fake_actor_service->CreateTaskForTesting();
  fake_actor_service->PauseTaskForTesting(id_1, /*from_actor=*/true);

  actor::TaskId id_2 = fake_actor_service->CreateTaskForTesting();
  fake_actor_service->PauseTaskForTesting(id_2, /*from_actor=*/true);

  actor::TaskId id_3 = fake_actor_service->CreateTaskForTesting();
  fake_actor_service->StopTaskForTesting(
      id_3, actor::ActorTask::StoppedReason::kTaskComplete);

  actor::TaskId id_4 = fake_actor_service->CreateTaskForTesting();

  auto rows = GlicActivityListBubbleController::GetActorTaskRowViewModels(
      profile_, {{id_1, true}, {id_2, false}, {id_3, true}, {id_4, false}});

  ASSERT_EQ(4u, rows.size());
  EXPECT_EQ(
      id_1,
      rows[0].task_id.value());  // Priority 1: Unprocessed needing attention
  EXPECT_EQ(
      id_2,
      rows[1].task_id.value());  // Priority 2: Processed needing attention
  EXPECT_EQ(
      id_3,
      rows[2].task_id.value());  // Priority 3: Remaining needing processing
  EXPECT_EQ(id_4, rows[3].task_id.value());  // Priority 4: All other tasks
}

TEST_F(GlicActivityListBubbleControllerTest,
       GetActorTaskRowViewModels_TieBreakByTaskIdAndContents) {
  auto* fake_actor_service = static_cast<actor::ActorKeyedServiceFake*>(
      actor::ActorKeyedService::Get(profile_));

  actor::TaskId id_1 = fake_actor_service->CreateTaskForTesting();
  fake_actor_service->PauseTaskForTesting(id_1, /*from_actor=*/true);

  actor::TaskId id_2 = fake_actor_service->CreateTaskForTesting();
  fake_actor_service->PauseTaskForTesting(id_2, /*from_actor=*/true);

  actor::TaskId id_exp =
      fake_actor_service->CreateExperimentalTriggeringTaskForTesting();
  fake_actor_service->GetTask(id_exp)->SetState(
      actor::ActorTask::State::kActing);

  // id_1 and id_2 tie on priority 1; id_exp is priority 3; 99999 does not
  // exist.
  auto rows = GlicActivityListBubbleController::GetActorTaskRowViewModels(
      profile_, {{id_2, true},
                 {id_1, true},
                 {id_exp, true},
                 {actor::TaskId(99999), true}});

  ASSERT_EQ(3u, rows.size());

  // Tie-break by task_id ascending + verify row contents.
  EXPECT_EQ(id_1, rows[0].task_id.value());
  EXPECT_EQ(u"Test Task", rows[0].title);
  EXPECT_FALSE(rows[0].has_been_seen);
  EXPECT_FALSE(rows[0].has_tab);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ACTOR_TASK_LIST_BUBBLE_ROW_TAB_CLOSED_SUBTITLE),
            rows[0].subtitle);
  EXPECT_TRUE(rows[0].is_enabled);
  EXPECT_TRUE(rows[0].needs_review);

  // id_2 does not have an associated tab and displays "Tab closed".
  EXPECT_EQ(id_2, rows[1].task_id.value());
  EXPECT_FALSE(rows[1].has_tab);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ACTOR_TASK_LIST_BUBBLE_ROW_TAB_CLOSED_SUBTITLE),
            rows[1].subtitle);

  // Experimental triggering overrides has_tab to true.
  EXPECT_EQ(id_exp, rows[2].task_id.value());
  EXPECT_TRUE(rows[2].has_tab);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ACTOR_TASK_LIST_BUBBLE_ROW_ACTING_TASK_SUBTITLE),
            rows[2].subtitle);
  EXPECT_TRUE(rows[2].is_enabled);
  EXPECT_FALSE(rows[2].needs_review);
}

class GlicActivityListBubbleControllerOsNotificationTest
    : public GlicActivityListBubbleControllerTest {
 public:
  GlicActivityListBubbleControllerOsNotificationTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kGlicExperimentalTriggeringOsNotification);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GlicActivityListBubbleControllerOsNotificationTest,
       ShowBubble_InactiveBrowserWindow_DoesNotShowBubble) {
  // If the browser window is inactive and
  // kGlicExperimentalTriggeringOsNotification is enabled, ShowBubble should
  // return early and NOT show the bubble.
  EXPECT_CALL(*browser_window_interface_, IsActive())
      .WillRepeatedly(testing::Return(false));

  controller_->ShowBubble(
      /*is_start_notification=*/true);

  // Fast forward for delayed show.
  task_environment()->FastForwardBy(
      base::Milliseconds(features::kGlicActorUiTaskListBubbleDelayMs.Get()));

  // Bubble widget should not be created.
  EXPECT_FALSE(controller_->IsBubbleShowing());
}

TEST_F(GlicActivityListBubbleControllerTest,
       CloseBubbleFiresOnBubbleDestroyedCallback) {
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  glic::LegacyGlicActivityManager* manager =
      &mock_glic_keyed_service_->legacy_activity_manager();
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor_service->GetTask(task_id)->Pause(true);
  manager->UpdateTaskIconComponents(task_id);

  bool destroyed_called = false;
  auto sub = controller_->RegisterBubbleDestroyedCallback(
      base::BindRepeating([](bool* flag) { *flag = true; }, &destroyed_called));

  controller_->ShowBubble();
  ASSERT_TRUE(controller_->IsBubbleShowing());
  views::Widget* widget = test_delegate_->bubble_widget();
  ASSERT_TRUE(widget);

  views::test::WidgetDestroyedWaiter waiter(widget);
  controller_->CloseBubble();
  waiter.Wait();

  EXPECT_TRUE(destroyed_called);
}

class GlicActivityListBubbleControllerActivityHubTest
    : public GlicActivityListBubbleControllerTest {
 public:
  GlicActivityListBubbleControllerActivityHubTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicActivityHub);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GlicActivityListBubbleControllerActivityHubTest,
       CreateRowViewModels_ActorTasks) {
  auto* fake_actor_service = static_cast<actor::ActorKeyedServiceFake*>(
      actor::ActorKeyedService::Get(profile_));
  auto* activity_manager = &mock_glic_keyed_service_->activity_manager();
  ASSERT_TRUE(activity_manager);

  actor::TaskId id_1 = fake_actor_service->CreateTaskForTesting();
  fake_actor_service->PauseTaskForTesting(id_1, /*from_actor=*/true);
  activity_manager->OnTabAddedToTask(id_1);

  actor::TaskId id_2 = fake_actor_service->CreateTaskForTesting();
  activity_manager->OnTabAddedToTask(id_2);

  auto rows = controller_->CreateRowViewModels();
  ASSERT_EQ(2u, rows.size());
  EXPECT_EQ(id_1, rows[0].task_id);
  EXPECT_TRUE(rows[0].needs_review);
  EXPECT_FALSE(rows[0].has_been_seen);
  EXPECT_EQ(id_2, rows[1].task_id);
  EXPECT_FALSE(rows[1].needs_review);

  // Clicking the row marks the activity as seen in GlicActivityManager.
  ASSERT_TRUE(rows[0].on_clicked);
  rows[0].on_clicked.Run();

  GlicActivityKey key{GlicActivityType::kActorTask,
                      base::NumberToString(id_1.value())};
  auto it = activity_manager->activities().find(key);
  ASSERT_NE(it, activity_manager->activities().end());
  EXPECT_TRUE(it->second.has_been_seen);
}

TEST_F(GlicActivityListBubbleControllerActivityHubTest,
       OnActivityAlert_InterruptionLevels) {
  auto* activity_manager = &mock_glic_keyed_service_->activity_manager();
  ASSERT_TRUE(activity_manager);

  // Seed an in-progress conversation activity so there is a displayable row
  // without immediately triggering an automatic bubble open on nudge update.
  activity_manager->OnConversationActivityStateChanged(
      "conv_1", {.state = GlicActivityState::kInProgress,
                 .title = "Chat 1",
                 .preview_snippet = "Working..."});

  // 1. Silent alert does not pop open the bubble when closed.
  controller_->OnActivityAlert(ActivityInterruptionLevel::kSilent);
  EXPECT_FALSE(controller_->IsBubbleShowing());

  // 2. Passive nudge alert does not pop open the bubble when closed.
  controller_->OnActivityAlert(ActivityInterruptionLevel::kPassiveNudge);
  EXPECT_FALSE(controller_->IsBubbleShowing());

  // 3. Delayed task start alert shows bubble with delay.
  controller_->OnActivityAlert(ActivityInterruptionLevel::kDelayedTaskStart);
  EXPECT_FALSE(controller_->IsBubbleShowing());

  task_environment()->FastForwardBy(
      base::Milliseconds(features::kGlicActorUiTaskListBubbleDelayMs.Get()));
  ASSERT_TRUE(controller_->IsBubbleShowing());
  views::Widget* first_widget = test_delegate_->bubble_widget();
  ASSERT_TRUE(first_widget);
  EXPECT_EQ(
      1u, GetContentViewInActorTaskListBubble(first_widget)->children().size());

  // 4. When the bubble is already showing, a passive nudge (e.g., from a new
  // conversation response) refreshes the open bubble with the updated rows.
  views::test::WidgetDestroyedWaiter waiter(first_widget);
  activity_manager->OnConversationActivityStateChanged(
      "conv_2", {.state = GlicActivityState::kCompleted,
                 .title = "Chat 2",
                 .preview_snippet = "Response ready"});
  waiter.Wait();
  ASSERT_TRUE(controller_->IsBubbleShowing());
  EXPECT_EQ(2u,
            GetContentViewInActorTaskListBubble(test_delegate_->bubble_widget())
                ->children()
                .size());
}

TEST_F(GlicActivityListBubbleControllerActivityHubTest,
       CreateRowViewModels_ConversationActivities) {
  auto* manager = &mock_glic_keyed_service_->activity_manager();
  ASSERT_TRUE(manager);

  manager->OnConversationActivityStateChanged(
      "conv_seen", {.state = GlicActivityState::kCompleted,
                    .title = "Seen Topic",
                    .preview_snippet = "Old snippet",
                    .is_currently_visible = true});
  manager->OnConversationActivityStateChanged(
      "conv_test_123", {.state = GlicActivityState::kCompleted,
                        .title = "My Research Topic",
                        .preview_snippet = "Here is the answer"});
  manager->OnConversationActivityStateChanged(
      "conv_working", {.state = GlicActivityState::kInProgress, .title = ""});

  auto rows = controller_->CreateRowViewModels();
  ASSERT_EQ(3u, rows.size());

  // Priority 1: Unseen completed response.
  EXPECT_EQ("conv_test_123", rows[0].conversation_id);
  EXPECT_EQ(u"My Research Topic", rows[0].title);
  EXPECT_EQ(u"Here is the answer", rows[0].subtitle);
  EXPECT_TRUE(rows[0].is_enabled);
  EXPECT_FALSE(rows[0].has_been_seen);
  EXPECT_EQ(ui::kColorSysPrimary, rows[0].icon_and_subtitle_color);

  // Priority 2: Conversation in progress (with default Gemini title fallback).
  EXPECT_EQ("conv_working", rows[1].conversation_id);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_GLIC_ACTIVITY_TITLE_GEMINI),
            rows[1].title);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_GLIC_ACTIVITY_WORKING),
            rows[1].subtitle);
  EXPECT_FALSE(rows[1].has_been_seen);

  // Priority 4: Seen completed conversation.
  EXPECT_EQ("conv_seen", rows[2].conversation_id);
  EXPECT_EQ(u"Seen Topic", rows[2].title);
  EXPECT_EQ(u"", rows[2].subtitle);
  EXPECT_TRUE(rows[2].has_been_seen);
  EXPECT_EQ(ui::kColorMenuIcon, rows[2].icon_and_subtitle_color);

  // Clicking a conversation row marks it as seen and invokes GlicKeyedService.
  EXPECT_CALL(*mock_glic_keyed_service_, Invoke(testing::_)).Times(1);
  ASSERT_TRUE(rows[0].on_clicked);
  rows[0].on_clicked.Run();

  GlicActivityKey key{GlicActivityType::kConversationResponse, "conv_test_123"};
  auto it = manager->activities().find(key);
  ASSERT_NE(it, manager->activities().end());
  EXPECT_TRUE(it->second.has_been_seen);
}

}  // namespace glic
