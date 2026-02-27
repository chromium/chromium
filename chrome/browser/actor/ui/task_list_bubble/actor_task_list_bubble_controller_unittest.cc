// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager_factory.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
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

    profile_ =
        TestingProfile::Builder()
            .AddTestingFactory(
                actor::ActorKeyedServiceFactory::GetInstance(),
                base::BindRepeating(
                    &ActorTaskListBubbleControllerTest::BuildActorKeyedService,
                    base::Unretained(this)))
            .AddTestingFactory(
                tabs::GlicActorTaskIconManagerFactory::GetInstance(),
                base::BindRepeating(&ActorTaskListBubbleControllerTest::
                                        BuildGlicActorTaskIconManager,
                                    base::Unretained(this)))
            .Build();
    browser_window_interface_ = std::make_unique<MockBrowserWindowInterface>();
    ON_CALL(*browser_window_interface_, GetUnownedUserDataHost)
        .WillByDefault(::testing::ReturnRef(user_data_host_));
    ON_CALL(*browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
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
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_.get());
    auto manager = std::make_unique<tabs::GlicActorTaskIconManager>(
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

  void TearDown() override {
    actor_task_list_bubble_controller_.reset();
    browser_window_interface_.reset();
    profile_.reset();
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

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ActorTaskListBubbleController>
      actor_task_list_bubble_controller_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  ui::UnownedUserDataHost user_data_host_;
  views::UniqueWidgetPtr anchor_widget_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ActorTaskListBubbleControllerTest, ShowBubbleRecordsHistogram) {
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_.get());
  tabs::GlicActorTaskIconManager* manager =
      tabs::GlicActorTaskIconManagerFactory::GetForProfile(profile_.get());
  actor::TaskId task_id =
      actor_service->CreateTask(actor::NoEnterprisePolicyChecker());
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
    actor::TaskId new_task_id =
        actor_service->CreateTask(actor::NoEnterprisePolicyChecker());
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
