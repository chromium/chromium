// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_context.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/dictation_browser_test_base.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/metrics.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_api.h"
#include "content/public/browser/global_dom_node_id.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"

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

 protected:
  // Navigates to `path` on the test server. The page is marked eligible for
  // page context unless `add_eligible_hint` is false.
  void NavigateToUrl(std::string_view path, bool add_eligible_hint = true) {
    const GURL url = embedded_test_server()->GetURL(path);
    if (add_eligible_hint) {
      AddOptimizationGuideEligibleHint(url);
    }
    ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DictationContextBrowserTest, APCCaptured) {
  base::HistogramTester histogram_tester;
  // This test page has a bit of text content.
  NavigateToUrl("/simple.html");

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
  histogram_tester.ExpectUniqueSample(
      kPageContextEligibilityHistogramName,
      DictationPageContextEligibility::kEligible, 1);
}

IN_PROC_BROWSER_TEST_F(DictationContextBrowserTest, SelectedTextCaptured) {
  NavigateToUrl("/textinput/simple_textarea.html");

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
  NavigateToUrl("/simple.html");

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
  NavigateToUrl("/textinput/simple_textarea.html");

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
  NavigateToUrl("/actor/simple_iframe.html");

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
  base::HistogramTester histogram_tester;
  ScopedPageContextEligibilityForTesting scoped_eligibility(&g_ineligible_api);

  const GURL url = embedded_test_server()->GetURL("/simple.html");
  // This test verifies CSE/IRM ineligibility mocked in `scoped_eligibility`
  // above.
  AddOptimizationGuideEligibleHint(url);
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, ui::mojom::WindowOpenDisposition::kNewForegroundTab,
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
  histogram_tester.ExpectUniqueSample(
      kPageContextEligibilityHistogramName,
      DictationPageContextEligibility::kCseIrmIneligible, 1);
}

// Pages whose Optimization Guide hint marks them ineligible must not have any
// page context extracted.
IN_PROC_BROWSER_TEST_F(DictationContextBrowserTest,
                       NoPageContextWithIneligibleHint) {
  base::HistogramTester histogram_tester;
  AddOptimizationGuideIneligibleHint(
      embedded_test_server()->GetURL("/simple.html"));
  NavigateToUrl("/simple.html", /*add_eligible_hint=*/false);

  StartSession();

  ASSERT_NE(session_controller(), nullptr);

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      session_controller()->attached_stream_provider());
  ASSERT_NE(provider, nullptr);

  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());
  std::optional<DictationContext> context = ExtensionGetStartStreamDetails(
      profile(), provider->stream_id_for_testing());
  ASSERT_TRUE(context.has_value());

  EXPECT_FALSE(context->annotated_page_content.has_value());
  EXPECT_FALSE(context->inner_text.has_value());
  EXPECT_FALSE(context->editable_content.has_value());
  histogram_tester.ExpectUniqueSample(
      kPageContextEligibilityHistogramName,
      DictationPageContextEligibility::kHintIneligible, 1);
}

// Pages with no cached Optimization Guide hint (where `CanApplyOptimization`
// returns `kUnknown`) must not have any page context extracted.
IN_PROC_BROWSER_TEST_F(DictationContextBrowserTest,
                       NoPageContextWithoutEligibleHint) {
  base::HistogramTester histogram_tester;
  NavigateToUrl("/simple.html", /*add_eligible_hint=*/false);

  StartSession();

  ASSERT_NE(session_controller(), nullptr);

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      session_controller()->attached_stream_provider());
  ASSERT_NE(provider, nullptr);

  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());
  std::optional<DictationContext> context = ExtensionGetStartStreamDetails(
      profile(), provider->stream_id_for_testing());
  ASSERT_TRUE(context.has_value());

  EXPECT_FALSE(context->annotated_page_content.has_value());
  EXPECT_FALSE(context->inner_text.has_value());
  EXPECT_FALSE(context->editable_content.has_value());
  histogram_tester.ExpectUniqueSample(
      kPageContextEligibilityHistogramName,
      DictationPageContextEligibility::kHintIneligible, 1);
}

