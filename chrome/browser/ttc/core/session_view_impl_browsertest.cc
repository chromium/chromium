// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_view_impl.h"

#include "chrome/browser/ttc/core/ttc_interactive_browser_test_base.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace ttc {

namespace {

class SessionViewImplBrowserTest : public TtcInteractiveBrowserTestBase {
 public:
  SessionViewImplBrowserTest() = default;
  ~SessionViewImplBrowserTest() override = default;

 protected:
  StepBuilder StartSession() {
    return Do([this]() { ttc_service().StartSession(); });
  }
};

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest, StartSessionShowsUI) {
  // clang-format off
  RunTestSequence(
    CheckHasSession(false),
    EnsureNotPresent(dictation::DictationBubbleUi::kViewElementIdForTesting),

    StartSession(),
    CheckHasSession(true),
    WaitForShow(dictation::DictationBubbleUi::kViewElementIdForTesting)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest, EndSessionTearsDownUI) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(dictation::DictationBubbleUi::kViewElementIdForTesting),

    PressButton(dictation::DictationBubbleUi::kCloseButtonElementIdForTesting),

    WaitForHide(dictation::DictationBubbleUi::kViewElementIdForTesting),
    CheckHasSession(false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest,
                       EndSessionFromServiceTearsDownUI) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(dictation::DictationBubbleUi::kViewElementIdForTesting),

    Do([this]() { ttc_service().EndSession(); }),

    WaitForHide(dictation::DictationBubbleUi::kViewElementIdForTesting),
    CheckHasSession(false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest, TabSwitchLeavesUIInPlace) {
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));

  // clang-format off
  RunTestSequence(
    SelectTab(kTabStripElementId, 0),
    StartSession(),
    WaitForShow(dictation::DictationBubbleUi::kViewElementIdForTesting),

    SelectTab(kTabStripElementId, 1),

    EnsurePresent(dictation::DictationBubbleUi::kViewElementIdForTesting),
    CheckHasSession(true)
  );
  // clang-format on
}

}  // namespace

}  // namespace ttc
