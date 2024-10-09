// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/banners/app_banner_manager.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/banners/app_banner_manager_browsertest_base.h"
#include "chrome/browser/profiles/profile.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/banners/app_banner_metrics.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/banners/install_banner_config.h"
#include "components/webapps/browser/banners/installable_web_app_check_result.h"
#include "components/webapps/browser/banners/web_app_banner_data.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#endif

namespace webapps {

using State = AppBannerManager::State;

// Browser tests for web app banners.
// NOTE: this test relies on service workers; failures and flakiness may be due
// to changes in SW code.
// TODO(http://crbug.com/329145718): Use AppBannerManagerNoFakeBrowserTest style
// instead of overriding like this.
// TODO(http://crbug.com/322342499): Completely remove this class.
class AppBannerManagerTest : public AppBannerManager {
 public:
  explicit AppBannerManagerTest(content::WebContents* web_contents)
      : AppBannerManager(web_contents) {}

  AppBannerManagerTest(const AppBannerManagerTest&) = delete;
  AppBannerManagerTest& operator=(const AppBannerManagerTest&) = delete;

  ~AppBannerManagerTest() override {}

  bool TriggeringDisabledForTesting() const override { return false; }

  void RequestAppBanner() override {
    // Filter out about:blank navigations - we use these in testing to force
    // Stop() to be called.
    if (validated_url_ == GURL("about:blank")) {
      return;
    }

    AppBannerManager::RequestAppBanner();
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

  void OnMlInstallPrediction(base::PassKey<MLInstallabilityPromoter>,
                             std::string result_label) override {}

 protected:
  bool CanRequestAppBanner() const override { return true; }

  InstallableParams ParamsToPerformInstallableWebAppCheck() override {
    InstallableParams params;
    params.valid_primary_icon = true;
    params.installable_criteria =
        InstallableCriteria::kImplicitManifestFieldsHTML;
    params.fetch_screenshots = true;
    return params;
  }

  bool ShouldDoNativeAppCheck(
      const blink::mojom::Manifest& manifest) const override {
    return false;
  }

  void DoNativeAppInstallableCheck(content::WebContents* web_contents,
                                   const GURL& validated_url,
                                   const blink::mojom::Manifest& manifest,
                                   NativeCheckCallback callback) override {
    NOTREACHED();
  }

  void OnWebAppInstallableCheckedNoErrors(
      const ManifestId& manifest_id) const override {}

  base::expected<void, InstallableStatusCode> CanRunWebAppInstallableChecks(
      const blink::mojom::Manifest& manifest) override {
    return base::ok();
  }

  void MaybeShowAmbientBadge(const InstallBannerConfig& config) override {
    return;
  }

  void ResetCurrentPageData() override {}

  // The overridden RequestAppBanner() can filter out about:blank calls
  // to force Stop() to be called, however, the newly introduced
  // AppBannerManagerBrowserTestWithChromeBFCache starts a server and navigates
  // to a dynamic/installable banner link and then retriggers the pipeline by
  // terminating an existing banner. As a result, there can exist banners in an
  // intermediary state (on_done_ not initialized, banner still shown) that
  // needs to be cleaned in these overridden functions for Stop() and
  // UpdateState(State::PENDING).
  //
  // As a result, calls to RequestAppBanner should always terminate in
  // ShowBannerUi(), but not necessarily in one of Stop() (not showing banner)
  // or UpdateState(State::PENDING_ENGAGEMENT) (waiting for sufficient
  // engagement). Override these methods to capture test status.
  void Stop(InstallableStatusCode code) override {
    AppBannerManager::Stop(code);
    if (banner_shown_)
      clear_will_show();
    ASSERT_FALSE(banner_shown_.get());
    banner_shown_ = std::make_unique<bool>(false);
    install_source_ =
        std::make_unique<WebappInstallSource>(WebappInstallSource::COUNT);
    if (on_done_)
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(on_done_));
  }

  void ShowBannerUi(WebappInstallSource install_source,
                    const InstallBannerConfig& config) override {
    // Fake the call to ReportStatus here - this is usually called in
    // platform-specific code which is not exposed here.
    ReportStatus(InstallableStatusCode::SHOWING_WEB_APP_BANNER);
    ASSERT_FALSE(banner_shown_.get());
    banner_shown_ = std::make_unique<bool>(true);
    install_source_ = std::make_unique<WebappInstallSource>(install_source);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_done_));
  }

  void UpdateState(AppBannerManager::State state) override {
    AppBannerManager::UpdateState(state);
    if (state == AppBannerManager::State::PENDING_ENGAGEMENT ||
        state == AppBannerManager::State::PENDING_PROMPT_CANCELED ||
        state == AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED) {
      if (on_done_)
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(on_done_));
    }
  }

  void OnBannerPromptReply(
      const InstallBannerConfig& install_config,
      mojo::Remote<blink::mojom::AppBannerController> controller,
      blink::mojom::AppBannerPromptReply reply) override {
    AppBannerManager::OnBannerPromptReply(install_config, std::move(controller),
                                          reply);
    if (on_banner_prompt_reply_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(on_banner_prompt_reply_));
    }
  }

  base::WeakPtr<AppBannerManager> GetWeakPtrForThisNavigation() override {
    return weak_factory_.GetWeakPtr();
  }

  void InvalidateWeakPtrsForThisNavigation() override {
    weak_factory_.InvalidateWeakPtrs();
  }

  bool IsSupportedNonWebAppPlatform(
      const std::u16string& platform) const override {
    return base::EqualsASCII(platform, "chrome_web_store");
  }

  bool IsRelatedNonWebAppInstalled(
      const blink::Manifest::RelatedApplication& related_app) const override {
    // Corresponds to the id listed in manifest_listing_related_chrome_app.json.
    return base::EqualsASCII(related_app.platform.value_or(std::u16string()),
                             "chrome_web_store") &&
           base::EqualsASCII(related_app.id.value_or(std::u16string()),
                             "installed-extension-id");
  }

  base::OnceClosure on_done_;

 private:
  // If non-null, |on_banner_prompt_reply_| will be invoked from
  // OnBannerPromptReply.
  base::OnceClosure on_banner_prompt_reply_;

  std::unique_ptr<bool> banner_shown_;
  std::unique_ptr<WebappInstallSource> install_source_;

  base::WeakPtrFactory<AppBannerManagerTest> weak_factory_{this};
};

