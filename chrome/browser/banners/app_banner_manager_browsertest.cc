// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/banners/app_banner_manager_browsertest_base.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/installable/installable_logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"

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

  void RequestAppBanner(const GURL& validated_url,
                        bool is_debug_mode) override {
    // Filter out about:blank navigations - we use these in testing to force
    // Stop() to be called.
    if (validated_url == GURL("about:blank"))
      return;

    AppBannerManager::RequestAppBanner(validated_url, is_debug_mode);
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
  void PrepareDone(base::Closure on_done) { on_done_ = on_done; }

  // Configures a callback to be invoked from OnBannerPromptReply.
  void PrepareBannerPromptReply(base::Closure on_banner_prompt_reply) {
    on_banner_prompt_reply_ = on_banner_prompt_reply;
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
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, on_done_);
  }

  void ShowBannerUi(WebappInstallSource install_source) override {
    // Fake the call to ReportStatus here - this is usually called in
    // platform-specific code which is not exposed here.
    ReportStatus(SHOWING_WEB_APP_BANNER);
    RecordDidShowBanner("AppBanner.WebApp.Shown");

    ASSERT_FALSE(banner_shown_.get());
    banner_shown_.reset(new bool(true));
    install_source_.reset(new WebappInstallSource(install_source));
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, on_done_);
  }

  void UpdateState(AppBannerManager::State state) override {
    AppBannerManager::UpdateState(state);

    if (state == AppBannerManager::State::PENDING_ENGAGEMENT ||
        (AppBannerManager::IsExperimentalAppBannersEnabled() &&
         state == AppBannerManager::State::PENDING_PROMPT)) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, on_done_);
    }
  }

  void OnBannerPromptReply(blink::mojom::AppBannerControllerPtr controller,
                           blink::mojom::AppBannerPromptReply reply,
                           const std::string& referrer) override {
    AppBannerManager::OnBannerPromptReply(std::move(controller), reply,
                                          referrer);
    if (on_banner_prompt_reply_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(on_banner_prompt_reply_));
    }
  }

 private:
  base::Closure on_done_;

  // If non-null, |on_banner_prompt_reply_| will be invoked from
  // OnBannerPromptReply.
  base::Closure on_banner_prompt_reply_;

  std::unique_ptr<bool> banner_shown_;
  std::unique_ptr<WebappInstallSource> install_source_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerManagerTest);
};

class AppBannerManagerBrowserTest : public AppBannerManagerBrowserTestBase {
 public:
  AppBannerManagerBrowserTest() : AppBannerManagerBrowserTestBase() {}

  void SetUpOnMainThread() override {
    AppBannerSettingsHelper::SetTotalEngagementToTrigger(10);
    SiteEngagementScore::SetParamValuesForTesting();

    feature_list_.InitWithFeatures({}, {features::kExperimentalAppBanners,
                                        features::kDesktopPWAWindowing});

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

  void RunBannerTest(Browser* browser,
                     AppBannerManagerTest* manager,
                     const GURL& url,
                     const std::vector<double>& engagement_scores,
                     WebappInstallSource expected_install_source,
                     InstallableStatusCode expected_code_for_histogram,
                     bool expected_to_record_minutes_histogram) {
    RunBannerTest(browser, manager, url, engagement_scores,
                  expected_install_source, expected_code_for_histogram,
                  expected_to_record_minutes_histogram, base::string16());
  }

  void RunBannerTest(Browser* browser,
                     AppBannerManagerTest* manager,
                     const GURL& url,
                     const std::vector<double>& engagement_scores,
                     WebappInstallSource expected_install_source,
                     InstallableStatusCode expected_code_for_histogram,
                     bool expected_to_record_minutes_histogram,
                     const base::string16 expected_tab_title) {
    base::HistogramTester histograms;
    manager->clear_will_show();

    // Loop through the vector of engagement scores. We only expect the banner
    // pipeline to trigger on the last one; otherwise, nothing is expected to
    // happen.
    int iterations = 0;
    SiteEngagementService* service =
        SiteEngagementService::Get(browser->profile());
    for (double engagement : engagement_scores) {
      if (iterations > 0) {
        ui_test_utils::NavigateToURL(browser, url);

        EXPECT_FALSE(manager->banner_shown());
        EXPECT_EQ(State::INACTIVE, manager->state());

        histograms.ExpectTotalCount(banners::kMinutesHistogram, 0);
        histograms.ExpectTotalCount(banners::kInstallableStatusCodeHistogram,
                                    0);
      }
      service->ResetBaseScoreForURL(url, engagement);
      ++iterations;
    }

    // The final loop should have set sufficient engagement for the banner to
    // trigger. Spin the run loop and wait for the manager to finish.
    base::RunLoop run_loop;
    manager->clear_will_show();
    manager->PrepareDone(run_loop.QuitClosure());
    NavigateParams nav_params(browser, url, ui::PAGE_TRANSITION_LINK);
    ui_test_utils::NavigateToURL(&nav_params);
    run_loop.Run();

    EXPECT_EQ(expected_code_for_histogram == SHOWING_WEB_APP_BANNER,
              manager->banner_shown());
    EXPECT_EQ(expected_install_source, manager->install_source());

    // Generally the manager will be in the complete state, however some test
    // cases navigate the page, causing the state to go back to INACTIVE.
    EXPECT_TRUE(manager->state() == State::COMPLETE ||
                manager->state() == State::INACTIVE);

    // Check the tab title; this allows the test page to send data back out to
    // be inspected by the test case.
    if (!expected_tab_title.empty()) {
      base::string16 title;
      EXPECT_TRUE(ui_test_utils::GetCurrentTabTitle(browser, &title));
      EXPECT_EQ(expected_tab_title, title);
    }

    // If in incognito, ensure that nothing is recorded.
    // If showing the banner, ensure that the minutes histogram is recorded if
    // expected.
    if (browser->profile()->IsOffTheRecord()) {
      histograms.ExpectTotalCount(banners::kMinutesHistogram, 0);
      histograms.ExpectTotalCount(banners::kInstallableStatusCodeHistogram, 0);
    } else {
      histograms.ExpectTotalCount(
          banners::kMinutesHistogram,
          (expected_to_record_minutes_histogram ? 1 : 0));
      histograms.ExpectUniqueSample(banners::kInstallableStatusCodeHistogram,
                                    expected_code_for_histogram, 1);
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
            ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION),
        expected_will_show, expected_state);
  }

