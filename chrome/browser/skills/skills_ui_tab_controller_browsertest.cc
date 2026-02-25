// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_tab_controller.h"

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_view.h"
#include "chrome/browser/ui/webui/skills/skills_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/skills/features.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_metrics.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace skills {

class SkillsUiTabControllerBrowserTest : public InProcessBrowserTest {
 public:
  SkillsUiTabControllerBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kSkillsEnabled);
  }

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

  // Helper to get the ElementContext for the current browser window.
  ui::ElementContext GetBrowserContext() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return views::ElementTrackerViews::GetContextForWidget(
        browser_view->GetWidget());
  }

  // Locates the WebContents inside the open dialog using the ElementIdentifier.
  content::WebContents* GetDialogWebContents() {
    auto* view = views::ElementTrackerViews::GetInstance()->GetUniqueView(
        SkillsDialogView::kSkillsDialogElementId, GetBrowserContext());

    if (!view) {
      return nullptr;
    }

    auto* web_view = static_cast<views::WebView*>(view);
    return web_view ? web_view->GetWebContents() : nullptr;
  }

  // Helper to get the underlying widget of the dialog.
  views::Widget* GetDialogWidget() {
    auto* view = views::ElementTrackerViews::GetInstance()->GetUniqueView(
        SkillsDialogView::kSkillsDialogElementId, GetBrowserContext());

    if (!view) {
      return nullptr;
    }
    return view->GetWidget();
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify we can open the dialog.
IN_PROC_BROWSER_TEST_F(SkillsUiTabControllerBrowserTest,
                       ShowDialogOpensWidget) {
  EXPECT_FALSE(IsDialogVisible());
  histogram_tester_.ExpectBucketCount("Skills.Dialog.Creation.Action",
                                      SkillsDialogAction::kOpened, 0);
  skills::Skill test_skill("", "skill_name", "icon", "Test Prompt");
  skills_ui_tab_controller()->ShowDialog(std::move(test_skill));

  EXPECT_TRUE(IsDialogVisible());
  EXPECT_NE(nullptr, GetDialogWebContents());
  histogram_tester_.ExpectBucketCount("Skills.Dialog.Creation.Action",
                                      SkillsDialogAction::kOpened, 1);
}

// Verify calling ShowDialog twice doesn't open two dialogs.
IN_PROC_BROWSER_TEST_F(SkillsUiTabControllerBrowserTest, PreventDoubleOpen) {
  skills::Skill test_skill("id", "skill_name", "icon", "Test Prompt");
  skills_ui_tab_controller()->ShowDialog(std::move(test_skill));

  views::Widget* first_widget = GetDialogWidget();
  ASSERT_TRUE(first_widget);

  // Try to open again immediately.
  skills::Skill test_skill2("id2", "skill_name2", "icon", "Test Prompt");
  skills_ui_tab_controller()->ShowDialog(std::move(test_skill2));

  // Widget should be exactly the same instance.
  views::Widget* second_widget = GetDialogWidget();
  EXPECT_EQ(first_widget, second_widget);
  EXPECT_FALSE(first_widget->IsClosed());
}

// Verify calling CloseDialog destroys the widget.
IN_PROC_BROWSER_TEST_F(SkillsUiTabControllerBrowserTest,
                       CloseDialogDestroysWidget) {
  skills::Skill test_skill("id", "skill_name", "icon", "Test Prompt");
  skills_ui_tab_controller()->ShowDialog(std::move(test_skill));
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
  skills_ui_tab_controller()->ShowDialog(std::move(test_skill));

  views::Widget* widget = GetDialogWidget();
  ASSERT_TRUE(widget);

  // Setup Waiter to verify the callback runs.
  base::test::TestFuture<void> close_future;
  skills_ui_tab_controller()->SetOnDialogClosedCallbackForTesting(
      close_future.GetCallback());

  // Simulate User Clicking "X".
  widget->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);

  // Wait for the loop to complete.
  ASSERT_TRUE(close_future.Wait());
  EXPECT_FALSE(IsDialogVisible());

  // Verify we can reopen (implies internal state was reset).
  skills::Skill test_skill2("id2", "name2", "icon2", "prompt2");
  skills_ui_tab_controller()->ShowDialog(std::move(test_skill2));
  EXPECT_TRUE(IsDialogVisible());
}

