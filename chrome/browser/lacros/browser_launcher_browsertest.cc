// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/lacros/browser_service_lacros.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_test_utils.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/ui/views/session_crashed_bubble_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"

namespace {

webapps::AppId InstallPWA(Profile* profile, const GURL& start_url) {
  auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info->start_url = start_url;
  web_app_info->scope = start_url.GetWithoutFilename();
  web_app_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app_info->title = u"A Web App";
  return web_app::test::InstallWebApp(profile, std::move(web_app_info));
}

}  // namespace

class BrowserLauncherTest : public InProcessBrowserTest {
 public:
  BrowserLauncherTest() {
    // Suppress the test-created about blank tab to be more representative of
    // the startup and launch environment for testing.
    set_open_about_blank_on_browser_launch(false);
  }
  BrowserLauncherTest(const BrowserLauncherTest&) = delete;
  BrowserLauncherTest& operator=(const BrowserLauncherTest&) = delete;
  ~BrowserLauncherTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    // The kNoStartupWindow is applied when launching lacros-chrome with the
    // kDoNotOpenWindow initial browser action.
    command_line->AppendSwitch(switches::kNoStartupWindow);
  }
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    browser_service_ = std::make_unique<BrowserServiceLacros>();
  }
  void TearDownOnMainThread() override {
    if (!skip_uninstall_) {
      UninstallWebApps();
    }
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void NewWindowSync(bool incognito, bool should_trigger_session_restore) {
    base::test::TestFuture<crosapi::mojom::CreationResult> new_window_future;
    browser_service()->NewWindow(
        incognito, should_trigger_session_restore,
        display::Screen::GetScreen()->GetDisplayForNewWindows().id(),
        /*profile_id=*/absl::nullopt, new_window_future.GetCallback());
    ASSERT_TRUE(new_window_future.Wait())
        << "NewWindow did not trigger the callback.";
  }

  void DisableWelcomePages(const std::vector<Profile*>& profiles) {
    for (Profile* profile : profiles)
      profile->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage, true);

    // Also disable What's New.
    PrefService* pref_service = g_browser_process->local_state();
    pref_service->SetInteger(prefs::kLastWhatsNewVersion, CHROME_VERSION_MAJOR);
  }

  // Creates a new profile with a URLS startup preference and an open tab.
  void SetupSingleProfileWithURLSPreference() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();

    ASSERT_TRUE(embedded_test_server()->Start());

    // Create two profiles.
    base::FilePath dest_path = profile_manager->user_data_dir();
    Profile& profile = profiles::testing::CreateProfileSync(
        profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile")));
    DisableWelcomePages({&profile});

    // Don't delete Profile too early.
    ScopedProfileKeepAlive profile_keep_alive(
        &profile, ProfileKeepAliveOrigin::kBrowserWindow);

    // Open some urls in the browser.
    Browser* browser = Browser::Create(
        Browser::CreateParams(Browser::TYPE_NORMAL, &profile, true));
    chrome::NewTab(browser);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser, embedded_test_server()->GetURL("/empty.html")));

    // Establish the startup preference.
    std::vector<GURL> urls;
    urls.push_back(ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(FILE_PATH_LITERAL("title1.html"))));
    urls.push_back(ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(FILE_PATH_LITERAL("title2.html"))));

    // Set different startup preferences for the profile.
    SessionStartupPref pref(SessionStartupPref::URLS);
    pref.urls = urls;
    SessionStartupPref::SetStartupPref(&profile, pref);
    profile.GetPrefs()->CommitPendingWrite();

    // Ensure the session ends with the profile created above.
    auto last_opened_profiles = profile_manager->GetLastOpenedProfiles();
    EXPECT_EQ(1u, last_opened_profiles.size());
    EXPECT_TRUE(base::Contains(last_opened_profiles, &profile));
  }

  BrowserServiceLacros* browser_service() const {
    return browser_service_.get();
  }

  GURL GetWebAppStartUrl() const {
    return GURL("https://lacros.example.com/example/index");
  }

  void SetSkipUninstall(bool value) { skip_uninstall_ = value; }

 private:
  void UninstallWebApps() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    auto* profile = profile_manager->GetProfileByPath(
        profile_manager->GetPrimaryUserProfilePath());
    ASSERT_TRUE(profile);

    web_app::WebAppRegistrar& registrar =
        web_app::WebAppProvider::GetForTest(profile)->registrar_unsafe();
    for (auto& app_id : registrar.GetAppIds()) {
      web_app::test::UninstallWebApp(profile, app_id);
    }
  }

  std::unique_ptr<BrowserServiceLacros> browser_service_;
  bool skip_uninstall_ = false;
};

