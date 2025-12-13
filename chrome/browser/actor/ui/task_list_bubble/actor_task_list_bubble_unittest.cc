// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble.h"

#include <memory>
#include <string>

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/unique_widget_ptr.h"

class ActorTaskListBubbleTest : public ChromeViewsTestBase {
 public:
  ActorTaskListBubbleTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();
  }

  void TearDown() override {
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  views::Widget* CreateBubbleView(
      std::vector<ActorTaskListBubbleRowButtonParams> param_list) {
    return ActorTaskListBubble::ShowBubble(anchor_widget_->GetContentsView(),
                                           std::move(param_list));
  }

  ActorTaskListBubbleRowButtonParams CreateRowButtonParamsWithTitle(
      std::u16string title_text) {
    return ActorTaskListBubbleRowButtonParams(
        {.title = title_text,
         .subtitle = u"Needs attention",
         .on_click_callback = views::Button::PressedCallback()});
  }

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

 private:
  views::UniqueWidgetPtr anchor_widget_;
};

TEST_F(ActorTaskListBubbleTest, CreateAndShowBubbleWithTasks) {
  std::vector<ActorTaskListBubbleRowButtonParams> param_list;
  param_list.push_back(CreateRowButtonParamsWithTitle(u"Test task"));
  param_list.push_back(CreateRowButtonParamsWithTitle(u"Test task 2"));
  views::Widget* actor_task_list_bubble =
      CreateBubbleView(std::move(param_list));

  EXPECT_TRUE(actor_task_list_bubble->IsVisible());

  views::View* content_view =
      GetContentViewInActorTaskListBubble(std::move(actor_task_list_bubble));

  EXPECT_EQ(2u, content_view->children().size());
  EXPECT_EQ(u"Test task",
            static_cast<RichHoverButton*>(content_view->children().front())
                ->GetTitleText());
  EXPECT_EQ(u"Test task 2",
            static_cast<RichHoverButton*>(content_view->children().back())
                ->GetTitleText());
}
