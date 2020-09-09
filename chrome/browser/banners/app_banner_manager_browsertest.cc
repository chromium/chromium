// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/banners/app_banner_manager_browsertest_base.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/installable/installable_logging.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace banners {

using State = AppBannerManager::State;

// Browser tests for web app banners.
// NOTE: this test relies on service workers; failures and flakiness may be due
// to changes in SW code.
class AppBannerManagerTest : public AppBannerManager {
 public:
  explicit AppBannerManagerTest(content::WebContents* web_contents)
      : AppBannerManager(web_contents) {}

  ~AppBannerManagerTest() override {}

  void RequestAppBanner(const GURL& validated_url) override {
    // Filter out about:blank navigations - we use these in testing to force
    // Stop() to be called.
    if (validated_url == GURL("about:blank"))
      return;

    AppBannerManager::RequestAppBanner(validated_url);
  }

  bool banner_shown() { return banner_shown_.get() && *banner_shown_; }

  WebappInstallSource install_source() {
    if (install_source_.get())
      return *install_source_;

    return WebappInstallSource::COUNT;
  }

  void clear_will_show() { banner_shown_.reset(); }

  State state() { return AppBannerManager::state(); }

  // Configures a callback to be invoked when the app banner flow finishes.
  void PrepareDone(base::OnceClosure on_done) { on_done_ = std::move(on_done); }

  // Configures a callback to be invoked from OnBannerPromptReply.
  void PrepareBannerPromptReply(base::OnceClosure on_banner_prompt_reply) {
    on_banner_prompt_reply_ = std::move(on_banner_prompt_reply);
  }

 protected:
  // All calls to RequestAppBanner should terminate in one of Stop() (not
  // showing banner), UpdateState(State::PENDING_ENGAGEMENT) (waiting for
  // sufficient engagement), or ShowBannerUi(). Override these methods to
  // capture test status.
  void Stop(InstallableStatusCode code) override {
    AppBannerManager::Stop(code);
    ASSERT_FALSE(banner_shown_.get());
    banner_shown_.reset(new bool(false));
    install_source_.reset(new WebappInstallSource(WebappInstallSource::COUNT));
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(on_done_));
  }

  void ShowBannerUi(WebappInstallSource install_source) override {
    // Fake the call to ReportStatus here - this is usually called in
    // platform-specific code which is not exposed here.
    ReportStatus(SHOWING_WEB_APP_BANNER);
    RecordDidShowBanner();

    ASSERT_FALSE(banner_shown_.get());
    banner_shown_.reset(new bool(true));
    install_source_.reset(new WebappInstallSource(install_source));
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(on_done_));
  }

  void UpdateState(AppBannerManager::State state) override {
    AppBannerManager::UpdateState(state);

    if (state == AppBannerManager::State::PENDING_ENGAGEMENT ||
        state == AppBannerManager::State::PENDING_PROMPT) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                    std::move(on_done_));
    }
  }

  void OnBannerPromptReply(
      mojo::Remote<blink::mojom::AppBannerController> controller,
      blink::mojom::AppBannerPromptReply reply) override {
    AppBannerManager::OnBannerPromptReply(std::move(controller), reply);
    if (on_banner_prompt_reply_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(on_banner_prompt_reply_));
    }
  }

  base::WeakPtr<AppBannerManager> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  void InvalidateWeakPtrs() override { weak_factory_.InvalidateWeakPtrs(); }

  bool IsSupportedAppPlatform(const base::string16& platform) const override {
    return base::EqualsASCII(platform, "chrome_web_store");
  }

  bool IsRelatedAppInstalled(
      const blink::Manifest::RelatedApplication& related_app) const override {
    // Corresponds to the id listed in manifest_listing_related_chrome_app.json.
    return base::EqualsASCII(related_app.platform.value_or(base::string16()),
                             "chrome_web_store") &&
           base::EqualsASCII(related_app.id.value_or(base::string16()),
                             "installed-extension-id");
  }

 private:
  base::OnceClosure on_done_;

  // If non-null, |on_banner_prompt_reply_| will be invoked from
  // OnBannerPromptReply.
  base::OnceClosure on_banner_prompt_reply_;

  std::unique_ptr<bool> banner_shown_;
  std::unique_ptr<WebappInstallSource> install_source_;

  base::WeakPtrFactory<AppBannerManagerTest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppBannerManagerTest);
};

