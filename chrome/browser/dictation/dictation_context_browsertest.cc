// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_context.h"

#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/dictation_browser_test_base.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace dictation {

class DictationContextBrowserTest : public DictationBrowserTestBase {
 public:
  DictationContextBrowserTest() = default;
  ~DictationContextBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DictationContextBrowserTest, APCCaptured) {
  // This test page has a bit of text content.
  const GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  dictation_service().StartSession(*GetBrowserWindowInterface(),
                                   DefaultInPageTargetId(web_contents()));

  ASSERT_NE(session_controller(), nullptr);

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      session_controller()->attached_stream_provider());
  ASSERT_NE(provider, nullptr);

  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());
  std::optional<DictationContext> context = ExtensionGetStartStreamDetails(
      profile(), provider->stream_id_for_testing());
  ASSERT_TRUE(context.has_value());

  // Verify that the annotated page content was captured.
  ASSERT_TRUE(context->annotated_page_content.has_value());
  ASSERT_TRUE(context->annotated_page_content->has_root_node());

  const auto& root = context->annotated_page_content->root_node();
  ASSERT_GT(root.children_nodes_size(), 0);

  const auto& first_child = root.children_nodes(0);
  ASSERT_TRUE(first_child.content_attributes().has_text_data());
  EXPECT_EQ(base::TrimWhitespaceASCII(
                first_child.content_attributes().text_data().text_content(),
                base::TRIM_ALL),
            "Non empty simple page");
  EXPECT_EQ(context->annotated_page_content->main_frame_data().title(), "OK");
}

IN_PROC_BROWSER_TEST_F(DictationContextBrowserTest, SelectedTextCaptured) {
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string script = R"JS(
    var textarea = document.getElementById('text_id');
    textarea.value = 'the quick brown fox';
    textarea.focus();
    textarea.setSelectionRange(4, 15); // "quick brown"
    textarea.value.substring(textarea.selectionStart, textarea.selectionEnd);
  )JS";
  ASSERT_EQ(content::EvalJs(web_contents(), script), "quick brown");

  dictation_service().StartSession(*GetBrowserWindowInterface(),
                                   DefaultInPageTargetId(web_contents()));

  ASSERT_NE(session_controller(), nullptr);

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      session_controller()->attached_stream_provider());
  ASSERT_NE(provider, nullptr);

  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());
  std::optional<DictationContext> context = ExtensionGetStartStreamDetails(
      profile(), provider->stream_id_for_testing());
  ASSERT_TRUE(context.has_value());

  // Verify that the editable content was captured and matches.
  ASSERT_TRUE(context->editable_content.has_value());
  EXPECT_EQ(*context->editable_content, "quick brown");
}

IN_PROC_BROWSER_TEST_F(DictationContextBrowserTest, InnerTextCaptured) {
  const GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  dictation_service().StartSession(*GetBrowserWindowInterface(),
                                   DefaultInPageTargetId(web_contents()));

  SessionController* controller = dictation_service().session_controller();
  ASSERT_NE(controller, nullptr);

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      controller->attached_stream_provider());
  ASSERT_NE(provider, nullptr);

  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());
  std::optional<DictationContext> context = ExtensionGetStartStreamDetails(
      profile(), provider->stream_id_for_testing());
  ASSERT_TRUE(context.has_value());

  // Verify that the inner text was captured.
  ASSERT_TRUE(context->inner_text.has_value());
  EXPECT_EQ(base::TrimWhitespaceASCII(*context->inner_text, base::TRIM_ALL),
            "Non empty simple page");
}

class DictationContextAsyncBrowserTest : public DictationContextBrowserTest {
 public:
  DictationContextAsyncBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kDictation,
        {{"use_component_extension", "false"}, {"send_context_async", "true"}});
  }
  ~DictationContextAsyncBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DictationContextAsyncBrowserTest, AsyncContextCaptured) {
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string script = R"JS(
    var textarea = document.getElementById('text_id');
    textarea.value = 'the quick brown fox';
    textarea.focus();
    textarea.setSelectionRange(4, 15); // "quick brown"
    textarea.value.substring(textarea.selectionStart, textarea.selectionEnd);
  )JS";
  ASSERT_EQ(content::EvalJs(web_contents(), script), "quick brown");

  dictation_service().StartSession(*GetBrowserWindowInterface(),
                                   DefaultInPageTargetId(web_contents()));

  SessionController* controller = dictation_service().session_controller();
  ASSERT_NE(controller, nullptr);

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      controller->attached_stream_provider());
  ASSERT_NE(provider, nullptr);

  // Wait for the stream to start.
  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());

  // Verify that the initial context is empty (since it is sent async).
  std::optional<DictationContext> initial_context =
      ExtensionGetStartStreamDetails(profile(),
                                     provider->stream_id_for_testing());
  EXPECT_FALSE(initial_context.has_value());

  // Now wait for the context update.
  DictationContext updated_context =
      ExtensionGetUpdatedContext(profile(), provider->stream_id_for_testing());

  // Verify that the context was eventually captured.
  ASSERT_TRUE(updated_context.annotated_page_content.has_value());
  EXPECT_TRUE(updated_context.annotated_page_content->has_root_node());

  ASSERT_TRUE(updated_context.editable_content.has_value());
  EXPECT_EQ(*updated_context.editable_content, "quick brown");
}

IN_PROC_BROWSER_TEST_F(DictationContextBrowserTest,
                       SelectedTextCapturedFromIframe) {
  // Load the page with the iframe.
  const GURL url = embedded_test_server()->GetURL("/actor/simple_iframe.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Navigate the iframe to the simple textarea page.
  const GURL iframe_url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");
  ASSERT_TRUE(
      content::NavigateIframeToURL(web_contents(), "iframe", iframe_url));

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* iframe = content::ChildFrameAt(main_frame, 0);
  ASSERT_NE(iframe, nullptr);
  EXPECT_EQ(iframe->GetLastCommittedURL(), iframe_url);

  std::string setup_script = R"JS(
    const textarea = document.getElementById('text_id');
    textarea.value = 'the quick brown fox';
    textarea.focus();
    textarea.setSelectionRange(4, 15); // "quick brown"
    textarea.value.substring(textarea.selectionStart, textarea.selectionEnd);
  )JS";
  ASSERT_EQ(content::EvalJs(iframe, setup_script), "quick brown");

  dictation_service().StartSession(*GetBrowserWindowInterface(),
                                   TargetId{iframe->GetWeakDocumentPtr()});

  ASSERT_NE(session_controller(), nullptr);

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      session_controller()->attached_stream_provider());
  ASSERT_NE(provider, nullptr);

  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());
  std::optional<DictationContext> context = ExtensionGetStartStreamDetails(
      profile(), provider->stream_id_for_testing());
  ASSERT_TRUE(context.has_value());

  ASSERT_TRUE(context->editable_content.has_value());
  EXPECT_EQ(*context->editable_content, "quick brown");
}

}  // namespace dictation
