// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/test_support/glic_browser_interactive_test.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

// TODO(b/537847327): This test should be migrated to use GlicApiBrowserTest,
// and not the test client.
class GlicSelectionObserverInteractiveUiTest
    : public GlicBrowserInteractiveTest {
 public:
  GlicSelectionObserverInteractiveUiTest() {
    glic_test_environment().SetGlicPagePath("/glic/test_client/index.html");
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicSelectionPrompt, {{"updates_only", "true"}}}}, {});
  }

 protected:
  static constexpr char kCheckContextJs[] = R"JS(
       () => {
          let c = document.querySelector('#additionalContextResult');
          return !!c &&
              c.innerText.includes('Tab ID: ') &&
              c.innerText.includes('MIME Type: application/x-glic-selection') &&
              c.innerText.includes('Data: This page');
       }
    )JS";

  auto SelectAll() {
    return Do([this] {
      content::WebContents* web_contents =
          GetTabListInterface()->GetActiveTab()->GetContents();
      web_contents->SelectAll();
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicSelectionObserverInteractiveUiTest,
                       SelectionUpdatesContext) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  const GURL url = embedded_test_server()->GetURL("/title2.html");
  const DeepQuery kPathToBody{
      "body",
  };

  RunTestSequence(InstrumentTab(kActiveTab),
                  NavigateWebContents(kActiveTab, url));

  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  RunTestSequence(WaitForWebContentsPainted(kActiveTab),
                  MoveMouseTo(kActiveTab, kPathToBody), ClickMouse(),
                  ClickMouse(), SelectAll());

  EXPECT_TRUE(RunUntil(
      [&]() {
        auto* guest_frame = instance->host().GetGuestMainFrame();
        return guest_frame &&
               content::EvalJs(guest_frame, base::StrCat({"(", kCheckContextJs,
                                                          ")()"})) == true;
      },
      "Timeout waiting for selection context in Glic"));
}

}  // namespace
}  // namespace glic