  void TriggerBannerFlow(Browser* browser,
                         AppBannerManagerTest* manager,
                         base::OnceClosure trigger_task,
                         bool expected_will_show,
                         State expected_state) {
    base::RunLoop run_loop;
    manager->clear_will_show();
    manager->PrepareDone(run_loop.QuitClosure());
    std::move(trigger_task).Run();
    run_loop.Run();

    EXPECT_EQ(expected_will_show, manager->banner_shown());
    EXPECT_EQ(expected_state, manager->state());
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerManagerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerCreated) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  std::vector<double> engagement_scores{0, 10};
  RunBannerTest(browser(), manager.get(), GetBannerURL(), engagement_scores,
                WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB,
                SHOWING_WEB_APP_BANNER, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerCreatedImmediately) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  std::vector<double> engagement_scores{10};
  RunBannerTest(browser(), manager.get(), GetBannerURL(), engagement_scores,
                WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB,
                SHOWING_WEB_APP_BANNER, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerCreatedAfterSeveralVisits) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  std::vector<double> engagement_scores{0, 1, 2, 3, 4, 5, 10};
  RunBannerTest(browser(), manager.get(), GetBannerURL(), engagement_scores,
                WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB,
                SHOWING_WEB_APP_BANNER, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerNotSeenAfterShowing) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  std::vector<double> engagement_scores{0, 10};
  RunBannerTest(browser(), manager.get(), GetBannerURL(), engagement_scores,
                WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB,
                SHOWING_WEB_APP_BANNER, true);

  AppBannerManager::SetTimeDeltaForTesting(1);
  RunBannerTest(browser(), manager.get(), GetBannerURL(), engagement_scores,
                WebappInstallSource::COUNT, PREVIOUSLY_IGNORED, false);

  AppBannerManager::SetTimeDeltaForTesting(13);
  RunBannerTest(browser(), manager.get(), GetBannerURL(), engagement_scores,
                WebappInstallSource::COUNT, PREVIOUSLY_IGNORED, false);

  AppBannerManager::SetTimeDeltaForTesting(14);
  RunBannerTest(browser(), manager.get(), GetBannerURL(), engagement_scores,
                WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB,
                SHOWING_WEB_APP_BANNER, false);

  AppBannerSettingsHelper::SetDaysAfterDismissAndIgnoreToTrigger(90, 2);