IN_PROC_BROWSER_TEST_F(DictationContextBrowserTest,
                       NewTabPageContextAndSelectedTextCaptured) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUINewTabPageURL)));

  const std::string script = R"JS(
    (() => {
      const input = document.createElement('input');
      input.id = 'test_input';
      input.value = 'This is a test';
      document.body.appendChild(input);
      input.focus();
      input.setSelectionRange(5, 9);
      return input.value.substring(input.selectionStart, input.selectionEnd);
    })();
  )JS";
  ASSERT_EQ(content::EvalJs(web_contents(), script), "is a");

  content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
  std::optional<int> node_id = content::GetDOMNodeId(*rfh, "#test_input");
  ASSERT_TRUE(node_id.has_value());
  StartSession(
      TargetDetails(content::GlobalDOMNodeId(rfh->GetWeakDocumentPtr(),
                                             blink::DOMNodeIdType(*node_id)),
                    /*richly_editable=*/false));

  ASSERT_NE(session_controller(), nullptr);

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      session_controller()->attached_stream_provider());
  ASSERT_NE(provider, nullptr);

  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());
  std::optional<DictationContext> context = ExtensionGetStartStreamDetails(
      profile(), provider->stream_id_for_testing());
  ASSERT_TRUE(context.has_value());

  ASSERT_TRUE(context->annotated_page_content.has_value());
  EXPECT_TRUE(context->annotated_page_content->has_root_node());
  ASSERT_TRUE(context->inner_text.has_value());
  ASSERT_TRUE(context->editable_content.has_value());
  EXPECT_EQ(*context->editable_content, "is a");
  histogram_tester.ExpectUniqueSample(
      kPageContextEligibilityHistogramName,
      DictationPageContextEligibility::kEligible, 1);
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
  // world" and selects "world". The page is marked eligible for page context
  // unless `add_eligible_hint` is false.
  void NavigateToEditor(std::string_view host, bool add_eligible_hint = true) {
    const GURL url = https_server_.GetURL(host, "/simple.html");
    if (add_eligible_hint) {
      AddOptimizationGuideEligibleHint(url);
    }
    ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
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

// Pages marked ineligible by Optimization Guide must not receive synthetic IME
// input, even on hosts listed for EditContext population.
IN_PROC_BROWSER_TEST_F(DictationEditContextBrowserTest,
                       EditContextNotPopulatedWithIneligibleHint) {
  AddOptimizationGuideIneligibleHint(
      https_server_.GetURL(kListedHost, "/simple.html"));
  NavigateToEditor(kListedHost, /*add_eligible_hint=*/false);

  std::optional<DictationContext> context = StartDictationInEditor();
  ASSERT_TRUE(context.has_value());

  EXPECT_EQ(GetImeEvents(), "");
  EXPECT_EQ(content::EvalJs(web_contents(), "ec.text"), "_");
}

// End-to-end multi-process Dictation tests for EditContextOnDemand across
// `DictationContextFetcher`, `DictationAgentImpl::PopulateEditContext`,
// `AIPageContent`, and `InnerText`.
class DictationEditContextOnDemandBrowserTest
    : public DictationEditContextBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DictationEditContextBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "EditContextOnDemand");
  }

 protected:
  void SetupOnDemandEditor(content::ToRenderFrameHost target,
                           std::string_view full_text,
                           int sel_start,
                           int sel_end) {
    ASSERT_TRUE(content::ExecJs(target, content::JsReplace(
                                            R"JS(
              const existing = document.getElementById('editor');
              if (existing) existing.remove();
              const editor = document.createElement('div');
              editor.id = 'editor';
              editor.textContent = 'visual-canvas-placeholder';
              document.body.appendChild(editor);
              window.ec = new EditContext({
                text: '_',
                selectionStart: 1,
                selectionEnd: 1,
              });
              editor.editContext = window.ec;
              window.imeEvents = [];
              window.contextRequestCount = 0;
              window.modelText = $1;
              window.modelSelStart = $2;
              window.modelSelEnd = $3;
              editor.addEventListener(
                  'keydown',
                  (e) => window.imeEvents.push('keydown:' + e.keyCode));
              window.ec.addEventListener('textupdate', () => {
                window.imeEvents.push('textupdate');
              });
              window.ec.addEventListener('contextrequest', () => {
                window.contextRequestCount++;
                window.ec.updateText(0, window.ec.text.length, window.modelText);
                window.ec.updateSelection(
                    window.modelSelStart, window.modelSelEnd);
              });
              editor.focus();
              window.ec.invalidate();
            )JS",
                                            full_text, sel_start, sel_end)));
  }
};