enum class SiteEngagementStates {
  kEnabled = 0,
  kBypassed = 1,
  kMaxValue = kBypassed
};

class AppBannerManagerBrowserTest
    : public AppBannerManagerBrowserTestBase,
      public ::testing::WithParamInterface<SiteEngagementStates> {
 public:
  AppBannerManagerBrowserTest()
      : disable_banner_trigger_(&test::g_disable_banner_triggering_for_testing,
                                true),
        total_engagement_(
            AppBannerSettingsHelper::ScopeTotalEngagementForTesting(10)) {}

  AppBannerManagerBrowserTest(const AppBannerManagerBrowserTest&) = delete;
  AppBannerManagerBrowserTest& operator=(const AppBannerManagerBrowserTest&) =
      delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam() == SiteEngagementStates::kBypassed) {
      feature_list_.InitAndEnableFeature(
          features::kBypassAppBannerEngagementChecks);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kBypassAppBannerEngagementChecks);
    }
  }

  void SetUpOnMainThread() override {
    site_engagement::SiteEngagementScore::SetParamValuesForTesting();

    AppBannerManagerBrowserTestBase::SetUpOnMainThread();
  }

 protected:
  std::unique_ptr<AppBannerManagerTest> CreateAppBannerManager() {
    content::WebContents* web_contents =
        chrome_test_utils::GetActiveWebContents(this);
    return std::make_unique<AppBannerManagerTest>(web_contents);
  }

  void RunBannerTest(
      content::WebContents* web_contents,
      AppBannerManagerTest* manager,
      const GURL& url,
      std::optional<InstallableStatusCode> expected_code_for_histogram,
      bool is_off_the_record = false) {
    base::HistogramTester histograms;

    site_engagement::SiteEngagementService* service =
        site_engagement::SiteEngagementService::Get(profile());
    service->ResetBaseScoreForURL(url, 10);

    // Spin the run loop and wait for the manager to finish.
    base::RunLoop run_loop;
    manager->clear_will_show();
    manager->PrepareDone(run_loop.QuitClosure());
    ASSERT_TRUE(content::NavigateToURL(web_contents, url));
    run_loop.Run();

    EXPECT_EQ(expected_code_for_histogram.value_or(
                  InstallableStatusCode::MAX_ERROR_CODE) ==
                  InstallableStatusCode::SHOWING_WEB_APP_BANNER,
              manager->banner_shown());
    EXPECT_EQ(WebappInstallSource::COUNT, manager->install_source());

    // Generally the manager will be in the complete state, however some test
    // cases navigate the page, causing the state to go back to INACTIVE.
    EXPECT_TRUE(manager->state() == State::COMPLETE ||
                manager->state() == State::PENDING_PROMPT_CANCELED ||
                manager->state() == State::PENDING_PROMPT_NOT_CANCELED ||
                manager->state() == State::INACTIVE);

    // If in incognito, ensure that nothing is recorded.
    if (is_off_the_record || !expected_code_for_histogram) {
      histograms.ExpectTotalCount(kInstallableStatusCodeHistogram, 0);
    } else {
      EXPECT_THAT(histograms.GetAllSamples(kInstallableStatusCodeHistogram),
                  base::BucketsAre(base::Bucket(
                      expected_code_for_histogram.value(), /*count=*/1)));
    }
  }

  void TriggerBannerFlowWithNavigation(AppBannerManagerTest* manager,
                                       const GURL& url,
                                       bool expected_will_show,
                                       std::optional<State> expected_state) {
    // Use NavigateToURLWithDisposition as it isn't overloaded, so can be used
    // with Bind.
    TriggerBannerFlow(
        manager, base::BindLambdaForTesting([&]() {
          ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
        }),
        expected_will_show, expected_state);
  }

  void TriggerBannerFlow(AppBannerManagerTest* manager,
                         base::OnceClosure trigger_task,
                         bool expected_will_show,
                         std::optional<State> expected_state) {
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
  // Disable the banners in the browser so it won't interfere with the test.
  base::AutoReset<bool> disable_banner_trigger_;
  base::AutoReset<double> total_engagement_;
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/370270547): Many tests are failing.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_WebAppBannerNoTypeInManifest DISABLED_WebAppBannerNoTypeInManifest
#else
#define MAYBE_WebAppBannerNoTypeInManifest WebAppBannerNoTypeInManifest
#endif
IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest,
                       MAYBE_WebAppBannerNoTypeInManifest) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  RunBannerTest(web_contents(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_no_type.json"),
                std::nullopt);
}

// TODO(crbug.com/370270547): Many tests are failing.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_WebAppBannerNoTypeInManifestCapsExtension \
  DISABLED_WebAppBannerNoTypeInManifestCapsExtension
#else
#define MAYBE_WebAppBannerNoTypeInManifestCapsExtension \
  WebAppBannerNoTypeInManifestCapsExtension
