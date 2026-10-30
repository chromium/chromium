// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/target.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/dictation_interactive_browser_test_base.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/test_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "url/gurl.h"

namespace dictation {

namespace {

#define EXPECT_EDITABLE_TEXT_EQ(selector, expected_text) \
  EXPECT_EQ(expected_text, GetEditableExpectedText(selector, expected_text));

class DictationTargetBrowserTest : public DictationInteractiveBrowserTestBase {
 public:
  DictationTargetBrowserTest()
      : DictationInteractiveBrowserTestBase(
            kSessionEndsOnStreamEnd.default_value) {}
  ~DictationTargetBrowserTest() override = default;

  std::string GetEditableExpectedText(std::string_view selector,
                                      std::string_view expected) {
    return dictation::GetEditableExpectedText(
        web_contents()->GetPrimaryMainFrame(), selector, expected);
  }

  // Performs a renderer round-trip to prevent `Target` from coalescing
  // back-to-back composition operations while an earlier operation is still in
  // flight.
  StepBuilder RoundTrip(ui::ElementIdentifier web_contents_id) {
    return ExecuteJs(web_contents_id, "() => {}");
  }
};

IN_PROC_BROWSER_TEST_F(DictationTargetBrowserTest, InsertIntoEmptyInput) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textinput.html");

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    ExtensionAPIUpdateTranscription(
        ExtensionTranscriptionType::kFinal, "Hello world"),
    ExtensionAPISetStreamState(ExtensionStreamState::kComplete)
  );
  // clang-format on

  EXPECT_EDITABLE_TEXT_EQ("#text_id", "Hello world");
}

IN_PROC_BROWSER_TEST_F(DictationTargetBrowserTest, InsertInMiddleOfText) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textinput.html");
  const DeepQuery kInput{"#text_id"};

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    ExecuteJsAt(kWebContentsElementId, kInput,
                "(el) => {"
                "  el.value = 'Hello world';"
                "  el.setSelectionRange(5, 5);"
                "}"),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    ExtensionAPIUpdateTranscription(
        ExtensionTranscriptionType::kFinal, "there"),
    ExtensionAPISetStreamState(ExtensionStreamState::kComplete)
  );
  // clang-format on

  // Note that the formatting logic prepends a space to "there" before
  // insertion.
  EXPECT_EDITABLE_TEXT_EQ("#text_id", "Hello there world");
}

IN_PROC_BROWSER_TEST_F(DictationTargetBrowserTest, ReplaceSelectedText) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textinput.html");
  const DeepQuery kInput{"#text_id"};

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    ExecuteJsAt(kWebContentsElementId, kInput,
                "(el) => {"
                "  el.value = 'Initial text';"
                "  el.select();"
                "}"),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    ExtensionAPIUpdateTranscription(
        ExtensionTranscriptionType::kFinal, "Hello world"),
    ExtensionAPISetStreamState(ExtensionStreamState::kComplete)
  );
  // clang-format on

  EXPECT_EDITABLE_TEXT_EQ("#text_id", "Hello world");
}

IN_PROC_BROWSER_TEST_F(DictationTargetBrowserTest, InsertIntoIframeInput) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url = embedded_test_server()->GetURL("/iframe_blank.html");
  const GURL iframe_url =
      embedded_test_server()->GetURL("/textinput/simple_textinput.html");

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    Do([this, &iframe_url] {
      ASSERT_TRUE(
          content::NavigateIframeToURL(web_contents(), "test", iframe_url));
    }),
    StartSessionWithTargetInFrame(
        kWebContentsElementId,
        base::BindRepeating(&content::FrameIsChildOfMainFrame), "#text_id"),
    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    ExtensionAPIUpdateTranscription(
        ExtensionTranscriptionType::kFinal, "Hello world"),
    ExtensionAPISetStreamState(ExtensionStreamState::kComplete)
  );
  // clang-format on

  content::RenderFrameHost* iframe =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_NE(iframe, nullptr);
  EXPECT_EQ("Hello world", dictation::GetEditableExpectedText(
                               iframe, "#text_id", "Hello world"));
}

IN_PROC_BROWSER_TEST_F(DictationTargetBrowserTest, InsertIntoContentEditable) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/content_editable.html");
  const DeepQuery kContentEditable{"#anchor_id"};

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    ExecuteJsAt(kWebContentsElementId, kContentEditable,
                "(el) => { el.innerText = ''; }"),
    StartSessionWithTarget(kWebContentsElementId, "#anchor_id"),
    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    ExtensionAPIUpdateTranscription(
        ExtensionTranscriptionType::kFinal, "Hello world"),
    ExtensionAPISetStreamState(ExtensionStreamState::kComplete)
  );
  // clang-format on

  EXPECT_EDITABLE_TEXT_EQ("#anchor_id", "Hello world");
}

class DictationTargetShowPartialsBrowserTest
    : public DictationTargetBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  DictationTargetShowPartialsBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kDictation, {{"use_component_extension", "false"},
                     {"auto_session_end_delay", "0ms"},
                     {"show_partials", GetParam() ? "true" : "false"}});
  }
  ~DictationTargetShowPartialsBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Some sites have contenteditables that interact poorly with IME composition
// when the composition contains newlines. The test page used here intentionally
// handles newlines in compositions poorly, and we test that the final text
// makes it into the element correctly. Regression test for
// https://crbug.com/537833858
IN_PROC_BROWSER_TEST_P(DictationTargetShowPartialsBrowserTest,
                       NewlineRejectingContentEditable) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url = embedded_test_server()->GetURL(
      "/dictation/newline_rejecting_contenteditable.html");

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    StartSessionWithTarget(kWebContentsElementId, "#contenteditable"),
    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    ExtensionAPIUpdateTranscription(
        ExtensionTranscriptionType::kPartial, "* one"),
    RoundTrip(kWebContentsElementId),
    ExtensionAPIUpdateTranscription(
        ExtensionTranscriptionType::kFinal, "* one\n* two"),
    RoundTrip(kWebContentsElementId),
    ExtensionAPISetStreamState(ExtensionStreamState::kComplete)
  );
  // clang-format on

  EXPECT_EDITABLE_TEXT_EQ("#contenteditable", "* one\n* two");
}

INSTANTIATE_TEST_SUITE_P(All,
                         DictationTargetShowPartialsBrowserTest,
                         testing::Bool());

}  // namespace

}  // namespace dictation
