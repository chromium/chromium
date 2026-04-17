// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_api_test.h"
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

}  // namespace
}  // namespace glic
