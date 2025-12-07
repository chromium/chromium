// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/dom_node_geometry.h"

#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"

namespace actor::ui {
namespace {
using blink::mojom::AIPageContentMode;
using blink::mojom::AIPageContentOptionsPtr;
using optimization_guide::AIPageContentResult;
using optimization_guide::AIPageContentResultOrError;
using optimization_guide::OnAIPageContentDone;
using optimization_guide::proto::AnnotatedPageContent;
using optimization_guide::proto::ContentNode;
using optimization_guide::proto::FrameData;
using views::ViewSkiaGoldPixelDiff;

using AriaToDomNodeMap = absl::flat_hash_map<std::string, DomNode>;

void BuildAriaLabelMap(const ContentNode& root,
                       std::string doc_id,
                       AriaToDomNodeMap* map) {
  if (root.has_content_attributes()) {
    std::string label = root.content_attributes().label();
    if (!label.empty()) {
      int node_id = root.content_attributes().common_ancestor_dom_node_id();
      auto node = DomNode{.node_id = node_id, .document_identifier = doc_id};
      map->emplace(label, node);
    }
  }
  if (root.content_attributes().has_iframe_data() &&
      root.content_attributes().iframe_data().has_frame_data()) {
    doc_id = root.content_attributes()
                 .iframe_data()
                 .frame_data()
                 .document_identifier()
                 .serialized_token();
  }
  for (auto& child : root.children_nodes()) {
    BuildAriaLabelMap(child, doc_id, map);
  }
}

std::string_view Trim(const std::string_view str) {
  const std::string whitespace = " \t\n\r\f\v";
  const auto strBegin = str.find_first_not_of(whitespace);
  if (std::string::npos == strBegin) {
    return "";
  }
  const auto strEnd = str.find_last_not_of(whitespace);
  const auto strRange = strEnd - strBegin + 1;
  return str.substr(strBegin, strRange);
}

MATCHER_P(IsEqualToTrimmed, expected, "") {
  return Trim(arg) == Trim(expected);
}

class ActorUiDomNodeGeometryBrowserTest : public InProcessBrowserTest {
 public:
  ActorUiDomNodeGeometryBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActorUi,
        {{features::kGlicActorUiOverlayMagicCursorName, "true"}});
  }

  ~ActorUiDomNodeGeometryBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    if (command_line->HasSwitch(switches::kVerifyPixels)) {
      // Increment "v0" for Skia Gold diff when different images are expected.
      pixel_diff_ =
          std::make_unique<ViewSkiaGoldPixelDiff>("ActorUiDomNodeGeometry_v0");
    }
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override { tab_data_ = nullptr; }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  AIPageContentOptionsPtr AIPageContentOptions() {
    auto options = optimization_guide::DefaultAIPageContentOptions(
        /*on_critical_path =*/true);
    options->mode = AIPageContentMode::kActionableElements;
    return options;
  }