class AppBannerManagerBrowserTest : public AppBannerManagerBrowserTestBase {
 public:
  AppBannerManagerBrowserTest() = default;

  void SetUpOnMainThread() override {
    AppBannerSettingsHelper::SetTotalEngagementToTrigger(10);
    SiteEngagementScore::SetParamValuesForTesting();

    // Make sure app banners are disabled in the browser, otherwise they will
    // interfere with the test.
    AppBannerManagerDesktop::DisableTriggeringForTesting();
    AppBannerManagerBrowserTestBase::SetUpOnMainThread();
  }

 protected:
  std::unique_ptr<AppBannerManagerTest> CreateAppBannerManager(
      Browser* browser) {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<AppBannerManagerTest>(web_contents);
  }

  void RunBannerTest(
      Browser* browser,
      AppBannerManagerTest* manager,
      const GURL& url,
      base::Optional<InstallableStatusCode> expected_code_for_histogram) {
    base::HistogramTester histograms;

    SiteEngagementService* service =
        SiteEngagementService::Get(browser->profile());
    service->ResetBaseScoreForURL(url, 10);

    // Spin the run loop and wait for the manager to finish.
    base::RunLoop run_loop;
    manager->clear_will_show();
    manager->PrepareDone(run_loop.QuitClosure());
    NavigateParams nav_params(browser, url, ui::PAGE_TRANSITION_LINK);
    ui_test_utils::NavigateToURL(&nav_params);
    run_loop.Run();

    EXPECT_EQ(expected_code_for_histogram.value_or(MAX_ERROR_CODE) ==
                  SHOWING_WEB_APP_BANNER,
              manager->banner_shown());
    EXPECT_EQ(WebappInstallSource::COUNT, manager->install_source());

    // Generally the manager will be in the complete state, however some test
    // cases navigate the page, causing the state to go back to INACTIVE.
    EXPECT_TRUE(manager->state() == State::COMPLETE ||
                manager->state() == State::PENDING_PROMPT ||
                manager->state() == State::INACTIVE);

    // If in incognito, ensure that nothing is recorded.
    histograms.ExpectTotalCount(banners::kMinutesHistogram, 0);
    if (browser->profile()->IsOffTheRecord() || !expected_code_for_histogram) {
      histograms.ExpectTotalCount(banners::kInstallableStatusCodeHistogram, 0);
    } else {
      histograms.ExpectUniqueSample(banners::kInstallableStatusCodeHistogram,
                                    *expected_code_for_histogram, 1);
    }
  }

  void TriggerBannerFlowWithNavigation(Browser* browser,
                                       AppBannerManagerTest* manager,
                                       const GURL& url,
                                       bool expected_will_show,
                                       State expected_state) {
    // Use NavigateToURLWithDisposition as it isn't overloaded, so can be used
    // with Bind.
    TriggerBannerFlow(
        browser, manager,
        base::BindOnce(
            base::IgnoreResult(&ui_test_utils::NavigateToURLWithDisposition),
            browser, url, WindowOpenDisposition::CURRENT_TAB,
            ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP),
        expected_will_show, expected_state);
  }