#endif
IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest,
                       MAYBE_WebAppBannerNoTypeInManifestCapsExtension) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  RunBannerTest(web_contents(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_no_type_caps.json"),
                std::nullopt);
}

// TODO(crbug.com/370270547): Many tests are failing.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_WebAppBannerSvgIcon DISABLED_WebAppBannerSvgIcon
#else
#define MAYBE_WebAppBannerSvgIcon WebAppBannerSvgIcon
#endif
IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest, MAYBE_WebAppBannerSvgIcon) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  RunBannerTest(web_contents(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_svg_icon.json"),
                std::nullopt);
}

// TODO(crbug.com/370270547): Many tests are failing.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_WebAppBannerWebPIcon DISABLED_WebAppBannerWebPIcon
#else
#define MAYBE_WebAppBannerWebPIcon WebAppBannerWebPIcon
#endif
IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest,
                       MAYBE_WebAppBannerWebPIcon) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  RunBannerTest(web_contents(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_webp_icon.json"),
                std::nullopt);
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest,
                       DelayedManifestTriggersPipeline) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  RunBannerTest(
      web_contents(), manager.get(),
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html"),
      InstallableStatusCode::NO_MANIFEST);

  // Dynamically add the manifest.
  base::HistogramTester histograms;
  TriggerBannerFlow(
      manager.get(), base::BindLambdaForTesting([&]() {
        EXPECT_TRUE(content::ExecJs(web_contents(), "addManifestLinkTag()"));
      }),
      /*expected_will_show=*/false, std::nullopt);
  TriggerBannerFlow(manager.get(), base::DoNothing(),
                    /*expected_will_show=*/false,
                    AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);
  histograms.ExpectTotalCount(kInstallableStatusCodeHistogram, 0);
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest,
                       RemovingManifestStopsPipeline) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  RunBannerTest(
      web_contents(), manager.get(),
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"),
      std::nullopt);
  EXPECT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);

  // Dynamically remove the manifest.
  base::HistogramTester histograms;
  TriggerBannerFlow(
      manager.get(), base::BindLambdaForTesting([&]() {
        EXPECT_TRUE(content::ExecJs(web_contents(), "removeAllManifestTags()"));
      }),
      false, AppBannerManager::State::COMPLETE);
  histograms.ExpectTotalCount(kInstallableStatusCodeHistogram, 1);
  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                InstallableStatusCode::NO_MANIFEST, 1);
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest,
                       ManifestChangeTriggersPipeline) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());

  // Cause the manifest test page to reach the PENDING_PROMPT stage of the
  // app banner pipeline.
  RunBannerTest(
      web_contents(), manager.get(),
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"),
      std::nullopt);
  EXPECT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);

  // Dynamically change the manifest, which results in a
  // Stop(MANIFEST_URL_CHANGED), and a restart of the pipeline.
  {
    base::HistogramTester histograms;
    // Note - The state of the appbannermanager here will be racy, so don't
    // check for that.
    TriggerBannerFlow(
        manager.get(), base::BindLambdaForTesting([&]() {
          EXPECT_TRUE(content::ExecJs(
              web_contents(),
              "addManifestLinkTag('/banners/manifest_one_icon.json')"));
        }),
        false, std::nullopt);
    histograms.ExpectTotalCount(kInstallableStatusCodeHistogram, 1);
    histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                  InstallableStatusCode::MANIFEST_URL_CHANGED,
                                  1);
  }
  // The pipeline should either have completed, or it is scheduled in the
  // background. Wait for the next prompt request if so.
  if (manager->state() !=
      AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED) {
    base::HistogramTester histograms;
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());
    run_loop.Run();
    histograms.ExpectTotalCount(kInstallableStatusCodeHistogram, 0);
  }
  EXPECT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest,
                       NoPageManifestProvidesDefaultManifest) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  GURL page_url =
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html");
  RunBannerTest(web_contents(), manager.get(), page_url,
                InstallableStatusCode::NO_MANIFEST);
  std::optional<WebAppBannerData> banner =
      manager->GetCurrentWebAppBannerData();
  // Check the default manifest was populated.
  ASSERT_TRUE(banner);
  EXPECT_TRUE(blink::IsDefaultManifest(banner->manifest(), page_url));
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest, MissingManifest) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  RunBannerTest(web_contents(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_missing.json"),
                InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR);
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest, WebAppBannerInIFrame) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  GURL url = embedded_test_server()->GetURL("/banners/iframe_test_page.html");
  RunBannerTest(web_contents(), manager.get(), url,
                InstallableStatusCode::NO_MANIFEST);
  EXPECT_EQ(manager->GetInstallableWebAppCheckResult(),
            webapps::InstallableWebAppCheckResult::kNo);
  // The banner will be the default one for the current page.
  std::optional<WebAppBannerData> banner =
      manager->GetCurrentWebAppBannerData();
  ASSERT_TRUE(banner);
  EXPECT_TRUE(blink::IsDefaultManifest(banner->manifest(), url));
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest, DoesNotShowInIncognito) {
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  content::WebContents* web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<AppBannerManagerTest> manager =
      std::make_unique<AppBannerManagerTest>(web_contents);
  RunBannerTest(web_contents, manager.get(), GetBannerURL(),
                InstallableStatusCode::IN_INCOGNITO, true);
}
#endif

IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest,
                       WebAppBannerInsufficientEngagement) {
  if (GetParam() == SiteEngagementStates::kBypassed) {
    GTEST_SKIP()
        << "This tests the install banner behavior with site engagement";
  }
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());

  base::HistogramTester histograms;
  GURL test_url = GetBannerURL();

  // First run through: expect the manager to end up stopped in the pending
  // state, without showing a banner.
  TriggerBannerFlowWithNavigation(manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_ENGAGEMENT);

  // Navigate and expect Stop() to be called.
  TriggerBannerFlowWithNavigation(manager.get(), GURL("about:blank"),
                                  false /* expected_will_show */,
                                  State::INACTIVE);

  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                InstallableStatusCode::INSUFFICIENT_ENGAGEMENT,
                                1);
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest, WebAppBannerNotCreated) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  base::HistogramTester histograms;

  site_engagement::SiteEngagementService* service =
      site_engagement::SiteEngagementService::Get(profile());
  GURL test_url = GetBannerURL();
  service->ResetBaseScoreForURL(test_url, 10);

  // Navigate and expect the manager to end up waiting for prompt to be called.
  TriggerBannerFlowWithNavigation(manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_PROMPT_NOT_CANCELED);

  // Navigate and expect Stop() to be called.
  TriggerBannerFlowWithNavigation(manager.get(), GURL("about:blank"),
                                  false /* expected_will_show */,
                                  State::INACTIVE);

  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                InstallableStatusCode::RENDERER_CANCELLED, 1);
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest, WebAppBannerCancelled) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  base::HistogramTester histograms;

  site_engagement::SiteEngagementService* service =
      site_engagement::SiteEngagementService::Get(profile());

  // Explicitly call preventDefault(), but don't call prompt().
  GURL test_url = GetBannerURLWithAction("cancel_prompt");
  service->ResetBaseScoreForURL(test_url, 10);

  // Navigate and expect the manager to end up waiting for prompt() to be
  // called.
  TriggerBannerFlowWithNavigation(manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_PROMPT_CANCELED);

  // Navigate to about:blank and expect Stop() to be called.
  TriggerBannerFlowWithNavigation(manager.get(), GURL("about:blank"),
                                  false /* expected_will_show */,
                                  State::INACTIVE);

  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                InstallableStatusCode::RENDERER_CANCELLED, 1);
}

// TODO(crbug.com/370270547): Many tests are failing.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_WebAppBannerPromptWithGesture \
  DISABLED_WebAppBannerPromptWithGesture
#else
#define MAYBE_WebAppBannerPromptWithGesture WebAppBannerPromptWithGesture
#endif
IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest,
                       MAYBE_WebAppBannerPromptWithGesture) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  base::HistogramTester histograms;

  site_engagement::SiteEngagementService* service =
      site_engagement::SiteEngagementService::Get(profile());
  GURL test_url = GetBannerURLWithAction("stash_event");
  service->ResetBaseScoreForURL(test_url, 10);

  // Navigate to page and get the pipeline started.
  TriggerBannerFlowWithNavigation(manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_PROMPT_NOT_CANCELED);

  // Now let the page call prompt with a gesture. The banner should be shown.
  TriggerBannerFlow(manager.get(),
                    base::BindOnce(&AppBannerManagerBrowserTest::ExecuteScript,
                                   web_contents(), "callStashedPrompt();",
                                   true /* with_gesture */),
                    true /* expected_will_show */, State::COMPLETE);

  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                InstallableStatusCode::SHOWING_WEB_APP_BANNER,
                                1);
}

// TODO(crbug.com/370270547): Many tests are failing.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_WebAppBannerNeedsEngagement DISABLED_WebAppBannerNeedsEngagement
#else
#define MAYBE_WebAppBannerNeedsEngagement WebAppBannerNeedsEngagement
#endif
IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest,
                       MAYBE_WebAppBannerNeedsEngagement) {
  if (GetParam() == SiteEngagementStates::kBypassed) {
    GTEST_SKIP()
        << "This tests the install banner behavior with site engagement";
  }
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  base::AutoReset<double> scoped_engagement =
      AppBannerSettingsHelper::ScopeTotalEngagementForTesting(1);
  base::HistogramTester histograms;

  site_engagement::SiteEngagementService* service =
      site_engagement::SiteEngagementService::Get(profile());
  GURL test_url = GetBannerURLWithAction("stash_event");
  service->ResetBaseScoreForURL(test_url, 0);

  // Navigate and expect the manager to end up waiting for sufficient
  // engagement.
  TriggerBannerFlowWithNavigation(manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_ENGAGEMENT);

  // Trigger an engagement increase that signals observers and expect the
  // manager to end up waiting for prompt to be called.
  TriggerBannerFlow(
      manager.get(),
      base::BindOnce(&site_engagement::SiteEngagementService::HandleNavigation,
                     base::Unretained(service), web_contents(),
                     ui::PageTransition::PAGE_TRANSITION_TYPED),
      false /* expected_will_show */, State::PENDING_PROMPT_NOT_CANCELED);

  // Trigger prompt() and expect the banner to be shown.
  TriggerBannerFlow(manager.get(),
                    base::BindOnce(&AppBannerManagerBrowserTest::ExecuteScript,
                                   web_contents(), "callStashedPrompt();",
                                   true /* with_gesture */),
                    true /* expected_will_show */, State::COMPLETE);

  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                InstallableStatusCode::SHOWING_WEB_APP_BANNER,
                                1);
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest, WebAppBannerReprompt) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  base::HistogramTester histograms;

  site_engagement::SiteEngagementService* service =
      site_engagement::SiteEngagementService::Get(profile());
  GURL test_url = GetBannerURLWithAction("stash_event");
  service->ResetBaseScoreForURL(test_url, 10);

  // Navigate to page and get the pipeline started.
  TriggerBannerFlowWithNavigation(manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_PROMPT_NOT_CANCELED);

  // Call prompt to show the banner.
  TriggerBannerFlow(manager.get(),
                    base::BindOnce(&AppBannerManagerBrowserTest::ExecuteScript,
                                   web_contents(), "callStashedPrompt();",
                                   true /* with_gesture */),
                    true /* expected_will_show */, State::COMPLETE);

  // Dismiss the banner.
  base::RunLoop run_loop;
  manager->PrepareDone(base::DoNothing());
  manager->PrepareBannerPromptReply(run_loop.QuitClosure());
  manager->SendBannerDismissed();
  // Wait for OnBannerPromptReply event.
  run_loop.Run();

  // Call prompt again to show the banner again.
  TriggerBannerFlow(manager.get(),
                    base::BindOnce(&AppBannerManagerBrowserTest::ExecuteScript,
                                   web_contents(), "callStashedPrompt();",
                                   true /* with_gesture */),
                    true /* expected_will_show */, State::COMPLETE);

  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                InstallableStatusCode::SHOWING_WEB_APP_BANNER,
                                1);
}

