// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"

namespace glic {
namespace {

class GlicFocusInteractiveTest : public InteractiveGlicApiTest {
 public:
  GlicFocusInteractiveTest()
      : InteractiveGlicApiTest("./glic_focus_interactive_uitest.js") {}
};

// Regression test for b/475260887. The autofocus <input> element in the client
// page does not receive focus on opening the side panel.
IN_PROC_BROWSER_TEST_F(GlicFocusInteractiveTest, testFocusOnSidePanelOpen) {
  RunTestSequence(OpenGlic());
  ExecuteJsTest();
}

// Regression test for b/504144250. The client page in the side panel does not
// lose page focus.
IN_PROC_BROWSER_TEST_F(GlicFocusInteractiveTest, testBlurOnOmniboxFocus) {
  RunTestSequence(OpenGlic());
  ExecuteJsTest();
  RunTestSequence(Do(base::BindLambdaForTesting([&]() {
    views::View* omnibox =
        BrowserElementsViews::From(browser())->GetView(kOmniboxElementId);
    ASSERT_TRUE(omnibox);
    omnibox->RequestFocus();
  })));

  ContinueJsTest();
}

}  // namespace
}  // namespace glic