  AppBannerManager::SetTimeDeltaForTesting(16);
  RunBannerTest(browser(), manager.get(), GetBannerURL(), engagement_scores,
                WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB,
                SHOWING_WEB_APP_BANNER, false);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerNoTypeInManifest) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  std::vector<double> engagement_scores{0, 10};
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_no_type.json"),
                engagement_scores,
                WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB,
                SHOWING_WEB_APP_BANNER, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerNoTypeInManifestCapsExtension) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  std::vector<double> engagement_scores{0, 10};
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_no_type_caps.json"),
                engagement_scores,
                WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB,
                SHOWING_WEB_APP_BANNER, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, NoManifest) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  std::vector<double> engagement_scores{10};
  RunBannerTest(
      browser(), manager.get(),
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html"),
      engagement_scores, WebappInstallSource::COUNT, NO_MANIFEST, false);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, MissingManifest) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  std::vector<double> engagement_scores{10};
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_missing.json"),
                engagement_scores, WebappInstallSource::COUNT, MANIFEST_EMPTY,
                false);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       BeforeInstallPromptEventReceived) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  std::vector<double> engagement_scores{0, 5, 10};

  // Expect that the page sets the tab title to indicate that it got the event
  // twice: once for addEventListener('beforeinstallprompt'), and once for the
  // onbeforeinstallprompt attribute.
  // Note that this test does not call beforeinstallpromptevent.prompt(); it
  // merely ensures that the event was sent and received by the page.
  RunBannerTest(
      browser(), manager.get(),
      GetBannerURLWithAction("verify_beforeinstallprompt"), engagement_scores,
      WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB, SHOWING_WEB_APP_BANNER,
      true, base::ASCIIToUTF16("Got beforeinstallprompt: listener, attr"));
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, CancelBannerDirect) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  std::vector<double> engagement_scores{10};
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithAction("cancel_prompt_and_navigate"),
                engagement_scores, WebappInstallSource::COUNT,
                RENDERER_CANCELLED, false);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, PromptBanner) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  std::vector<double> engagement_scores{0, 5, 10};
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithAction("call_prompt_delayed"),
                engagement_scores, WebappInstallSource::API_BROWSER_TAB,
                SHOWING_WEB_APP_BANNER, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, PromptBannerInHandler) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  std::vector<double> engagement_scores{0, 2, 5, 10};
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithAction("call_prompt_in_handler"),
                engagement_scores, WebappInstallSource::API_BROWSER_TAB,
                SHOWING_WEB_APP_BANNER, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       CancelBannerAfterPromptInHandler) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  std::vector<double> engagement_scores{10};
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithAction("call_prompt_in_handler"),
                engagement_scores, WebappInstallSource::API_BROWSER_TAB,
                SHOWING_WEB_APP_BANNER, true);
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithManifestAndQuery(
                    "/banners/manifest_different_start_url.json", "action",
                    "cancel_prompt_and_navigate"),
                engagement_scores, WebappInstallSource::COUNT,
                RENDERER_CANCELLED, false);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerInIFrame) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  std::vector<double> engagement_scores{10};
  RunBannerTest(
      browser(), manager.get(),
      embedded_test_server()->GetURL("/banners/iframe_test_page.html"),
      engagement_scores, WebappInstallSource::COUNT, NO_MANIFEST, false);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, DoesNotShowInIncognito) {
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(incognito_browser));
  std::vector<double> engagement_scores{10};
  RunBannerTest(incognito_browser, manager.get(), GetBannerURL(),
                engagement_scores, WebappInstallSource::COUNT, IN_INCOGNITO,
                false);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       ExperimentalFlowWebAppBannerInsufficientEngagement) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kExperimentalAppBanners);
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

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       ExperimentalFlowWebAppBannerNotCreated) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kExperimentalAppBanners);
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

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       ExperimentalFlowWebAppBannerCancelled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kExperimentalAppBanners);
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
                       ExperimentalFlowWebAppBannerPromptWithGesture) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kExperimentalAppBanners);
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
                       ExperimentalFlowWebAppBannerNeedsEngagement) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(1);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kExperimentalAppBanners);
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

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       ExperimentalFlowWebAppBannerReprompt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kExperimentalAppBanners);
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

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, OverlappingDebugRequest) {
  base::HistogramTester histograms;
  GURL test_url = GetBannerURL();
  SiteEngagementService* service =
      SiteEngagementService::Get(browser()->profile());
  service->ResetBaseScoreForURL(test_url, 10);

  ui_test_utils::NavigateToURL(browser(), test_url);

  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  base::RunLoop run_loop;
  manager->PrepareDone(run_loop.QuitClosure());

  // Call RequestAppBanner to start the pipeline, then call it again in debug
  // mode to ensure that this doesn't fail.
  manager->RequestAppBanner(test_url, false);
  EXPECT_EQ(State::FETCHING_MANIFEST, manager->state());
  manager->RequestAppBanner(test_url, true);
  run_loop.Run();

  EXPECT_TRUE(manager->banner_shown());
  EXPECT_EQ(WebappInstallSource::DEVTOOLS, manager->install_source());
  EXPECT_EQ(State::COMPLETE, manager->state());

  // Ensure that we do not record any histograms.
  histograms.ExpectTotalCount(banners::kInstallableStatusCodeHistogram, 0);
}

}  // namespace banners
