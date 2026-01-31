// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_tab_controller.h"

#include "base/test/test_future.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_delegate.h"
#include "chrome/browser/ui/webui/skills/skills_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/skills/features.h"
#include "components/skills/public/skill.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/test/widget_test.h"

namespace skills {

class SkillsUiTabControllerBrowserTest : public InProcessBrowserTest {
 public:
  SkillsUiTabControllerBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kSkillsEnabled);
  }

  // Helper to get the controller for the current active tab.
  SkillsUiTabController* skills_ui_tab_controller() {
    auto* tab = browser()->GetActiveTabInterface();
    if (!tab) {
      return nullptr;
    }
    return static_cast<SkillsUiTabController*>(
        SkillsUiTabControllerInterface::From(tab));
  }

  // Helper to determine if a dialog is visible on the current active tab.
  bool IsDialogVisible() {
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    auto* manager =
        web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
    return manager && manager->IsDialogActive();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify we can open the dialog.
IN_PROC_BROWSER_TEST_F(SkillsUiTabControllerBrowserTest,
                       ShowDialogOpensWidget) {
  EXPECT_FALSE(IsDialogVisible());

  skills::Skill test_skill("id", "skill_name", "icon", "Test Prompt");
  skills_ui_tab_controller()->ShowDialog(test_skill);

  EXPECT_TRUE(IsDialogVisible());
}

// Verify calling ShowDialog twice doesn't open two dialogs.
IN_PROC_BROWSER_TEST_F(SkillsUiTabControllerBrowserTest, PreventDoubleOpen) {
  // Open first dialog.
  skills::Skill test_skill("id", "skill_name", "icon", "Test Prompt");
  skills_ui_tab_controller()->ShowDialog(test_skill);
  EXPECT_TRUE(IsDialogVisible());

  ConstrainedWebDialogDelegate* first_delegate =
      skills_ui_tab_controller()->GetDialogDelegateForTesting();
  ASSERT_TRUE(first_delegate);

  // Try to open again immediately.
  skills::Skill test_skill2("id2", "skill_name2", "icon", "Test Prompt");
  skills_ui_tab_controller()->ShowDialog(test_skill2);

  // Dialog should still be visible.
  EXPECT_TRUE(IsDialogVisible());

  // Verify the widget pointer has not changed.
  ConstrainedWebDialogDelegate* second_delegate =
      skills_ui_tab_controller()->GetDialogDelegateForTesting();

  // If the controller had destroyed/recreated the dialog, these pointers would
  // differ.
  EXPECT_EQ(first_delegate, second_delegate);
}

// Verify calling CloseDialog destroys the widget.
IN_PROC_BROWSER_TEST_F(SkillsUiTabControllerBrowserTest,
                       CloseDialogDestroysWidget) {
  skills::Skill test_skill("id", "skill_name", "icon", "Test Prompt");
  skills_ui_tab_controller()->ShowDialog(test_skill);
  EXPECT_TRUE(IsDialogVisible());

  // Trigger the close.
  skills_ui_tab_controller()->CloseDialog();

  EXPECT_FALSE(IsDialogVisible());
}

// Verify that closing the native widget (e.g. user clicks X) correctly
// triggers the callback and cleans up the controller state.
IN_PROC_BROWSER_TEST_F(SkillsUiTabControllerBrowserTest,
                       NativeCloseCleansUpState) {
  // Open dialog.
  skills::Skill test_skill("id", "name", "icon", "prompt");
  skills_ui_tab_controller()->ShowDialog(test_skill);
  EXPECT_TRUE(IsDialogVisible());

  // Get the Widget to simulate a native close (like clicking 'X')
  auto* delegate = skills_ui_tab_controller()->GetDialogDelegateForTesting();
  ASSERT_TRUE(delegate);

  // Retrieve the Views Widget from the delegate.
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(delegate->GetNativeDialog());
  ASSERT_TRUE(widget);

  // Setup Waiter to verify the callback runs.
  base::test::TestFuture<void> close_future;
  skills_ui_tab_controller()->SetOnDialogClosedCallbackForTesting(
      close_future.GetCallback());

  // Simulate User Clicking "X"
  widget->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);

  // Wait for the loop to complete.
  ASSERT_TRUE(close_future.Wait());
  EXPECT_FALSE(IsDialogVisible());

  // If OnDialogClosed didn't run, 'dialog_delegate_' would still be non-null,
  // and ShowDialog() would likely exit early.
  skills_ui_tab_controller()->ShowDialog(test_skill);
  EXPECT_TRUE(IsDialogVisible());
}

