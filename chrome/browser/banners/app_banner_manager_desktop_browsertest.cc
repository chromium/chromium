// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager_desktop.h"

#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/banners/app_banner_manager_browsertest_base.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/webapps/browser/banners/app_banner_metrics.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

namespace webapps {

namespace {

std::vector<base::test::FeatureRef> GetDisabledFeatures() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::standalone_browser::GetFeatureRefs();
#else
  return {};
#endif
}

}  // namespace

using State = AppBannerManager::State;

class AppBannerManagerDesktopBrowserTest
    : public AppBannerManagerBrowserTestBase {
 public:
  AppBannerManagerDesktopBrowserTest()
      : total_engagement_(
            AppBannerSettingsHelper::ScopeTotalEngagementForTesting(0)) {}

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({}, GetDisabledFeatures());
    TestAppBannerManagerDesktop::SetUp();
    AppBannerManagerBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    web_app::SetAutoAcceptPWAInstallConfirmationForTesting(true);

    AppBannerManagerBrowserTestBase::SetUpOnMainThread();
  }

  void TearDown() override {
    web_app::SetAutoAcceptPWAInstallConfirmationForTesting(false);
  }

  AppBannerManagerDesktopBrowserTest(
      const AppBannerManagerDesktopBrowserTest&) = delete;
  AppBannerManagerDesktopBrowserTest& operator=(
      const AppBannerManagerDesktopBrowserTest&) = delete;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  // Scope engagement needed to trigger banners instantly.
  base::AutoReset<double> total_engagement_;
};

IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest,
                       WebAppBannerResolvesUserChoice) {
  base::HistogramTester tester;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* manager = TestAppBannerManagerDesktop::FromWebContents(web_contents);

  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GetBannerURLWithAction("stash_event")));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT_NOT_CANCELED, manager->state());
  }

  {
    // Trigger the installation prompt and wait for installation to occur.
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());
    ExecuteScript(web_contents, "callStashedPrompt();",
                  true /* with_gesture */);
    run_loop.Run();
    EXPECT_EQ(State::COMPLETE, manager->state());
  }

  // Ensure that the userChoice promise resolves.
  const std::u16string title = u"Got userChoice: accepted";
  content::TitleWatcher watcher(web_contents, title);
  EXPECT_EQ(title, watcher.WaitAndGetTitle());

  tester.ExpectUniqueSample(kInstallDisplayModeHistogram,
                            blink::mojom::DisplayMode::kStandalone, 1);
}

// TODO(crbug.com/40637899): Flakes on most platforms.
IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest,
                       DISABLED_WebAppBannerFiresAppInstalled) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* manager = TestAppBannerManagerDesktop::FromWebContents(web_contents);

  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GetBannerURLWithAction("verify_appinstalled_stash_event")));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT_NOT_CANCELED, manager->state());
  }

  {
    // Trigger the installation prompt and wait for installation to occur.
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    const GURL url = GetBannerURL();
    bool callback_called = false;

    web_app::SetInstalledCallbackForTesting(
        base::BindLambdaForTesting([&](const webapps::AppId& installed_app_id,
                                       webapps::InstallResultCode code) {
          EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
          EXPECT_EQ(installed_app_id,
                    web_app::GenerateAppId(/*manifest_id=*/std::nullopt, url));
          callback_called = true;
        }));

    ExecuteScript(web_contents, "callStashedPrompt();",
                  true /* with_gesture */);

    run_loop.Run();

    EXPECT_EQ(State::COMPLETE, manager->state());
    EXPECT_TRUE(callback_called);
  }

  // Ensure that the appinstalled event fires.
  const std::u16string title = u"Got appinstalled: listener, attr";
  content::TitleWatcher watcher(web_contents, title);
  EXPECT_EQ(title, watcher.WaitAndGetTitle());
}

