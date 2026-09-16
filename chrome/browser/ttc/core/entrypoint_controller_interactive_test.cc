// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_deref.h"
#include "chrome/browser/ttc/core/entrypoint_controller.h"
#include "chrome/browser/ttc/core/ttc_interactive_browser_test_base.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
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
          return CHECK_DEREF(
                     target_browser->GetFeatures().pinned_toolbar_actions())
              .IsActionPoppedOut(kActionTtcToolbar);
        },
        expected_popped_out);
  }
};

IN_PROC_BROWSER_TEST_F(EntrypointControllerInteractiveTest,
                       ActionItemRegistrationAndSessionToggle) {
  // clang-format off
  RunTestSequence(
    CheckHasSession(false),

    SetToolbarButtonPinned(true),
    WaitForShow(kPinnedToolbarActionTtcElementId),
    CheckToolbarButtonHighlighted(false),

    PressButton(kPinnedToolbarActionTtcElementId),
    CheckHasSession(true),
    CheckToolbarButtonHighlighted(true),

    PressButton(kPinnedToolbarActionTtcElementId),
    CheckHasSession(false),
    CheckToolbarButtonHighlighted(false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerInteractiveTest,
                       MultiWindowActionStateSync) {
  BrowserWindowInterface* const second_browser = CreateBrowser(profile());
  ASSERT_NE(second_browser, nullptr);
  const ui::ElementContext second_context =
      BrowserElements::From(second_browser)->GetContext();

  // clang-format off
  RunTestSequence(
    CheckHasSession(false),

    SetToolbarButtonPinned(true),
    WaitForShow(kPinnedToolbarActionTtcElementId),
    CheckToolbarButtonHighlighted(false),
    InContext(
      second_context,
      WaitForShow(kPinnedToolbarActionTtcElementId),
      CheckToolbarButtonHighlighted(false)
    ),

    PressButton(kPinnedToolbarActionTtcElementId),
    CheckHasSession(true),
    CheckToolbarButtonHighlighted(true),
    InContext(
      second_context,
      CheckToolbarButtonHighlighted(true)
    ),

    InContext(
      second_context,
      PressButton(kPinnedToolbarActionTtcElementId)
    ),
    CheckHasSession(false),
    CheckToolbarButtonHighlighted(false),
    InContext(
      second_context,
      CheckToolbarButtonHighlighted(false)
    )
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerInteractiveTest,
                       UnpinnedButtonPopsOutDuringSession) {
  // clang-format off
  RunTestSequence(
    CheckHasSession(false),

    SetToolbarButtonPinned(false),
    WaitForHide(kPinnedToolbarActionTtcElementId),
    CheckToolbarButtonPoppedOut(browser(), false),

    Do([this]() { ttc_service().StartSession(); }),
    CheckHasSession(true),

    WaitForShow(kPinnedToolbarActionTtcElementId),
    CheckToolbarButtonPoppedOut(browser(), true),
    CheckToolbarButtonHighlighted(true),

    Do([this]() { ttc_service().EndSession(); }),
    CheckHasSession(false),
    WaitForHide(kPinnedToolbarActionTtcElementId),
    CheckToolbarButtonPoppedOut(browser(), false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(EntrypointControllerInteractiveTest,
                       NewBrowserWindowReflectsActiveSession) {
  // clang-format off
  RunTestSequence(
    CheckHasSession(false),

    SetToolbarButtonPinned(true),
    WaitForShow(kPinnedToolbarActionTtcElementId),

    PressButton(kPinnedToolbarActionTtcElementId),
    CheckHasSession(true),
    CheckToolbarButtonHighlighted(true)
  );
  // clang-format on

  BrowserWindowInterface* const second_browser = CreateBrowser(profile());
  ASSERT_NE(second_browser, nullptr);

  // clang-format off
  RunTestSequenceInContext(
    BrowserElements::From(second_browser)->GetContext(),

    WaitForShow(kPinnedToolbarActionTtcElementId),
    CheckToolbarButtonHighlighted(true),
    CheckHasSession(true)
  );
  // clang-format on
}

}  // namespace

}  // namespace ttc