  void TriggerBannerFlow(Browser* browser,
                         AppBannerManagerTest* manager,
                         base::OnceClosure trigger_task,
                         bool expected_will_show,
                         base::Optional<State> expected_state) {
    base::RunLoop run_loop;
    manager->clear_will_show();
    manager->PrepareDone(run_loop.QuitClosure());
    std::move(trigger_task).Run();
    run_loop.Run();

    EXPECT_EQ(expected_will_show, manager->banner_shown());
    if (expected_state)
      EXPECT_EQ(expected_state, manager->state());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AppBannerManagerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerNoTypeInManifest) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_no_type.json"),
                base::nullopt);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerNoTypeInManifestCapsExtension) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_no_type_caps.json"),
                base::nullopt);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerSvgIcon) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_svg_icon.json"),
                base::nullopt);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerWebPIcon) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_webp_icon.json"),
                base::nullopt);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       DelayedManifestTriggersPipeline) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(
      browser(), manager.get(),
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html"),
      NO_MANIFEST);

  // Dynamically add the manifest.
  base::HistogramTester histograms;
  TriggerBannerFlow(browser(), manager.get(), base::BindLambdaForTesting([&]() {
                      EXPECT_TRUE(content::ExecJs(
                          browser()->tab_strip_model()->GetActiveWebContents(),
                          "addManifestLinkTag()"));
                    }),
                    false, AppBannerManager::State::PENDING_PROMPT);
  histograms.ExpectTotalCount(banners::kInstallableStatusCodeHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       RemovingManifestStopsPipeline) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(
      browser(), manager.get(),
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"),
      base::nullopt);
  EXPECT_EQ(manager->state(), AppBannerManager::State::PENDING_PROMPT);

  // Dynamically remove the manifest.
  base::HistogramTester histograms;
  TriggerBannerFlow(browser(), manager.get(), base::BindLambdaForTesting([&]() {
                      EXPECT_TRUE(content::ExecJs(
                          browser()->tab_strip_model()->GetActiveWebContents(),
                          "removeAllManifestTags()"));
                    }),
                    false, AppBannerManager::State::COMPLETE);
  histograms.ExpectTotalCount(banners::kInstallableStatusCodeHistogram, 1);
  histograms.ExpectUniqueSample(banners::kInstallableStatusCodeHistogram,
                                RENDERER_CANCELLED, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       ManifestChangeTriggersPipeline) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));

  // Cause the manifest test page to reach the PENDING_PROMPT stage of the
  // app banner pipeline.
  RunBannerTest(
      browser(), manager.get(),
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"),
      base::nullopt);
  EXPECT_EQ(manager->state(), AppBannerManager::State::PENDING_PROMPT);

  // Dynamically change the manifest, which results in a
  // Stop(RENDERER_CANCELLED), and a restart of the pipeline.
  {
    base::HistogramTester histograms;
    // Note - The state of the appbannermanager here will be racy, so don't
    // check for that.
    TriggerBannerFlow(
        browser(), manager.get(), base::BindLambdaForTesting([&]() {
          EXPECT_TRUE(content::ExecJs(
              browser()->tab_strip_model()->GetActiveWebContents(),
              "addManifestLinkTag('/banners/manifest_one_icon.json')"));
        }),
        false, base::nullopt);
    histograms.ExpectTotalCount(banners::kInstallableStatusCodeHistogram, 1);
    histograms.ExpectUniqueSample(banners::kInstallableStatusCodeHistogram,
                                  RENDERER_CANCELLED, 1);
  }
  // The pipeline should either have completed, or it is scheduled in the
  // background. Wait for the next prompt request if so.
  if (manager->state() != AppBannerManager::State::PENDING_PROMPT) {
    base::HistogramTester histograms;
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());
    run_loop.Run();
    histograms.ExpectTotalCount(banners::kInstallableStatusCodeHistogram, 0);
  }
  EXPECT_EQ(manager->state(), AppBannerManager::State::PENDING_PROMPT);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, NoManifest) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(
      browser(), manager.get(),
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html"),
      NO_MANIFEST);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, MissingManifest) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_missing.json"),
                MANIFEST_EMPTY);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerInIFrame) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(
      browser(), manager.get(),
      embedded_test_server()->GetURL("/banners/iframe_test_page.html"),
      NO_MANIFEST);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, DoesNotShowInIncognito) {
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(incognito_browser));
  RunBannerTest(incognito_browser, manager.get(), GetBannerURL(), IN_INCOGNITO);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerInsufficientEngagement) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));

  base::HistogramTester histograms;
  GURL test_url = GetBannerURL();

  // First run through: expect the manager to end up stopped in the pending
  // state, without showing a banner.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_ENGAGEMENT);

  // Navigate and expect Stop() to be called.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), GURL("about:blank"),
                                  false /* expected_will_show */,
                                  State::INACTIVE);

  histograms.ExpectTotalCount(banners::kMinutesHistogram, 0);
  histograms.ExpectUniqueSample(banners::kInstallableStatusCodeHistogram,
                                INSUFFICIENT_ENGAGEMENT, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerNotCreated) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  base::HistogramTester histograms;

  SiteEngagementService* service =
      SiteEngagementService::Get(browser()->profile());
  GURL test_url = GetBannerURL();
  service->ResetBaseScoreForURL(test_url, 10);

  // Navigate and expect the manager to end up waiting for prompt to be called.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_PROMPT);

  // Navigate and expect Stop() to be called.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), GURL("about:blank"),
                                  false /* expected_will_show */,
                                  State::INACTIVE);

  histograms.ExpectTotalCount(banners::kMinutesHistogram, 0);
  histograms.ExpectUniqueSample(banners::kInstallableStatusCodeHistogram,
                                RENDERER_CANCELLED, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerCancelled) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  base::HistogramTester histograms;

  SiteEngagementService* service =
      SiteEngagementService::Get(browser()->profile());

  // Explicitly call preventDefault(), but don't call prompt().
  GURL test_url = GetBannerURLWithAction("cancel_prompt");
  service->ResetBaseScoreForURL(test_url, 10);

  // Navigate and expect the manager to end up waiting for prompt() to be
  // called.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_PROMPT);

  // Navigate to about:blank and expect Stop() to be called.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), GURL("about:blank"),
                                  false /* expected_will_show */,
                                  State::INACTIVE);

  histograms.ExpectTotalCount(banners::kMinutesHistogram, 0);
  histograms.ExpectUniqueSample(banners::kInstallableStatusCodeHistogram,
                                RENDERER_CANCELLED, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerPromptWithGesture) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  base::HistogramTester histograms;

  SiteEngagementService* service =
      SiteEngagementService::Get(browser()->profile());
  GURL test_url = GetBannerURLWithAction("stash_event");
  service->ResetBaseScoreForURL(test_url, 10);

  // Navigate to page and get the pipeline started.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_PROMPT);

  // Now let the page call prompt with a gesture. The banner should be shown.
  TriggerBannerFlow(
      browser(), manager.get(),
      base::BindOnce(&AppBannerManagerBrowserTest::ExecuteScript, browser(),
                     "callStashedPrompt();", true /* with_gesture */),
      true /* expected_will_show */, State::COMPLETE);

  histograms.ExpectTotalCount(banners::kMinutesHistogram, 1);
  histograms.ExpectUniqueSample(banners::kInstallableStatusCodeHistogram,
                                SHOWING_WEB_APP_BANNER, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerNeedsEngagement) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(1);
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  base::HistogramTester histograms;

  SiteEngagementService* service =
      SiteEngagementService::Get(browser()->profile());
  GURL test_url = GetBannerURLWithAction("stash_event");
  service->ResetBaseScoreForURL(test_url, 0);

  // Navigate and expect the manager to end up waiting for sufficient
  // engagement.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_ENGAGEMENT);

  // Trigger an engagement increase that signals observers and expect the
  // manager to end up waiting for prompt to be called.
  TriggerBannerFlow(
      browser(), manager.get(),
      base::BindOnce(&SiteEngagementService::HandleNavigation,
                     base::Unretained(service),
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     ui::PageTransition::PAGE_TRANSITION_TYPED),
      false /* expected_will_show */, State::PENDING_PROMPT);

  // Trigger prompt() and expect the banner to be shown.
  TriggerBannerFlow(
      browser(), manager.get(),
      base::BindOnce(&AppBannerManagerBrowserTest::ExecuteScript, browser(),
                     "callStashedPrompt();", true /* with_gesture */),
      true /* expected_will_show */, State::COMPLETE);

  histograms.ExpectTotalCount(banners::kMinutesHistogram, 1);
  histograms.ExpectUniqueSample(banners::kInstallableStatusCodeHistogram,
                                SHOWING_WEB_APP_BANNER, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerReprompt) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  base::HistogramTester histograms;

  SiteEngagementService* service =
      SiteEngagementService::Get(browser()->profile());
  GURL test_url = GetBannerURLWithAction("stash_event");
  service->ResetBaseScoreForURL(test_url, 10);

  // Navigate to page and get the pipeline started.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_PROMPT);

  // Call prompt to show the banner.
  TriggerBannerFlow(
      browser(), manager.get(),
      base::BindOnce(&AppBannerManagerBrowserTest::ExecuteScript, browser(),
                     "callStashedPrompt();", true /* with_gesture */),
      true /* expected_will_show */, State::COMPLETE);

  // Dismiss the banner.
  base::RunLoop run_loop;
  manager->PrepareDone(base::DoNothing());
  manager->PrepareBannerPromptReply(run_loop.QuitClosure());
  manager->SendBannerDismissed();
  // Wait for OnBannerPromptReply event.
  run_loop.Run();

  // Call prompt again to show the banner again.
  TriggerBannerFlow(
      browser(), manager.get(),
      base::BindOnce(&AppBannerManagerBrowserTest::ExecuteScript, browser(),
                     "callStashedPrompt();", true /* with_gesture */),
      true /* expected_will_show */, State::COMPLETE);

  histograms.ExpectTotalCount(banners::kMinutesHistogram, 1);
  histograms.ExpectUniqueSample(banners::kInstallableStatusCodeHistogram,
                                SHOWING_WEB_APP_BANNER, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, PreferRelatedAppUnknown) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));

  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest="
      "manifest_prefer_related_apps_unknown.json");
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_ENGAGEMENT);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, PreferRelatedChromeApp) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  base::HistogramTester histograms;

  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest="
      "manifest_prefer_related_chrome_app.json");
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::COMPLETE);
  histograms.ExpectUniqueSample(banners::kInstallableStatusCodeHistogram,
                                PREFER_RELATED_APPLICATIONS, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       ListedRelatedChromeAppInstalled) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  base::HistogramTester histograms;

  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest="
      "manifest_listing_related_chrome_app.json");
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::COMPLETE);
  histograms.ExpectUniqueSample(banners::kInstallableStatusCodeHistogram,
                                PREFER_RELATED_APPLICATIONS, 1);
}

