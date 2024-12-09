// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_data_keyed_service.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ai/ai_data_keyed_service_factory.h"
#include "chrome/browser/autofill_ai/chrome_autofill_ai_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill_ai/core/browser/autofill_ai_features.h"
#include "components/autofill_ai/core/browser/suggestion/autofill_ai_model_executor.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::ReturnRef;

class MockAutofillAiModelExecutor
    : public autofill_ai::AutofillAiModelExecutor {
 public:
  MOCK_METHOD(
      void,
      GetPredictions,
      (autofill::FormData form_data,
       (base::flat_map<autofill::FieldGlobalId, bool> field_eligibility_map),
       (base::flat_map<autofill::FieldGlobalId, bool> sensitivity_map),
       optimization_guide::proto::AXTreeUpdate ax_tree_update,
       PredictionsReceivedCallback callback),
      (override));
  MOCK_METHOD(
      const std::optional<optimization_guide::proto::FormsPredictionsRequest>&,
      GetLatestRequest,
      (),
      (const override));
  MOCK_METHOD(
      const std::optional<optimization_guide::proto::FormsPredictionsResponse>&,
      GetLatestResponse,
      (),
      (const override));
};

class AiDataKeyedServiceBrowserTest : public InProcessBrowserTest {
 public:
  AiDataKeyedServiceBrowserTest() = default;

  AiDataKeyedServiceBrowserTest(const AiDataKeyedServiceBrowserTest&) = delete;
  AiDataKeyedServiceBrowserTest& operator=(
      const AiDataKeyedServiceBrowserTest&) = delete;

  ~AiDataKeyedServiceBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->AddDefaultHandlers(GetChromeTestDataDir());
    content::SetupCrossSiteRedirector(https_server_.get());

    ASSERT_TRUE(https_server_->Start());

    url_ = https_server_->GetURL("/simple.html");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetAiData(base::OnceClosure quit_closure,
                 AiDataKeyedService::AiData ai_data) {
    ai_data_ = std::move(ai_data);
    std::move(quit_closure).Run();
  }

  GURL url() { return url_; }

  const AiDataKeyedService::AiData& ai_data() { return ai_data_; }

