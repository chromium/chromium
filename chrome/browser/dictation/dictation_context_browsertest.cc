// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_context.h"

#include <optional>
#include <string>
#include <string_view>

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
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_api.h"
#include "content/public/browser/global_dom_node_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/window_open_disposition.h"

namespace dictation {

namespace {

bool MockIsEligible(
    const std::string& host,
    const std::string& path,
    const std::vector<optimization_guide::FrameMetadata>& frame_metadata) {
  return false;
}

bool MockIsEligibleWithAccount(
    const std::string& host,
    const std::string& path,
    const std::string& account_email,
    const std::vector<optimization_guide::FrameMetadata>& frame_metadata) {
  return false;
}

bool MockShouldReextractPageContext(
    const std::string& host,
    const std::string& path,
    const std::vector<std::string>& updated_meta_tags) {
  return false;
}

optimization_guide::StringViewSpan MockGetMeta(
    std::string_view,
    std::string_view,
    const std::vector<optimization_guide::FrameMetadata>&) {
  return optimization_guide::StringViewSpan{.data = nullptr, .size = 0};
}

optimization_guide::PageEligibilityResult MockCheckPageEligibility(
    const std::vector<optimization_guide::FrameUrl>& frames) {
  return optimization_guide::PageEligibilityResult{
      .status = optimization_guide::PageEligibility::kIneligible,
      .meta_tag_names_affecting_eligibility = {.data = nullptr, .size = 0}};
}

optimization_guide::PageContextEligibilityAPI g_ineligible_api = {
    .IsPageContextEligible = &MockIsEligible,
    .IsPageContextEligibleWithAccount = &MockIsEligibleWithAccount,
    .ShouldReextractPageContext = &MockShouldReextractPageContext,
    .GetMetaTagNamesAffectingEligibility = &MockGetMeta,
    .CheckPageEligibility = &MockCheckPageEligibility,
};

class ScopedPageContextEligibilityForTesting {
 public:
  explicit ScopedPageContextEligibilityForTesting(
      const optimization_guide::PageContextEligibilityAPI* api)
      : holder_(api) {
    optimization_guide::PageContextEligibility::SetForTesting(&holder_);
  }
  ~ScopedPageContextEligibilityForTesting() {
    optimization_guide::PageContextEligibility::SetForTesting(nullptr);
  }

  ScopedPageContextEligibilityForTesting(
      const ScopedPageContextEligibilityForTesting&) = delete;
  ScopedPageContextEligibilityForTesting& operator=(
      const ScopedPageContextEligibilityForTesting&) = delete;

 private:
  optimization_guide::PageContextEligibility holder_;
};

}  // namespace

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

  StartSession();

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

  StartSession();

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

  StartSession();

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

  StartSession();

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

  StartSession(
      TargetDetails(content::GlobalDOMNodeId{iframe->GetWeakDocumentPtr()}));

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

IN_PROC_BROWSER_TEST_F(DictationContextBrowserTest, IneligiblePageElided) {
  ScopedPageContextEligibilityForTesting scoped_eligibility(&g_ineligible_api);

  const GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  StartSession();

  ASSERT_NE(session_controller(), nullptr);

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      session_controller()->attached_stream_provider());
  ASSERT_NE(provider, nullptr);

  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());
  std::optional<DictationContext> context = ExtensionGetStartStreamDetails(
      profile(), provider->stream_id_for_testing());
  ASSERT_TRUE(context.has_value());

  // Verify that the context was elided because the page is not eligible.
  EXPECT_FALSE(context->annotated_page_content.has_value());
  EXPECT_FALSE(context->inner_text.has_value());
  EXPECT_FALSE(context->editable_content.has_value());
}