  void LoadPage(GURL url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
    ASSERT_TRUE(content::EvalJs(web_contents(),
                                R"js(new Promise(resolve => {
                                  if (window.isIframeLoaded) {
                                    resolve(true);
                                  } else {
                                    const interval = setInterval(() => {
                                      if (window.isIframeLoaded) {
                                        clearInterval(interval);
                                        resolve(true);
                                      }
                                    }, 100);
                                  }
                                });)js")
                    .ExtractBool());
  }

  void SetPageContent(base::OnceClosure quit_closure,
                      AIPageContentResultOrError page_content) {
    auto apc = std::move(page_content->proto);
    aria_label_to_dom_node_.clear();
    BuildAriaLabelMap(
        apc.root_node(),
        apc.main_frame_data().document_identifier().serialized_token(),
        &aria_label_to_dom_node_);

    tab_data_ =
        ActorTabData::From(browser()->tab_strip_model()->GetActiveTab());
    tab_data_->DidObserveContent(apc);
    std::move(quit_closure).Run();
  }

  void GetPageApc() {
    base::RunLoop run_loop;
    auto options = AIPageContentOptions();
    GetAIPageContent(
        web_contents(), std::move(options),
        base::BindOnce(&ActorUiDomNodeGeometryBrowserTest::SetPageContent,
                       base::Unretained(this), run_loop.QuitClosure()));

    run_loop.Run();
  }

  std::string ElementTextAtPoint(const gfx::Point& pt) {
    content::EvalJsResult result = content::EvalJs(
        web_contents(),
        absl::StrFormat("document.elementFromPoint(%d, %d).textContent", pt.x(),
                        pt.y()));
    EXPECT_THAT(result, content::EvalJsResult::IsOk());
    EXPECT_TRUE(result.is_string());
    return result.ExtractString();
  }

  DomNode GetDomNodeForAriaLabel(std::string_view label) {
    return aria_label_to_dom_node_.at(label);
  }

  void ColorPoint(gfx::Point point, std::string_view color) {
    EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                       absl::StrFormat("placeDot(%d, %d, '%s');", point.x(),
                                       point.y(), color)));
  }

  gfx::Point GetAriaLabelPoint(std::string_view label) {
    auto point = tab_data_->GetLastObservedDomNodeGeometry()->GetDomNode(
        GetDomNodeForAriaLabel(label));
    EXPECT_TRUE(point.has_value());
    EXPECT_GT(point->x(), 0);
    EXPECT_GT(point->y(), 0);
    return point.value();
  }

  void SkiaWebContentsDiff() {
    if (!pixel_diff_) {
      LOG(WARNING) << "Skipping Skia diffs";
      return;
    }
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    pixel_diff_->CompareViewScreenshot(
        absl::StrFormat("%s-%s", test_info->test_suite_name(),
                        test_info->name()),
        browser_view->GetActiveContentsContainerView());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ViewSkiaGoldPixelDiff> pixel_diff_;
  raw_ptr<ActorTabData> tab_data_;
  AriaToDomNodeMap aria_label_to_dom_node_;
};

IN_PROC_BROWSER_TEST_F(ActorUiDomNodeGeometryBrowserTest,
                       GetDomNodePointFromApc_TopFrame) {
  LoadPage(embedded_test_server()->GetURL("/actor/dom_node_geometry.html"));
  GetPageApc();

  gfx::Point point_a = GetAriaLabelPoint("a_div");
  LOG(INFO) << "Point A: " << point_a;

  gfx::Point point_b = GetAriaLabelPoint("b");
  LOG(INFO) << "Point B: " << point_b;
  EXPECT_THAT(ElementTextAtPoint(point_b), IsEqualToTrimmed("B"));

  gfx::Point point_c = GetAriaLabelPoint("c_div");
  LOG(INFO) << "Point C: " << point_c;
  EXPECT_THAT(ElementTextAtPoint(point_c), IsEqualToTrimmed("C"));

  gfx::Point point_d = GetAriaLabelPoint("d_div");
  LOG(INFO) << "Point D: " << point_d;
  EXPECT_THAT(ElementTextAtPoint(point_d), IsEqualToTrimmed("D"));

  gfx::Point point_e = GetAriaLabelPoint("e_div");
  LOG(INFO) << "Point E: " << point_e;
  EXPECT_THAT(ElementTextAtPoint(point_e), IsEqualToTrimmed("E"));

  ColorPoint(point_a, "purple");
  ColorPoint(point_b, "blue");
  ColorPoint(point_c, "orange");
  ColorPoint(point_d, "green");
  ColorPoint(point_e, "red");

  SkiaWebContentsDiff();
}

IN_PROC_BROWSER_TEST_F(ActorUiDomNodeGeometryBrowserTest,
                       GetDomNodePointFromApc_SubFrame) {
  LoadPage(embedded_test_server()->GetURL("/actor/dom_node_geometry.html"));
  GetPageApc();

  gfx::Point point_div1 = GetAriaLabelPoint("div1_label");
  LOG(INFO) << "Point Div1: " << point_div1;

  gfx::Point point_div2 = GetAriaLabelPoint("div2_label");
  LOG(INFO) << "Point Div2: " << point_div2;

  ColorPoint(point_div1, "red");
  ColorPoint(point_div2, "green");

  SkiaWebContentsDiff();
}

}  // namespace
}  // namespace actor::ui
