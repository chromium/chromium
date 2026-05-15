// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_selection_observer.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/ui/browser.h"
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

class GlicSelectionObserverInteractiveUiTest
    : public test::InteractiveGlicTest {
 public:
  GlicSelectionObserverInteractiveUiTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlic, {}},
         {features::kGlicSelectionPrompt, {{"updates_only", "true"}}}},
        {});
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
          browser()->tab_strip_model()->GetActiveWebContents();
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

  RunTestSequence(
      InstrumentTab(kActiveTab, std::nullopt, browser(), true),
      NavigateWebContents(kActiveTab, url), OpenGlic(),
      WaitForWebContentsPainted(kActiveTab),
      MoveMouseTo(kActiveTab, kPathToBody), ClickMouse(), ClickMouse(),
      SelectAll(),
      WaitForJsResult(test::kGlicContentsElementId, kCheckContextJs));
}

}  // namespace
}  // namespace glic
