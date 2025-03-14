// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic.mojom-shared.h"
#include "chrome/browser/glic/interactive_glic_test.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/element_identifier.h"

namespace glic::test {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTabId);

class GlicAnnotationManagerUiTest : public test::InteractiveGlicTest {
 public:
  GlicAnnotationManagerUiTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicScrollTo);
  }
  ~GlicAnnotationManagerUiTest() override = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    test::InteractiveGlicTest::SetUpOnMainThread();
  }

  auto ScrollTo(base::Value::Dict selector) {
    return Steps(CheckJsResult(kGlicContentsElementId,
                               content::JsReplace(R"js(
                        () => {
                          return client.browser.scrollTo({selector: $1});
                        }
                      )js",
                                                  std::move(selector))));
  }

  auto ScrollToExpectingError(base::Value::Dict selector,
                              glic::mojom::ScrollToErrorReason error_reason) {
    return Steps(CheckJsResult(kGlicContentsElementId,
                               content::JsReplace(R"js(
                        async () => {
                          try {
                            await client.browser.scrollTo({selector: $1});
                          } catch (err) {
                            return err.reason;
                          }
                        }
                      )js",
                                                  std::move(selector)),
                               ::testing::Eq(static_cast<int>(error_reason))));
  }

  base::Value::Dict ExactTextSelector(std::string exact_text) {
    return base::Value::Dict().Set("exactText",
                                   base::Value::Dict()  //
                                       .Set("text", exact_text));
  }

  base::Value::Dict TextFragmentSelector(std::string text_start,
                                         std::string text_end) {
    return base::Value::Dict().Set("textFragment",
                                   base::Value::Dict()  //
                                       .Set("textStart", text_start)
                                       .Set("textEnd", text_end));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, ScrollToExactText) {
  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(
                      kActiveTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  ScrollTo(ExactTextSelector("Some text")),
                  WaitForJsResult(kActiveTabId, "() => did_scroll"));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, ScrollToTextFragment) {
  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(
                      kActiveTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  ScrollTo(TextFragmentSelector("Some", "text")),
                  WaitForJsResult(kActiveTabId, "() => did_scroll"));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, NoMatchFound) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ScrollToExpectingError(ExactTextSelector("Text does not exist"),
                             glic::mojom::ScrollToErrorReason::kNoMatchFound));
}

class GlicAnnotationManagerWithScrollToDisabledUiTest
    : public test::InteractiveGlicTest {
 public:
  GlicAnnotationManagerWithScrollToDisabledUiTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kGlicScrollTo);
  }
  ~GlicAnnotationManagerWithScrollToDisabledUiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerWithScrollToDisabledUiTest,
                       ScrollToNotAvailable) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached),
                  InAnyContext(CheckJsResult(
                      kGlicContentsElementId,
                      "() => { return !(client.browser.scrollTo); }")));
}
}  // namespace glic::test
