// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"

namespace glic {
namespace {

class GlicFocusBrowserTest : public GlicApiBrowserTest {
 public:
  GlicFocusBrowserTest()
      : GlicApiBrowserTest(GlicTestJsPath("./glic_focus_browsertest.js")) {}
};

// Regression test for b/475260887. The autofocus <input> element in the client
// page does not receive focus on opening the side panel.
IN_PROC_BROWSER_TEST_F(GlicFocusBrowserTest, testFocusOnSidePanelOpen) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicFocusBrowserTest, testFocusOnInvoke) {
  glic::GlicInvokeOptions options(
      glic::Target(*GetTabListInterface()->GetActiveTab()),
      glic::mojom::InvocationSource::kAutofill);
  service()->Invoke(std::move(options));
  ExecuteJsTest();
}

// Regression test for b/504144250. The client page in the side panel does not
// lose page focus.
IN_PROC_BROWSER_TEST_F(GlicFocusBrowserTest, testBlurOnOmniboxFocus) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  chrome::FocusLocationBar(GetBrowserWindowInterface());

  ContinueJsTest();
}

}  // namespace
}  // namespace glic