// Unlisted hosts (not in `populate_edit_context_hosts`) pull fresh text and
// selection on demand via `MultiSourcePageContextFetcher` (`AIPageContent` +
// `InnerText`), firing `contextrequest` once across both extractors and never
// dispatching synthetic `keydown(229)` / `textupdate`.
IN_PROC_BROWSER_TEST_F(DictationEditContextOnDemandBrowserTest,
                       UnlistedHostPopulatesDictationContextWithoutAllowlist) {
  const GURL url = https_server_.GetURL(kUnlistedHost, "/simple.html");
  AddOptimizationGuideEligibleHint(url);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  SetupOnDemandEditor(web_contents(), "Paragraph 1: Hello world", 19, 24);

  std::optional<DictationContext> context = StartDictationInEditor();
  ASSERT_TRUE(context.has_value());

  EXPECT_EQ(GetImeEvents(), "");
  EXPECT_EQ(content::EvalJs(web_contents(), "window.contextRequestCount"), 1);
  ASSERT_TRUE(context->editable_content.has_value());
  EXPECT_EQ(*context->editable_content, "world");
  ASSERT_TRUE(context->inner_text.has_value());
  EXPECT_THAT(*context->inner_text,
              testing::HasSubstr("Paragraph 1: Hello world"));
}

// Listed hosts (`populate_edit_context_hosts`) clean the invalidated
// EditContext inside `DictationAgentImpl::PopulateEditContext`, skipping the
// legacy synthetic `keydown(229)` + `textupdate` workaround and avoiding
// duplicate `contextrequest` dispatches during subsequent `AIPageContent` /
// `InnerText` extraction.
IN_PROC_BROWSER_TEST_F(
    DictationEditContextOnDemandBrowserTest,
    ListedHostSkipsSyntheticKeydownAndFiresSingleContextRequest) {
  const GURL url = https_server_.GetURL(kListedHost, "/simple.html");
  AddOptimizationGuideEligibleHint(url);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  SetupOnDemandEditor(web_contents(), "Listed host document", 7, 11);

  std::optional<DictationContext> context = StartDictationInEditor();
  ASSERT_TRUE(context.has_value());

  EXPECT_EQ(GetImeEvents(), "");
  EXPECT_EQ(content::EvalJs(web_contents(), "window.contextRequestCount"), 1);
  ASSERT_TRUE(context->editable_content.has_value());
  EXPECT_EQ(*context->editable_content, "host");
  ASSERT_TRUE(context->inner_text.has_value());
  EXPECT_THAT(*context->inner_text, testing::HasSubstr("Listed host document"));
}