IN_PROC_BROWSER_TEST_F(BrowserLauncherTest, PRE_FullRestoreWithTwoProfiles) {
  // Simulate a full restore by creating the profiles in a PRE_ test.
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ASSERT_TRUE(embedded_test_server()->Start());

  // Create two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
  Profile& profile2 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
  DisableWelcomePages({&profile1, &profile2});

  // Don't delete Profiles too early.
  ScopedProfileKeepAlive profile1_keep_alive(
      &profile1, ProfileKeepAliveOrigin::kBrowserWindow);
  ScopedProfileKeepAlive profile2_keep_alive(
      &profile2, ProfileKeepAliveOrigin::kBrowserWindow);

  // Open some urls with the browsers, and close them.
  Browser* browser1 = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, &profile1, true));
  chrome::NewTab(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1, embedded_test_server()->GetURL("/empty.html")));

  Browser* browser2 = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, &profile2, true));
  chrome::NewTab(browser2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser2, embedded_test_server()->GetURL("/form.html")));

  // Set different startup preferences for the 2 profiles.
  std::vector<GURL> urls1;
  urls1.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  std::vector<GURL> urls2;
  urls2.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Set different startup preferences for the 2 profiles.
  SessionStartupPref pref1(SessionStartupPref::URLS);
  pref1.urls = urls1;
  SessionStartupPref::SetStartupPref(&profile1, pref1);
  SessionStartupPref pref2(SessionStartupPref::URLS);
  pref2.urls = urls2;
  SessionStartupPref::SetStartupPref(&profile2, pref2);

  profile1.GetPrefs()->CommitPendingWrite();
  profile2.GetPrefs()->CommitPendingWrite();

  // Ensure the session ends with the above two profiles.
  auto last_opened_profiles =
      g_browser_process->profile_manager()->GetLastOpenedProfiles();
  EXPECT_EQ(2u, last_opened_profiles.size());
  EXPECT_TRUE(base::Contains(last_opened_profiles, &profile1));
  EXPECT_TRUE(base::Contains(last_opened_profiles, &profile2));
}

IN_PROC_BROWSER_TEST_F(BrowserLauncherTest, FullRestoreWithTwoProfiles) {
  // Browser launch should be suppressed with the kNoStartupWindow switch.
  ASSERT_FALSE(browser());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Open the two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
  Profile& profile2 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));

  // The profiles to be restored should match those setup in the PRE_ test.
  auto last_opened_profiles =
      g_browser_process->profile_manager()->GetLastOpenedProfiles();
  EXPECT_EQ(2u, last_opened_profiles.size());
  EXPECT_TRUE(base::Contains(last_opened_profiles, &profile1));
  EXPECT_TRUE(base::Contains(last_opened_profiles, &profile2));

  // Trigger Lacros full restore.
  base::test::TestFuture<void> restore_waiter_future;
  testing::SessionsRestoredWaiter restore_waiter(
      restore_waiter_future.GetCallback(), 2);
  browser_service()->OpenForFullRestore(/*skip_crash_restore=*/true);
  ASSERT_TRUE(restore_waiter_future.Wait())
      << "restore_waiter did not trigger the callback.";

  // The startup URLs are ignored, and instead the last open sessions are
  // restored.
  EXPECT_TRUE(profile1.restored_last_session());
  EXPECT_TRUE(profile2.restored_last_session());

  Browser* new_browser = nullptr;
  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile1));
  new_browser = chrome::FindBrowserWithProfile(&profile1);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());

  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile2));
  new_browser = chrome::FindBrowserWithProfile(&profile2);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/form.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());
}

