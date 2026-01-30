// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_policy_checker.h"
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/unique_widget_ptr.h"

using ::tabs::MockTabInterface;
class ActorTaskListBubbleTest : public ChromeViewsTestBase,
                                public testing::WithParamInterface<bool> {
 public:
  ActorTaskListBubbleTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    base::test::FeatureRefAndParams enable_glic_policy = {
        features::kGlicActor,
        {{features::kGlicActorPolicyControlExemption.name, "true"}}};
    if (GetParam()) {
      feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/{enable_glic_policy,
                                {features::kGlicActorUiGlobalTaskIndicator,
                                 {}}},
          /*disabled_features=*/{});
    } else {
      feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/{enable_glic_policy},
          /*disabled_features=*/{features::kGlicActorUiGlobalTaskIndicator});
    }

    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        actor::ActorKeyedServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<actor::ActorKeyedServiceFake>(
              Profile::FromBrowserContext(context));
        }));
    profile_ = builder.Build();

    actor_service_ = static_cast<actor::ActorKeyedServiceFake*>(
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_.get()));

    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();
  }

  void TearDown() override {
    anchor_widget_.reset();
    actor_service_ = nullptr;
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  TestingProfile* profile() { return profile_.get(); }

  // Mock callback for task clicks.
  void OnTaskClicked(actor::TaskId task_id) {}

  actor::TaskId CreatePausedTask() {
    actor::TaskId id = actor_service_->CreateTaskForTesting();
    base::RunLoop loop;
    actor_service_->GetTask(id)->AddTab(
        mock_tab().GetHandle(),
        base::BindLambdaForTesting([&](actor::mojom::ActionResultPtr result) {
          EXPECT_TRUE(actor::IsOk(*result));
          loop.Quit();
        }));
    loop.Run();
    actor_service_->GetTask(id)->Pause(/*from_actor=*/true);
    return id;
  }

  views::Widget* CreateBubbleView(
      absl::flat_hash_map<actor::TaskId, bool> task_list) {
    return ActorTaskListBubble::ShowBubble(
        profile_.get(), anchor_widget_->GetContentsView(), std::move(task_list),
        base::BindRepeating(&ActorTaskListBubbleTest::OnTaskClicked,
                            base::Unretained(this)));
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

 protected:
  raw_ptr<actor::ActorKeyedServiceFake> actor_service_;
  MockTabInterface& mock_tab() { return mock_tab_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  MockTabInterface mock_tab_;
  views::UniqueWidgetPtr anchor_widget_;
};

TEST_P(ActorTaskListBubbleTest, CreateAndShowBubbleWithTasks) {
  absl::flat_hash_map<actor::TaskId, bool> task_list;
  task_list[CreatePausedTask()] = true;
  task_list[CreatePausedTask()] = false;
  views::Widget* actor_task_list_bubble =
      CreateBubbleView(std::move(task_list));

  EXPECT_TRUE(actor_task_list_bubble->IsVisible());

  views::View* content_view =
      GetContentViewInActorTaskListBubble(std::move(actor_task_list_bubble));

  EXPECT_EQ(2u, content_view->children().size());
  EXPECT_EQ(u"Test Task",
            static_cast<RichHoverButton*>(content_view->children().front())
                ->GetTitleText());
  EXPECT_EQ(u"Test Task",
            static_cast<RichHoverButton*>(content_view->children().back())
                ->GetTitleText());
}

// TODO(crbug.com/469817191): Handle non-existent task_ids alongside completed
// task ids.
TEST_P(ActorTaskListBubbleTest, CreateShowBubbleWithInvalidTask) {
  base::HistogramTester histogram_tester;
  absl::flat_hash_map<actor::TaskId, bool> task_list;
  task_list[actor::TaskId(1)] = true;

  views::Widget* actor_task_list_bubble =
      CreateBubbleView(std::move(task_list));
  EXPECT_FALSE(actor_task_list_bubble);
  histogram_tester.ExpectUniqueSample(
      "Actor.Ui.TaskIcon.Error",
      actor::ui::ActorUiTaskIconError::kBubbleTaskDoesntExist, 1);
}

TEST_P(ActorTaskListBubbleTest, CreateAndShowBubbleWithClosedTabTask) {
  actor::TaskId id = actor_service_->CreateTaskForTesting();
  actor_service_->GetTask(id)->Pause(/*from_actor=*/true);
  absl::flat_hash_map<actor::TaskId, bool> task_list;
  task_list[id] = false;

  views::Widget* actor_task_list_bubble =
      CreateBubbleView(std::move(task_list));

  EXPECT_TRUE(actor_task_list_bubble->IsVisible());

  views::View* content_view =
      GetContentViewInActorTaskListBubble(std::move(actor_task_list_bubble));

  // Check for correct subtitle
  EXPECT_EQ(1u, content_view->children().size());
  EXPECT_EQ(u"Tab closed",
            static_cast<RichHoverButton*>(content_view->children().front())
                ->GetSubtitleText());
  // Check for disabled state correctly set (requires_processing is set to
  // false)
  EXPECT_FALSE(static_cast<RichHoverButton*>(content_view->children().front())
                   ->GetEnabled());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ActorTaskListBubbleTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "GlobalIndicatorEnabled"
                                             : "GlobalIndicatorDisabled";
                         });
