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
#include "chrome/browser/glic/browser_ui/glic_split_button_controller.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
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
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget_delegate.h"

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

    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());

    profile_ = testing_profile_manager_->CreateTestingProfile(
        "profile",
        TestingProfile::TestingFactories{
            TestingProfile::TestingFactory{
                actor::ActorKeyedServiceFactory::GetInstance(),
                base::BindRepeating([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<actor::ActorKeyedServiceFake>(
                      Profile::FromBrowserContext(context));
                })},
            TestingProfile::TestingFactory{
                glic::GlicActorTaskIconManagerFactory::GetInstance(),
                base::BindRepeating([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
                  Profile* profile = Profile::FromBrowserContext(context);
                  auto* actor_service =
                      actor::ActorKeyedServiceFactory::GetActorKeyedService(
                          profile);
                  return std::make_unique<glic::GlicActorTaskIconManager>(
                      profile, actor_service);
                })},
            TestingProfile::TestingFactory{
                glic::GlicKeyedServiceFactory::GetInstance(),
                base::BindRepeating(
                    &ActorTaskListBubbleTest::BuildMockGlicKeyedService,
                    base::Unretained(this))}});

    glic_test_env_.SetupProfile(profile_);

    actor_service_ = static_cast<actor::ActorKeyedServiceFake*>(
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile_));

    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();

    browser_window_interface_ = std::make_unique<MockBrowserWindowInterface>();
    ON_CALL(*browser_window_interface_, GetUnownedUserDataHost)
        .WillByDefault(::testing::ReturnRef(user_data_host_));
    ON_CALL(*browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile_));
    ON_CALL(*browser_window_interface_, IsActive())
        .WillByDefault(testing::Return(true));
    mock_glic_service_ = static_cast<glic::MockGlicKeyedService*>(
        glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile_));
    glic_split_button_controller_ =
        std::make_unique<glic::GlicSplitButtonController>(
            browser_window_interface_.get(), mock_glic_service_);
    controller_ =
        ActorTaskListBubbleController::From(browser_window_interface_.get());
    ASSERT_TRUE(controller_);
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
    return mock_service;
  }

  void TearDown() override {
    bubble_.reset();
    controller_ = nullptr;
    glic_split_button_controller_.reset();
    mock_glic_service_ = nullptr;
    browser_window_interface_.reset();
    anchor_widget_.reset();
    actor_service_ = nullptr;
    profile_ = nullptr;
    testing_profile_manager_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  TestingProfile* profile() { return profile_; }

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
  glic::GlicEnabling::ScopedBypassEnablementChecksForTesting
      scoped_glic_bypass_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  signin::IdentityTestEnvironment identity_test_env_;
  glic::GlicProfileManager glic_profile_manager_;
  glic::GlicUnitTestEnvironment glic_test_env_;
  MockTabInterface mock_tab_;
  views::UniqueWidgetPtr anchor_widget_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  ui::UnownedUserDataHost user_data_host_;
  raw_ptr<glic::MockGlicKeyedService> mock_glic_service_ = nullptr;
  std::unique_ptr<glic::GlicSplitButtonController>
      glic_split_button_controller_;
  raw_ptr<ActorTaskListBubbleController> controller_ = nullptr;
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

// The bubble is shown activated in response to a task update. It has no dialog
// buttons, so unless it explicitly focuses one of its rows the widget is
// activated with nothing focused inside it, and screen readers are left with no
// way into the bubble.
TEST_F(ActorTaskListBubbleTest, ShowBubbleFocusesFirstRow) {
  absl::flat_hash_map<actor::TaskId, bool> task_list;
  task_list[CreatePausedTask()] = true;
  task_list[CreatePausedTask()] = false;

  views::Widget* actor_task_list_bubble =
      CreateBubbleView(std::move(task_list));
  ASSERT_TRUE(actor_task_list_bubble);

  views::View* content_view =
      GetContentViewInActorTaskListBubble(actor_task_list_bubble);
  ASSERT_EQ(2u, content_view->children().size());

  views::WidgetDelegate* delegate = actor_task_list_bubble->widget_delegate();
  EXPECT_EQ(content_view->children().front(),
            delegate->GetInitiallyFocusedView());
  // Having a focused view makes the bubble expose the dialog role instead of
  // the alert dialog role, which is what makes screen readers announce it as a
  // dialog when focus moves into it.
  EXPECT_EQ(ax::mojom::Role::kDialog, delegate->GetAccessibleWindowRole());
}

// Rows for tasks whose tab was closed are disabled and cannot take focus, so
// initial focus must skip past them.
TEST_F(ActorTaskListBubbleTest, ShowBubbleSkipsDisabledRowsForInitialFocus) {
  // Paused task with no tab. Sorts first because it requires attention, but is
  // rendered as a disabled "Tab closed" row.
  actor::TaskId closed_tab_task = actor_service_->CreateTaskForTesting();
  actor_service_->GetTask(closed_tab_task)->Pause(/*from_actor=*/true);

  // Acting task with a tab. Sorts last but is the first enabled row.
  actor::TaskId acting_task = actor_service_->CreateTaskForTesting();
  base::RunLoop loop;
  actor_service_->GetTask(acting_task)
      ->AddTab(
          mock_tab().GetHandle(),
          /*stop_task_on_detach=*/true,
          base::BindLambdaForTesting([&](actor::mojom::ActionResultPtr result) {
            EXPECT_TRUE(actor::IsOk(*result));
            loop.Quit();
          }));
  loop.Run();

  absl::flat_hash_map<actor::TaskId, bool> task_list;
  task_list[closed_tab_task] = false;
  task_list[acting_task] = false;

  views::Widget* actor_task_list_bubble =
      CreateBubbleView(std::move(task_list));
  ASSERT_TRUE(actor_task_list_bubble);

  views::View* content_view =
      GetContentViewInActorTaskListBubble(actor_task_list_bubble);
  ASSERT_EQ(2u, content_view->children().size());
  ASSERT_FALSE(content_view->children().front()->GetEnabled());
  ASSERT_TRUE(content_view->children().back()->GetEnabled());

  EXPECT_EQ(
      content_view->children().back(),
      actor_task_list_bubble->widget_delegate()->GetInitiallyFocusedView());
}

// When nothing in the bubble can take focus, it must keep the alert dialog role
// so that its contents are still announced when it appears.
TEST_F(ActorTaskListBubbleTest, BubbleWithOnlyDisabledRowsKeepsAlertRole) {
  actor::TaskId id = actor_service_->CreateTaskForTesting();
  actor_service_->GetTask(id)->Pause(/*from_actor=*/true);
  absl::flat_hash_map<actor::TaskId, bool> task_list;
  task_list[id] = false;

  views::Widget* actor_task_list_bubble =
      CreateBubbleView(std::move(task_list));
  ASSERT_TRUE(actor_task_list_bubble);

  views::WidgetDelegate* delegate = actor_task_list_bubble->widget_delegate();
  EXPECT_EQ(nullptr, delegate->GetInitiallyFocusedView());
  EXPECT_EQ(ax::mojom::Role::kAlertDialog, delegate->GetAccessibleWindowRole());
}