IN_PROC_BROWSER_TEST_F(BrowserLauncherTest,
                       PRE_FullRestoreWillRestoreWebAppsIfPreviouslyOpen) {
  // Browser launch should be suppressed with the kNoStartupWindow switch.
  ASSERT_FALSE(browser());
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(embedded_test_server()->Start());

  Profile& profile = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GetPrimaryUserProfilePath());

  // Don't delete the profile too early.
  ScopedProfileKeepAlive profile_keep_alive(
      &profile, ProfileKeepAliveOrigin::kBrowserWindow);

  // Set the startup pref to restore the last session.
  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(&profile, pref);
  profile.GetPrefs()->CommitPendingWrite();

  // Install and launch a PWA.
  webapps::AppId app_id = InstallPWA(&profile, GetWebAppStartUrl());
  Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(&profile, app_id);
  ASSERT_NE(app_browser, nullptr);
  ASSERT_EQ(app_browser->type(), Browser::Type::TYPE_APP);
  ASSERT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  // Launch a browser.
  Browser* browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, &profile, true));
  chrome::NewTab(browser);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser, embedded_test_server()->GetURL("/empty.html")));

  // Verify the state of the browser's tab strip.
  TabStripModel* tab_strip = browser->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());

  // Ensure the session ends with the profile in last profiles.
  auto last_opened_profiles =
      g_browser_process->profile_manager()->GetLastOpenedProfiles();
  EXPECT_EQ(1u, last_opened_profiles.size());
  EXPECT_TRUE(base::Contains(last_opened_profiles, &profile));

  SetSkipUninstall(true);
}

IN_PROC_BROWSER_TEST_F(BrowserLauncherTest,
                       FullRestoreWillRestoreWebAppsIfPreviouslyOpen) {
  // Browser launch should be suppressed with the kNoStartupWindow switch.
  ASSERT_FALSE(browser());
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(embedded_test_server()->Start());

  Profile& profile = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GetPrimaryUserProfilePath());

  // The profile should match the one set up in the PRE_ test.
  auto last_opened_profiles =
      g_browser_process->profile_manager()->GetLastOpenedProfiles();
  EXPECT_EQ(1u, last_opened_profiles.size());
  EXPECT_TRUE(base::Contains(last_opened_profiles, &profile));

  // Trigger Lacros full restore.
  EXPECT_FALSE(profile.restored_last_session());
  base::test::TestFuture<void> restore_waiter_future;
  testing::SessionsRestoredWaiter restore_waiter(
      restore_waiter_future.GetCallback(), 1);
  browser_service()->OpenForFullRestore(/*skip_crash_restore=*/true);
  ASSERT_TRUE(restore_waiter_future.Wait())
      << "restore_waiter did not trigger the callback.";

  // The last session should be logged as restored.
  EXPECT_TRUE(profile.restored_last_session());

  // A tabbed browser and app browser should have been restored.
  ASSERT_EQ(2u, chrome::GetBrowserCount(&profile));
  Browser* tabbed_browser =
      chrome::FindAllTabbedBrowsersWithProfile(&profile)[0];
  TabStripModel* tab_strip = tabbed_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());

  Browser* app_browser = nullptr;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->type() != Browser::Type::TYPE_APP)
      continue;
    EXPECT_FALSE(app_browser);
    app_browser = browser;
  }
  ASSERT_TRUE(app_browser);
}

// Lacros Apps should only be restored when launching for full restore. Ensure
// Apps are not restored when performing a non-full-restore session restore for
// the browser (i.e. if all lacros windows are closed and a browser window is
// later re-opened we should trigger a session restore, but not restore any
// previously open app windows).
IN_PROC_BROWSER_TEST_F(
    BrowserLauncherTest,
    SessionRestoreDoesNotTriggerAppRestoreWhenOpeningNewWindows) {
  // Browser launch should be suppressed with the kNoStartupWindow switch.
  ASSERT_FALSE(browser());
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(embedded_test_server()->Start());

  Profile& profile = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GetPrimaryUserProfilePath());

  // Keep the browser process running while the browsers are closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive profile_keep_alive(
      &profile, ProfileKeepAliveOrigin::kBrowserWindow);

  // Install and launch a PWA.
  webapps::AppId app_id = InstallPWA(&profile, GetWebAppStartUrl());
  Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(&profile, app_id);
  ASSERT_NE(app_browser, nullptr);
  ASSERT_EQ(app_browser->type(), Browser::Type::TYPE_APP);
  ASSERT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  // Launch a browser.
  Browser* browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, &profile, true));
  chrome::NewTab(browser);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser, embedded_test_server()->GetURL("/empty.html")));

  // Verify the state of the browser's tab strip.
  TabStripModel* tab_strip = browser->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());

  // Close all browser windows and wait for the operation to complete.
  size_t browser_count = chrome::GetTotalBrowserCount();
  CloseAllBrowsers();
  for (size_t i = 0; i < browser_count; ++i)
    ui_test_utils::WaitForBrowserToClose();
  ASSERT_EQ(0u, BrowserList::GetInstance()->size());

  // Trigger a new window with session restore.
  base::test::TestFuture<void> restore_waiter_future;
  testing::SessionsRestoredWaiter restore_waiter(
      restore_waiter_future.GetCallback(), 1);
  NewWindowSync(/*incognito=*/false, /*should_trigger_session_restore=*/true);
  ASSERT_TRUE(restore_waiter_future.Wait())
      << "restore_waiter did not trigger the callback.";

  // The browser window should be restored but app browser should not.
  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile));
  browser = chrome::FindAllTabbedBrowsersWithProfile(&profile)[0];
  tab_strip = browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());
}