// Flaky on Android. crbug.com/369804412
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PreferRelatedAppUnknown DISABLED_PreferRelatedAppUnknown
#else
#define MAYBE_PreferRelatedAppUnknown PreferRelatedAppUnknown
#endif
IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest,
                       MAYBE_PreferRelatedAppUnknown) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());

  State expected_state =
      base::FeatureList::IsEnabled(
          webapps::features::kBypassAppBannerEngagementChecks)
          ? State::PENDING_PROMPT_NOT_CANCELED
          : State::PENDING_ENGAGEMENT;

  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest="
      "manifest_prefer_related_apps_unknown.json");
  TriggerBannerFlowWithNavigation(
      manager.get(), test_url, false /* expected_will_show */, expected_state);
}

// Flaky on Android. crbug.com/369804412
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PreferRelatedChromeApp DISABLED_PreferRelatedChromeApp
#else
#define MAYBE_PreferRelatedChromeApp PreferRelatedChromeApp
#endif
IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest,
                       MAYBE_PreferRelatedChromeApp) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  base::HistogramTester histograms;

  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest="
      "manifest_prefer_related_chrome_app.json");
  TriggerBannerFlowWithNavigation(
      manager.get(), test_url, false /* expected_will_show */, State::COMPLETE);
  histograms.ExpectUniqueSample(
      kInstallableStatusCodeHistogram,
      InstallableStatusCode::PREFER_RELATED_APPLICATIONS, 1);
}

// TODO(crbug.com/370270547): Many related tests are failing.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ListedRelatedChromeAppInstalled \
  DISABLED_ListedRelatedChromeAppInstalled
#else
#define MAYBE_ListedRelatedChromeAppInstalled ListedRelatedChromeAppInstalled
#endif
IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest,
                       MAYBE_ListedRelatedChromeAppInstalled) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  base::HistogramTester histograms;

  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest="
      "manifest_listing_related_chrome_app.json");
  TriggerBannerFlowWithNavigation(
      manager.get(), test_url, false /* expected_will_show */, State::COMPLETE);
  histograms.ExpectUniqueSample(
      kInstallableStatusCodeHistogram,
      InstallableStatusCode::PREFER_RELATED_APPLICATIONS, 1);
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest, WebAppBannerTerminated) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  base::HistogramTester histograms;

  site_engagement::SiteEngagementService* service =
      site_engagement::SiteEngagementService::Get(profile());

  GURL test_url = GetBannerURL();
  service->ResetBaseScoreForURL(test_url, 10);

  // Navigate and expect the manager to end up waiting for prompt() to be
  // called.
  TriggerBannerFlowWithNavigation(manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_PROMPT_NOT_CANCELED);

  // Navigate to about:blank and expect it to be terminated because the previous
  // URL is still pending.
  TriggerBannerFlowWithNavigation(manager.get(), GURL("about:blank"),
                                  false /* expected_will_show */,
                                  State::INACTIVE);

  // Expect the installation config to be empty, as the page is not eligible
  // for installation.
  EXPECT_EQ(manager->GetCurrentWebAppBannerData(), std::nullopt);
  EXPECT_EQ(manager->GetCurrentBannerConfig(), std::nullopt);

  // Expect RENDERER_CANCELLED to be called when an existing call is terminated.
  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                InstallableStatusCode::RENDERER_CANCELLED, 1);
}

