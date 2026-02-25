// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_window_controller.h"

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
#include "chrome/test/base/in_process_browser_test.h"
#include "components/skills/features.h"
#include "components/skills/public/skill.h"
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
                       OnSkillDeletedShowToast) {
  // Ensure no toast is initially showing.
  const auto* toast_controller = browser()->GetFeatures().toast_controller();
  EXPECT_FALSE(toast_controller->IsShowingToast());

  window_controller()->OnSkillDeleted();

  // Verify that the toast is now showing.
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_EQ(toast_controller->GetCurrentToastId(), ToastId::kSkillDeleted);
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

// Test that switching tabs targets the new tab, not the old one.
IN_PROC_BROWSER_TEST_F(SkillsUiWindowControllerBrowserTest,
                       InvokeRoutesToNewTabAfterSwitch) {
  const std::string kSkillId = "cross-tab-skill";

  // Save skill on Tab 0
  window_controller()->OnSkillSaved(kSkillId);
  // Open a new tab and switch to it
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 1);
  // Verify we have the controller for the new tab has no skills yet.
  EXPECT_TRUE(tab_controller()->GetPendingSkillIdForTesting().empty());
  // Enable Glic late to avoid a crash in GlicTabIndicatorHelper during tab
  // creation.
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);
  // Click toast "Try It".
  ClickToastActionButton();
  // Verify the new tab got the command.
  EXPECT_EQ(tab_controller()->GetPendingSkillIdForTesting(), kSkillId);
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
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
  tab_controller()->ShowDialog(std::move(initial_skill));

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
}

}  // namespace skills
