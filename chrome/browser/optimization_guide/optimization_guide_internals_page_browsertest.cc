// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_internals_ui.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mojo_web_ui_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
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
        optimization_guide_internals::kChromeUIOptimizationGuideInternalsHost) {
      return std::make_unique<OptimizationGuideInternalsUI>(
          web_ui, base::BindOnce(&SetUpWebUIDataSource, web_ui,
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

class ModelFileObserver
    : public optimization_guide::OptimizationTargetModelObserver {
 public:
  using ModelFileReceivedCallback =
      base::OnceCallback<void(optimization_guide::proto::OptimizationTarget,
                              const optimization_guide::ModelInfo&)>;

  ModelFileObserver() = default;
  ~ModelFileObserver() override = default;

  void set_model_file_received_callback(ModelFileReceivedCallback callback) {
    file_received_callback_ = std::move(callback);
  }

  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const optimization_guide::ModelInfo& model_info) override {
    if (file_received_callback_)
      std::move(file_received_callback_).Run(optimization_target, model_info);
  }

 private:
  ModelFileReceivedCallback file_received_callback_;
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

  void NavigateToInternalsModelsPage() {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(content::GetWebUIURLString(
                            optimization_guide_internals::
                                kChromeUIOptimizationGuideInternalsHost) +
                        "/#models")));
  }

 protected:
  void TriggerModelDownloadForOptimizationTarget(
      optimization_guide::proto::OptimizationTarget optimization_target) {
    base::RunLoop run_loop;
    ModelFileObserver model_file_observer;

    model_file_observer.set_model_file_received_callback(
        base::BindLambdaForTesting(
            [&run_loop](optimization_guide::proto::OptimizationTarget
                            optimization_target,
                        const optimization_guide::ModelInfo& model_info) {
              base::ScopedAllowBlockingForTesting scoped_allow_blocking;

              EXPECT_EQ(123, model_info.GetVersion());
              EXPECT_TRUE(model_info.GetModelFilePath().IsAbsolute());
              EXPECT_TRUE(base::PathExists(model_info.GetModelFilePath()));

              EXPECT_EQ(1U, model_info.GetAdditionalFiles().size());
              for (const base::FilePath& add_file :
                   model_info.GetAdditionalFiles()) {
                EXPECT_TRUE(add_file.IsAbsolute());
                EXPECT_TRUE(base::PathExists(add_file));
              }

              run_loop.Quit();
            }));

    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->AddObserverForOptimizationTargetModel(
            optimization_target,
            /*model_metadata=*/absl::nullopt, &model_file_observer);

    run_loop.Run();
  }

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

// Verifies downloaded models are added when #models page is open.
IN_PROC_BROWSER_TEST_F(OptimizationGuideInternalsPageBrowserTest,
                       InternalsModelsPageOpen) {
  auto* logger =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
          ->GetOptimizationGuideLogger();
  EXPECT_FALSE(logger->ShouldEnableDebugLogs());

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDebugLoggingEnabled);

  EXPECT_TRUE(logger->ShouldEnableDebugLogs());

  base::FilePath src_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &src_dir);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      optimization_guide::switches::kModelOverride,
      base::StrCat({
          "OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD",
          optimization_guide::ModelOverrideSeparator(),
          optimization_guide::FilePathToString(
              src_dir.AppendASCII("optimization_guide")
                  .AppendASCII("additional_file_exists.crx3")),
      }));

  NavigateToInternalsPage();
  TriggerModelDownloadForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  NavigateToInternalsModelsPage();
  content::WebContents* internals_models_page_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Verify that the downloaded model is added to the UI.
  EXPECT_EQ(true, EvalJs(internals_models_page_web_contents, R"(
        new Promise(resolve => {
          setTimeout(() => {
            const container =
              document.getElementById('downloaded-models-container');
            if (container.children[0].childElementCount > 0)
              resolve(true);
          }, 500);
        });)"));
  EXPECT_EQ(true, EvalJs(internals_models_page_web_contents, R"(
        new Promise(resolve => {
          setTimeout(() => {
            const tableRow =
              document.getElementById('OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD');
            if (tableRow)
              resolve(true);
          }, 500);
        });)"));
}