class AppBannerManagerBrowserTestWithChromeBFCache
    : public AppBannerManagerBrowserTest {
 public:
  AppBannerManagerBrowserTestWithChromeBFCache() = default;
  ~AppBannerManagerBrowserTestWithChromeBFCache() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // For using an HTTPS server.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kIgnoreCertificateErrors);

    SetupFeaturesAndParameters();
  }

  void SetupFeaturesAndParameters() {
    feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

  content::RenderFrameHost* current_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  GURL Get2ndInstallableURL() {
    return embedded_test_server()->GetURL("/banners/nested_sw_test_page.html");
  }

  bool IsRenderHostStoredInBackForwardCache(content::RenderFrameHost* rfh) {
    return rfh->IsInLifecycleState(
        content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  }

  void AssertBackForwardCacheIsUsedAsExpected(
      const content::RenderFrameHostWrapper& rfh) {
    ASSERT_EQ(IsRenderHostStoredInBackForwardCache(rfh.get()),
              content::BackForwardCache::IsBackForwardCacheFeatureEnabled());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTestWithChromeBFCache,
                       VerifyBFCacheBehavior) {
  base::AutoReset<double> scoped_engagement =
      AppBannerSettingsHelper::ScopeTotalEngagementForTesting(1);

  ASSERT_TRUE(embedded_test_server()->Start());
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());

  base::HistogramTester histograms;
  // Triggering flow to first URL with a pending prompt.
  TriggerBannerFlowWithNavigation(manager.get(), GetBannerURL(),
                                  /*expected_will_show=*/false,
                                  State::PENDING_PROMPT_NOT_CANCELED);
  content::RenderFrameHostWrapper rfh_a(current_frame_host());
  ASSERT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);
  histograms.ExpectTotalCount(kInstallableStatusCodeHistogram, 0);

  // Navigating to 2nd installable URL while PENDING_PROMPT will trigger
  // the pipeline.
  TriggerBannerFlowWithNavigation(manager.get(), Get2ndInstallableURL(),
                                  /*expected_will_show=*/false, std::nullopt);
  AssertBackForwardCacheIsUsedAsExpected(rfh_a);

  content::RenderFrameHostWrapper rfh_b(current_frame_host());

  // Navigate backward to 1st installable URL.
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  // Verify pipeline has been triggered for new page load.
  EXPECT_NE(manager->state(), AppBannerManager::State::INACTIVE);

  AssertBackForwardCacheIsUsedAsExpected(rfh_b);

  // Navigate forward to 2nd installable URL.
  web_contents()->GetController().GoForward();
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  // Verify pipeline has been triggered for new page load.
  EXPECT_NE(manager->state(), AppBannerManager::State::INACTIVE);

  AssertBackForwardCacheIsUsedAsExpected(rfh_a);
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

class AppBannerManagerMPArchBrowserTest : public AppBannerManagerBrowserTest {
 public:
  AppBannerManagerMPArchBrowserTest() = default;
  ~AppBannerManagerMPArchBrowserTest() override = default;
  AppBannerManagerMPArchBrowserTest(const AppBannerManagerMPArchBrowserTest&) =
      delete;

  AppBannerManagerMPArchBrowserTest& operator=(
      const AppBannerManagerMPArchBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    AppBannerManagerBrowserTest::SetUpOnMainThread();
  }
};

class AppBannerManagerPrerenderBrowserTest
    : public AppBannerManagerMPArchBrowserTest {
 public:
  AppBannerManagerPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &AppBannerManagerPrerenderBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~AppBannerManagerPrerenderBrowserTest() override = default;
  AppBannerManagerPrerenderBrowserTest(
      const AppBannerManagerPrerenderBrowserTest&) = delete;

  AppBannerManagerPrerenderBrowserTest& operator=(
      const AppBannerManagerPrerenderBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    AppBannerManagerMPArchBrowserTest::SetUp();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(AppBannerManagerPrerenderBrowserTest,
                       PrerenderingShouldNotUpdateState) {
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));

  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  EXPECT_EQ(manager->state(), AppBannerManager::State::INACTIVE);

  // Load a page in the prerender.
  GURL prerender_url = GetBannerURL();
  const content::FrameTreeNodeId host_id =
      prerender_test_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  EXPECT_FALSE(host_observer.was_activated());
  EXPECT_EQ(manager->state(), AppBannerManager::State::INACTIVE);

  // Activate the prerender page.
  prerender_test_helper().NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());
  EXPECT_EQ(manager->state(), AppBannerManager::State::FETCHING_MANIFEST);
}

class AppBannerManagerFencedFrameBrowserTest
    : public AppBannerManagerMPArchBrowserTest {
 public:
  AppBannerManagerFencedFrameBrowserTest() = default;
  ~AppBannerManagerFencedFrameBrowserTest() override = default;
  AppBannerManagerFencedFrameBrowserTest(
      const AppBannerManagerFencedFrameBrowserTest&) = delete;

  AppBannerManagerFencedFrameBrowserTest& operator=(
      const AppBannerManagerFencedFrameBrowserTest&) = delete;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(AppBannerManagerFencedFrameBrowserTest,
                       FencedFrameShouldNotUpdateState) {
  // Navigate to an initial page.
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));

  // Initialize a MockWebContentsObserver to ensure that DidUpdateManifestURL is
  // not invoked for fenced frame.
  testing::NiceMock<content::MockWebContentsObserver> observer(web_contents());

  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  EXPECT_EQ(manager->state(), AppBannerManager::State::INACTIVE);

  // Create a fenced frame.
  GURL fenced_frame_url = embedded_test_server()->GetURL(
      "/banners/fenced_frames/manifest_test_page.html?manifest=/banners/"
      "manifest.json");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  EXPECT_NE(nullptr, fenced_frame_host);
  EXPECT_EQ(manager->state(), AppBannerManager::State::INACTIVE);

  // Cross check that  DidUpdateWebManifestURL is not called for fenced frame
  // RenderFrameHost.
  EXPECT_CALL(observer, DidUpdateWebManifestURL(fenced_frame_host, testing::_))
      .Times(0);

  // Navigate the fenced frame.
  fenced_frame_test_helper().NavigateFrameInFencedFrameTree(fenced_frame_host,
                                                            fenced_frame_url);
  EXPECT_EQ(manager->state(), AppBannerManager::State::INACTIVE);
}

