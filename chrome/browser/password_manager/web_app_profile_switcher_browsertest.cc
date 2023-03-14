// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/web_app_profile_switcher.h"

#include "base/files/file_path.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

namespace {

const char kTestWebUIAppURL[] = "chrome://password-manager/?source=pwa";

std::unique_ptr<WebAppInstallInfo> CreateTestWebAppInstallInfo() {
  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->start_url = GURL(kTestWebUIAppURL);
  web_app_info->title = u"Test app";
  web_app_info->manifest_id = "";
  return web_app_info;
}

Profile* CreateAdditionalProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  size_t starting_number_of_profiles = profile_manager->GetNumberOfProfiles();

  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  Profile* profile =
      profiles::testing::CreateProfileSync(profile_manager, new_path);
  EXPECT_EQ(starting_number_of_profiles + 1,
            profile_manager->GetNumberOfProfiles());
  web_app::test::WaitUntilWebAppProviderAndSubsystemsReady(
      web_app::WebAppProvider::GetForTest(profile));
  return profile;
}

}  // namespace

class WebAppProfileSwitcherBrowserTest
    : public web_app::WebAppControllerBrowserTest {
 private:
  // TODO(https://github.com/llvm/llvm-project/issues/61334): Explicit
  // [[maybe_unused]] attribute shouuld not be necessary here.
  [[maybe_unused]] base::ScopedAllowBlockingForTesting allow_blocking_;
};

IN_PROC_BROWSER_TEST_F(WebAppProfileSwitcherBrowserTest,
                       SwitchWebAppProfileRequiresInstall) {
  Profile* first_profile = profile();

  // Install WebApp for the first profile.
  auto web_app_info = CreateTestWebAppInstallInfo();
  web_app::AppId app_id = web_app::GenerateAppId(web_app_info->manifest_id,
                                                 web_app_info->start_url);
  web_app::test::InstallWebApp(first_profile, std::move(web_app_info));
  ASSERT_TRUE(web_app::FindInstalledAppWithUrlInScope(first_profile,
                                                      GURL(kTestWebUIAppURL)));

  // Create a second profile.
  Profile* second_profile = CreateAdditionalProfile();
  ASSERT_FALSE(chrome::FindBrowserWithProfile(second_profile));
  // Confirm that the profile has no installed app.
  ASSERT_FALSE(web_app::FindInstalledAppWithUrlInScope(second_profile,
                                                       GURL(kTestWebUIAppURL)));

  // Verify that the app is installed and launched.
  ui_test_utils::AllBrowserTabAddedWaiter waiter;
  base::test::TestFuture<void> profile_switch_complete;
  WebAppProfileSwitcher profile_switcher(app_id, *first_profile,
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

  ASSERT_TRUE(web_app::FindInstalledAppWithUrlInScope(second_profile,
                                                      GURL(kTestWebUIAppURL)));
  EXPECT_TRUE(profile_switch_complete.Wait());
}

IN_PROC_BROWSER_TEST_F(WebAppProfileSwitcherBrowserTest,
                       SwitchWebAppProfileLaunchOnly) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  Profile* first_profile = profile();

  auto web_app_info = CreateTestWebAppInstallInfo();
  web_app::AppId app_id = web_app::GenerateAppId(web_app_info->manifest_id,
                                                 web_app_info->start_url);
  // Install web app.
  web_app::test::InstallWebApp(first_profile, std::move(web_app_info));
  ASSERT_TRUE(web_app::FindInstalledAppWithUrlInScope(first_profile,
                                                      GURL(kTestWebUIAppURL)));

  // Create a second profile and install the app for it.
  Profile* second_profile = CreateAdditionalProfile();
  auto web_app_info_copy = CreateTestWebAppInstallInfo();
  web_app::test::InstallWebApp(second_profile, std::move(web_app_info_copy));
  ASSERT_TRUE(web_app::FindInstalledAppWithUrlInScope(second_profile,
                                                      GURL(kTestWebUIAppURL)));
  ASSERT_FALSE(chrome::FindBrowserWithProfile(second_profile));

  // Verify that the app is launched for the second profile.
  ui_test_utils::AllBrowserTabAddedWaiter waiter;
  base::test::TestFuture<void> profile_switch_complete;
  WebAppProfileSwitcher profile_switcher(app_id, *first_profile,
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

  EXPECT_TRUE(profile_switch_complete.Wait());
}