// Tests for editors that keep their text and selection in an EditContext
// rather than in the DOM.
class DictationEditContextBrowserTest : public DictationBrowserTestBase {
 public:
  DictationEditContextBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kDictation, {{"use_component_extension", "false"},
                     {"auto_session_end_delay", "0ms"},
                     {"populate_edit_context_hosts", kListedHost}});
  }
  ~DictationEditContextBrowserTest() override = default;

  void SetUpOnMainThread() override {
    DictationBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_server_.Start());
  }

 protected:
  static constexpr char kListedHost[] = "a.test";
  static constexpr char kUnlistedHost[] = "b.test";

  // Loads a page on `host` and adds a focused `#editor` whose EditContext only
  // holds a "_" placeholder until it sees IME input, then fills in "Hello
  // world" and selects "world".
  void NavigateToEditor(std::string_view host) {
    ASSERT_TRUE(content::NavigateToURL(
        web_contents(), https_server_.GetURL(host, "/simple.html")));
    ASSERT_TRUE(content::ExecJs(web_contents(), R"JS(
      const editor = document.createElement('div');
      editor.id = 'editor';
      document.body.appendChild(editor);
      window.ec =
          new EditContext({text: '_', selectionStart: 1, selectionEnd: 1});
      editor.editContext = ec;
      window.imeEvents = [];
      editor.addEventListener(
          'keydown', (e) => imeEvents.push('keydown:' + e.keyCode));
      ec.addEventListener('textupdate', () => {
        imeEvents.push('textupdate');
        if (ec.text === '_') {
          ec.updateText(0, 1, 'Hello world');
          ec.updateSelection(6, 11);
        }
      });
      editor.focus();
    )JS"));
  }

  // Starts dictation in the page's editor and returns the context sent with the
  // stream start.
  std::optional<DictationContext> StartDictationInEditor() {
    content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
    std::optional<int> node_id = content::GetDOMNodeId(*rfh, "#editor");
    if (!node_id) {
      ADD_FAILURE() << "No #editor";
      return std::nullopt;
    }
    StartSession(
        TargetDetails(content::GlobalDOMNodeId(rfh->GetWeakDocumentPtr(),
                                               blink::DOMNodeIdType(*node_id)),
                      /*richly_editable=*/true));

    ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
        session_controller()->attached_stream_provider());
    if (!provider) {
      ADD_FAILURE() << "No stream provider";
      return std::nullopt;
    }
    ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());
    return ExtensionGetStartStreamDetails(profile(),
                                          provider->stream_id_for_testing());
  }

  // Returns the IME events the editor saw, e.g. "keydown:229,textupdate".
  std::string GetImeEvents() {
    return content::EvalJs(web_contents(), "imeEvents.join(',')")
        .ExtractString();
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DictationEditContextBrowserTest,
                       EditContextPopulatedOnListedHost) {
  NavigateToEditor(kListedHost);

  std::optional<DictationContext> context = StartDictationInEditor();
  ASSERT_TRUE(context.has_value());

  EXPECT_EQ(GetImeEvents(), "keydown:229,textupdate");
  ASSERT_TRUE(context->editable_content.has_value());
  EXPECT_EQ(*context->editable_content, "world");
  ASSERT_TRUE(context->inner_text.has_value());
  EXPECT_THAT(*context->inner_text, testing::HasSubstr("Hello world"));
}

IN_PROC_BROWSER_TEST_F(DictationEditContextBrowserTest,
                       EditContextNotPopulatedOnUnlistedHost) {
  NavigateToEditor(kUnlistedHost);

  std::optional<DictationContext> context = StartDictationInEditor();
  ASSERT_TRUE(context.has_value());

  EXPECT_EQ(GetImeEvents(), "");
  EXPECT_EQ(content::EvalJs(web_contents(), "ec.text"), "_");
  EXPECT_EQ(context->inner_text.value_or("").find("Hello world"),
            std::string::npos);
}

IN_PROC_BROWSER_TEST_F(DictationEditContextBrowserTest,
                       EditContextReadWhenAlreadyPopulated) {
  NavigateToEditor(kUnlistedHost);
  // An editor that already exposes its text doesn't need to be populated.
  ASSERT_TRUE(content::ExecJs(web_contents(), R"JS(
    ec.updateText(0, ec.text.length, 'Real text');
    ec.updateSelection(0, 4);
  )JS"));

  std::optional<DictationContext> context = StartDictationInEditor();
  ASSERT_TRUE(context.has_value());

  EXPECT_EQ(GetImeEvents(), "");
  ASSERT_TRUE(context->editable_content.has_value());
  EXPECT_EQ(*context->editable_content, "Real");
  ASSERT_TRUE(context->inner_text.has_value());
  EXPECT_THAT(*context->inner_text, testing::HasSubstr("Real text"));
}

}  // namespace dictation
