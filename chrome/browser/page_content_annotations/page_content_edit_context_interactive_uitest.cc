// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "url/gurl.h"

namespace page_content_annotations {

namespace {

const optimization_guide::proto::FrameData* FindIframeFrameDataByUrl(
    const optimization_guide::proto::ContentNode& node,
    std::string_view url) {
  if (node.content_attributes().has_iframe_data() &&
      node.content_attributes().iframe_data().has_frame_data() &&
      node.content_attributes().iframe_data().frame_data().url() == url) {
    return &node.content_attributes().iframe_data().frame_data();
  }
  for (const auto& child : node.children_nodes()) {
    if (const auto* match = FindIframeFrameDataByUrl(child, url)) {
      return match;
    }
  }
  return nullptr;
}

const optimization_guide::proto::IframeData* FindFirstIframeData(
    const optimization_guide::proto::ContentNode& node) {
  if (node.content_attributes().has_iframe_data()) {
    return &node.content_attributes().iframe_data();
  }
  for (const auto& child : node.children_nodes()) {
    if (const auto* match = FindFirstIframeData(child)) {
      return match;
    }
  }
  return nullptr;
}

// Multi-process OOPIF `GetAIPageContent()` tests for `EditContextOnDemand`.
// Lives in `interactive_ui_tests` because `AIPageContentAgent` only pulls
// on-demand context for the focused `EditContext`, which requires real window
// focus across frames.
class PageContentEditContextOnDemandInteractiveUiTest
    : public InProcessBrowserTest {
 public:
  PageContentEditContextOnDemandInteractiveUiTest() {
    // Disable automatic on-load background APC extraction (enabled by
    // `fieldtrial_testing_config.json` with a 5s capture delay) so background
    // extractions do not race with explicit `GetAIPageContent()` calls and
    // `contextRequestCount` assertions on slower bots.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kEditContextOnDemand},
        /*disabled_features=*/{features::kAnnotatedPageContentExtraction});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void SyncVisualState(content::RenderFrameHost* rfh) {
    base::test::TestFuture<bool> future;
    rfh->GetRenderWidgetHost()->InsertVisualStateCallback(future.GetCallback());
    ASSERT_TRUE(future.Wait()) << "Timeout waiting for frame visual state sync";
    ASSERT_TRUE(future.Get());
  }

  content::RenderFrameHost* AppendIframe(content::RenderFrameHost* parent,
                                         const GURL& url) {
    EXPECT_TRUE(content::ExecJs(
        parent,
        content::JsReplace("new Promise((resolve) => {"
                           "  const f = document.createElement('iframe');"
                           "  f.onload = () => resolve();"
                           "  f.src = $1;"
                           "  document.body.appendChild(f);"
                           "})",
                           url)));
    EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
    content::RenderFrameHost* child = content::ChildFrameAt(parent, 0);
    EXPECT_NE(child, nullptr);
    if (child) {
      SyncVisualState(parent);
      SyncVisualState(child);
    }
    return child;
  }

  optimization_guide::AIPageContentResultOrError GetAIPageContent(
      bool include_same_site_only) {
    base::test::TestFuture<optimization_guide::AIPageContentResultOrError>
        future;
    blink::mojom::AIPageContentOptionsPtr options =
        optimization_guide::DefaultAIPageContentOptions(
            /*on_critical_path=*/true);
    options->include_same_site_only = include_same_site_only;
    optimization_guide::GetAIPageContent(web_contents(), std::move(options),
                                         future.GetCallback());
    return future.Take();
  }

  void SetupOnDemandEditor(content::ToRenderFrameHost target,
                           std::string_view full_text,
                           int sel_start,
                           int sel_end,
                           bool focus_editor = true,
                           bool re_invalidate_in_context_request = false) {
    ASSERT_TRUE(
        content::ExecJs(target, content::JsReplace(
                                    R"JS(
              window.modelText = $1;
              window.modelSelStart = $2;
              window.modelSelEnd = $3;
              window.ec.addEventListener('contextrequest', () => {
                window.contextRequestCount++;
                window.ec.updateText(
                    0, window.ec.text.length, window.modelText);
                window.ec.updateSelection(
                    window.modelSelStart, window.modelSelEnd);
                if ($5) {
                  window.ec.invalidate();
                }
              });
              if ($4) {
                document.getElementById('editor').focus();
              }
              window.ec.invalidate();
            )JS",
                                    full_text, sel_start, sel_end, focus_editor,
                                    re_invalidate_in_context_request)));
    if (focus_editor) {
      ASSERT_EQ(true, content::EvalJs(target, R"JS(
        new Promise((resolve) => {
          const check = () => {
            if (document.hasFocus() &&
                document.activeElement === document.getElementById('editor')) {
              resolve(true);
            } else {
              requestAnimationFrame(check);
            }
          };
          check();
        })
      )JS"));
    }
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Nested same-site-inside-cross-site OOPIF hierarchy (`a.test -> b.test (OOPIF)
// -> a.test`): verifies `GetAIPageContent()` + `AIPageContentAgent` does not
// cross `RemoteFrame` boundaries from the outer `a.test` frame to double-fire
// `contextrequest` in the nested `a.test` local root (even when `inner_rfh`
// re-invalidates inside `contextrequest`), while unfocused frames receive 0
// `contextrequest` events and the refreshed selection is carried in
// `AnnotatedPageContent`.
IN_PROC_BROWSER_TEST_F(PageContentEditContextOnDemandInteractiveUiTest,
                       NestedSameSiteOopifFiresSingleContextRequest) {
  const GURL top_url = https_server_.GetURL("a.test", "/simple.html");
  const GURL mid_url =
      https_server_.GetURL("b.test", "/workspace_like_editcontext.html");
  const GURL inner_url =
      https_server_.GetURL("a.test", "/workspace_like_editcontext.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), top_url));
  web_contents()->Focus();

  content::RenderFrameHost* top_rfh = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* mid_rfh = AppendIframe(top_rfh, mid_url);
  ASSERT_TRUE(mid_rfh);
  ASSERT_NE(top_rfh->GetProcess(), mid_rfh->GetProcess());

  content::RenderFrameHost* inner_rfh = AppendIframe(mid_rfh, inner_url);
  ASSERT_TRUE(inner_rfh);
  ASSERT_NE(mid_rfh->GetProcess(), inner_rfh->GetProcess());
  ASSERT_EQ(top_rfh->GetProcess(), inner_rfh->GetProcess());

  SetupOnDemandEditor(mid_rfh, "CrossSite secret", 0, 9,
                      /*focus_editor=*/false);
  // Re-invalidate inside `contextrequest` so if `top_rfh` accidentally crossed
  // the `RemoteFrame` boundary into `inner_rfh`, `inner_rfh`'s own extraction
  // pass would observe a dirty context and bump `contextRequestCount` to 2.
  SetupOnDemandEditor(inner_rfh, "Inner same-site text", 0, 5,
                      /*focus_editor=*/true,
                      /*re_invalidate_in_context_request=*/true);

  optimization_guide::AIPageContentResultOrError result =
      GetAIPageContent(/*include_same_site_only=*/false);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(content::EvalJs(inner_rfh, "window.contextRequestCount"), 1);
  EXPECT_EQ(content::EvalJs(inner_rfh, "window.ec.text"),
            "Inner same-site text");
  EXPECT_EQ(content::EvalJs(mid_rfh, "window.contextRequestCount"), 0);
  EXPECT_EQ(content::EvalJs(mid_rfh, "window.ec.text"), "_");

  const optimization_guide::proto::FrameData* inner_frame_data =
      FindIframeFrameDataByUrl(result->proto.root_node(), inner_url.spec());
  ASSERT_NE(inner_frame_data, nullptr);
  ASSERT_TRUE(inner_frame_data->frame_interaction_info().has_selection());
  const optimization_guide::proto::Selection& inner_selection =
      inner_frame_data->frame_interaction_info().selection();
  EXPECT_EQ(inner_selection.selected_text(), "Inner");
  EXPECT_EQ(inner_selection.start_offset(), 0);
  EXPECT_EQ(inner_selection.end_offset(), 5);

  const optimization_guide::proto::FrameData* mid_frame_data =
      FindIframeFrameDataByUrl(result->proto.root_node(), mid_url.spec());
  ASSERT_NE(mid_frame_data, nullptr);
  EXPECT_FALSE(mid_frame_data->frame_interaction_info().has_selection());
}

// Focused cross-site OOPIF (`a.test -> b.test (OOPIF)`) with an invalidated
// `EditContext` must not receive `contextrequest` when `include_same_site_only`
// is true, and must receive it and report the refreshed selection in
// `AnnotatedPageContent` when `include_same_site_only` is false.
IN_PROC_BROWSER_TEST_F(PageContentEditContextOnDemandInteractiveUiTest,
                       CrossSiteOopifRespectsSameSiteFilter) {
  const GURL top_url = https_server_.GetURL("a.test", "/simple.html");
  const GURL subframe_url =
      https_server_.GetURL("b.test", "/workspace_like_editcontext.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), top_url));
  web_contents()->Focus();

  content::RenderFrameHost* top_rfh = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* subframe_rfh = AppendIframe(top_rfh, subframe_url);
  ASSERT_TRUE(subframe_rfh);
  ASSERT_NE(top_rfh->GetProcess(), subframe_rfh->GetProcess());

  SetupOnDemandEditor(subframe_rfh, "CrossSite secret", 0, 9,
                      /*focus_editor=*/true);

  optimization_guide::AIPageContentResultOrError filtered_result =
      GetAIPageContent(/*include_same_site_only=*/true);
  ASSERT_TRUE(filtered_result.has_value());
  EXPECT_EQ(content::EvalJs(subframe_rfh, "window.contextRequestCount"), 0);
  EXPECT_EQ(content::EvalJs(subframe_rfh, "window.ec.text"), "_");
  const optimization_guide::proto::IframeData* redacted_iframe =
      FindFirstIframeData(filtered_result->proto.root_node());
  ASSERT_NE(redacted_iframe, nullptr);
  EXPECT_FALSE(redacted_iframe->has_frame_data());
  EXPECT_EQ(redacted_iframe->redacted_frame_metadata().reason(),
            optimization_guide::proto::IframeData::RedactedFrameMetadata::
                REASON_CROSS_SITE);

  optimization_guide::AIPageContentResultOrError full_result =
      GetAIPageContent(/*include_same_site_only=*/false);
  ASSERT_TRUE(full_result.has_value());
  EXPECT_EQ(content::EvalJs(subframe_rfh, "window.contextRequestCount"), 1);
  EXPECT_EQ(content::EvalJs(subframe_rfh, "window.ec.text"),
            "CrossSite secret");
  const optimization_guide::proto::FrameData* subframe_data =
      FindIframeFrameDataByUrl(full_result->proto.root_node(),
                               subframe_url.spec());
  ASSERT_NE(subframe_data, nullptr);
  ASSERT_TRUE(subframe_data->frame_interaction_info().has_selection());
  const optimization_guide::proto::Selection& selection =
      subframe_data->frame_interaction_info().selection();
  EXPECT_EQ(selection.selected_text(), "CrossSite");
  EXPECT_EQ(selection.start_offset(), 0);
  EXPECT_EQ(selection.end_offset(), 9);
}

}  // namespace

}  // namespace page_content_annotations
