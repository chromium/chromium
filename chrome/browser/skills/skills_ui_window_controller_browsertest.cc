// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_window_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/skills/skills_ui_tab_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/skills/features.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skill.mojom.h"
#include "components/skills/public/skills_metrics.h"
#include "components/skills/public/skills_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"

namespace skills {

class SkillsUiWindowControllerBrowserTest : public InProcessBrowserTest {
 public:
  SkillsUiWindowControllerBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kSkillsEnabled);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    skills::SkillsService* skills_service =
        skills::SkillsServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(skills_service);
    skills_service->SetServiceStatusForTesting(
        skills::SkillsService::ServiceStatus::kReady);
    skills_service->AddObserver(&skills_service_observer_);
  }

  void TearDownOnMainThread() override {
    skills::SkillsService* skills_service =
        skills::SkillsServiceFactory::GetForProfile(browser()->profile());
    skills_service->RemoveObserver(&skills_service_observer_);
    InProcessBrowserTest::TearDownOnMainThread();
  }

  SkillsUiWindowController* window_controller() {
    return SkillsUiWindowController::From(browser());
  }

  SkillsUiTabController* tab_controller() {
    auto* tab = browser()->GetActiveTabInterface();
    if (!tab) {
      return nullptr;
    }
    return static_cast<SkillsUiTabController*>(
        SkillsUiTabControllerInterface::From(tab));
  }

  content::WebContents* GetDialogWebContents() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    ui::ElementContext context =
        views::ElementTrackerViews::GetContextForWidget(
            browser_view->GetWidget());

    auto* view = views::ElementTrackerViews::GetInstance()->GetUniqueView(
        SkillsDialogView::kSkillsDialogElementId, context);

    if (!view) {
      return nullptr;
    }
    return static_cast<views::WebView*>(view)->GetWebContents();
  }

  void ClickToastActionButton() {
    auto* toast_controller = browser()->GetFeatures().toast_controller();
    ASSERT_TRUE(toast_controller->IsShowingToast());
    auto* toast_view = toast_controller->GetToastViewForTesting();
    ASSERT_TRUE(toast_view);
    auto* button = toast_view->action_button_for_testing();
    ASSERT_TRUE(button);

    views::test::ButtonTestApi(button).NotifyClick(
        ui::MouseEvent(::ui::EventType::kMouseReleased, gfx::Point(),
                       gfx::Point(), ui::EventTimeForNow(), 0, 0));
  }

  class TestObserver : public skills::SkillsService::Observer {
   public:
    void OnTemporarySkillDisplay(
        std::string_view skill_id,
        skills::SkillsService::DisplayState display_state) override {
      last_temporarily_displayed_skill_id_ = std::string(skill_id);
      last_display_state_ = display_state;
    }

    std::string last_temporarily_displayed_skill_id_;
    skills::SkillsService::DisplayState last_display_state_;
  };

 protected:
  TestObserver skills_service_observer_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SkillsUiWindowControllerBrowserTest,
                       OnSkillSavedShowToast) {
  // Ensure no toast is initially showing.
  const auto* toast_controller = browser()->GetFeatures().toast_controller();
  EXPECT_FALSE(toast_controller->IsShowingToast());

  // Call OnSkillSaved with an empty skill ID.
  window_controller()->OnSkillSaved("");

  // Verify that the toast is now showing.
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_EQ(toast_controller->GetCurrentToastId(), ToastId::kSkillSaved);
}

IN_PROC_BROWSER_TEST_F(SkillsUiWindowControllerBrowserTest,
                       OnSkillDeletedShowsToastAndTemporarilyDeletesSkill) {
  skills::SkillsService* skills_service =
      skills::SkillsServiceFactory::GetForProfile(browser()->profile());
  const skills::Skill* skill = skills_service->AddSkill(
      /*source_skill_id=*/"", "Test Skill", "test-icon", "Test Prompt");

  const auto* toast_controller = browser()->GetFeatures().toast_controller();
  EXPECT_FALSE(toast_controller->IsShowingToast());

  window_controller()->OnSkillDeleted(skill->id);

  // Verify that the toast is now showing.
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_EQ(toast_controller->GetCurrentToastId(), ToastId::kSkillDeleted);

  EXPECT_EQ(skills_service_observer_.last_temporarily_displayed_skill_id_,
            skill->id);
  EXPECT_EQ(skills_service_observer_.last_display_state_,
            skills::SkillsService::DisplayState::kDeleted);

  // Still exists since it's just temporarily deleted.
  EXPECT_NE(skills_service->GetSkillById(skill->id), nullptr);
}

IN_PROC_BROWSER_TEST_F(SkillsUiWindowControllerBrowserTest,
                       UndoLastSkillRemovalReshowsSkill) {
  skills::SkillsService* skills_service =
      skills::SkillsServiceFactory::GetForProfile(browser()->profile());
  const skills::Skill* skill = skills_service->AddSkill(
      /*source_skill_id=*/"", "Test Skill", "test-icon", "Test Prompt");

  window_controller()->OnSkillDeleted(skill->id);
  EXPECT_EQ(skills_service_observer_.last_display_state_,
            skills::SkillsService::DisplayState::kDeleted);

  window_controller()->UndoLastSkillRemoval();

  EXPECT_EQ(skills_service_observer_.last_temporarily_displayed_skill_id_,
            skill->id);
  EXPECT_EQ(skills_service_observer_.last_display_state_,
            skills::SkillsService::DisplayState::kReshown);

  // Verify that it still exists in the service.
  EXPECT_NE(skills_service->GetSkillById(skill->id), nullptr);
}

