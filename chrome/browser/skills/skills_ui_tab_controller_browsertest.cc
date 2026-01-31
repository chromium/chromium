// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_tab_controller.h"

#include "base/test/test_future.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/skills/features.h"
#include "components/skills/public/skill.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test.h"
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

  base::test::TestFuture<void> close_future;
  skills_ui_tab_controller()->SetOnDialogClosedCallbackForTesting(
      close_future.GetCallback());
  // Trigger the close.
  skills_ui_tab_controller()->CloseDialog();

  // Ensure the controller's OnDialogClosed callback has run and updated state.
  ASSERT_TRUE(close_future.Wait());
  EXPECT_FALSE(IsDialogVisible());
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

}  // namespace skills