IN_PROC_BROWSER_TEST_F(BrowserLauncherTest,
                       PRE_FullRestoreDoNotSkipCrashRestore) {
  // Simulate a full restore by setting up a profile in a PRE_ test.
  SetupSingleProfileWithURLSPreference();
}

IN_PROC_BROWSER_TEST_F(BrowserLauncherTest, FullRestoreDoNotSkipCrashRestore) {
  // Browser launch should be suppressed with the kNoStartupWindow switch.
  ASSERT_FALSE(browser());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Open the profile.
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile& profile = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile")));

  // The profile to be restored should match the one in the PRE_ test.
  auto last_opened_profiles =
      g_browser_process->profile_manager()->GetLastOpenedProfiles();
  EXPECT_EQ(1u, last_opened_profiles.size());
  EXPECT_TRUE(base::Contains(last_opened_profiles, &profile));

  // Disable the profile picker and set the exit type to crashed.
  g_browser_process->local_state()->SetInteger(
      prefs::kBrowserProfilePickerAvailabilityOnStartup,
      static_cast<int>(ProfilePicker::AvailabilityOnStartup::kDisabled));
  ExitTypeService::GetInstanceForProfile(&profile)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);

  // Trigger Lacros full restore but do not skip crash restore prompts.
  browser_service()->OpenForFullRestore(/*skip_crash_restore=*/false);

  // The browser should not be restored but instead we should have the browser's
  // crash restore bubble prompt.
  Browser* new_browser = nullptr;
  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile));
  new_browser = chrome::FindBrowserWithProfile(&profile);
  ASSERT_TRUE(new_browser);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(content::WaitForLoadStop(tab_strip->GetWebContentsAt(0)));

  views::BubbleDialogDelegate* crash_bubble_delegate =
      SessionCrashedBubbleView::GetInstanceForTest();
  EXPECT_TRUE(crash_bubble_delegate);
}

IN_PROC_BROWSER_TEST_F(BrowserLauncherTest, PRE_FullRestoreSkipCrashRestore) {
  // Simulate a full restore by setting up a profile in a PRE_ test.
  SetupSingleProfileWithURLSPreference();
}

IN_PROC_BROWSER_TEST_F(BrowserLauncherTest, FullRestoreSkipCrashRestore) {
  // Browser launch should be suppressed with the kNoStartupWindow switch.
  ASSERT_FALSE(browser());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Open the profile.
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile& profile = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile")));

  // The profile to be restored should match the one in the PRE_ test.
  auto last_opened_profiles =
      g_browser_process->profile_manager()->GetLastOpenedProfiles();
  EXPECT_EQ(1u, last_opened_profiles.size());
  EXPECT_TRUE(base::Contains(last_opened_profiles, &profile));

  // Disable the profile picker and set the exit type to crashed.
  g_browser_process->local_state()->SetInteger(
      prefs::kBrowserProfilePickerAvailabilityOnStartup,
      static_cast<int>(ProfilePicker::AvailabilityOnStartup::kDisabled));
  ExitTypeService::GetInstanceForProfile(&profile)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);

  // Trigger Lacros full restore and skip crash restore prompts.
  base::test::TestFuture<void> restore_waiter_future;
  testing::SessionsRestoredWaiter restore_waiter(
      restore_waiter_future.GetCallback(), 1);
  browser_service()->OpenForFullRestore(/*skip_crash_restore=*/true);
  ASSERT_TRUE(restore_waiter_future.Wait())
      << "restore_waiter did not trigger the callback.";

  // The browser should be restored (ignoring startup preference).
  Browser* new_browser = nullptr;
  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile));
  new_browser = chrome::FindBrowserWithProfile(&profile);
  ASSERT_TRUE(new_browser);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());
}

