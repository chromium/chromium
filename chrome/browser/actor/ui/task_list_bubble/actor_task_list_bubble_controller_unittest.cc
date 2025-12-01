// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"

#include <memory>
#include <string>

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_policy_checker.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/unique_widget_ptr.h"
#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/test_support/mock_glic_window_controller.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager_factory.h"
#endif

class ActorTaskListBubbleControllerTest : public ChromeViewsTestBase {
 public:
  ActorTaskListBubbleControllerTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
#if BUILDFLAG(ENABLE_GLIC)
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
#endif
  }

#if BUILDFLAG(ENABLE_GLIC)
  std::unique_ptr<KeyedService> BuildGlicActorTaskIconManager(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    window_controller_ = std::make_unique<glic::MockGlicWindowController>();
    auto* actor_service =
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_.get());
    auto manager = std::make_unique<tabs::GlicActorTaskIconManager>(
        profile, actor_service, *window_controller_.get());
    return std::move(manager);
  }

  std::unique_ptr<KeyedService> BuildActorKeyedService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto actor_keyed_service =
        std::make_unique<actor::ActorKeyedServiceFake>(profile);

    return std::move(actor_keyed_service);
  }
#endif

  void TearDown() override {
#if BUILDFLAG(ENABLE_GLIC)
    actor_task_list_bubble_controller_.reset();
    browser_window_interface_.reset();
    profile_.reset();
    window_controller_.reset();
    anchor_widget_.reset();
#endif
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

#if BUILDFLAG(ENABLE_GLIC)
  std::unique_ptr<glic::MockGlicWindowController> window_controller_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ActorTaskListBubbleController>
      actor_task_list_bubble_controller_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  ui::UnownedUserDataHost user_data_host_;
  views::UniqueWidgetPtr anchor_widget_;
#endif
};

TEST_F(ActorTaskListBubbleControllerTest, RemoveRowFromBubbleOnClick) {
#if BUILDFLAG(ENABLE_GLIC)
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_.get());
  tabs::GlicActorTaskIconManager* manager =
      tabs::GlicActorTaskIconManagerFactory::GetForProfile(profile_.get());
  actor_service->GetPolicyChecker().SetActOnWebForTesting(true);
  actor::TaskId task_id = actor_service->CreateTask();
  actor_service->GetTask(task_id)->Pause(true);
  manager->UpdateTaskListBubble(task_id);
  actor_task_list_bubble_controller_->ShowBubble(
      anchor_widget_->GetContentsView());

  EXPECT_TRUE(
      actor_task_list_bubble_controller_->GetBubbleWidget()->IsVisible());

  views::View* content_view = GetContentViewInActorTaskListBubble(
      actor_task_list_bubble_controller_->GetBubbleWidget());

  EXPECT_EQ(1u, manager->GetActorTaskListBubbleRows().size());
  EXPECT_EQ(1u, content_view->children().size());

  RichHoverButton* button =
      static_cast<RichHoverButton*>(content_view->children().front());
  Click(button);
  // Wait for bubble to be closed and removed from the view.
  base::RunLoop().RunUntilIdle();

  // Bubble should be reset after click.
  EXPECT_EQ(nullptr, actor_task_list_bubble_controller_->GetBubbleWidget());
  EXPECT_EQ(0u, manager->GetActorTaskListBubbleRows().size());

  actor_task_list_bubble_controller_->ShowBubble(
      anchor_widget_->GetContentsView());
  content_view = GetContentViewInActorTaskListBubble(
      actor_task_list_bubble_controller_->GetBubbleWidget());

  EXPECT_EQ(0u, content_view->children().size());
#endif
}
