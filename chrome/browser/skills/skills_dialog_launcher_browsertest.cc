// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_dialog_launcher.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/skills/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace skills {

class SkillsDialogLauncherBrowserTest : public InProcessBrowserTest {
 public:
  SkillsDialogLauncherBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kSkillsEnabled);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Test Tab is already loaded (e.g. chrome://newtab).
// Expectation: Callback runs immediately with Success.
IN_PROC_BROWSER_TEST_F(SkillsDialogLauncherBrowserTest, LaunchOnLoadedTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab")));

  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);

  base::RunLoop run_loop;
  bool callback_success = false;

  skills::Skill test_skill("id", "skill_name", "icon", "Test Prompt");
  SkillsDialogLauncher::CreateForTab(
      tab, std::move(test_skill), base::BindLambdaForTesting([&](bool success) {
        callback_success = success;
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_TRUE(callback_success);
}

// Test Tab is loading (Race Condition simulation).
// Expectation: Launcher waits for navigation to finish, then succeeds.
IN_PROC_BROWSER_TEST_F(SkillsDialogLauncherBrowserTest,
                       LaunchWaitsForNavigation) {
  GURL url("chrome://newtab");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  content::WebContents* web_contents = tab->GetContents();

  base::RunLoop run_loop;
  bool callback_success = false;

  skills::Skill test_skill("id", "skill_name", "icon", "Test Prompt");
  SkillsDialogLauncher::CreateForTab(
      tab, std::move(test_skill), base::BindLambdaForTesting([&](bool success) {
        callback_success = success;
        run_loop.Quit();
      }));

  // Ensure the page actually finishes loading so the observer fires.
  content::WaitForLoadStop(web_contents);
  run_loop.Run();

  EXPECT_TRUE(callback_success);
  EXPECT_EQ(web_contents->GetLastCommittedURL(), url);
}

// Test Tab is closed while loading.
// Expectation: Callback runs with Failure (false), and no crash occurs.
IN_PROC_BROWSER_TEST_F(SkillsDialogLauncherBrowserTest,
                       TabDestroyedDuringLoad) {
  GURL url("chrome://newtab");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();

  base::RunLoop run_loop;
  std::optional<bool> result;

  // If 'SkillsDialogLauncher' is destroyed, this wrapper detects it
  // and runs the lambda with 'false'.
  base::OnceCallback<void(bool)> test_callback =
      base::BindLambdaForTesting([&](bool success) {
        result = success;
        run_loop.Quit();
      });
  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(test_callback), false);

  // Launch the dialog.
  skills::Skill test_skill("id", "skill_name", "icon", "Test Prompt");
  SkillsDialogLauncher::CreateForTab(tab, std::move(test_skill),
                                     std::move(wrapped_callback));

  // Close the tab immediately.
  browser()->tab_strip_model()->CloseAllTabs();

  run_loop.Run();

  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());
}

}  // namespace skills