namespace {
class FailingInstallableManager : public InstallableManager {
 public:
  explicit FailingInstallableManager(content::WebContents* web_contents)
      : InstallableManager(web_contents) {}

  void FailNext(std::unique_ptr<InstallableData> installable_data) {
    failure_data_ = std::move(installable_data);
  }

  void GetData(const InstallableParams& params,
               InstallableCallback callback) override {
    if (failure_data_) {
      auto temp_data = std::move(failure_data_);
      std::move(callback).Run(*temp_data);
      return;
    }
    InstallableManager::GetData(params, std::move(callback));
  }

 private:
  std::unique_ptr<InstallableData> failure_data_;
};

class AppBannerManagerBrowserTestWithFailableInstallableManager
    : public AppBannerManagerBrowserTest {
 public:
  AppBannerManagerBrowserTestWithFailableInstallableManager() = default;
  ~AppBannerManagerBrowserTestWithFailableInstallableManager() override =
      default;

  void SetUpOnMainThread() override {
    // Manually inject the FailingInstallableManager as a "InstallableManager"
    // WebContentsUserData. We can't directly call ::CreateForWebContents due to
    // typing issues since FailingInstallableManager doesn't directly inherit
    // from WebContentsUserData.
    browser()->tab_strip_model()->GetActiveWebContents()->SetUserData(
        FailingInstallableManager::UserDataKey(),
        base::WrapUnique(new FailingInstallableManager(
            browser()->tab_strip_model()->GetActiveWebContents())));
    installable_manager_ = static_cast<FailingInstallableManager*>(
        browser()->tab_strip_model()->GetActiveWebContents()->GetUserData(
            FailingInstallableManager::UserDataKey()));

    AppBannerManagerBrowserTest::SetUpOnMainThread();
  }

 protected:
  FailingInstallableManager* installable_manager_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(
    AppBannerManagerBrowserTestWithFailableInstallableManager,
    AppBannerManagerRetriesPipeline) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));

  SiteEngagementService* service =
      SiteEngagementService::Get(browser()->profile());
  GURL test_url = GetBannerURLWithAction("stash_event");
  service->ResetBaseScoreForURL(test_url, 10);

  installable_manager_->FailNext(base::WrapUnique(
      new InstallableData({MANIFEST_URL_CHANGED}, GURL(), nullptr, GURL(),
                          nullptr, false, GURL(), nullptr, false, false)));

  // The page should record one failure of MANIFEST_URL_CHANGED, but it should
  // still successfully get to the PENDING_PROMPT state of the pipeline, as it
  // should retry the call to GetData on the InstallableManager.
  RunBannerTest(browser(), manager.get(), test_url, MANIFEST_URL_CHANGED);
  EXPECT_EQ(manager->state(), AppBannerManager::State::PENDING_PROMPT);

  {
    base::HistogramTester histograms;
    // Now let the page call prompt with a gesture. The banner should be shown.
    TriggerBannerFlow(
        browser(), manager.get(),
        base::BindOnce(&AppBannerManagerBrowserTest::ExecuteScript, browser(),
                       "callStashedPrompt();", true /* with_gesture */),
        true /* expected_will_show */, State::COMPLETE);

    histograms.ExpectTotalCount(banners::kMinutesHistogram, 1);
    histograms.ExpectUniqueSample(banners::kInstallableStatusCodeHistogram,
                                  SHOWING_WEB_APP_BANNER, 1);
  }
}

}  // namespace
}  // namespace banners