// Verify that closing the Tab does not crash the browser.
// (Regression test for the destruction race condition).
IN_PROC_BROWSER_TEST_F(SkillsUiTabControllerBrowserTest, TabCloseDoesNotCrash) {
  skills::Skill test_skill("id", "name", "icon", "prompt");
  skills_ui_tab_controller()->ShowDialog(std::move(test_skill));
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
  skills::Skill test_skill("id", "skill_name", "icon", "Test Prompt");
  controller_a->ShowDialog(std::move(test_skill));
  EXPECT_TRUE(IsDialogVisible());

  // Open a new Tab B and switch to it.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  auto* controller_b = skills_ui_tab_controller();
  ASSERT_NE(controller_a, controller_b);

  // Tab B should have no dialogs visible.
  EXPECT_FALSE(IsDialogVisible());

  // Controller B shouldn't have a dialog open.
  // Verify this by calling ShowDialog and ensuring it does open one.
  skills::Skill test_skill2("id2", "skill_name2", "icon", "Test Prompt");
  controller_b->ShowDialog(std::move(test_skill2));

  EXPECT_TRUE(IsDialogVisible());

  // Switch back to A to prove the dialog is still there.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(IsDialogVisible());
}

// Verify that the UI Controller (SkillsUI) received the delegate pointer.
IN_PROC_BROWSER_TEST_F(SkillsUiTabControllerBrowserTest, VerifyWebUIPlumbing) {
  // Enable Glic late to avoid a crash in GlicTabIndicatorHelper during tab
  // creation.
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);

  // Show the dialog.
  skills::Skill test_skill("id", "skill_name", "icon", "Test Prompt");
  skills_ui_tab_controller()->ShowDialog(std::move(test_skill));

  // Dig down to find the SkillsUI.
  // The controller holds the delegate -> which holds WebContents -> which holds
  // WebUI
  content::WebContents* dialog_contents = GetDialogWebContents();
  ASSERT_TRUE(dialog_contents);

  // Wait for the WebUI to fully load and attach.
  ASSERT_TRUE(content::WaitForLoadStop(dialog_contents));

  content::WebUI* web_ui = dialog_contents->GetWebUI();
  ASSERT_TRUE(web_ui);

  auto* skills_ui = web_ui->GetController()->GetAs<skills::SkillsUI>();
  ASSERT_TRUE(skills_ui);

  EXPECT_TRUE(skills_ui->GetDelegateForTesting());
  EXPECT_EQ(skills_ui->GetInitialSkillForTesting().name, "skill_name");
  EXPECT_EQ(skills_ui->GetInitialSkillForTesting().icon, "icon");
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
}

// Verify that clicking the Cancel button closes the dialog.
IN_PROC_BROWSER_TEST_F(SkillsUiTabControllerBrowserTest,
                       CancelButtonClosesDialog) {
  // Enable Glic late to avoid a crash in GlicTabIndicatorHelper during tab
  // creation.
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);
  skills::Skill test_skill("", "name", "icon", "prompt");
  skills_ui_tab_controller()->ShowDialog(std::move(test_skill));

  content::WebContents* web_contents = GetDialogWebContents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  histogram_tester_.ExpectBucketCount("Skills.Dialog.Creation.Action",
                                      SkillsDialogAction::kCancelled, 0);

  // Setup Listener.
  base::test::TestFuture<void> close_future;
  skills_ui_tab_controller()->SetOnDialogClosedCallbackForTesting(
      close_future.GetCallback());

  // Script to click button.
  static constexpr char kClickScript[] = R"(
    (async function() {
      const app = document.querySelector('skills-dialog-app');
      const button = app ? app.shadowRoot.getElementById('cancelButton') : null;
      if (button) {
        button.click();
        return true;
      }
      return false;
    })();
  )";

  std::ignore = content::ExecJs(web_contents, kClickScript);
  // Wait for native close.
  ASSERT_TRUE(close_future.Wait());
  EXPECT_FALSE(IsDialogVisible());

  histogram_tester_.ExpectBucketCount("Skills.Dialog.Creation.Action",
                                      SkillsDialogAction::kCancelled, 1);
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
}

