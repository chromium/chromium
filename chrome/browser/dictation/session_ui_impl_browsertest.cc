// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/session_ui_impl.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/session_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"

namespace dictation {

class DictationSessionUiImplBrowserTest : public InteractiveBrowserTest {
 public:
  DictationSessionUiImplBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kDictation);
  }
  ~DictationSessionUiImplBrowserTest() override = default;

  Profile* profile() { return chrome_test_utils::GetProfile(this); }

  DictationKeyedService& dictation_service() {
    return *DictationKeyedService::Get(profile());
  }

  SessionUiImpl* session_ui() {
    if (!dictation_service().session_controller()) {
      return nullptr;
    }

    return static_cast<SessionUiImpl*>(
        dictation_service().session_controller()->ui_for_testing());
  }

  auto StartSession() {
    // clang-format off
    return Steps(
      Do([this]{ dictation_service().StartSession(*browser(), nullptr); }),
      Check([this]{ return session_ui() != nullptr; })
    );
    // clang-format on
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest, StartSessionShowsUI) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting)
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

}  // namespace dictation
