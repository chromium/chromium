// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/web_app_id_constants.h"
#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/password_manager/web_app_profile_switcher.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestWebUIManifestId[] = "chrome://password-manager/";
const char kTestWebUIAppURL[] = "chrome://password-manager/?source=pwa";

std::unique_ptr<web_app::WebAppInstallInfo> GetTestWebAppInstallInfo() {
  auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>(
      GURL(kTestWebUIManifestId), GURL(kTestWebUIAppURL));
  web_app_info->title = u"Test app";
  return web_app_info;
}

webapps::AppId GetTestWebAppId() {
  return web_app::GenerateAppIdFromManifestId(GURL(kTestWebUIManifestId));
}

Profile* CreateAdditionalProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  size_t starting_number_of_profiles = profile_manager->GetNumberOfProfiles();

  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  Profile& profile =
      profiles::testing::CreateProfileSync(profile_manager, new_path);
  EXPECT_EQ(starting_number_of_profiles + 1,
            profile_manager->GetNumberOfProfiles());
  web_app::test::WaitUntilWebAppProviderAndSubsystemsReady(
      web_app::WebAppProvider::GetForTest(&profile));
  return &profile;
}

void InstallAppForProfile(
    Profile* profile,
    std::unique_ptr<web_app::WebAppInstallInfo> app_info) {
  GURL app_url(app_info->start_url());
  web_app::test::InstallWebApp(profile, std::move(app_info));
  ASSERT_TRUE(web_app::FindInstalledAppWithUrlInScope(profile, app_url));
}

}  // namespace

class WebAppProfileSwitcherBrowserTest : public web_app::WebAppBrowserTestBase {
};

IN_PROC_BROWSER_TEST_F(WebAppProfileSwitcherBrowserTest,
                       SwitchWebAppProfileRequiresInstall) {
  Profile* first_profile = profile();
  InstallAppForProfile(first_profile, GetTestWebAppInstallInfo());

  // Create a second profile.
  Profile* second_profile = CreateAdditionalProfile();
  ASSERT_FALSE(chrome::FindBrowserWithProfile(second_profile));
  // Confirm that the profile has no installed app.
  ASSERT_FALSE(web_app::FindInstalledAppWithUrlInScope(second_profile,
                                                       GURL(kTestWebUIAppURL)));

  // Verify that the app is installed and launched.
  ui_test_utils::AllBrowserTabAddedWaiter waiter;
  base::test::TestFuture<void> profile_switch_complete;
  WebAppProfileSwitcher profile_switcher(GetTestWebAppId(), *first_profile,
                                         profile_switch_complete.GetCallback());
  profile_switcher.SwitchToProfile(second_profile->GetPath());

  content::WebContents* new_web_contents = waiter.Wait();
  ASSERT_TRUE(new_web_contents);
  EXPECT_EQ(new_web_contents->GetVisibleURL(), GURL(kTestWebUIAppURL));

  // Check that the new WebContents belong to the second profile.
  Browser* new_browser = chrome::FindBrowserWithProfile(second_profile);
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(new_browser->tab_strip_model()->GetActiveWebContents(),
            new_web_contents);

  std::optional<webapps::AppId> app_id =
      web_app::FindInstalledAppWithUrlInScope(second_profile,
                                              GURL(kTestWebUIAppURL));
  ASSERT_TRUE(app_id);
  EXPECT_TRUE(web_app::AppBrowserController::IsWebApp(new_browser));
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForTest(second_profile);
  EXPECT_EQ(provider->registrar_unsafe().GetAppUserDisplayMode(app_id.value()),
            web_app::mojom::UserDisplayMode::kStandalone);

  EXPECT_TRUE(profile_switch_complete.Wait());
}

IN_PROC_BROWSER_TEST_F(WebAppProfileSwitcherBrowserTest,
                       SwitchWebAppProfileLaunchOnly) {
  Profile* first_profile = profile();
  InstallAppForProfile(first_profile, GetTestWebAppInstallInfo());

  // Create a second profile and install the app for it.
  Profile* second_profile = CreateAdditionalProfile();
  InstallAppForProfile(second_profile, GetTestWebAppInstallInfo());
  ASSERT_FALSE(chrome::FindBrowserWithProfile(second_profile));

  // Verify that the app is launched for the second profile.
  ui_test_utils::AllBrowserTabAddedWaiter waiter;
  base::test::TestFuture<void> profile_switch_complete;
  WebAppProfileSwitcher profile_switcher(GetTestWebAppId(), *first_profile,
                                         profile_switch_complete.GetCallback());
  profile_switcher.SwitchToProfile(second_profile->GetPath());

  Browser* new_browser = ui_test_utils::WaitForBrowserToOpen();

  // Check that the new Browser belong to the second profile and Password
  // Manager is opened.
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(chrome::FindBrowserWithProfile(second_profile), new_browser);
  EXPECT_EQ(
      GURL(kTestWebUIAppURL),
      new_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());

  EXPECT_TRUE(profile_switch_complete.Wait());
}

IN_PROC_BROWSER_TEST_F(WebAppProfileSwitcherBrowserTest,
                       SwitchWebAppProfileActivateWindowOnly) {
  base::HistogramTester histogram_tester;
  Profile* first_profile = profile();
  InstallAppForProfile(first_profile, GetTestWebAppInstallInfo());

  // Launch the app for the first profile.
  web_app::LaunchWebAppBrowser(first_profile, web_app::kPasswordManagerAppId);
  Browser* first_profile_app_browser =
      web_app::AppBrowserController::FindForWebApp(
          *first_profile, web_app::kPasswordManagerAppId);
  ASSERT_TRUE(first_profile_app_browser);
  ASSERT_EQ(chrome::FindAllTabbedBrowsersWithProfile(first_profile).size(), 1U);

  // Create a second profile and install the app for it.
  Profile* second_profile = CreateAdditionalProfile();
  InstallAppForProfile(second_profile, GetTestWebAppInstallInfo());
  // Launch the app.
  web_app::LaunchWebAppBrowser(second_profile, web_app::kPasswordManagerAppId);
  Browser* second_profile_app_browser =
      web_app::AppBrowserController::FindForWebApp(
          *second_profile, web_app::kPasswordManagerAppId);
  ASSERT_TRUE(second_profile_app_browser);
  EXPECT_EQ(chrome::FindLastActive(), second_profile_app_browser);

  // Switch to the first profile from the second.
  base::test::TestFuture<void> profile_switch_complete;
  WebAppProfileSwitcher profile_switcher(web_app::kPasswordManagerAppId,
                                         *second_profile,
                                         profile_switch_complete.GetCallback());
  profile_switcher.SwitchToProfile(first_profile->GetPath());
  ui_test_utils::BrowserActivationWaiter(first_profile_app_browser)
      .WaitForActivation();
  EXPECT_TRUE(profile_switch_complete.Wait());

  // Check that there is only one browser for the first_profile and it's active.
  ASSERT_EQ(chrome::FindAllTabbedBrowsersWithProfile(first_profile).size(), 1U);
  EXPECT_EQ(chrome::FindBrowserWithActiveWindow(), first_profile_app_browser);

  EXPECT_THAT(histogram_tester.GetAllSamples("PasswordManager.ShortcutMetric"),
              base::BucketsAre(base::Bucket(2, 1)));
}