// TODO(crbug.com/40817384): Flaky failures.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_DestroyWebContents DISABLED_DestroyWebContents
#else
#define MAYBE_DestroyWebContents DestroyWebContents
#endif
IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest,
                       MAYBE_DestroyWebContents) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* manager = TestAppBannerManagerDesktop::FromWebContents(web_contents);

  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GetBannerURLWithAction("stash_event")));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT_NOT_CANCELED, manager->state());
  }

  {
    // Trigger the installation and wait for termination to occur.
    base::RunLoop run_loop;
    bool callback_called = false;

    web_app::SetInstalledCallbackForTesting(
        base::BindLambdaForTesting([&](const webapps::AppId& installed_app_id,
                                       webapps::InstallResultCode code) {
          EXPECT_EQ(webapps::InstallResultCode::kWebContentsDestroyed, code);
          callback_called = true;
          run_loop.Quit();
        }));

    ExecuteScript(web_contents, "callStashedPrompt();",
                  true /* with_gesture */);

    // Closing WebContents destroys WebContents and AppBannerManager.
    browser()->tab_strip_model()->CloseWebContentsAt(
        browser()->tab_strip_model()->active_index(), 0);
    manager = nullptr;
    web_contents = nullptr;

    run_loop.Run();
    EXPECT_TRUE(callback_called);
  }
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest,
                       InstallPromptAfterUserMenuInstall) {
  base::HistogramTester tester;

  TestAppBannerManagerDesktop* manager =
      TestAppBannerManagerDesktop::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GetBannerURLWithManifestAndQuery("/banners/minimal-ui.json",
                                                    "action", "stash_event")));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT_NOT_CANCELED, manager->state());
  }

  // Install the app via the menu instead of the banner.
  web_app::SetAutoAcceptPWAInstallConfirmationForTesting(true);
  browser()->command_controller()->ExecuteCommand(IDC_INSTALL_PWA);
  manager->AwaitAppInstall();
  web_app::SetAutoAcceptPWAInstallConfirmationForTesting(false);

  EXPECT_FALSE(manager->IsPromptAvailableForTesting());

  tester.ExpectUniqueSample(kInstallDisplayModeHistogram,
                            blink::mojom::DisplayMode::kMinimalUi, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest,
                       InstallPromptAfterUserOmniboxInstall) {
  base::HistogramTester tester;

  TestAppBannerManagerDesktop* manager =
      TestAppBannerManagerDesktop::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GetBannerURLWithManifestAndQuery("/banners/fullscreen.json",
                                                    "action", "stash_event")));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT_NOT_CANCELED, manager->state());
  }

  // Install the app via the menu instead of the banner.
  web_app::SetAutoAcceptPWAInstallConfirmationForTesting(true);
  browser()->window()->ExecutePageActionIconForTesting(
      PageActionIconType::kPwaInstall);
  manager->AwaitAppInstall();
  web_app::SetAutoAcceptPWAInstallConfirmationForTesting(false);

  EXPECT_FALSE(manager->IsPromptAvailableForTesting());

  tester.ExpectUniqueSample(kInstallDisplayModeHistogram,
                            blink::mojom::DisplayMode::kFullscreen, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest,
                       PolicyAppInstalled_Prompt) {
  TestAppBannerManagerDesktop* manager =
      TestAppBannerManagerDesktop::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  // Install web app by policy.
  web_app::ExternalInstallOptions options =
      web_app::CreateInstallOptions(GetBannerURL());
  options.install_source = web_app::ExternalInstallSource::kExternalPolicy;
  options.user_display_mode = web_app::mojom::UserDisplayMode::kBrowser;
  web_app::ExternallyManagedAppManagerInstall(browser()->profile(), options);

  // Run promotability check.
  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetBannerURL()));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT_NOT_CANCELED, manager->state());
  }

  EXPECT_EQ(InstallableWebAppCheckResult::kYes_Promotable,
            manager->GetInstallableWebAppCheckResult());
  EXPECT_TRUE(manager->IsPromptAvailableForTesting());
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest,
                       PolicyAppUninstalled_Prompt) {
  TestAppBannerManagerDesktop* manager =
      TestAppBannerManagerDesktop::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
  Profile* profile = browser()->profile();

  // Install web app by policy.
  web_app::ExternalInstallOptions options =
      web_app::CreateInstallOptions(GetBannerURL());
  options.install_source = web_app::ExternalInstallSource::kExternalPolicy;
  options.user_display_mode = web_app::mojom::UserDisplayMode::kBrowser;
  web_app::ExternallyManagedAppManagerInstall(profile, options);

  // Uninstall web app by policy.
  {
    base::RunLoop run_loop;
    web_app::WebAppProvider::GetForTest(profile)
        ->externally_managed_app_manager()
        .UninstallApps(
            {GetBannerURL()}, web_app::ExternalInstallSource::kExternalPolicy,
            base::BindLambdaForTesting(
                [&run_loop](const GURL& app_url,
                            webapps::UninstallResultCode code) {
                  EXPECT_EQ(code, webapps::UninstallResultCode::kAppRemoved);
                  run_loop.Quit();
                }));
    run_loop.Run();
  }

  // Run promotability check.
  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetBannerURL()));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT_NOT_CANCELED, manager->state());
  }

  EXPECT_EQ(InstallableWebAppCheckResult::kYes_Promotable,
            manager->GetInstallableWebAppCheckResult());
  EXPECT_TRUE(manager->IsPromptAvailableForTesting());
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest,
                       InstallPromptAfterUserMenuInstall_DisplayOverride) {
  base::HistogramTester tester;

  TestAppBannerManagerDesktop* manager =
      TestAppBannerManagerDesktop::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GetBannerURLWithManifestAndQuery(
                       "/banners/manifest_display_override.json", "action",
                       "stash_event")));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT_NOT_CANCELED, manager->state());
  }

  // Install the app via the menu instead of the banner.
  web_app::SetAutoAcceptPWAInstallConfirmationForTesting(true);
  browser()->command_controller()->ExecuteCommand(IDC_INSTALL_PWA);
  manager->AwaitAppInstall();
  web_app::SetAutoAcceptPWAInstallConfirmationForTesting(false);

  EXPECT_FALSE(manager->IsPromptAvailableForTesting());

  tester.ExpectUniqueSample(kInstallDisplayModeHistogram,
                            blink::mojom::DisplayMode::kMinimalUi, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest,
                       WebAppBannerResolvesUserChoice_DisplayOverride) {
  base::HistogramTester tester;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* manager = TestAppBannerManagerDesktop::FromWebContents(web_contents);

  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        GetBannerURLWithManifestAndQuery(
            "/banners/manifest_display_override_display_is_browser.json",
            "action", "stash_event")));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT_NOT_CANCELED, manager->state());
  }

  {
    // Trigger the installation prompt and wait for installation to occur.
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());
    ExecuteScript(web_contents, "callStashedPrompt();",
                  true /* with_gesture */);
    run_loop.Run();
    EXPECT_EQ(State::COMPLETE, manager->state());
  }

  // Ensure that the userChoice promise resolves.
  const std::u16string title = u"Got userChoice: accepted";
  content::TitleWatcher watcher(web_contents, title);
  EXPECT_EQ(title, watcher.WaitAndGetTitle());

  tester.ExpectUniqueSample(kInstallDisplayModeHistogram,
                            blink::mojom::DisplayMode::kStandalone, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest,
                       PolicyAppInstalled_Prompt_DisplayOverride) {
  TestAppBannerManagerDesktop* manager =
      TestAppBannerManagerDesktop::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  // Install web app by policy.
  web_app::ExternalInstallOptions options =
      web_app::CreateInstallOptions(GetBannerURLWithManifest(
          "/banners/manifest_display_override_contains_browser.json"));
  options.install_source = web_app::ExternalInstallSource::kExternalPolicy;
  options.user_display_mode = web_app::mojom::UserDisplayMode::kBrowser;
  web_app::ExternallyManagedAppManagerInstall(browser()->profile(), options);

  // Run promotability check.
  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetBannerURL()));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT_NOT_CANCELED, manager->state());
  }

  EXPECT_EQ(InstallableWebAppCheckResult::kYes_Promotable,
            manager->GetInstallableWebAppCheckResult());
  EXPECT_TRUE(manager->IsPromptAvailableForTesting());
}

class AppBannerManagerDesktopBrowserTestForPasswordManagerPage
    : public AppBannerManagerDesktopBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{}, GetDisabledFeatures());
    TestAppBannerManagerDesktop::SetUp();
    AppBannerManagerBrowserTestBase::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTestForPasswordManagerPage,
                       WebUiPasswordManagerApp) {
  TestAppBannerManagerDesktop* manager =
      TestAppBannerManagerDesktop::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  // Simulate loading a PasswordManager page.
  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        GURL(base::StrCat(
            {"chrome://", password_manager::kChromeUIPasswordManagerHost}))));
    run_loop.Run();
  }

  EXPECT_EQ(InstallableWebAppCheckResult::kYes_Promotable,
            manager->GetInstallableWebAppCheckResult());
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest,
                       PipelineRunsAfterStop) {
  TestAppBannerManagerDesktop* manager =
      TestAppBannerManagerDesktop::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("/web_apps/stop-loading-early.html")));
    run_loop.Run();
  }

  EXPECT_EQ(InstallableWebAppCheckResult::kYes_Promotable,
            manager->GetInstallableWebAppCheckResult())
      << manager->debug_log();
}

}  // namespace webapps