IN_PROC_BROWSER_TEST_F(SkillsUiWindowControllerBrowserTest,
                       ToastCloseDeletesSkill) {
  skills::SkillsService* skills_service =
      skills::SkillsServiceFactory::GetForProfile(browser()->profile());
  const skills::Skill* skill = skills_service->AddSkill(
      /*source_skill_id=*/"", "Test Skill", "test-icon", "Test Prompt");

  std::string skill_id = skill->id;
  window_controller()->OnSkillDeleted(skill_id);

  auto* toast_controller = browser()->GetFeatures().toast_controller();
  EXPECT_TRUE(toast_controller->IsShowingToast());

  // Close the toast widget directly to simulate it being dismissed.
  views::Widget* toast_widget = toast_controller->GetToastWidgetForTesting();
  ASSERT_TRUE(toast_widget);
  toast_widget->CloseNow();

  // Toast should not be visible anymore.
  EXPECT_FALSE(toast_controller->IsShowingToast());

  // Verify that it was actually deleted from the service.
  EXPECT_EQ(skills_service->GetSkillById(skill_id), nullptr);
  histogram_tester_.ExpectUniqueSample(
      "Toast.TriggeredToShow", static_cast<int>(ToastId::kSkillDeleted), 1);
}

IN_PROC_BROWSER_TEST_F(SkillsUiWindowControllerBrowserTest,
                       InvokeLastSavedSkillRoutesToActiveTab) {
  const std::string kSkillId = "skill-123";
  // Save skill on active tab.
  window_controller()->OnSkillSaved(kSkillId);
  // Verify Toast is visible
  EXPECT_TRUE(browser()->GetFeatures().toast_controller()->IsShowingToast());
  // Enable Glic late to avoid a crash in GlicTabIndicatorHelper during tab
  // creation.
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);

  // Click toast "Try It".
  ClickToastActionButton();
  // Verify Result
  EXPECT_EQ(tab_controller()->GetPendingSkillIdForTesting(), kSkillId);
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
}

IN_PROC_BROWSER_TEST_F(SkillsUiWindowControllerBrowserTest,
                       OnSkillSavedFromSkillsPage) {
  NavigateParams params(browser(), GURL(chrome::kChromeUISkillsURL),
                        ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params);
  const auto* toast_controller = browser()->GetFeatures().toast_controller();
  EXPECT_FALSE(toast_controller->IsShowingToast());
  tab_controller()->OnSkillSaved("");
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_EQ(toast_controller->GetCurrentToastId(),
            ToastId::kSkillSavedWithoutInvokeButton);
}

IN_PROC_BROWSER_TEST_F(SkillsUiWindowControllerBrowserTest,
                       UserFlow_CreateSkill_ThenInvoke) {
  // Enable Glic late to avoid a crash in GlicTabIndicatorHelper during tab
  // creation.
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);

  // Open Dialog.
  skills::Skill initial_skill(/*id=*/"",
                              /*name=*/"",
                              /*icon=*/"", "Skill Prompt");
  tab_controller()->ShowDialog(std::move(initial_skill),
                               SkillsDialogEntryPoint::kWebClientPrefilled,
                               mojom::SkillsDialogType::kAdd);

  // Get WebContents to inject JS.
  content::WebContents* web_contents = GetDialogWebContents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  // Setup Listener for "Dialog Closed".
  base::test::TestFuture<void> close_future;
  tab_controller()->SetOnDialogClosedCallbackForTesting(
      close_future.GetCallback());

  static constexpr char kSaveScript[] = R"(
  (async () => {
    const root = document.querySelector('skills-dialog-app').shadowRoot;

    for (let i = 0; i < 50; i++) {
      const btn = root.querySelector('#saveButton');
      if (btn && !btn.disabled) {
          setTimeout(() => btn.click(), 0);
          return 'CLICKED';
      }

      // Fill inputs if found & empty
      let el = root.querySelector('#nameText');
      if (el && !el.value) {
        el.value = 'Test';
        el.dispatchEvent(new CustomEvent('value-changed', {
          bubbles: true,
          composed: true,
          detail: { value: 'Test' }
        }));
      }
      el = root.querySelector('#instructionsText');
      if (el && !el.value) {
        el.value = 'Test';
        el.dispatchEvent(new Event('input', {
          bubbles: true,
          composed: true
        }));
      }
      await new Promise(r => setTimeout(r, 100));
    }
    return 'TIMEOUT';
  })();
)";

  EXPECT_EQ("CLICKED", content::EvalJs(web_contents, kSaveScript));

  // Wait for the C++ backend to process the save and close the dialog.
  ASSERT_TRUE(close_future.Wait());
  EXPECT_EQ(nullptr, GetDialogWebContents());

  // Click the Toast "Try It" button.
  ClickToastActionButton();

  // Verify the Invoke happened by checking that some ID is pending (since the
  // ID was auto-generated by the service)
  EXPECT_FALSE(tab_controller()->GetPendingSkillIdForTesting().empty());
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);

  histogram_tester_.ExpectBucketCount(
      "Toast.TriggeredToShow", static_cast<int>(ToastId::kSkillSaved), 1);
  histogram_tester_.ExpectUniqueSample("Toast.SkillSaved.Dismissed",
                                       1,  // kActionButton
                                       1);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "Toast.ActionButtonClicked.SkillSaved"),
            1);
}

}  // namespace skills
