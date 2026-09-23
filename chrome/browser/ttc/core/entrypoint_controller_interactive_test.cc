// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_deref.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ttc/core/entrypoint_controller.h"
#include "chrome/browser/ttc/core/ttc_interactive_browser_test_base.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"

namespace ttc {

namespace {

class EntrypointControllerInteractiveTest
    : public TtcInteractiveBrowserTestBase {
 public:
  EntrypointControllerInteractiveTest() = default;
  ~EntrypointControllerInteractiveTest() override = default;

 protected:
  StepBuilder SetToolbarButtonPinned(bool pinned) {
    return Do([this, pinned]() {
      CHECK_DEREF(PinnedToolbarActionsModel::Get(profile()))
          .UpdatePinnedState(kActionTtcToolbar, pinned);
    });
  }

  StepBuilder CheckToolbarButtonHighlighted(bool expected_highlighted) {
    return CheckView(
        kPinnedToolbarActionTtcElementId,
        [](PinnedActionToolbarButton* button) { return button->IsActive(); },
        expected_highlighted);
  }

  StepBuilder CheckToolbarButtonPoppedOut(
      BrowserWindowInterface* target_browser,
      bool expected_popped_out) {
    return CheckResult(
        [target_browser]() {
          return CHECK_DEREF(BrowserWindow::FromBrowser(target_browser)
                                 ->GetPinnedToolbarActions())
              .IsActionPoppedOut(kActionTtcToolbar);
        },
        expected_popped_out);
  }

  StepBuilder CheckAppMenuCommandEnabled(BrowserWindowInterface* target_browser,
                                         bool expected_enabled) {
    return CheckResult(
        [target_browser]() {
          return chrome::IsCommandEnabled(target_browser, IDC_SHOW_TTC_MENU);
        },
        expected_enabled);
  }

  StepBuilder CheckAppMenuActionEnabled(BrowserWindowInterface* target_browser,
                                        bool expected_enabled) {
    return CheckResult(
        [target_browser]() {
          const actions::ActionItem* const action =
              actions::ActionManager::Get().FindAction(
                  kActionShowTtcMenu,
                  CHECK_DEREF(BrowserActions::From(target_browser))
                      .root_action_item());
          return action && action->GetEnabled();
        },
        expected_enabled);
  }

  MultiStep CheckAppMenuEntrypointEnabled(
      BrowserWindowInterface* target_browser,
      bool expected_enabled) {
    return Steps(CheckAppMenuCommandEnabled(target_browser, expected_enabled),
                 CheckAppMenuActionEnabled(target_browser, expected_enabled));
  }

  StepBuilder InvokeAppMenuAction(BrowserWindowInterface* target_browser) {
    return Do([target_browser]() {
      CHECK_DEREF(actions::ActionManager::Get().FindAction(
                      kActionShowTtcMenu,
                      CHECK_DEREF(BrowserActions::From(target_browser))
                          .root_action_item()))
          .InvokeAction();
    });
  }
};

class EntrypointControllerAppMenuInteractiveTest
    : public EntrypointControllerInteractiveTest,
      public testing::WithParamInterface<bool> {
 public:
  EntrypointControllerAppMenuInteractiveTest() {
    glow_up_feature_list_.InitWithFeatureState(features::kAppMenuGlowUp,
                                               GetParam());
  }
  ~EntrypointControllerAppMenuInteractiveTest() override = default;

 private:
  base::test::ScopedFeatureList glow_up_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         EntrypointControllerAppMenuInteractiveTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "GlowUpEnabled"
                                             : "GlowUpDisabled";
                         });

IN_PROC_BROWSER_TEST_F(EntrypointControllerInteractiveTest,
                       ActionItemRegistrationAndSessionToggle) {
  RunTestSequence(CheckHasSession(false),

                  SetToolbarButtonPinned(true),
                  WaitForShow(kPinnedToolbarActionTtcElementId),
                  CheckToolbarButtonHighlighted(false),

                  PressButton(kPinnedToolbarActionTtcElementId),
                  CheckHasSession(true), CheckToolbarButtonHighlighted(true),

                  PressButton(kPinnedToolbarActionTtcElementId),
                  CheckHasSession(false), CheckToolbarButtonHighlighted(false));
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerInteractiveTest,
                       MultiWindowActionStateSync) {
  BrowserWindowInterface* const second_browser = CreateBrowser(profile());
  ASSERT_NE(second_browser, nullptr);
  const ui::ElementContext second_context =
      BrowserElements::From(second_browser)->GetContext();

  RunTestSequence(
      CheckHasSession(false),

      SetToolbarButtonPinned(true),
      WaitForShow(kPinnedToolbarActionTtcElementId),
      CheckToolbarButtonHighlighted(false),
      InContext(second_context, WaitForShow(kPinnedToolbarActionTtcElementId),
                CheckToolbarButtonHighlighted(false)),

      PressButton(kPinnedToolbarActionTtcElementId), CheckHasSession(true),
      CheckToolbarButtonHighlighted(true),
      InContext(second_context, CheckToolbarButtonHighlighted(true)),

      InContext(second_context, PressButton(kPinnedToolbarActionTtcElementId)),
      CheckHasSession(false), CheckToolbarButtonHighlighted(false),
      InContext(second_context, CheckToolbarButtonHighlighted(false)));
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerInteractiveTest,
                       UnpinnedButtonPopsOutDuringSession) {
  RunTestSequence(
      CheckHasSession(false),

      SetToolbarButtonPinned(false),
      WaitForHide(kPinnedToolbarActionTtcElementId),
      CheckToolbarButtonPoppedOut(browser(), false),

      Do([this]() { ttc_service().StartSession(); }), CheckHasSession(true),

      WaitForShow(kPinnedToolbarActionTtcElementId),
      CheckToolbarButtonPoppedOut(browser(), true),
      CheckToolbarButtonHighlighted(true),

      Do([this]() { ttc_service().EndSession(); }), CheckHasSession(false),
      WaitForHide(kPinnedToolbarActionTtcElementId),
      CheckToolbarButtonPoppedOut(browser(), false));
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerInteractiveTest,
                       NewBrowserWindowReflectsActiveSession) {
  RunTestSequence(CheckHasSession(false),

                  SetToolbarButtonPinned(true),
                  WaitForShow(kPinnedToolbarActionTtcElementId),

                  PressButton(kPinnedToolbarActionTtcElementId),
                  CheckHasSession(true), CheckToolbarButtonHighlighted(true));

  BrowserWindowInterface* const second_browser = CreateBrowser(profile());
  ASSERT_NE(second_browser, nullptr);

  RunTestSequenceInContext(BrowserElements::From(second_browser)->GetContext(),

                           WaitForShow(kPinnedToolbarActionTtcElementId),
                           CheckToolbarButtonHighlighted(true),
                           CheckHasSession(true));
}

IN_PROC_BROWSER_TEST_P(EntrypointControllerAppMenuInteractiveTest,
                       AppMenuEntrypointStateFollowsSession) {
  RunTestSequence(
      CheckHasSession(false), CheckAppMenuEntrypointEnabled(browser(), true),

      Do([this]() { ttc_service().StartSession(); }), CheckHasSession(true),
      CheckAppMenuEntrypointEnabled(browser(), false),

      Do([this]() { ttc_service().EndSession(); }), CheckHasSession(false),
      CheckAppMenuEntrypointEnabled(browser(), true));
}

IN_PROC_BROWSER_TEST_P(EntrypointControllerAppMenuInteractiveTest,
                       AppMenuCommandStartsSession) {
  RunTestSequence(
      CheckHasSession(false),

      Do([this]() { chrome::ExecuteCommand(browser(), IDC_SHOW_TTC_MENU); }),
      CheckHasSession(true), CheckAppMenuCommandEnabled(browser(), false));
}

IN_PROC_BROWSER_TEST_P(EntrypointControllerAppMenuInteractiveTest,
                       AppMenuActionStartsSession) {
  RunTestSequence(CheckHasSession(false),

                  InvokeAppMenuAction(browser()), CheckHasSession(true),
                  CheckAppMenuActionEnabled(browser(), false));
}

IN_PROC_BROWSER_TEST_P(EntrypointControllerAppMenuInteractiveTest,
                       AppMenuEntrypointStateSyncsAcrossWindows) {
  BrowserWindowInterface* const second_browser = CreateBrowser(profile());
  ASSERT_NE(second_browser, nullptr);

  RunTestSequence(
      CheckHasSession(false), CheckAppMenuEntrypointEnabled(browser(), true),
      CheckAppMenuEntrypointEnabled(second_browser, true),

      Do([this]() { ttc_service().StartSession(); }), CheckHasSession(true),
      CheckAppMenuEntrypointEnabled(browser(), false),
      CheckAppMenuEntrypointEnabled(second_browser, false),

      Do([this]() { ttc_service().EndSession(); }), CheckHasSession(false),
      CheckAppMenuEntrypointEnabled(browser(), true),
      CheckAppMenuEntrypointEnabled(second_browser, true));
}

IN_PROC_BROWSER_TEST_P(EntrypointControllerAppMenuInteractiveTest,
                       NewBrowserWindowAppMenuEntrypointReflectsActiveSession) {
  RunTestSequence(Do([this]() { ttc_service().StartSession(); }),
                  CheckHasSession(true));

  BrowserWindowInterface* const second_browser = CreateBrowser(profile());
  ASSERT_NE(second_browser, nullptr);

  RunTestSequence(CheckAppMenuEntrypointEnabled(second_browser, false));
}

}  // namespace

}  // namespace ttc