// Verify that the Skill data passed to ShowDialog correctly populates the
// HTML input fields in the WebUI.
IN_PROC_BROWSER_TEST_F(SkillsUiTabControllerBrowserTest,
                       SkillPopulatesUIFields) {
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);

  // Setup a specific test skill.
  const std::string kTestName = "Vegan for 3";
  const std::string kTestPrompt =
      "Convert this recipe to vegan and adjust to serve 3 people.";
  const std::string kTestIcon = "🥦";
  skills::Skill test_skill("test-id", kTestName, kTestIcon, kTestPrompt);

  // Open the dialog.
  skills_ui_tab_controller()->ShowDialog(std::move(test_skill));
  EXPECT_TRUE(IsDialogVisible());

  //  Get the WebContents and wait for it to load.
  content::WebContents* web_contents = GetDialogWebContents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  // Script to check values inside the shadow DOM of the custom element.
  static constexpr char kCheckFieldsScript[] = R"(
    (async function() {
      const getApp = () => document.querySelector('skills-dialog-app');

      for (let i = 0; i < 20; i++) {
        const app = getApp();
        // Wait for the app and its internal Lit rendering to be ready.
        if (app && app.shadowRoot) {
          await app.updateComplete;

          const nameInput = app.$['nameText'];
          const instructionText = app.$['instructionsText'];
          const emojiInput = app.shadowRoot.querySelector('.emoji-trigger');

          return {
            name: nameInput ? nameInput.value : '',
            prompt: instructionText ? instructionText.value : '',
            icon: emojiInput ? emojiInput.value : ''
          };
        }
        await new Promise(r => setTimeout(r, 50));
      }
      return {error: 'app not found'};
    })();
  )";

  auto result = content::EvalJs(web_contents, kCheckFieldsScript);
  const base::DictValue& values = result.ExtractDict();
  // Assert the UI state matches the C++ test_skill.
  const std::string* name = values.FindString("name");
  const std::string* prompt = values.FindString("prompt");
  const std::string* icon = values.FindString("icon");
  ASSERT_TRUE(name);
  ASSERT_TRUE(prompt);
  ASSERT_TRUE(icon);
  EXPECT_EQ(*name, kTestName);
  EXPECT_EQ(*prompt, kTestPrompt);
  EXPECT_EQ(*icon, kTestIcon);

  glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
}

IN_PROC_BROWSER_TEST_F(SkillsUiTabControllerBrowserTest,
                       KeyboardShortcutsAreRouted) {
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);

  skills::Skill test_skill("id", "name", "icon", "prompt");
  skills_ui_tab_controller()->ShowDialog(std::move(test_skill));

  content::WebContents* dialog_contents = GetDialogWebContents();
  ASSERT_TRUE(dialog_contents);
  ASSERT_TRUE(content::WaitForLoadStop(dialog_contents));

  // Focus the WebView to ensure it's ready for input.
  views::Widget* widget = GetDialogWidget();
  widget->GetFocusManager()->SetFocusedView(
      views::ElementTrackerViews::GetInstance()->GetUniqueView(
          SkillsDialogView::kSkillsDialogElementId, GetBrowserContext()));

  static constexpr char kSetupScript[] = R"(
    (async function() {
      await customElements.whenDefined('skills-dialog-app');
      for (let i = 0; i < 40; i++) {
        const app = document.querySelector('skills-dialog-app');
        if (app && app.shadowRoot) {
            const crInput = app.shadowRoot.getElementById('nameText');
            if (crInput && crInput.shadowRoot) {
              const nativeInput = crInput.shadowRoot.querySelector('input');
              if (nativeInput) {
                // Use cr-input's API to set the value safely
                crInput.value = 'Hello World';
                nativeInput.focus();
                // Force an input event so the Undo stack registers it
                nativeInput.dispatchEvent(
                  new Event('input', { bubbles: true }));
                return true;
              }
            }
          }
        await new Promise(r => setTimeout(r, 100));
      }
      return false;
    })();
  )";
  EXPECT_EQ(true, content::EvalJs(dialog_contents, kSetupScript));

  // Simulate the Accelerator (Ctrl+A / Cmd+A).
  ui::KeyboardCode key = ui::VKEY_A;
  bool control = false;
  bool command = false;
#if BUILDFLAG(IS_MAC)
  command = true;
#else
  control = true;
#endif

  content::SimulateKeyPress(dialog_contents, ui::DomKey::FromCharacter('a'),
                            ui::DomCode::US_A, key, control, /*shift=*/false,
                            /*alt=*/false, command);

  auto selection_check = content::EvalJs(dialog_contents, R"(
    (function() {
      const app = document.querySelector('skills-dialog-app');
      const crInput = app && app.shadowRoot ?
                      app.shadowRoot.getElementById('nameText') : null;
      const nativeInput = crInput && crInput.shadowRoot ?
                          crInput.shadowRoot.querySelector('input') : null;
      return nativeInput ?
        (nativeInput.selectionEnd - nativeInput.selectionStart) : 0;
    })()
  )");
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
}

}  // namespace skills
