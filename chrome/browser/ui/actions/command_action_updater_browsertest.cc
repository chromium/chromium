// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/command_action_updater.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_action_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/actions/actions.h"
#include "ui/base/class_property.h"

namespace {
DEFINE_UI_CLASS_PROPERTY_KEY(int, kTestPropertyKey, -1)
}  // namespace

class CommandActionUpdaterBrowserTest : public InProcessBrowserTest {
 public:
  CommandActionUpdaterBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kUseActionsForBrowserCommands);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Find the existing kActionBack action.
    actions::ActionItem* root = browser()->GetActions()->root_action_item();
    ASSERT_TRUE(root);
    action_item_ = actions::ActionManager::Get().FindAction(kActionBack, root);
    ASSERT_TRUE(action_item_);

    // Set our test callback.
    action_item_->SetInvokeActionCallback(
        base::BindRepeating(&CommandActionUpdaterBrowserTest::OnActionInvoked,
                            base::Unretained(this)));

    // InProcessBrowserTest should already have one tab open.
    ASSERT_NE(browser()->tab_strip_model()->active_index(),
              TabStripModel::kNoTab);
  }

  void TearDownOnMainThread() override {
    if (action_item_) {
      action_item_->SetInvokeActionCallback(
          actions::ActionItem::InvokeActionCallback());
    }
    action_item_ = nullptr;  // Avoid dangling pointer
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void OnActionInvoked(actions::ActionItem* action,
                       actions::ActionInvocationContext context) {
    action_invoked_ = true;
    last_context_ = std::move(context);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<actions::ActionItem> action_item_ = nullptr;
  bool action_invoked_ = false;
  actions::ActionInvocationContext last_context_;
};

IN_PROC_BROWSER_TEST_F(CommandActionUpdaterBrowserTest,
                       CommandEnablementSyncsToAction) {
  // Force enable it to start with known state.
  chrome::BrowserCommandController::From(browser())->UpdateCommandEnabled(
      IDC_BACK, true);
  EXPECT_TRUE(action_item_->GetEnabled());

  // Disable command.
  chrome::BrowserCommandController::From(browser())->UpdateCommandEnabled(
      IDC_BACK, false);
  // Action should be disabled.
  EXPECT_FALSE(action_item_->GetEnabled());

  // Enable command.
  chrome::BrowserCommandController::From(browser())->UpdateCommandEnabled(
      IDC_BACK, true);
  // Action should be enabled.
  EXPECT_TRUE(action_item_->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(CommandActionUpdaterBrowserTest,
                       CommandExecutionInvokesAction) {
  EXPECT_FALSE(action_invoked_);

  // Force enable it.
  chrome::BrowserCommandController::From(browser())->UpdateCommandEnabled(
      IDC_BACK, true);

  // Execute command.
  EXPECT_TRUE(chrome::BrowserCommandController::From(browser())->ExecuteCommand(
      IDC_BACK));

  // Action should be invoked.
  EXPECT_TRUE(action_invoked_);
}

IN_PROC_BROWSER_TEST_F(CommandActionUpdaterBrowserTest,
                       ExecuteCommandWithContextPassesContextToAction) {
  EXPECT_FALSE(action_invoked_);

  chrome::BrowserCommandController::From(browser())->UpdateCommandEnabled(
      IDC_BACK, true);

  auto context = actions::ActionInvocationContext::Builder()
                     .SetProperty(kTestPropertyKey, 42)
                     .Build();

  EXPECT_TRUE(chrome::ExecuteCommandWithContext(browser(), IDC_BACK,
                                                std::move(context)));
  EXPECT_TRUE(action_invoked_);
  EXPECT_EQ(last_context_.GetProperty(kTestPropertyKey), 42);
}

IN_PROC_BROWSER_TEST_F(CommandActionUpdaterBrowserTest,
                       ExecuteCommandWithDispositionAndContextPassesContext) {
  EXPECT_FALSE(action_invoked_);

  chrome::BrowserCommandController::From(browser())->UpdateCommandEnabled(
      IDC_BACK, true);

  auto context = actions::ActionInvocationContext::Builder()
                     .SetProperty(kTestPropertyKey, 99)
                     .Build();

  EXPECT_TRUE(chrome::ExecuteCommandWithDispositionAndContext(
      browser(), IDC_BACK, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      std::move(context)));
  EXPECT_TRUE(action_invoked_);
  EXPECT_EQ(last_context_.GetProperty(kTestPropertyKey), 99);
}

IN_PROC_BROWSER_TEST_F(CommandActionUpdaterBrowserTest,
                       ExecuteActionPreservesSidePanelOpenTrigger) {
  EXPECT_FALSE(action_invoked_);

  chrome::BrowserCommandController::From(browser())->UpdateCommandEnabled(
      IDC_BACK, true);

  auto context =
      actions::ActionInvocationContext::Builder()
          .SetProperty(kSidePanelOpenTriggerKey,
                       static_cast<int>(SidePanelOpenTrigger::kBookmarkBar))
          .Build();

  EXPECT_TRUE(chrome::ExecuteCommandWithContext(browser(), IDC_BACK,
                                                std::move(context)));
  EXPECT_TRUE(action_invoked_);
  EXPECT_EQ(static_cast<SidePanelOpenTrigger>(
                last_context_.GetProperty(kSidePanelOpenTriggerKey)),
            SidePanelOpenTrigger::kBookmarkBar);
}

IN_PROC_BROWSER_TEST_F(CommandActionUpdaterBrowserTest,
                       ExecuteCommandDoesNotInjectDefaultSidePanelTrigger) {
  EXPECT_FALSE(action_invoked_);

  chrome::BrowserCommandController::From(browser())->UpdateCommandEnabled(
      IDC_BACK, true);
  EXPECT_TRUE(chrome::BrowserCommandController::From(browser())->ExecuteCommand(
      IDC_BACK));
  EXPECT_TRUE(action_invoked_);
  EXPECT_EQ(static_cast<SidePanelOpenTrigger>(
                last_context_.GetProperty(kSidePanelOpenTriggerKey)),
            SidePanelOpenTrigger::kUnknown);
}

IN_PROC_BROWSER_TEST_F(CommandActionUpdaterBrowserTest,
                       ExecuteCommandPreservesInvocationSourceContext) {
  EXPECT_FALSE(action_invoked_);

  browser()->command_controller()->UpdateCommandEnabled(IDC_BACK, true);
  actions::ActionInvocationContext context =
      actions::ActionInvocationContext::Builder()
          .SetProperty(chrome::kActionInvocationSourceKey,
                       chrome::ActionInvocationSource::kKeyboardShortcut)
          .Build();
  EXPECT_TRUE(chrome::ExecuteCommandWithContext(browser(), IDC_BACK,
                                                std::move(context)));
  EXPECT_TRUE(action_invoked_);
  EXPECT_EQ(last_context_.GetProperty(chrome::kActionInvocationSourceKey),
            chrome::ActionInvocationSource::kKeyboardShortcut);
}

IN_PROC_BROWSER_TEST_F(
    CommandActionUpdaterBrowserTest,
    ExecuteToggleVerticalTabsCollapsePreservesKeyboardShortcutSource) {
  actions::ActionItem* root = browser()->GetActions()->root_action_item();
  ASSERT_TRUE(root);
  actions::ActionItem* toggle_action = actions::ActionManager::Get().FindAction(
      kActionToggleCollapseVertical, root);
  ASSERT_TRUE(toggle_action);

  actions::ActionInvocationContext last_context;
  bool toggle_invoked = false;
  toggle_action->SetInvokeActionCallback(base::BindRepeating(
      [](bool* invoked, actions::ActionInvocationContext* ctx,
         actions::ActionItem* action,
         actions::ActionInvocationContext context) {
        *invoked = true;
        *ctx = std::move(context);
      },
      &toggle_invoked, &last_context));

  browser()->command_controller()->UpdateCommandEnabled(
      IDC_TOGGLE_VERTICAL_TABS_COLLAPSE, true);

  // When executed via shortcut (passing kKeyboardShortcut in context):
  actions::ActionInvocationContext shortcut_context =
      actions::ActionInvocationContext::Builder()
          .SetProperty(chrome::kActionInvocationSourceKey,
                       chrome::ActionInvocationSource::kKeyboardShortcut)
          .Build();
  EXPECT_TRUE(chrome::ExecuteCommandWithContext(
      browser(), IDC_TOGGLE_VERTICAL_TABS_COLLAPSE,
      std::move(shortcut_context)));
  EXPECT_TRUE(toggle_invoked);
  EXPECT_EQ(last_context.GetProperty(chrome::kActionInvocationSourceKey),
            chrome::ActionInvocationSource::kKeyboardShortcut);

  // When executed without shortcut context (e.g., from a menu click):
  toggle_invoked = false;
  last_context = actions::ActionInvocationContext();
  EXPECT_TRUE(browser()->command_controller()->ExecuteCommand(
      IDC_TOGGLE_VERTICAL_TABS_COLLAPSE));
  EXPECT_TRUE(toggle_invoked);
  EXPECT_EQ(last_context.GetProperty(chrome::kActionInvocationSourceKey),
            chrome::ActionInvocationSource::kUnknown);

  toggle_action->SetInvokeActionCallback(
      actions::ActionItem::InvokeActionCallback());
}
