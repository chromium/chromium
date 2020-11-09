// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/banners/app_banner_manager_browsertest_base.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"

namespace banners {

using State = AppBannerManager::State;

class AppBannerManagerDesktopBrowserTest
    : public AppBannerManagerBrowserTestBase {
 public:
  AppBannerManagerDesktopBrowserTest() : AppBannerManagerBrowserTestBase() {}

  void SetUp() override {
    TestAppBannerManagerDesktop::SetUp();
    AppBannerManagerBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    // Trigger banners instantly.
    AppBannerSettingsHelper::SetTotalEngagementToTrigger(0);
    chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);

    AppBannerManagerBrowserTestBase::SetUpOnMainThread();
  }

  void TearDown() override {
    chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);
  }

  AppBannerManagerDesktopBrowserTest(
      const AppBannerManagerDesktopBrowserTest&) = delete;
  AppBannerManagerDesktopBrowserTest& operator=(
      const AppBannerManagerDesktopBrowserTest&) = delete;
};

// A dedicated test fixture for DisplayOverride, which is supported
// only for the new web apps mode, and requires a command line switch
// to enable manifest parsing.
class AppBannerManagerDesktopBrowserTest_DisplayOverride
    : public AppBannerManagerDesktopBrowserTest {
 public:
  AppBannerManagerDesktopBrowserTest_DisplayOverride() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kWebAppManifestDisplayOverride);
  }

  AppBannerManagerDesktopBrowserTest_DisplayOverride(
      const AppBannerManagerDesktopBrowserTest_DisplayOverride&) = delete;
  AppBannerManagerDesktopBrowserTest_DisplayOverride& operator=(
      const AppBannerManagerDesktopBrowserTest_DisplayOverride&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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

    ui_test_utils::NavigateToURL(browser(),
                                 GetBannerURLWithAction("stash_event"));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT, manager->state());
  }

  {
    // Trigger the installation prompt and wait for installation to occur.
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());
    ExecuteScript(browser(), "callStashedPrompt();", true /* with_gesture */);
    run_loop.Run();
    EXPECT_EQ(State::COMPLETE, manager->state());
  }

  // Ensure that the userChoice promise resolves.
  const base::string16 title = base::ASCIIToUTF16("Got userChoice: accepted");
  content::TitleWatcher watcher(web_contents, title);
  EXPECT_EQ(title, watcher.WaitAndGetTitle());

  tester.ExpectUniqueSample(kInstallDisplayModeHistogram,
                            blink::mojom::DisplayMode::kStandalone, 1);
}