// Subframe `#editor` detaching its own `<iframe>` synchronously inside
// `contextrequest` during `DictationContextFetcher::Fetch` recovers cleanly and
// extracts surviving main-frame text without crashing or hanging.
IN_PROC_BROWSER_TEST_F(DictationEditContextOnDemandBrowserTest,
                       SubframeDetachInsideContextRequestDoesNotCrashOrHang) {
  const GURL url = https_server_.GetURL(kListedHost, "/simple.html");
  AddOptimizationGuideEligibleHint(url);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace(
          "const p = document.createElement('p'); "
          "p.textContent = 'Surviving main text'; "
          "document.body.appendChild(p); "
          "const f = document.createElement('iframe'); f.id = 'sub'; "
          "f.src = $1; document.body.appendChild(f);",
          url)));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));

  content::RenderFrameHost* sub_rfh =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(sub_rfh);
  ASSERT_TRUE(content::ExecJs(sub_rfh, R"JS(
    const editor = document.createElement('div');
    editor.id = 'editor';
    document.body.appendChild(editor);
    const ec = new EditContext({text: '_'});
    editor.editContext = ec;
    ec.addEventListener('contextrequest', () => {
      window.frameElement.remove();
    });
    editor.focus();
    ec.invalidate();
  )JS"));

  StartSession();
  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      session_controller()->attached_stream_provider());
  ASSERT_NE(provider, nullptr);
  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());
  std::optional<DictationContext> context = ExtensionGetStartStreamDetails(
      profile(), provider->stream_id_for_testing());
  ASSERT_TRUE(context.has_value());
  ASSERT_TRUE(context->inner_text.has_value());
  EXPECT_THAT(*context->inner_text, testing::HasSubstr("Surviving main text"));
  EXPECT_EQ(content::EvalJs(web_contents(),
                            "document.getElementById('sub') === null"),
            true);
}

// Listed-host nested OOPIF local root (`a.test -> b.test -> a.test`) whose
// `contextrequest` listener removes `#editor` synchronously inside
// `DictationAgentImpl::PopulateEditContext` does not dispatch synthetic
// `keydown` or crash.
IN_PROC_BROWSER_TEST_F(
    DictationEditContextOnDemandBrowserTest,
    PopulateEditContextHandlesTargetRemovalInsideContextRequest) {
  const GURL a_url = https_server_.GetURL(kListedHost, "/simple.html");
  const GURL b_url = https_server_.GetURL(kUnlistedHost, "/simple.html");
  AddOptimizationGuideEligibleHint(a_url);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), a_url));
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace(
          "const p = document.createElement('p'); "
          "p.textContent = 'Main survivor'; "
          "document.body.appendChild(p); "
          "const mid = document.createElement('iframe'); mid.src = $1; "
          "document.body.appendChild(mid);",
          b_url)));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  content::RenderFrameHost* mid_rfh =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(mid_rfh);
  ASSERT_TRUE(content::ExecJs(
      mid_rfh,
      content::JsReplace(
          "const inner = document.createElement('iframe'); inner.src = $1; "
          "document.body.appendChild(inner);",
          a_url)));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  content::RenderFrameHost* inner_rfh = content::ChildFrameAt(mid_rfh, 0);
  ASSERT_TRUE(inner_rfh);

  ASSERT_TRUE(content::ExecJs(inner_rfh, R"JS(
    const editor = document.createElement('div');
    editor.id = 'editor';
    document.body.appendChild(editor);
    window.ec = new EditContext({text: '_'});
    editor.editContext = window.ec;
    window.imeEvents = [];
    editor.addEventListener('keydown', () => window.imeEvents.push('keydown'));
    window.ec.addEventListener('contextrequest', () => {
      editor.remove();
    });
    editor.focus();
    window.ec.invalidate();
  )JS"));

  std::optional<int> inner_node_id =
      content::GetDOMNodeId(*inner_rfh, "#editor");
  ASSERT_TRUE(inner_node_id.has_value());
  StartSession(TargetDetails(
      content::GlobalDOMNodeId(inner_rfh->GetWeakDocumentPtr(),
                               blink::DOMNodeIdType(*inner_node_id)),
      /*richly_editable=*/true));
  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      session_controller()->attached_stream_provider());
  ASSERT_NE(provider, nullptr);
  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());
  std::optional<DictationContext> context = ExtensionGetStartStreamDetails(
      profile(), provider->stream_id_for_testing());
  ASSERT_TRUE(context.has_value());
  EXPECT_EQ(content::EvalJs(inner_rfh, "window.imeEvents.join(',')"), "");
  ASSERT_TRUE(context->inner_text.has_value());
  EXPECT_THAT(*context->inner_text, testing::HasSubstr("Main survivor"));
}

}  // namespace dictation
