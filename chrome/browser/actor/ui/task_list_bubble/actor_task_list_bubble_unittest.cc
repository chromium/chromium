// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_row_button.h"
#include "chrome/browser/glic/browser_ui/glic_actor_task_icon_manager_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/unique_widget_ptr.h"

using ::tabs::MockTabInterface;
class ActorTaskListBubbleTest : public ChromeViewsTestBase {
 public:
  ActorTaskListBubbleTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlicActor,
         {{features::kGlicActorPolicyControlExemption.name, "true"}}}};
    feature_list_.InitWithFeaturesAndParameters(std::move(enabled_features),
                                                {});

    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        actor::ActorKeyedServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<actor::ActorKeyedServiceFake>(
              Profile::FromBrowserContext(context));
        }));
    builder.AddTestingFactory(
        glic::GlicActorTaskIconManagerFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(context);
          auto* actor_service =
              actor::ActorKeyedServiceFactory::GetActorKeyedService(profile);
          return std::make_unique<glic::GlicActorTaskIconManager>(
              profile, actor_service);
        }));
    profile_ = builder.Build();

    actor_service_ = static_cast<actor::ActorKeyedServiceFake*>(
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_.get()));

    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();

    browser_window_interface_ = std::make_unique<MockBrowserWindowInterface>();
    ON_CALL(*browser_window_interface_, GetUnownedUserDataHost)
        .WillByDefault(::testing::ReturnRef(user_data_host_));
    ON_CALL(*browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
    ON_CALL(*browser_window_interface_, IsActive())
        .WillByDefault(testing::Return(true));
    controller_ = std::make_unique<ActorTaskListBubbleController>(
        browser_window_interface_.get());
  }

  void TearDown() override {
    bubble_.reset();
    controller_.reset();
    browser_window_interface_.reset();
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
        /*stop_task_on_detach=*/true,
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
    bubble_ = std::make_unique<ActorTaskListBubble>(
        profile_.get(), browser_window_interface_.get(), task_list,
        base::BindRepeating(&ActorTaskListBubbleTest::OnTaskClicked,
                            base::Unretained(this)));
    bubble_->Show(anchor_widget_->GetContentsView());
    return bubble_->widget();
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
  std::unique_ptr<TestingProfile> profile_;
  MockTabInterface mock_tab_;
  views::UniqueWidgetPtr anchor_widget_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<ActorTaskListBubbleController> controller_;
  std::unique_ptr<ActorTaskListBubble> bubble_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ActorTaskListBubbleTest, CreateAndShowBubbleWithTasks) {
  absl::flat_hash_map<actor::TaskId, bool> task_list;
  task_list[CreatePausedTask()] = true;
  task_list[CreatePausedTask()] = false;
  views::Widget* actor_task_list_bubble =
      CreateBubbleView(std::move(task_list));

  EXPECT_TRUE(actor_task_list_bubble->IsVisible());

  views::View* content_view =
      GetContentViewInActorTaskListBubble(std::move(actor_task_list_bubble));

  EXPECT_EQ(2u, content_view->children().size());
  EXPECT_EQ(u"Test Task", static_cast<ActorTaskListBubbleRowButton*>(
                              content_view->children().front())
                              ->GetTitleText());
  EXPECT_EQ(u"Test Task", static_cast<ActorTaskListBubbleRowButton*>(
                              content_view->children().back())
                              ->GetTitleText());
}

// TODO(crbug.com/469817191): Handle non-existent task_ids alongside completed
// task ids.
TEST_F(ActorTaskListBubbleTest, CreateShowBubbleWithInvalidTask) {
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

TEST_F(ActorTaskListBubbleTest, CreateAndShowBubbleWithClosedTabTask) {
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
  EXPECT_EQ(u"Tab closed", static_cast<ActorTaskListBubbleRowButton*>(
                               content_view->children().front())
                               ->GetSubtitleText());
  // Check for disabled state correctly set (requires_processing is set to
  // false)
  EXPECT_FALSE(static_cast<ActorTaskListBubbleRowButton*>(
                   content_view->children().front())
                   ->GetEnabled());
}

TEST_F(ActorTaskListBubbleTest, CreateAndShowBubbleWithTasksInOrder) {
  actor::TaskId id_1 = CreatePausedTask();
  actor::TaskId id_2 = CreatePausedTask();
  actor::TaskId id_3 = CreatePausedTask();
  actor::TaskId id_4 = actor_service_->CreateTaskForTesting();

  actor_service_->StopTaskForTesting(
      id_3, actor::ActorTask::StoppedReason::kTaskComplete);

  base::RunLoop loop;
  actor_service_->GetTask(id_4)->AddTab(
      mock_tab().GetHandle(),
      /*stop_task_on_detach=*/true,
      base::BindLambdaForTesting([&](actor::mojom::ActionResultPtr result) {
        EXPECT_TRUE(actor::IsOk(*result));
        loop.Quit();
      }));
  loop.Run();

  absl::flat_hash_map<actor::TaskId, bool> task_list;
  task_list[id_1] = true;   // Paused, requires processing.
  task_list[id_2] = false;  // Paused, does not require processing.
  task_list[id_3] = true;   // Completed, does require processing.
  task_list[id_4] = false;  // Active, does not require processing.

  views::Widget* actor_task_list_bubble =
      CreateBubbleView(std::move(task_list));

  EXPECT_TRUE(actor_task_list_bubble->IsVisible());

  views::View* content_view =
      GetContentViewInActorTaskListBubble(std::move(actor_task_list_bubble));

  // Check for correct subtitles.
  EXPECT_EQ(4u, content_view->children().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_ACTOR_TASK_LIST_BUBBLE_ROW_CHECK_TASK_SUBTITLE),
      static_cast<ActorTaskListBubbleRowButton*>(content_view->children().at(0))
          ->GetSubtitleText());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_ACTOR_TASK_LIST_BUBBLE_ROW_CHECK_TASK_SUBTITLE),
      static_cast<ActorTaskListBubbleRowButton*>(content_view->children().at(1))
          ->GetSubtitleText());
  // Last tab is removed on Stop, so the finished task will have a tab closed
  // subtitle.
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_ACTOR_TASK_LIST_BUBBLE_ROW_TAB_CLOSED_SUBTITLE),
      static_cast<ActorTaskListBubbleRowButton*>(content_view->children().at(2))
          ->GetSubtitleText());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_ACTOR_TASK_LIST_BUBBLE_ROW_ACTING_TASK_SUBTITLE),
      static_cast<ActorTaskListBubbleRowButton*>(content_view->children().at(3))
          ->GetSubtitleText());
}

TEST_F(ActorTaskListBubbleTest,
       ExperimentalTriggeringCompletedTaskSubtitleText) {
  auto button = std::make_unique<ActorTaskListBubbleRowButton>(
      views::Button::PressedCallback(), actor::ActorTask::State::kFinished,
      u"Experimental Triggering Task", /*requires_processing=*/false,
      /*has_tab=*/true, glic::mojom::FeatureMode::kExperimentalTriggering);

  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_EXPERIMENTAL_TRIGGERING_TASK_LIST_BUBBLE_ROW_COMPLETED_TASK_SUBTITLE),
      button->GetSubtitleText());
}