// TODO(crbug.com/988292): Flakes on most platforms.
IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest,
                       DISABLED_WebAppBannerFiresAppInstalled) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* manager = TestAppBannerManagerDesktop::FromWebContents(web_contents);

  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ui_test_utils::NavigateToURL(
        browser(), GetBannerURLWithAction("verify_appinstalled_stash_event"));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT, manager->state());
  }

  {
    // Trigger the installation prompt and wait for installation to occur.
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    const GURL url = GetBannerURL();
    bool callback_called = false;

    web_app::SetInstalledCallbackForTesting(
        base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                       web_app::InstallResultCode code) {
          EXPECT_EQ(web_app::InstallResultCode::kSuccessNewInstall, code);
          EXPECT_EQ(installed_app_id, web_app::GenerateAppIdFromURL(url));
          callback_called = true;
        }));

    ExecuteScript(browser(), "callStashedPrompt();", true /* with_gesture */);

    run_loop.Run();

    EXPECT_EQ(State::COMPLETE, manager->state());
    EXPECT_TRUE(callback_called);
  }

  // Ensure that the appinstalled event fires.
  const base::string16 title =
      base::ASCIIToUTF16("Got appinstalled: listener, attr");
  content::TitleWatcher watcher(web_contents, title);
  EXPECT_EQ(title, watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest, DestroyWebContents) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* manager = TestAppBannerManagerDesktop::FromWebContents(web_contents);

  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ui_test_utils::NavigateToURL(browser(),
                                 GetBannerURLWithAction("stash_event"));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT, manager->state());
  }

  {
    // Trigger the installation and wait for termination to occur.
    base::RunLoop run_loop;
    bool callback_called = false;

    web_app::SetInstalledCallbackForTesting(
        base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                       web_app::InstallResultCode code) {
          EXPECT_EQ(web_app::InstallResultCode::kWebContentsDestroyed, code);
          callback_called = true;
          run_loop.Quit();
        }));

    ExecuteScript(browser(), "callStashedPrompt();", true /* with_gesture */);

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

    ui_test_utils::NavigateToURL(
        browser(), GetBannerURLWithManifestAndQuery("/banners/minimal-ui.json",
                                                    "action", "stash_event"));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT, manager->state());
  }

  // Install the app via the menu instead of the banner.
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);
  browser()->command_controller()->ExecuteCommand(IDC_INSTALL_PWA);
  manager->AwaitAppInstall();
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);

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

    ui_test_utils::NavigateToURL(
        browser(), GetBannerURLWithManifestAndQuery("/banners/fullscreen.json",
                                                    "action", "stash_event"));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT, manager->state());
  }

  // Install the app via the menu instead of the banner.
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);
  browser()
      ->window()
      ->ExecutePageActionIconForTesting(PageActionIconType::kPwaInstall);
  manager->AwaitAppInstall();
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);

  EXPECT_FALSE(manager->IsPromptAvailableForTesting());

  tester.ExpectUniqueSample(kInstallDisplayModeHistogram,
                            blink::mojom::DisplayMode::kFullscreen, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest,
                       PolicyAppInstalled_NoPrompt) {
  TestAppBannerManagerDesktop* manager =
      TestAppBannerManagerDesktop::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  // Install web app by policy.
  web_app::ExternalInstallOptions options =
      web_app::CreateInstallOptions(GetBannerURL());
  options.install_source = web_app::ExternalInstallSource::kExternalPolicy;
  options.user_display_mode = web_app::DisplayMode::kBrowser;
  web_app::PendingAppManagerInstall(browser()->profile(), options);

  // Run promotability check.
  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ui_test_utils::NavigateToURL(browser(), GetBannerURL());
    run_loop.Run();
    EXPECT_EQ(State::COMPLETE, manager->state());
  }

  EXPECT_EQ(AppBannerManager::InstallableWebAppCheckResult::kNoAlreadyInstalled,
            manager->GetInstallableWebAppCheckResultForTesting());
  EXPECT_FALSE(manager->IsPromptAvailableForTesting());
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
  options.user_display_mode = web_app::DisplayMode::kBrowser;
  web_app::PendingAppManagerInstall(profile, options);

  // Uninstall web app by policy.
  {
    base::RunLoop run_loop;
    web_app::WebAppProviderBase::GetProviderBase(profile)
        ->pending_app_manager()
        .UninstallApps({GetBannerURL()},
                       web_app::ExternalInstallSource::kExternalPolicy,
                       base::BindLambdaForTesting(
                           [&run_loop](const GURL& app_url, bool succeeded) {
                             EXPECT_TRUE(succeeded);
                             run_loop.Quit();
                           }));
    run_loop.Run();
  }

  // Run promotability check.
  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ui_test_utils::NavigateToURL(browser(), GetBannerURL());
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT, manager->state());
  }

  EXPECT_EQ(AppBannerManager::InstallableWebAppCheckResult::kPromotable,
            manager->GetInstallableWebAppCheckResultForTesting());
  EXPECT_TRUE(manager->IsPromptAvailableForTesting());
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest_DisplayOverride,
                       InstallPromptAfterUserMenuInstall) {
  base::HistogramTester tester;

  TestAppBannerManagerDesktop* manager =
      TestAppBannerManagerDesktop::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ui_test_utils::NavigateToURL(
        browser(), GetBannerURLWithManifestAndQuery(
                       "/banners/manifest_display_override.json", "action",
                       "stash_event"));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT, manager->state());
  }

  // Install the app via the menu instead of the banner.
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);
  browser()->command_controller()->ExecuteCommand(IDC_INSTALL_PWA);
  manager->AwaitAppInstall();
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);

  EXPECT_FALSE(manager->IsPromptAvailableForTesting());

  tester.ExpectUniqueSample(kInstallDisplayModeHistogram,
                            blink::mojom::DisplayMode::kMinimalUi, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest_DisplayOverride,
                       WebAppBannerResolvesUserChoice) {
  base::HistogramTester tester;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* manager = TestAppBannerManagerDesktop::FromWebContents(web_contents);

  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ui_test_utils::NavigateToURL(
        browser(),
        GetBannerURLWithManifestAndQuery(
            "/banners/manifest_display_override_display_is_browser.json",
            "action", "stash_event"));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT, manager->state());
  }

  {
    // Trigger the installation prompt and wait for installation to occur.
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());
    ExecuteScript(browser(), "callStashedPrompt();", true /* with_gesture */);
    run_loop.Run();
    EXPECT_EQ(State::COMPLETE, manager->state());
  }

  // Ensure that the userChoice promise resolves.
  const base::string16 title = base::ASCIIToUTF16("Got userChoice: accepted");
  content::TitleWatcher watcher(web_contents, title);
  EXPECT_EQ(title, watcher.WaitAndGetTitle());

  tester.ExpectUniqueSample(kInstallDisplayModeHistogram,
                            blink::mojom::DisplayMode::kStandalone, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest_DisplayOverride,
                       PolicyAppInstalled_NoPrompt) {
  TestAppBannerManagerDesktop* manager =
      TestAppBannerManagerDesktop::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  // Install web app by policy.
  web_app::ExternalInstallOptions options =
      web_app::CreateInstallOptions(GetBannerURLWithManifest(
          "/banners/manifest_display_override_contains_browser.json"));
  options.install_source = web_app::ExternalInstallSource::kExternalPolicy;
  options.user_display_mode = web_app::DisplayMode::kBrowser;
  web_app::PendingAppManagerInstall(browser()->profile(), options);

  // Run promotability check.
  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ui_test_utils::NavigateToURL(browser(), GetBannerURL());
    run_loop.Run();
    EXPECT_EQ(State::COMPLETE, manager->state());
  }

  EXPECT_EQ(AppBannerManager::InstallableWebAppCheckResult::kNoAlreadyInstalled,
            manager->GetInstallableWebAppCheckResultForTesting());
  EXPECT_FALSE(manager->IsPromptAvailableForTesting());
}

}  // namespace banners
