// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/session_ui_impl.h"

#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/dictation_interactive_browser_test_base.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/session_state.h"
#include "chrome/browser/dictation/session_ui.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"
#include "chrome/common/extensions/api/dictation_private.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/switches.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/controls/button/label_button.h"
#include "url/gurl.h"

namespace dictation {

class DictationSessionUiImplBrowserTest
    : public DictationInteractiveBrowserTestBase {
 public:
  DictationSessionUiImplBrowserTest() = default;
  ~DictationSessionUiImplBrowserTest() override = default;

 protected:
  auto CloseTab(int index) {
    return Do([this, index]() {
      browser()->tab_strip_model()->CloseWebContentsAt(
          index, TabCloseTypes::CLOSE_USER_GESTURE);
    });
  }

  auto MoveTabToWindow(Browser* source, Browser* target, int index) {
    return Do([source, target, index]() {
      chrome::MoveTabsToExistingWindow(source, target, {index});
    });
  }
};

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
                       SessionStateUpdatesToggleButton) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),
    WaitForShow(DictationBubbleUi::kWaveformElementIdForTesting),

    // kStreamInitializing.
    CheckResult(GetSessionState(), SessionState::kStreamInitializing),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Done"),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::View::GetEnabled, true),

    // kTranscribing.
    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    CheckResult(GetSessionState(), SessionState::kTranscribing),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Done"),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::View::GetEnabled, true),

    // kFinalizing.
    Do([this]{
      dictation_service().session_controller()->EndDictationStream();
    }),
    CheckResult(GetSessionState(), SessionState::kFinalizing),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Done"),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::View::GetEnabled, false),

    // kInactive
    ExtensionAPISetStreamState(ExtensionStreamState::kComplete),
    CheckResult(GetSessionState(), SessionState::kInactive),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Start"),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::View::GetEnabled, true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
                       EndSessionTearsDownUI) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    PressButton(DictationBubbleUi::kCloseButtonElementIdForTesting),
    WaitForHide(DictationBubbleUi::kViewElementIdForTesting),
    Check([this]{ return session_ui() == nullptr; })
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
                       DoneButtonEndsActiveStream) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    CheckResult(GetSessionState(), SessionState::kTranscribing),

    PressButton(DictationBubbleUi::kToggleButtonElementIdForTesting),

    // TODO(b/525943882): Currently the controller immediately disposes of the
    // stream but it should really be going into a finalization state. Update
    // once this is fixed.
    CheckResult(GetSessionState(), testing::Ne(SessionState::kTranscribing)),
    CheckResult(HasAttachedStreamProvider(), false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
                       ToggleStartStopFromUi) {
  // clang-format off
  RunTestSequence(
    // Open the session ui.
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    // Move to transcribing state
    CheckResult(GetSessionState(), SessionState::kStreamInitializing),
    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    CheckResult(GetSessionState(), SessionState::kTranscribing),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Done"),

    // Click "Done".
    PressButton(DictationBubbleUi::kToggleButtonElementIdForTesting),
    CheckResult(GetSessionState(), SessionState::kFinalizing),
    ExtensionAPISetStreamState(ExtensionStreamState::kComplete),
    CheckResult(GetSessionState(), SessionState::kInactive),

    // The button should become "Start"; click it.
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Start"),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::View::GetEnabled, true),
    PressButton(DictationBubbleUi::kToggleButtonElementIdForTesting),

    // Ensure the button becomes "Done" again and a new stream was started.
    CheckResult(GetSessionState(), SessionState::kStreamInitializing),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Done"),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::View::GetEnabled, true),
    CheckResult(HasAttachedStreamProvider(), true),

    // Click "Done" to end the second stream.
    PressButton(DictationBubbleUi::kToggleButtonElementIdForTesting),
    CheckResult(GetSessionState(), SessionState::kFinalizing),
    CheckResult(HasAttachedStreamProvider(), false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest, TabSwitchHidesUI) {
  // Add a second tab with the first tab in the foreground.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(0);

  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    // Switch to the second tab and ensure the UI hides.
    SelectTab(kTabStripElementId, 1),
    WaitForHide(DictationBubbleUi::kViewElementIdForTesting)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest, CloseTabEndsSession) {
  // Add a second tab with the first tab in the foreground.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(0);

  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    // Close the active tab (tab 0).
    CloseTab(0),

    // The UI should hide and the session should be ended.
    WaitForHide(DictationBubbleUi::kViewElementIdForTesting),
    CheckHasSession(false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
                       UiFollowsDetachedTab) {
  // Add a second tab with the first tab in the foreground.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Create a second browser window.
  Browser* second_browser = CreateBrowser(browser()->profile());

  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    // Move the dictating tab to the second window.
    MoveTabToWindow(browser(), second_browser, 0),

    // Ensure the UI follows the tab to the new window.
    InContext(BrowserElements::From(second_browser)->GetContext(),
              WaitForShow(DictationBubbleUi::kViewElementIdForTesting)),
    InContext(BrowserElements::From(browser())->GetContext(),
              EnsureNotPresent(DictationBubbleUi::kViewElementIdForTesting)),
    CheckHasSession(true)
  );
  // clang-format on
}

}  // namespace dictation
