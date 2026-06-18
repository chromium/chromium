// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/glic/browser_ui/glic_actor_task_icon_manager.h"
#include "chrome/browser/glic/browser_ui/glic_actor_task_icon_manager_factory.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/unique_widget_ptr.h"

class ActorTaskListBubbleControllerTest : public ChromeViewsTestBase {
 public:
  ActorTaskListBubbleControllerTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlicActor,
         {{features::kGlicActorPolicyControlExemption.name, "true"}}}};
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

    glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);

    profile_ = testing_profile_manager_->CreateTestingProfile(
        "profile",
        TestingProfile::TestingFactories{
            TestingProfile::TestingFactory{
                actor::ActorKeyedServiceFactory::GetInstance(),
                base::BindRepeating(
                    &ActorTaskListBubbleControllerTest::BuildActorKeyedService,
                    base::Unretained(this))},
            TestingProfile::TestingFactory{
                glic::GlicActorTaskIconManagerFactory::GetInstance(),
                base::BindRepeating(&ActorTaskListBubbleControllerTest::
                                        BuildGlicActorTaskIconManager,
                                    base::Unretained(this))},
            TestingProfile::TestingFactory{
                glic::GlicKeyedServiceFactory::GetInstance(),
                base::BindRepeating(&ActorTaskListBubbleControllerTest::
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
    actor_task_list_bubble_controller_ =
        std::make_unique<ActorTaskListBubbleController>(
            browser_window_interface_.get());
  }

  std::unique_ptr<KeyedService> BuildGlicActorTaskIconManager(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto* actor_service =
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile);
    auto manager = std::make_unique<glic::GlicActorTaskIconManager>(
        profile, actor_service);
    return std::move(manager);
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
        std::make_unique<testing::NiceMock<glic::MockGlicKeyedService>>(
            profile, identity_test_env_.identity_manager(),
            testing_profile_manager_->profile_manager(), &glic_profile_manager_,
            /*contextual_cueing_service=*/nullptr,
            actor::ActorKeyedServiceFactory::GetActorKeyedService(profile));
    mock_glic_keyed_service_ = mock_service.get();
    return mock_service;
  }

  void TearDown() override {
    actor_task_list_bubble_controller_.reset();
    browser_window_interface_.reset();
    mock_glic_keyed_service_ = nullptr;
    profile_ = nullptr;
    testing_profile_manager_.reset();
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
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

  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  signin::IdentityTestEnvironment identity_test_env_;
  glic::GlicProfileManager glic_profile_manager_;
  glic::GlicUnitTestEnvironment glic_test_env_;
  raw_ptr<glic::MockGlicKeyedService> mock_glic_keyed_service_ = nullptr;
  std::unique_ptr<ActorTaskListBubbleController>
      actor_task_list_bubble_controller_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  ui::UnownedUserDataHost user_data_host_;
  views::UniqueWidgetPtr anchor_widget_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ActorTaskListBubbleControllerTest, ShowBubbleRecordsHistogram) {
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  glic::GlicActorTaskIconManager* manager =
      glic::GlicActorTaskIconManagerFactory::GetForProfile(profile_);
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor_service->GetTask(task_id)->Pause(true);
  manager->UpdateTaskIconComponents(task_id);

  base::HistogramTester histogram_tester;

  actor_task_list_bubble_controller_->ShowBubble(
      anchor_widget_->GetContentsView());

  histogram_tester.ExpectBucketCount("Actor.Ui.TaskListBubble.Rows", 1, 1);

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

  actor_task_list_bubble_controller_->ShowBubble(
      anchor_widget_->GetContentsView());

  histogram_tester.ExpectBucketCount("Actor.Ui.TaskListBubble.Rows", 1, 1);
  histogram_tester.ExpectBucketCount("Actor.Ui.TaskListBubble.Rows", 4, 1);

  EXPECT_EQ(
      2u,
      histogram_tester.GetAllSamples("Actor.Ui.TaskListBubble.Rows").size());
}

TEST_F(ActorTaskListBubbleControllerTest,
       ShowBubble_InactiveBrowserWindow_NoStartNotification) {
  // If the browser window is inactive and it is NOT a start notification,
  // ShowBubble should return early and NOT show the bubble.
  EXPECT_CALL(*browser_window_interface_, IsActive())
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(*mock_glic_keyed_service_, IsPanelShowingForBrowser(testing::_))
      .WillRepeatedly(testing::Return(true));

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  glic::GlicActorTaskIconManager* manager =
      glic::GlicActorTaskIconManagerFactory::GetForProfile(profile_);
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor_service->GetTask(task_id)->Pause(true);
  manager->UpdateTaskIconComponents(task_id);

  actor_task_list_bubble_controller_->ShowBubble(
      anchor_widget_->GetContentsView(), /*is_start_notification=*/false);

  // Bubble widget should not be created.
  EXPECT_FALSE(actor_task_list_bubble_controller_->GetBubbleWidget());
}

TEST_F(ActorTaskListBubbleControllerTest,
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
  glic::GlicActorTaskIconManager* manager =
      glic::GlicActorTaskIconManagerFactory::GetForProfile(profile_);
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor_service->GetTask(task_id)->Pause(true);
  manager->UpdateTaskIconComponents(task_id);

  actor_task_list_bubble_controller_->ShowBubble(
      anchor_widget_->GetContentsView(), /*is_start_notification=*/true);

  // Bubble widget should not be created.
  EXPECT_FALSE(actor_task_list_bubble_controller_->GetBubbleWidget());
}

TEST_F(ActorTaskListBubbleControllerTest,
       ShowBubble_InactiveBrowserWindow_StartNotification_PanelShowing) {
  // If the browser window is inactive, and it IS a start notification,
  // and Glic panel IS showing, ShowBubble should show the bubble.
  EXPECT_CALL(*browser_window_interface_, IsActive())
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(*mock_glic_keyed_service_, IsPanelShowingForBrowser(testing::_))
      .WillRepeatedly(testing::Return(true));

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  glic::GlicActorTaskIconManager* manager =
      glic::GlicActorTaskIconManagerFactory::GetForProfile(profile_);
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor_service->GetTask(task_id)->Pause(true);
  manager->UpdateTaskIconComponents(task_id);

  actor_task_list_bubble_controller_->ShowBubble(
      anchor_widget_->GetContentsView(), /*is_start_notification=*/true);

  // Bubble widget should be created and visible.
  EXPECT_TRUE(actor_task_list_bubble_controller_->GetBubbleWidget());
  EXPECT_TRUE(
      actor_task_list_bubble_controller_->GetBubbleWidget()->IsVisible());
}

TEST_F(ActorTaskListBubbleControllerTest, ShowBubble_DelayedWhenIconHidden) {
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  glic::GlicActorTaskIconManager* manager =
      glic::GlicActorTaskIconManagerFactory::GetForProfile(profile_);
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor_service->GetTask(task_id)->Pause(true);
  manager->UpdateTaskIconComponents(task_id);

  // Set anchor view to hidden.
  views::View* anchor_view = anchor_widget_->GetContentsView();
  anchor_view->SetVisible(false);

  actor_task_list_bubble_controller_->ShowBubble(
      anchor_view, /*is_start_notification=*/true);

  // Bubble widget should NOT be created immediately.
  EXPECT_FALSE(actor_task_list_bubble_controller_->GetBubbleWidget());

  // Fast forward by 250ms.
  task_environment()->FastForwardBy(base::Milliseconds(250));

  // Bubble widget should now be created and visible.
  EXPECT_TRUE(actor_task_list_bubble_controller_->GetBubbleWidget());
  EXPECT_TRUE(
      actor_task_list_bubble_controller_->GetBubbleWidget()->IsVisible());
}

TEST_F(ActorTaskListBubbleControllerTest,
       ShowBubble_NotDelayedWhenIconVisible) {
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  glic::GlicActorTaskIconManager* manager =
      glic::GlicActorTaskIconManagerFactory::GetForProfile(profile_);
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor_service->GetTask(task_id)->Pause(true);
  manager->UpdateTaskIconComponents(task_id);

  // Set anchor view to visible.
  views::View* anchor_view = anchor_widget_->GetContentsView();
  anchor_view->SetVisible(true);

  actor_task_list_bubble_controller_->ShowBubble(
      anchor_view, /*is_start_notification=*/true);

  // Bubble widget should be created immediately.
  EXPECT_TRUE(actor_task_list_bubble_controller_->GetBubbleWidget());
  EXPECT_TRUE(
      actor_task_list_bubble_controller_->GetBubbleWidget()->IsVisible());
}