IN_PROC_BROWSER_TEST_F(BrowserLauncherTest,
                       PRE_OpenTwoWindowsAfterCrashWithTwoProfiles) {
  // Simulate a full restore by creating the profiles in a PRE_ test.
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ASSERT_TRUE(embedded_test_server()->Start());

  // Load the main profile and create one additional profile.
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GetPrimaryUserProfilePath());
  Profile& profile2 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
  DisableWelcomePages({&profile1, &profile2});

  // Don't delete Profiles too early.
  ScopedProfileKeepAlive profile1_keep_alive(
      &profile1, ProfileKeepAliveOrigin::kBrowserWindow);
  ScopedProfileKeepAlive profile2_keep_alive(
      &profile2, ProfileKeepAliveOrigin::kBrowserWindow);

  // Open some urls with the browsers, and close them.
  Browser* browser1 = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, &profile1, true));
  chrome::NewTab(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1, embedded_test_server()->GetURL("/empty.html")));

  Browser* browser2 = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, &profile2, true));
  chrome::NewTab(browser2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser2, embedded_test_server()->GetURL("/form.html")));

  // Set different startup preferences for the 2 profiles.
  std::vector<GURL> urls1;
  urls1.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  std::vector<GURL> urls2;
  urls2.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Set different startup preferences for the 2 profiles.
  SessionStartupPref pref1(SessionStartupPref::URLS);
  pref1.urls = urls1;
  SessionStartupPref::SetStartupPref(&profile1, pref1);
  SessionStartupPref pref2(SessionStartupPref::URLS);
  pref2.urls = urls2;
  SessionStartupPref::SetStartupPref(&profile2, pref2);

  profile1.GetPrefs()->CommitPendingWrite();
  profile2.GetPrefs()->CommitPendingWrite();

  // Ensure the session ends with the above two profiles.
  auto last_opened_profiles =
      g_browser_process->profile_manager()->GetLastOpenedProfiles();
  EXPECT_EQ(2u, last_opened_profiles.size());
  EXPECT_TRUE(base::Contains(last_opened_profiles, &profile1));
  EXPECT_TRUE(base::Contains(last_opened_profiles, &profile2));
}

IN_PROC_BROWSER_TEST_F(BrowserLauncherTest,
                       OpenTwoWindowsAfterCrashWithTwoProfiles) {
  // Browser launch should be suppressed with the kNoStartupWindow switch.
  ASSERT_FALSE(browser());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Open the two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GetPrimaryUserProfilePath());
  Profile& profile2 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));

  // The profiles to be restored should match those setup in the PRE_ test.
  auto last_opened_profiles =
      g_browser_process->profile_manager()->GetLastOpenedProfiles();
  EXPECT_EQ(2u, last_opened_profiles.size());
  EXPECT_TRUE(base::Contains(last_opened_profiles, &profile1));
  EXPECT_TRUE(base::Contains(last_opened_profiles, &profile2));

  // Disable the profile picker and set the exit type to crashed.
  g_browser_process->local_state()->SetInteger(
      prefs::kBrowserProfilePickerAvailabilityOnStartup,
      static_cast<int>(ProfilePicker::AvailabilityOnStartup::kDisabled));
  ExitTypeService::GetInstanceForProfile(&profile1)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);
  ExitTypeService::GetInstanceForProfile(&profile2)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);

  // Launch the browser.
  base::test::TestFuture<crosapi::mojom::CreationResult> launch_future;
  browser_service()->Launch(0, /*profile_id=*/absl::nullopt,
                            launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Wait()) << "Launch did not trigger the callback.";

  // Make sure 2 windows are preserved, one for each profile.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1u, chrome::GetBrowserCount(&profile1));
  EXPECT_EQ(1u, chrome::GetBrowserCount(&profile2));
}