// TODO(crbug.com/370270547): Many tests are failing.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ShowBanner DISABLED_ShowBanner
#else
#define MAYBE_ShowBanner ShowBanner
#endif
IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest, MAYBE_ShowBanner) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  RunBannerTest(
      web_contents(), manager.get(),
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"),
      std::nullopt);
  EXPECT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);
  EXPECT_EQ(manager->GetInstallableWebAppCheckResult(),
            InstallableWebAppCheckResult::kYes_Promotable);
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest, NoServiceWorker) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());

  RunBannerTest(web_contents(), manager.get(),
                embedded_test_server()->GetURL(
                    "/banners/manifest_no_service_worker.html"),
                /*expected_code_for_histogram=*/std::nullopt);

  EXPECT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);
  EXPECT_EQ(manager->GetInstallableWebAppCheckResult(),
            InstallableWebAppCheckResult::kYes_Promotable);
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest, NoFetchHandler) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());

  RunBannerTest(web_contents(), manager.get(),
                embedded_test_server()->GetURL(
                    "/banners/no_sw_fetch_handler_test_page.html"),
                /*expected_code_for_histogram=*/std::nullopt);

  EXPECT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);

  EXPECT_EQ(manager->GetInstallableWebAppCheckResult(),
            InstallableWebAppCheckResult::kYes_Promotable);
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest, PendingServiceWorker) {
  std::unique_ptr<AppBannerManagerTest> manager =
      std::make_unique<AppBannerManagerTest>(web_contents());

  RunBannerTest(web_contents(), manager.get(),
                embedded_test_server()->GetURL(
                    "/banners/manifest_no_service_worker.html"),
                std::nullopt);

  EXPECT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);

  EXPECT_EQ(manager->GetInstallableWebAppCheckResult(),
            InstallableWebAppCheckResult::kYes_Promotable);

  ASSERT_TRUE(manager->GetCurrentBannerConfig());
  EXPECT_EQ(manager->GetCurrentBannerConfig()->GetWebOrNativeAppName(),
            u"Manifest test app");
}

// Flaky on Android. crbug.com/369804412
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ValidManifestShowBanner DISABLED_ValidManifestShowBanner
#else
#define MAYBE_ValidManifestShowBanner ValidManifestShowBanner
#endif
IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest,
                       MAYBE_ValidManifestShowBanner) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());
  RunBannerTest(
      web_contents(), manager.get(),
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"),
      std::nullopt);
  EXPECT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);
  EXPECT_EQ(manager->GetInstallableWebAppCheckResult(),
            InstallableWebAppCheckResult::kYes_Promotable);
}

// Flaky on Android. crbug.com/369804412
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ImplicitName DISABLED_ImplicitName
#else
#define MAYBE_ImplicitName ImplicitName
#endif
IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest, MAYBE_ImplicitName) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());

  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest="
      "manifest_empty_name_short_name.json&application-name=TestApp");

  RunBannerTest(web_contents(), manager.get(), test_url, std::nullopt);

  ASSERT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);
  EXPECT_EQ(manager->GetInstallableWebAppCheckResult(),
            InstallableWebAppCheckResult::kYes_Promotable);
  ASSERT_TRUE(manager->GetCurrentBannerConfig());
    EXPECT_EQ(manager->GetCurrentBannerConfig()->GetWebOrNativeAppName(),
              u"TestApp");
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerBrowserTest, ImplicitNameDocumentTitle) {
  std::unique_ptr<AppBannerManagerTest> manager(CreateAppBannerManager());

  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest="
      "manifest_empty_name_short_name.json");

  RunBannerTest(web_contents(), manager.get(), test_url, std::nullopt);

  ASSERT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);
  EXPECT_EQ(manager->GetInstallableWebAppCheckResult(),
            InstallableWebAppCheckResult::kYes_Promotable);
  ASSERT_TRUE(manager->GetCurrentBannerConfig());
    EXPECT_EQ(manager->GetCurrentBannerConfig()->GetWebOrNativeAppName(),
              u"Web app banner test page");
}

#if !BUILDFLAG(IS_ANDROID)
// TODO(http://crbug.com/329255543): Add the config data after the struct is
// converted to a class w/o const members (this makes it hard to work with
// TestFuture).
using AppBannerInstallableCallback =
    base::RepeatingCallback<void(std::optional<ManifestId>)>;
class AppBannerManagerObserverAdapter : public AppBannerManager::Observer {
 public:
  AppBannerManagerObserverAdapter(AppBannerManager* source,
                                  AppBannerInstallableCallback callback,
                                  InstallableWebAppCheckResult filter)
      : callback_(std::move(callback)), filter_(filter) {
    observation_.Observe(source);
  }

  void OnInstallableWebAppStatusUpdated(
      InstallableWebAppCheckResult result,
      const std::optional<WebAppBannerData>& data) override {
    if (result != filter_) {
      LOG(WARNING) << "Filtered result: " << base::ToString(result);
      return;
    }
    callback_.Run(data.has_value() ? std::make_optional(data->manifest_id)
                                   : std::nullopt);
  }

 private:
  AppBannerInstallableCallback callback_;
  InstallableWebAppCheckResult filter_;
  base::ScopedObservation<AppBannerManager, AppBannerManager::Observer>
      observation_{this};
};