  void LoadData(content::WebContents* web_contents) {
    AiDataKeyedService* ai_data_service =
        AiDataKeyedServiceFactory::GetAiDataKeyedService(browser()->profile());

    base::RunLoop run_loop;
    auto dom_node_id = 0;
    ai_data_service->GetAiDataWithSpecifiers(
        1, dom_node_id, web_contents, "test",
        base::BindOnce(&AiDataKeyedServiceBrowserTest::SetAiData,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    DCHECK(ai_data());
  }

  void LoadPage(GURL url, bool with_ai_data = true) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::NavigateToURLBlockUntilNavigationsComplete(web_contents, url, 1);

    if (with_ai_data) {
      LoadData(web_contents);
    }
  }

  void LoadSimplePageAndData() { LoadPage(url()); }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 private:
  GURL url_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  AiDataKeyedService::AiData ai_data_;
};

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest,
                       AllowlistedExtensionList) {
  std::vector<std::string> expected_allowlisted_extensions = {
      "hpkopmikdojpadgmioifjjodbmnjjjca", "bgbpcgpcobgjpnpiginpidndjpggappi"};

  EXPECT_EQ(AiDataKeyedService::GetAllowlistedExtensions(),
            expected_allowlisted_extensions);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, GetsData) {
  LoadSimplePageAndData();
  EXPECT_TRUE(ai_data());
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, InnerText) {
  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  EXPECT_EQ(ai_data()->page_context().inner_text(), "Non empty simple page");
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, InnerTextOffset) {
  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  EXPECT_EQ(ai_data()->page_context().inner_text_offset(), 0u);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, Title) {
  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  EXPECT_EQ(ai_data()->page_context().title(), "OK");
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, Url) {
  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  EXPECT_NE(ai_data()->page_context().url().find("simple"), std::string::npos);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, AxTreeUpdate) {
  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  // If there are nodes and the titles is correct, then the AX tree is filled
  // out.
  EXPECT_GT(ai_data()->page_context().ax_tree_data().nodes().size(), 0);
  EXPECT_EQ(ai_data()->page_context().ax_tree_data().tree_data().title(), "OK");
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, TabData) {
  chrome::AddTabAt(browser(), GURL("foo.com"), -1, false);
  chrome::AddTabAt(browser(), GURL("bar.com"), -1, false);

  auto* tab_group1 = browser()->GetTabStripModel()->group_model()->GetTabGroup(
      browser()->GetTabStripModel()->AddToNewGroup({0}));
  auto vis_data1 = *tab_group1->visual_data();
  vis_data1.SetTitle(u"ok");
  tab_group1->SetVisualData(vis_data1);

  auto* tab_group2 = browser()->GetTabStripModel()->group_model()->GetTabGroup(
      browser()->GetTabStripModel()->AddToNewGroup({1, 2}));
  auto vis_data2 = *tab_group1->visual_data();
  vis_data2.SetTitle(u"ok");
  tab_group2->SetVisualData(vis_data2);

  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());

  EXPECT_EQ(ai_data()->active_tab_id(), 0);
  EXPECT_EQ(ai_data()->tabs().size(), 3);
  EXPECT_EQ(ai_data()->pre_existing_tab_groups().size(), 2);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, TabInnerText) {
  chrome::AddTabAt(browser(), GURL("foo.com"), -1, false);
  chrome::AddTabAt(browser(), GURL("bar.com"), -1, false);

  auto* tab_group1 = browser()->GetTabStripModel()->group_model()->GetTabGroup(
      browser()->GetTabStripModel()->AddToNewGroup({0}));
  auto vis_data1 = *tab_group1->visual_data();
  vis_data1.SetTitle(u"ok");
  tab_group1->SetVisualData(vis_data1);

  auto* tab_group2 = browser()->GetTabStripModel()->group_model()->GetTabGroup(
      browser()->GetTabStripModel()->AddToNewGroup({1, 2}));
  auto vis_data2 = *tab_group1->visual_data();
  vis_data2.SetTitle(u"ok");
  tab_group2->SetVisualData(vis_data2);

  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  EXPECT_EQ(ai_data()->active_tab_id(), 0);
  for (const auto& tab_in_proto : ai_data()->tabs()) {
    if (tab_in_proto.tab_id() == 0) {
      EXPECT_EQ(tab_in_proto.title(), "OK");
      EXPECT_NE(tab_in_proto.url().find("simple"), std::string::npos);
      EXPECT_EQ(tab_in_proto.page_context().inner_text(),
                "Non empty simple page");
    }
  }
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, TabInnerTextLimit) {
  LoadSimplePageAndData();
  chrome::AddTabAt(browser(), GURL("bar.com"), -1, true);
  LoadSimplePageAndData();
  EXPECT_EQ(ai_data()->active_tab_id(), 1);
  for (auto& tab : ai_data()->tabs()) {
    if (tab.tab_id() == 0) {
      EXPECT_EQ(tab.page_context().inner_text(), "Non empty simple page");
    }
    if (tab.tab_id() == 1) {
      EXPECT_EQ(tab.page_context().inner_text(), "");
    }
  }
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, Screenshot) {
  LoadSimplePageAndData();
  content::RequestFrame(browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_NE(ai_data()->page_context().tab_screenshot(), "");
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, SiteEngagementScores) {
  LoadSimplePageAndData();
  EXPECT_EQ(ai_data()->site_engagement().entries().size(), 1);
  EXPECT_NE(ai_data()->site_engagement().entries()[0].url(), "");
  EXPECT_GE(ai_data()->site_engagement().entries()[0].score(), 0);
}

namespace {
void AssertHasText(const optimization_guide::proto::ContentNode& node,
                   std::string text) {
  const auto& content_attributes = node.content_attributes();
  EXPECT_EQ(content_attributes.attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);
  EXPECT_EQ(content_attributes.text_info().size(), 1);
  EXPECT_EQ(content_attributes.text_info().at(0).text_content(), text);
}

void AssertRectsEqual(const optimization_guide::proto::BoundingRect& proto_rect,
                      gfx::Rect rect) {
  EXPECT_EQ(proto_rect.width(), rect.width());
  EXPECT_EQ(proto_rect.height(), rect.height());
  EXPECT_EQ(proto_rect.x(), rect.x());
  EXPECT_EQ(proto_rect.y(), rect.y());
}

void AssertRectsEqual(const optimization_guide::proto::BoundingRect& a,
                      const optimization_guide::proto::BoundingRect& b) {
  EXPECT_EQ(a.width(), b.width());
  EXPECT_EQ(a.height(), b.height());
  EXPECT_EQ(a.x(), b.x());
  EXPECT_EQ(a.y(), b.y());
}

void AssertValidURL(std::string url, std::string host) {
  GURL gurl(url);
  EXPECT_TRUE(gurl.is_valid());
  EXPECT_TRUE(gurl.SchemeIsHTTPOrHTTPS());
  EXPECT_EQ(gurl.host(), host);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, AIPageContent) {
  constexpr gfx::Size kWindowBounds{800, 1000};
  browser()->tab_strip_model()->GetActiveWebContents()->Resize(
      gfx::Rect(kWindowBounds));
  LoadSimplePageAndData();

  const auto& page_content = ai_data()->page_context().annotated_page_content();
  EXPECT_TRUE(page_content.root_node().children_nodes().empty());

  AssertHasText(page_content.root_node(), "Non empty simple page\n\n");

  const auto& root_geometry =
      page_content.root_node().content_attributes().geometry();
  EXPECT_EQ(root_geometry.outer_bounding_box().x(), 0);
  EXPECT_EQ(root_geometry.outer_bounding_box().y(), 0);
  EXPECT_EQ(root_geometry.outer_bounding_box().width(), kWindowBounds.width());
  EXPECT_EQ(root_geometry.outer_bounding_box().height(),
            kWindowBounds.height());

  EXPECT_EQ(root_geometry.visible_bounding_box().x(), 0);
  EXPECT_EQ(root_geometry.visible_bounding_box().y(), 0);
  EXPECT_EQ(root_geometry.visible_bounding_box().width(),
            kWindowBounds.width());
  EXPECT_EQ(root_geometry.visible_bounding_box().height(),
            kWindowBounds.height());
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest,
                       AIPageContentImageDataURL) {
  constexpr gfx::Size kWindowBounds{800, 1000};
  browser()->tab_strip_model()->GetActiveWebContents()->Resize(
      gfx::Rect(kWindowBounds));
  LoadPage(https_server()->GetURL("a.com", "/data_image.html"));

  const auto& page_content = ai_data()->page_context().annotated_page_content();
  EXPECT_TRUE(page_content.root_node().children_nodes().empty());

  ASSERT_EQ(page_content.root_node().content_attributes().image_info().size(),
            1);
  const auto& image_info =
      page_content.root_node().content_attributes().image_info()[0];
  // TODO(khushalsagar): This should be a.com.
  EXPECT_FALSE(GURL(image_info.source_url()).is_valid());
}

namespace {

std::string GetFilePathWithHostAndPortReplacement(
    const std::string& original_file_path,
    const net::HostPortPair& host_port_pair) {
  base::StringPairs replacement_text;
  replacement_text.push_back(
      make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
  return net::test_server::GetFilePathWithReplacements(original_file_path,
                                                       replacement_text);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest,
                       AIPageContentCrossOriginImage) {
  constexpr gfx::Size kWindowBounds{800, 1000};
  browser()->tab_strip_model()->GetActiveWebContents()->Resize(
      gfx::Rect(kWindowBounds));

  // Add a "replace_text=" query param that the test server will use to replace
  // the string "REPLACE_WITH_HOST_AND_PORT" in the destination page.
  net::HostPortPair host_port_pair =
      net::HostPortPair::FromURL(https_server()->GetURL("b.com", "/"));
  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/cross_origin_image.html", host_port_pair);

  LoadPage(https_server()->GetURL("a.com", replacement_path));

  const auto& page_content = ai_data()->page_context().annotated_page_content();
  EXPECT_TRUE(page_content.root_node().children_nodes().empty());

  ASSERT_EQ(page_content.root_node().content_attributes().image_info().size(),
            1);
  const auto& image_info =
      page_content.root_node().content_attributes().image_info()[0];
  // TODO(khushalsagar): This should be b.com.
  EXPECT_FALSE(GURL(image_info.source_url()).is_valid());
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest,
                       AIPageContentSandboxedIframe) {
  constexpr gfx::Size kWindowBounds{800, 1000};
  browser()->tab_strip_model()->GetActiveWebContents()->Resize(
      gfx::Rect(kWindowBounds));
  LoadPage(https_server()->GetURL("a.com", "/paragraph_iframe_sandbox.html"));

  const auto& page_content = ai_data()->page_context().annotated_page_content();
  EXPECT_EQ(page_content.root_node().children_nodes().size(), 1);

  const auto& iframe = page_content.root_node().children_nodes()[0];
  EXPECT_EQ(iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& iframe_data = iframe.content_attributes().iframe_data();
  AssertValidURL(iframe_data.url(), "a.com");
  EXPECT_FALSE(iframe_data.likely_ad_frame());

  EXPECT_EQ(iframe.children_nodes().size(), 1);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest,
                       AIPageContentIframeDataURL) {
  constexpr gfx::Size kWindowBounds{800, 1000};
  browser()->tab_strip_model()->GetActiveWebContents()->Resize(
      gfx::Rect(kWindowBounds));
  LoadPage(https_server()->GetURL("a.com", "/paragraph_iframe_data_url.html"));

  const auto& page_content = ai_data()->page_context().annotated_page_content();
  EXPECT_EQ(page_content.root_node().children_nodes().size(), 1);

  const auto& iframe = page_content.root_node().children_nodes()[0];
  EXPECT_EQ(iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& iframe_data = iframe.content_attributes().iframe_data();
  AssertValidURL(iframe_data.url(), "a.com");
  EXPECT_FALSE(iframe_data.likely_ad_frame());

  EXPECT_EQ(iframe.children_nodes().size(), 1);
}

class AiDataKeyedServiceBrowserTestSiteIsolation
    : public AiDataKeyedServiceBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  bool EnableCrossSiteFrames() const { return GetParam(); }

  std::string QueryParam() const {
    return EnableCrossSiteFrames() ? "?domain=/cross-site/b.com/" : "";
  }
};

// Ensure that clip from an ancestor frame is included in visible rect
// computation.
IN_PROC_BROWSER_TEST_P(AiDataKeyedServiceBrowserTestSiteIsolation,
                       AIPageContentIframePartiallyOffscreen) {
  constexpr gfx::Size kWindowBounds{800, 1000};
  browser()->tab_strip_model()->GetActiveWebContents()->Resize(
      gfx::Rect(kWindowBounds));

  LoadPage(https_server()->GetURL(
      "a.com",
      base::StringPrintf("/paragraph_iframe_partially_offscreen.html%s",
                         QueryParam())));

  const auto& page_content = ai_data()->page_context().annotated_page_content();
  ASSERT_EQ(page_content.root_node().children_nodes().size(), 1);

  const auto& iframe = page_content.root_node().children_nodes()[0];
  ASSERT_EQ(iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);

  ASSERT_EQ(iframe.children_nodes().size(), 1);
  const auto& iframe_root = iframe.children_nodes()[0];
  ASSERT_EQ(iframe_root.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  ASSERT_EQ(iframe_root.children_nodes().size(), 1);
  const auto& p = iframe_root.children_nodes()[0];
  EXPECT_EQ(p.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH);
  const auto& geometry = p.content_attributes().geometry();
  AssertRectsEqual(geometry.outer_bounding_box(),
                   gfx::Rect(-20, -10, 100, 200));
  AssertRectsEqual(geometry.visible_bounding_box(), gfx::Rect(0, 0, 80, 190));
}

// Ensure that clip from an ancestor frame's root scroller are included in
// visible rect computation.
IN_PROC_BROWSER_TEST_P(
    AiDataKeyedServiceBrowserTestSiteIsolation,
    AIPageContentIframePartiallyOffscreenAncestorRootScroller) {
  constexpr gfx::Size kWindowBounds{800, 1000};
  browser()->tab_strip_model()->GetActiveWebContents()->Resize(
      gfx::Rect(kWindowBounds));
  LoadPage(https_server()->GetURL(
      "a.com", base::StringPrintf(
                   "/paragraph_iframe_partially_scrolled_offscreen.html%s",
                   QueryParam())));

  const auto& page_content = ai_data()->page_context().annotated_page_content();
  ASSERT_EQ(page_content.root_node().children_nodes().size(), 1);

  const auto& iframe = page_content.root_node().children_nodes()[0];
  ASSERT_EQ(iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);

  ASSERT_EQ(iframe.children_nodes().size(), 1);
  const auto& iframe_root = iframe.children_nodes()[0];
  ASSERT_EQ(iframe_root.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  const auto& p = iframe_root.children_nodes()[0];
  EXPECT_EQ(p.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH);
  const auto& geometry = p.content_attributes().geometry();

  // TODO(khushalsagar): This is an existing bug where the scroll offset of the
  // root scroller in the ancestor remote frame is not applied.
  if (!EnableCrossSiteFrames()) {
    AssertRectsEqual(geometry.outer_bounding_box(),
                     gfx::Rect(-20, -10, 100, 200));
    AssertRectsEqual(geometry.visible_bounding_box(), gfx::Rect(0, 0, 80, 190));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         AiDataKeyedServiceBrowserTestSiteIsolation,
                         testing::Bool());

class AiDataKeyedServiceBrowserTestMultiProcess
    : public AiDataKeyedServiceBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  bool EnableProcessIsolation() const { return GetParam(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    AiDataKeyedServiceBrowserTest::SetUpCommandLine(command_line);

    if (EnableProcessIsolation()) {
      content::IsolateAllSitesForTesting(command_line);
    } else {
      // TODO(khushalsagar): Enable tests which force a single renderer process
      // for all frames.
      // content::RenderProcessHost::SetMaxRendererProcessCount(1) is not
      // sufficient for that.
      GTEST_SKIP();
    }
  }

 protected:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_P(AiDataKeyedServiceBrowserTestMultiProcess,
                       AIPageContentMultipleCrossSiteFrames) {
  constexpr gfx::Size kWindowBounds{800, 1000};
  browser()->tab_strip_model()->GetActiveWebContents()->Resize(
      gfx::Rect(kWindowBounds));
  LoadPage(https_server()->GetURL("a.com", "/iframe_cross_site.html"));

  const auto& page_content = ai_data()->page_context().annotated_page_content();
  EXPECT_EQ(page_content.root_node().children_nodes().size(), 2);

  const auto& b_frame = page_content.root_node().children_nodes()[0];
  EXPECT_EQ(b_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& b_frame_data = b_frame.content_attributes().iframe_data();
  AssertValidURL(b_frame_data.url(), "b.com");
  EXPECT_FALSE(b_frame_data.likely_ad_frame());

  EXPECT_EQ(b_frame.children_nodes().size(), 1);
  AssertHasText(b_frame.children_nodes()[0], "This page has no title.\n\n");
  const auto& b_geometry = b_frame.content_attributes().geometry();
  AssertRectsEqual(b_geometry.outer_bounding_box(),
                   b_geometry.visible_bounding_box());

  const auto& c_frame = page_content.root_node().children_nodes()[1];
  EXPECT_EQ(c_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& c_frame_data = c_frame.content_attributes().iframe_data();
  AssertValidURL(c_frame_data.url(), "c.com");
  EXPECT_FALSE(c_frame_data.likely_ad_frame());
  EXPECT_EQ(b_frame.children_nodes().size(), 1);
  AssertHasText(c_frame.children_nodes()[0], "This page has no title.\n\n");
  const auto& c_geometry = c_frame.content_attributes().geometry();
  AssertRectsEqual(c_geometry.outer_bounding_box(),
                   c_geometry.visible_bounding_box());

  EXPECT_EQ(b_geometry.outer_bounding_box().width(),
            c_geometry.outer_bounding_box().width());
  EXPECT_EQ(b_geometry.outer_bounding_box().height(),
            c_geometry.outer_bounding_box().height());
  EXPECT_EQ(b_geometry.outer_bounding_box().y(),
            c_geometry.outer_bounding_box().y());
  EXPECT_NE(b_geometry.outer_bounding_box().x(),
            c_geometry.outer_bounding_box().x());
}

INSTANTIATE_TEST_SUITE_P(All,
                         AiDataKeyedServiceBrowserTestMultiProcess,
                         testing::Bool());

class AiDataKeyedServiceBrowserTestFencedFrame
    : public AiDataKeyedServiceBrowserTest {
 protected:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTestFencedFrame,
                       AIPageContentFencedFrame) {
  constexpr gfx::Size kWindowBounds{800, 1000};
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->Resize(gfx::Rect(kWindowBounds));
  LoadPage(https_server()->GetURL("a.com", "/fenced_frame/basic.html"),
           /* with_ai_data = */ false);

  const GURL fenced_frame_url =
      https_server()->GetURL("b.com", "/fenced_frame/simple.html");
  auto* fenced_frame_rfh = fenced_frame_helper_.CreateFencedFrame(
      web_contents->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_NE(nullptr, fenced_frame_rfh);
  LoadData(web_contents);

  const auto& page_content = ai_data()->page_context().annotated_page_content();
  EXPECT_EQ(page_content.root_node().children_nodes().size(), 1);

  const auto& b_frame = page_content.root_node().children_nodes()[0];
  EXPECT_EQ(b_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& b_frame_data = b_frame.content_attributes().iframe_data();
  AssertValidURL(b_frame_data.url(), "b.com");
  EXPECT_FALSE(b_frame_data.likely_ad_frame());
  EXPECT_EQ(b_frame.children_nodes().size(), 1);
  AssertHasText(b_frame.children_nodes()[0], "Non empty simple page\n\n");
  const auto& b_geometry = b_frame.content_attributes().geometry();
  AssertRectsEqual(b_geometry.outer_bounding_box(),
                   b_geometry.visible_bounding_box());
}
#if !BUILDFLAG(IS_ANDROID)
class AiDataKeyedServiceBrowserTestWithFormsPredictions
    : public AiDataKeyedServiceBrowserTest {
 public:
  ~AiDataKeyedServiceBrowserTestWithFormsPredictions() override = default;
  AiDataKeyedServiceBrowserTestWithFormsPredictions() {
    scoped_feature_list_.InitAndEnableFeature(autofill_ai::kAutofillAi);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTestWithFormsPredictions,
                       GetFormsPredictionsDataForModelPrototyping) {
  browser()->profile()->GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillPredictionImprovementsEnabled, true);

  // Set up test data.
  auto request =
      std::make_optional<optimization_guide::proto::FormsPredictionsRequest>();
  optimization_guide::proto::UserAnnotationsEntry* entry =
      request->add_entries();
  entry->set_key("test_key");
  entry->set_value("test_value");
  auto response =
      std::make_optional<optimization_guide::proto::FormsPredictionsResponse>();
  optimization_guide::proto::FilledFormData* filled_form_data =
      response->mutable_form_data();
  optimization_guide::proto::FilledFormFieldData* filled_field =
      filled_form_data->add_filled_form_field_data();
  filled_field->set_normalized_label("test_label");

  // Set up mock.
  auto mock_autofill_ai_model_executor =
      std::make_unique<MockAutofillAiModelExecutor>();
  EXPECT_CALL(*mock_autofill_ai_model_executor, GetLatestRequest)
      .WillOnce(ReturnRef(request));
  EXPECT_CALL(*mock_autofill_ai_model_executor, GetLatestResponse)
      .WillOnce(ReturnRef(response));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
  ASSERT_TRUE(tab)
      << "Active WebContents isn't a tab. TabInterface::GetFromContents() "
         "was expected to crash.";
  ChromeAutofillAiClient* client =
      tab->GetTabFeatures()->chrome_autofill_ai_client();
  ASSERT_TRUE(client)
      << "TabFeatures hasn't created ChromeAutofillAiClient yet.";
  client->SetModelExecutorForTesting(
      std::move(mock_autofill_ai_model_executor));

  LoadSimplePageAndData();

  ASSERT_TRUE(ai_data());
  ASSERT_EQ(ai_data()->forms_predictions_request().entries().size(), 1);
  EXPECT_EQ(ai_data()->forms_predictions_request().entries()[0].key(),
            "test_key");
  EXPECT_EQ(ai_data()->forms_predictions_request().entries()[0].value(),
            "test_value");
  ASSERT_EQ(ai_data()
                ->forms_predictions_response()
                .form_data()
                .filled_form_field_data()
                .size(),
            1);
  EXPECT_EQ(ai_data()
                ->forms_predictions_response()
                .form_data()
                .filled_form_field_data()[0]
                .normalized_label(),
            "test_label");
}
#endif

class AiDataKeyedServiceBrowserTestWithBlocklistedExtensions
    : public AiDataKeyedServiceBrowserTest {
 public:
  ~AiDataKeyedServiceBrowserTestWithBlocklistedExtensions() override = default;
  AiDataKeyedServiceBrowserTestWithBlocklistedExtensions() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        AiDataKeyedService::GetAllowlistedAiDataExtensionsFeatureForTesting(),
        {{"blocked_extension_ids", "hpkopmikdojpadgmioifjjodbmnjjjca"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTestWithBlocklistedExtensions,
                       BlockedExtensionList) {
  std::vector<std::string> expected_allowlisted_extensions = {
      "bgbpcgpcobgjpnpiginpidndjpggappi"};

  EXPECT_EQ(AiDataKeyedService::GetAllowlistedExtensions(),
            expected_allowlisted_extensions);
}

class AiDataKeyedServiceBrowserTestWithRemotelyAllowlistedExtensions
    : public AiDataKeyedServiceBrowserTest {
 public:
  ~AiDataKeyedServiceBrowserTestWithRemotelyAllowlistedExtensions() override =
      default;
  AiDataKeyedServiceBrowserTestWithRemotelyAllowlistedExtensions() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        AiDataKeyedService::GetAllowlistedAiDataExtensionsFeatureForTesting(),
        {{"allowlisted_extension_ids", "1234"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    AiDataKeyedServiceBrowserTestWithRemotelyAllowlistedExtensions,
    RemotelyAllowlistedExtensionList) {
  std::vector<std::string> expected_allowlisted_extensions = {
      "1234",
      "hpkopmikdojpadgmioifjjodbmnjjjca",
      "bgbpcgpcobgjpnpiginpidndjpggappi",
  };

  EXPECT_EQ(AiDataKeyedService::GetAllowlistedExtensions(),
            expected_allowlisted_extensions);
}

class AiDataKeyedServiceBrowserTestWithAllowAndBlock
    : public AiDataKeyedServiceBrowserTest {
 public:
  ~AiDataKeyedServiceBrowserTestWithAllowAndBlock() override = default;
  AiDataKeyedServiceBrowserTestWithAllowAndBlock() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        AiDataKeyedService::GetAllowlistedAiDataExtensionsFeatureForTesting(),
        {{"allowlisted_extension_ids", "1234"},
         {"blocked_extension_ids", "1234"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTestWithAllowAndBlock,
                       AllowAndBlock) {
  std::vector<std::string> expected_allowlisted_extensions = {
      "hpkopmikdojpadgmioifjjodbmnjjjca", "bgbpcgpcobgjpnpiginpidndjpggappi"};

  EXPECT_EQ(AiDataKeyedService::GetAllowlistedExtensions(),
            expected_allowlisted_extensions);
}

}  // namespace