// Tests that if the previous session ended as the result of a crash, and a
// Chrome app is opened before the browser, the browser will still open for the
// last session's profiles.
// The test runs the following sequence.
//   1. Setup the previous session with two browsers of different profiles.
//   2. Simulate a crash for this session.
//   3. Start a new session, lacros starts in its windowless state.
//   4. Launch an app.
//   5. Attempt to perform a Launch action for the browser. This will attempt to
//      launch lacros for the last opened profiles.
// Even if the user opens the app browser before opening lacros, the last
// session profile windows should still open as expected.
// This is a regression test for crbug.com/1455065.
IN_PROC_BROWSER_TEST_F(BrowserLauncherTest,
                       PRE_LaunchForLastProfilesPostCrashAppOpenedFirst) {
  // Simulate a full restore by creating the profiles in a PRE_ test.
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ASSERT_TRUE(embedded_test_server()->Start());

  // Load the main profile and create one additional profile.
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GetPrimaryUserProfilePath());
  Profile& profile2 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
  DisableWelcomePages({&profile1, &profile2});

  // Don't delete Profiles too early.
  ScopedProfileKeepAlive profile1_keep_alive(
      &profile1, ProfileKeepAliveOrigin::kBrowserWindow);
  ScopedProfileKeepAlive profile2_keep_alive(
      &profile2, ProfileKeepAliveOrigin::kBrowserWindow);

  // Create a browser for each profile and navigate the tab to a unique URL.
  Browser* browser1 = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, &profile1, true));
  chrome::NewTab(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1, embedded_test_server()->GetURL("/empty.html")));

  Browser* browser2 = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, &profile2, true));
  chrome::NewTab(browser2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser2, embedded_test_server()->GetURL("/form.html")));

  // Set different startup preferences for the 2 profiles. Despite these
  // differences the browsers for each profile should relaunch as expected with
  // the crash recovery flow.
  std::vector<GURL> urls1;
  urls1.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  std::vector<GURL> urls2;
  urls2.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html"))));

  SessionStartupPref pref1(SessionStartupPref::URLS);
  pref1.urls = urls1;
  SessionStartupPref::SetStartupPref(&profile1, pref1);
  SessionStartupPref pref2(SessionStartupPref::URLS);
  pref2.urls = urls2;
  SessionStartupPref::SetStartupPref(&profile2, pref2);

  profile1.GetPrefs()->CommitPendingWrite();
  profile2.GetPrefs()->CommitPendingWrite();

  // Ensure the session ends with the above two profiles.
  auto last_opened_profiles =
      g_browser_process->profile_manager()->GetLastOpenedProfiles();
  EXPECT_EQ(2u, last_opened_profiles.size());
  EXPECT_TRUE(base::Contains(last_opened_profiles, &profile1));
  EXPECT_TRUE(base::Contains(last_opened_profiles, &profile2));
}

IN_PROC_BROWSER_TEST_F(BrowserLauncherTest,
                       LaunchForLastProfilesPostCrashAppOpenedFirst) {
  // Browser launch should be suppressed with the kNoStartupWindow switch.
  ASSERT_FALSE(browser());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Re-recreate the last two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GetPrimaryUserProfilePath());
  Profile& profile2 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));

  // The profiles to be restored should match those setup in the PRE_ test.
  auto last_opened_profiles =
      g_browser_process->profile_manager()->GetLastOpenedProfiles();
  EXPECT_EQ(2u, last_opened_profiles.size());
  EXPECT_TRUE(base::Contains(last_opened_profiles, &profile1));
  EXPECT_TRUE(base::Contains(last_opened_profiles, &profile2));

  // Disable the profile picker and set the exit type to crashed.
  g_browser_process->local_state()->SetInteger(
      prefs::kBrowserProfilePickerAvailabilityOnStartup,
      static_cast<int>(ProfilePicker::AvailabilityOnStartup::kDisabled));
  ExitTypeService::GetInstanceForProfile(&profile1)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);
  ExitTypeService::GetInstanceForProfile(&profile2)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);

  // First launch the app. Only the app browser should exist.
  webapps::AppId app_id = InstallPWA(&profile1, GetWebAppStartUrl());
  Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(&profile1, app_id);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  ASSERT_NE(app_browser, nullptr);
  ASSERT_EQ(app_browser->type(), Browser::Type::TYPE_APP);
  ASSERT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  // Launch the browser. A browser window for each last profile should be
  // restored.
  base::test::TestFuture<crosapi::mojom::CreationResult> launch_future;
  browser_service()->Launch(0, /*profile_id=*/absl::nullopt,
                            launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Wait()) << "Launch did not trigger the callback.";

  // Make sure 2 windows are preserved, one for each profile.
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2u, chrome::GetBrowserCount(&profile1));
  EXPECT_EQ(1u, chrome::GetBrowserCount(&profile2));
}