// Test class that doesn't do complex faking of the AppBannerManager.
class AppBannerManagerNoFakeBrowserTest
    : public AppBannerManagerBrowserTestBase,
      public ::testing::WithParamInterface<SiteEngagementStates> {
 public:
  AppBannerManagerNoFakeBrowserTest()
      : total_engagement_(
            AppBannerSettingsHelper::ScopeTotalEngagementForTesting(0)) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam() == SiteEngagementStates::kBypassed) {
      feature_list_.InitAndEnableFeature(
          features::kBypassAppBannerEngagementChecks);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kBypassAppBannerEngagementChecks);
    }
  }

 private:
  // Disable the banners in the browser so it won't interfere with the test.
  base::AutoReset<double> total_engagement_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(AppBannerManagerNoFakeBrowserTest, Prompts) {
  const GURL kAppUrl =
      embedded_test_server()->GetURL("/banners/manifest_test_page.html");

  AppBannerManager* app_banner_manager =
      AppBannerManager::FromWebContents(web_contents());
  base::test::TestFuture<std::optional<ManifestId>> future;
  AppBannerManagerObserverAdapter observer(
      app_banner_manager, future.GetRepeatingCallback(),
      InstallableWebAppCheckResult::kYes_Promotable);

  ASSERT_TRUE(content::NavigateToURL(web_contents(), kAppUrl));

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(kAppUrl, future.Get());
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerNoFakeBrowserTest,
                       PromptsForInnerCraftedOuterDiy) {
  const GURL kDiyAppUrl =
      embedded_test_server()->GetURL("/web_apps/nesting/index.html");
  const GURL kInnerAppUrl =
      embedded_test_server()->GetURL("/web_apps/nesting/nested/index.html");

  // Install a DIY app.
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(kDiyAppUrl);
  web_app_info->is_diy_app = true;
  web_app_info->title = u"test web app";
  web_app_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  AppBannerManager* app_banner_manager =
      AppBannerManager::FromWebContents(web_contents());
  base::test::TestFuture<std::optional<ManifestId>> future;
  AppBannerManagerObserverAdapter observer(
      app_banner_manager, future.GetRepeatingCallback(),
      InstallableWebAppCheckResult::kYes_Promotable);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), kInnerAppUrl));

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(kInnerAppUrl, future.Get());
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerNoFakeBrowserTest,
                       NoPromptsForInnerDiyOuterDiy) {
  const GURL kDiyAppUrl =
      embedded_test_server()->GetURL("/web_apps/nesting/index.html");
  const GURL kInnerDiyAppUrl =
      embedded_test_server()->GetURL("/web_apps/nesting/nested/diy.html");

  // Install a DIY app.
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(kDiyAppUrl);
  web_app_info->is_diy_app = true;
  web_app_info->title = u"test web app";
  web_app_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  AppBannerManager* app_banner_manager =
      AppBannerManager::FromWebContents(web_contents());
  base::test::TestFuture<std::optional<ManifestId>> future;
  AppBannerManagerObserverAdapter observer(app_banner_manager,
                                           future.GetRepeatingCallback(),
                                           InstallableWebAppCheckResult::kNo);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), kInnerDiyAppUrl));

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(kInnerDiyAppUrl, future.Get());
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerNoFakeBrowserTest,
                       NoPromptForOuterCraftedDisplayBrowserInnerCrafted) {
  const GURL kOuterAppUrl =
      embedded_test_server()->GetURL("/web_apps/nesting/index.html");
  const GURL kInnerAppUrl =
      embedded_test_server()->GetURL("/web_apps/nesting/nested/index.html");

  // Even if the outer crafted app opens in a browser tab, it should still block
  // any nested installations.
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(kOuterAppUrl);
  web_app_info->title = u"test web app";
  web_app_info->user_display_mode = web_app::mojom::UserDisplayMode::kBrowser;
  web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  AppBannerManager* app_banner_manager =
      AppBannerManager::FromWebContents(web_contents());
  base::test::TestFuture<std::optional<ManifestId>> future;
  AppBannerManagerObserverAdapter observer(
      app_banner_manager, future.GetRepeatingCallback(),
      InstallableWebAppCheckResult::kNo_AlreadyInstalled);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), kInnerAppUrl));

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(kInnerAppUrl, future.Get());
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerNoFakeBrowserTest,
                       NoPromptForOuterCraftedDisplayStandaloneInnerCrafted) {
  const GURL kOuterAppUrl =
      embedded_test_server()->GetURL("/web_apps/nesting/index.html");
  const GURL kInnerAppUrl =
      embedded_test_server()->GetURL("/web_apps/nesting/nested/index.html");

  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(kOuterAppUrl);
  web_app_info->title = u"test web app";
  web_app_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  AppBannerManager* app_banner_manager =
      AppBannerManager::FromWebContents(web_contents());
  base::test::TestFuture<std::optional<ManifestId>> future;
  AppBannerManagerObserverAdapter observer(
      app_banner_manager, future.GetRepeatingCallback(),
      InstallableWebAppCheckResult::kNo_AlreadyInstalled);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), kInnerAppUrl));

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(kInnerAppUrl, future.Get());
}

IN_PROC_BROWSER_TEST_P(AppBannerManagerNoFakeBrowserTest,
                       NoPromptForOuterCraftedDisplayBrowserInnerDiy) {
  const GURL kOuterAppUrl =
      embedded_test_server()->GetURL("/web_apps/nesting/index.html");
  const GURL kInnerAppUrl =
      embedded_test_server()->GetURL("/web_apps/nesting/nested/diy.html");

  // Install a DIY app.
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(kOuterAppUrl);
  web_app_info->title = u"test web app";
  web_app_info->user_display_mode = web_app::mojom::UserDisplayMode::kBrowser;
  web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  AppBannerManager* app_banner_manager =
      AppBannerManager::FromWebContents(web_contents());
  base::test::TestFuture<std::optional<ManifestId>> future;
  AppBannerManagerObserverAdapter observer(app_banner_manager,
                                           future.GetRepeatingCallback(),
                                           InstallableWebAppCheckResult::kNo);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), kInnerAppUrl));

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(kInnerAppUrl, future.Get());
}

INSTANTIATE_TEST_SUITE_P(All,
                         AppBannerManagerNoFakeBrowserTest,
                         ::testing::Values(SiteEngagementStates::kEnabled,
                                           SiteEngagementStates::kBypassed));

#endif  // !BUILDFLAG(IS_ANDROID)

INSTANTIATE_TEST_SUITE_P(All,
                         AppBannerManagerBrowserTest,
                         ::testing::Values(SiteEngagementStates::kEnabled,
                                           SiteEngagementStates::kBypassed));

}  // namespace
}  // namespace webapps