// Verify that closing the Tab does not crash the browser.
// (Regression test for the destruction race condition).
IN_PROC_BROWSER_TEST_F(SkillsUiTabControllerBrowserTest, TabCloseDoesNotCrash) {
  skills::Skill test_skill("id", "name", "icon", "prompt");
  skills_ui_tab_controller()->ShowDialog(test_skill);
  EXPECT_TRUE(IsDialogVisible());

  // Close the tab.
  // This destroys the Controller (SkillsUiTabController) AND
  // the View (SkillsUI) simultaneously.
  // The test passes if this line does not trigger an ASAN crash / Segfault.
  browser()->tab_strip_model()->CloseAllTabs();
}

// Verify Tab Switching Isolation
// (Opening a dialog on Tab A shouldn't show it on Tab B)
IN_PROC_BROWSER_TEST_F(SkillsUiTabControllerBrowserTest, DialogIsTabScoped) {
  auto* controller_a = skills_ui_tab_controller();
  ASSERT_TRUE(controller_a);

  // Open dialog on Tab A.
  skills::Skill test_skill("id", "skill_name", "icon", "Test Prompt");
  controller_a->ShowDialog(test_skill);
  EXPECT_TRUE(IsDialogVisible());

  // Open a new Tab B and switch to it.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  auto* controller_b = skills_ui_tab_controller();
  ASSERT_TRUE(controller_b);
  ASSERT_NE(controller_a, controller_b);

  // Tab B should have no dialogs visible.
  EXPECT_FALSE(IsDialogVisible());

  // Controller B shouldn't have a dialog open.
  // Verify this by calling ShowDialog and ensuring it does open one.
  skills::Skill test_skill2("id2", "skill_name2", "icon", "Test Prompt");
  controller_b->ShowDialog(test_skill2);

  EXPECT_TRUE(IsDialogVisible());

  // Switch back to A to prove the dialog is still there.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(IsDialogVisible());
}

// Verify that the UI Controller (SkillsUI) received the delegate pointer.
IN_PROC_BROWSER_TEST_F(SkillsUiTabControllerBrowserTest, VerifyWebUIPlumbing) {
  // Show the dialog.
  skills::Skill test_skill("id", "skill_name", "icon", "Test Prompt");
  skills_ui_tab_controller()->ShowDialog(test_skill);
  EXPECT_TRUE(IsDialogVisible());

  // Dig down to find the SkillsUI.
  // The controller holds the delegate -> which holds WebContents -> which holds
  // WebUI
  auto* dialog_delegate =
      skills_ui_tab_controller()->GetDialogDelegateForTesting();
  ASSERT_TRUE(dialog_delegate);

  content::WebContents* dialog_contents = dialog_delegate->GetWebContents();
  ASSERT_TRUE(dialog_contents);

  content::WebUI* web_ui = dialog_contents->GetWebUI();
  ASSERT_TRUE(web_ui);

  auto* skills_ui = web_ui->GetController()->GetAs<skills::SkillsUI>();
  ASSERT_TRUE(skills_ui);

  EXPECT_TRUE(skills_ui->GetDelegateForTesting());
}

// Verify that clicking the Cancel button closes the dialog.
IN_PROC_BROWSER_TEST_F(SkillsUiTabControllerBrowserTest,
                       CancelButtonClosesDialog) {
  skills::Skill test_skill("id", "name", "icon", "prompt");
  skills_ui_tab_controller()->ShowDialog(test_skill);
  EXPECT_TRUE(IsDialogVisible());

  auto* delegate = skills_ui_tab_controller()->GetDialogDelegateForTesting();
  ASSERT_TRUE(delegate);
  content::WebContents* web_contents = delegate->GetWebContents();
  ASSERT_TRUE(web_contents);

  // Wait for load.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Setup Listener.
  base::test::TestFuture<void> close_future;
  skills_ui_tab_controller()->SetOnDialogClosedCallbackForTesting(
      close_future.GetCallback());

  // Script to click button.
  static constexpr char kClickScript[] = R"(
    (async function() {
      const getButton = () => {
        const app = document.querySelector('skills-dialog-app');
        if (!app || !app.shadowRoot) return null;
        return app.$['cancelButton'];
      };

      for (let i = 0; i < 20; i++) {
        const button = getButton();
        if (button) {
          button.click();
          return 'CLICKED';
        }
        await new Promise(r => setTimeout(r, 50));
      }
      return 'FAIL: Button not found';
    })();
  )";

  EXPECT_EQ("CLICKED", content::EvalJs(web_contents, kClickScript));

  // Wait for native close.
  ASSERT_TRUE(close_future.Wait());
  EXPECT_FALSE(IsDialogVisible());
}

}  // namespace skills
