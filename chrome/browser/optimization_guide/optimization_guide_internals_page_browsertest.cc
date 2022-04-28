// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mojo_web_ui_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals_ui.h"
#include "components/optimization_guide/optimization_guide_internals/webui/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

namespace {

// TODO(crbug.com/1295080): Remove the test helpers that disable CSP once there
// is better support for disabling CSP in webui browser tests.
void SetUpWebUIDataSource(content::WebUI* web_ui,
                          const char* web_ui_host,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  auto source = base::WrapUnique(content::WebUIDataSource::Create(web_ui_host));
  webui::SetupWebUIDataSource(source.get(), resources, default_resource);
  // Disable CSP for tests so that EvalJS can be invoked without CSP violations.
  source->DisableContentSecurityPolicy();
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source.release());
}

class TestWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  TestWebUIControllerFactory() = default;

  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override {
    if (url.host_piece() ==
        optimization_guide_internals::kChromeUIOptimizationGuideInternalsHost) {
      Profile* profile = Profile::FromWebUI(web_ui);
      return std::make_unique<OptimizationGuideInternalsUI>(
          web_ui,
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile)
              ->GetOptimizationGuideLogger(),
          base::BindOnce(&SetUpWebUIDataSource, web_ui,
                         optimization_guide_internals::
                             kChromeUIOptimizationGuideInternalsHost));
    }

    return nullptr;
  }

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override {
    if (url.host_piece() ==
        optimization_guide_internals::kChromeUIOptimizationGuideInternalsHost)
      return optimization_guide_internals::
          kChromeUIOptimizationGuideInternalsHost;

    return content::WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override {
    return GetWebUIType(browser_context, url) != content::WebUI::kNoWebUI;
  }
};

}  // namespace

class OptimizationGuideInternalsPageBrowserTest : public MojoWebUIBrowserTest {
 public:
  OptimizationGuideInternalsPageBrowserTest() {
    factory_ = std::make_unique<TestWebUIControllerFactory>();
    content::WebUIControllerFactory::RegisterFactory(factory_.get());
    feature_list_.InitWithFeatures(
        {optimization_guide::features::kOptimizationHints}, {});
  }
  ~OptimizationGuideInternalsPageBrowserTest() override = default;

  void NavigateToInternalsPage() {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(content::GetWebUIURLString(
                       optimization_guide_internals::
                           kChromeUIOptimizationGuideInternalsHost))));
  }

 protected:
  std::unique_ptr<TestWebUIControllerFactory> factory_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OptimizationGuideInternalsPageBrowserTest,
                       DebugLogEnabledOnInternalsPage) {
  auto* logger =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
          ->GetOptimizationGuideLogger();
  EXPECT_FALSE(logger->ShouldEnableDebugLogs());
  NavigateToInternalsPage();
  // Once the internals page is open, debug logs should get enabled.
  EXPECT_TRUE(logger->ShouldEnableDebugLogs());
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideInternalsPageBrowserTest,
                       DebugLogEnabledOnCommandLineSwitch) {
  auto* logger =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
          ->GetOptimizationGuideLogger();
  EXPECT_FALSE(logger->ShouldEnableDebugLogs());
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDebugLoggingEnabled);
  // With the command-line switch, debug logs should get enabled.
  EXPECT_TRUE(logger->ShouldEnableDebugLogs());
}

// Verifies log message is added when internals page is open.
IN_PROC_BROWSER_TEST_F(OptimizationGuideInternalsPageBrowserTest,
                       InternalsPageOpen) {
  auto* logger =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
          ->GetOptimizationGuideLogger();
  EXPECT_FALSE(logger->ShouldEnableDebugLogs());

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDebugLoggingEnabled);

  EXPECT_TRUE(logger->ShouldEnableDebugLogs());

  NavigateToInternalsPage();
  content::WebContents* internals_page_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto* service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());
  service->RegisterOptimizationTypes({optimization_guide::proto::NOSCRIPT});
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://foo")));

  service->CanApplyOptimization(GURL("https://foo"),
                                optimization_guide::proto::NOSCRIPT,
                                /*optimization_metadata=*/nullptr);

  // Verify that log messages are added to the UI.
  EXPECT_EQ(true, EvalJs(internals_page_web_contents, R"(
        new Promise(resolve => {
          setTimeout(() => {
            const container = document.getElementById('log-message-container');
            if (container.children[0].childElementCount > 2)
              resolve(true);
          }, 500);
        });)"));
}
