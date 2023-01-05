// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mojo_web_ui_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/url_constants.h"
#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_ui.h"
#include "components/history_clusters/history_clusters_internals/webui/url_constants.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

namespace {

void SetUpWebUIDataSource(content::WebUI* web_ui,
                          const char* web_ui_host,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), web_ui_host);
  webui::SetupWebUIDataSource(source, resources, default_resource);
  // Disable CSP for tests so that EvalJS can be invoked without CSP violations.
  source->DisableContentSecurityPolicy();
}

class TestWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  TestWebUIControllerFactory() = default;

  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override {
    if (url.host_piece() ==
        history_clusters_internals::kChromeUIHistoryClustersInternalsHost) {
      Profile* profile = Profile::FromWebUI(web_ui);
      return std::make_unique<HistoryClustersInternalsUI>(
          web_ui, HistoryClustersServiceFactory::GetForBrowserContext(profile),
          HistoryServiceFactory::GetForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS),
          base::BindOnce(&SetUpWebUIDataSource, web_ui,
                         history_clusters_internals::
                             kChromeUIHistoryClustersInternalsHost));
    }

    return nullptr;
  }

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override {
    if (url.host_piece() ==
        history_clusters_internals::kChromeUIHistoryClustersInternalsHost)
      return history_clusters_internals::kChromeUIHistoryClustersInternalsHost;

    return content::WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override {
    return GetWebUIType(browser_context, url) != content::WebUI::kNoWebUI;
  }
};

class HistoryClustersInternalsDisabledBrowserTest
    : public MojoWebUIBrowserTest {
 public:
  HistoryClustersInternalsDisabledBrowserTest() {
    factory_ = std::make_unique<TestWebUIControllerFactory>();
    content::WebUIControllerFactory::RegisterFactory(factory_.get());
    feature_list_.InitWithFeatures({history_clusters::internal::kJourneys}, {});
  }
  ~HistoryClustersInternalsDisabledBrowserTest() override = default;

  void NavigateToHistoryClustersInternalsPage() {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(content::GetWebUIURLString(
                       history_clusters_internals::
                           kChromeUIHistoryClustersInternalsHost))));
  }

 protected:
  std::unique_ptr<TestWebUIControllerFactory> factory_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(HistoryClustersInternalsDisabledBrowserTest,
                       InternalsPageFeatureDisabled) {
  NavigateToHistoryClustersInternalsPage();
  content::WebContents* internals_page_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://foo")));

  // Trigger the debug messages to be added to the internals page.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(history_clusters::kChromeUIHistoryClustersURL)));

  // Verify that log messages are not added to the UI. There are still two
  // entries in the UI - the table header and the feature disabled message.
  EXPECT_EQ(true, EvalJs(internals_page_web_contents, R"(
        new Promise(resolve => {
          setInterval(() => {
            const container = document.getElementById('log-message-container');
            if (container.children[0].childElementCount <= 2)
              resolve(true);
          }, 500);
        });)"));
}

class HistoryClustersInternalsBrowserTest
    : public HistoryClustersInternalsDisabledBrowserTest {
 public:
  HistoryClustersInternalsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {history_clusters::internal::kHistoryClustersInternalsPage}, {});
  }
  ~HistoryClustersInternalsBrowserTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(HistoryClustersInternalsBrowserTest,
                       InternalsPageFeatureEnabled) {
  NavigateToHistoryClustersInternalsPage();
  content::WebContents* internals_page_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://foo")));

  // Trigger the debug messages to be added to the internals page.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(history_clusters::kChromeUIHistoryClustersURL)));

  // Verify that log messages are added to the UI.
  EXPECT_EQ(true, EvalJs(internals_page_web_contents, R"(
        new Promise(resolve => {
          setInterval(() => {
            const container = document.getElementById('log-message-container');
            if (container.children[0].childElementCount > 3)
              resolve(true);
          }, 500);
        });)"));
}

}  // namespace
