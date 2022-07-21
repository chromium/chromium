// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/app_mode/app_session.h"
#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"
#include "chrome/browser/lacros/browser_service_lacros.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_test_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/profile_ui_test_utils.h"
#include "chrome/browser/ui/startup/lacros_first_run_service.h"
#include "chrome/browser/ui/views/session_crashed_bubble_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

using crosapi::mojom::BrowserInitParams;
using crosapi::mojom::BrowserInitParamsPtr;
using crosapi::mojom::CreationResult;
using crosapi::mojom::SessionType;

namespace {
constexpr char kNavigationUrl[] = "https://www.google.com/";
}  // namespace

class BrowserServiceLacrosBrowserTest : public InProcessBrowserTest {
 public:
  BrowserServiceLacrosBrowserTest() = default;
  BrowserServiceLacrosBrowserTest(const BrowserServiceLacrosBrowserTest&) =
      delete;
  BrowserServiceLacrosBrowserTest& operator=(
      const BrowserServiceLacrosBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    browser_service_ = std::make_unique<BrowserServiceLacros>();
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetSessionType(SessionType type) {
    BrowserInitParamsPtr init_params = BrowserInitParams::New();
    init_params->session_type = type;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
  }

  void CreateFullscreenWindow() {
    bool use_callback = false;
    browser_service()->NewFullscreenWindow(
        GURL(kNavigationUrl),
        base::BindLambdaForTesting([&](CreationResult result) {
          use_callback = true;
          EXPECT_EQ(result, CreationResult::kSuccess);
        }));
    EXPECT_TRUE(use_callback);

    // Verify `AppSession` object is created when `NewFullscreenWindow` is
    // called in the Web Kiosk session. Then, disable the `AttemptUserExit`
    // method to do nothing.
    if (chromeos::BrowserParamsProxy::Get()->SessionType() ==
        SessionType::kWebKioskSession) {
      chromeos::AppSession* app_session =
          KioskSessionServiceLacros::Get()->GetAppSessionForTesting();
      EXPECT_TRUE(app_session);
      app_session->SetAttemptUserExitForTesting(base::DoNothing());
    }
  }

  void CreateNewWindow() {
    Browser::Create(Browser::CreateParams(browser()->profile(), false));
  }

  void VerifyFullscreenWindow() {
    // Verify the browser status.
    Browser* browser = BrowserList::GetInstance()->GetLastActive();
    EXPECT_EQ(browser->initial_show_state(), ui::SHOW_STATE_FULLSCREEN);
    EXPECT_TRUE(browser->is_trusted_source());
    EXPECT_TRUE(browser->window()->IsFullscreen());
    EXPECT_TRUE(browser->window()->IsVisible());

    // Verify the web content.
    content::WebContents* web_content =
        browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(web_content->GetVisibleURL(), kNavigationUrl);
  }

  void NewWindowSync(bool incognito, bool should_trigger_session_restore) {
    base::RunLoop run_loop;
    browser_service()->NewWindow(incognito, should_trigger_session_restore,
                                 run_loop.QuitClosure());
    run_loop.Run();
  }

  void NewTabSync(bool should_trigger_session_restore) {
    base::RunLoop run_loop;
    browser_service()->NewTab(should_trigger_session_restore,
                              run_loop.QuitClosure());
    run_loop.Run();
  }

  BrowserServiceLacros* browser_service() const {
    return browser_service_.get();
  }

 private:
  std::unique_ptr<BrowserServiceLacros> browser_service_;
};

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosBrowserTest, NewFullscreenWindow) {
  CreateFullscreenWindow();
  VerifyFullscreenWindow();
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosBrowserTest,
                       BlockAdditionalWindowsInWebKiosk) {
  SetSessionType(SessionType::kWebKioskSession);
  CreateFullscreenWindow();

  // The new window should be blocked in the web Kiosk session.
  const size_t browser_count = BrowserList::GetInstance()->size();
  CreateNewWindow();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(BrowserList::GetInstance()->size(), browser_count);
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosBrowserTest,
                       AllowAdditionalWindowsInRegularSession) {
  SetSessionType(SessionType::kRegularSession);
  CreateFullscreenWindow();

  // The new window should be allowed in the regular session.
  const size_t browser_count = BrowserList::GetInstance()->size();
  CreateNewWindow();
  EXPECT_EQ(BrowserList::GetInstance()->size(), browser_count + 1);
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosBrowserTest,
                       NewWindow_OpensProfilePicker) {
  // Keep the browser process running during the test while the browser is
  // closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Start in a state with no browser windows opened.
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());

  // `NewWindow()` should create a new window if the system has only one
  // profile.
  NewWindowSync(/*incognito=*/false, /*should_trigger_session_restore=*/false);
  EXPECT_FALSE(ProfilePicker::IsOpen());
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Create an additional profile.
  base::FilePath path_profile2 =
      profile_manager->user_data_dir().Append(FILE_PATH_LITERAL("Profile 2"));
  Profile* profile2 =
      profiles::testing::CreateProfileSync(profile_manager, path_profile2);
  // Open a browser window to make it the last used profile.
  chrome::NewEmptyWindow(profile2);
  Browser* browser2 = ui_test_utils::WaitForBrowserToOpen();
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  // Profile picker does _not_ open for incognito windows. Instead, the
  // incognito window for the main profile is directly opened.
  NewWindowSync(/*incognito=*/true, /*should_trigger_session_restore=*/false);
  EXPECT_FALSE(ProfilePicker::IsOpen());
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  Profile* profile = BrowserList::GetInstance()->GetLastActive()->profile();
  // Main profile should be always used.
  EXPECT_EQ(profile->GetPath(), profile_manager->GetPrimaryUserProfilePath());
  EXPECT_TRUE(profile->IsOffTheRecord());

  BrowserList::SetLastActive(browser2);
  // Profile picker does _not_ open if Chrome already has opened windows.
  // Instead, a new browser window for the main profile is directly opened.
  NewWindowSync(/*incognito=*/false, /*should_trigger_session_restore=*/false);
  EXPECT_FALSE(ProfilePicker::IsOpen());
  // A new browser is created for the main profile.
  EXPECT_EQ(BrowserList::GetInstance()->GetLastActive()->profile()->GetPath(),
            profile_manager->GetPrimaryUserProfilePath());
  EXPECT_EQ(4u, chrome::GetTotalBrowserCount());

  size_t browser_count = chrome::GetTotalBrowserCount();
  chrome::CloseAllBrowsers();
  for (size_t i = 0; i < browser_count; ++i)
    ui_test_utils::WaitForBrowserToClose();

  // `NewWindow()` should open the profile picker.
  NewWindowSync(/*incognito=*/false, /*should_trigger_session_restore=*/false);
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosBrowserTest,
                       NewTab_OpensProfilePicker_SingleProfile) {
  // Keep the browser process running during the test while the browser is
  // closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  // Start in a state with no browser windows opened.
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());

  // `NewTab()` should create a new window if the system has only one
  // profile.
  NewTabSync(/*should_trigger_session_restore=*/true);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_FALSE(ProfilePicker::IsOpen());
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  auto* main_profile = profile_manager->GetProfileByPath(
      profile_manager->GetPrimaryUserProfilePath());
  auto* browser = chrome::FindBrowserWithProfile(main_profile);
  auto* tab_strip = browser->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());

  // Consequent `NewTab()` should add a new tab to an existing browser.
  NewTabSync(/*should_trigger_session_restore=*/true);
  EXPECT_EQ(2, tab_strip->count());
  EXPECT_FALSE(ProfilePicker::IsOpen());
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosBrowserTest,
                       NewTab_OpensProfilePicker_MultiProfile) {
  // Keep the browser process running during the test while the browser is
  // closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);

  // Create and open an additional profile to move Chrome to the multi-profile
  // mode.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile2_path =
      profile_manager->user_data_dir().Append(FILE_PATH_LITERAL("Profile 2"));
  Profile* profile2 =
      profiles::testing::CreateProfileSync(profile_manager, profile2_path);
  chrome::NewEmptyWindow(profile2);
  ui_test_utils::WaitForBrowserToOpen();
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  auto* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());

  // `NewTab()` should add a tab to the main profile window;
  NewTabSync(/*should_trigger_session_restore=*/true);
  EXPECT_EQ(2, tab_strip->count());

  chrome::CloseAllBrowsers();
  // Wait for two browsers to be closed.
  ui_test_utils::WaitForBrowserToClose();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());

  // `NewTab()` should open the profile picker.
  NewTabSync(/*should_trigger_session_restore=*/true);
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

// Tests for lacros-chrome that require lacros starting in its windowless
// background state.
class BrowserServiceLacrosWindowlessBrowserTest
    : public BrowserServiceLacrosBrowserTest {
 public:
  // BrowserServiceLacrosBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    // The kNoStartupWindow is applied when launching lacros-chrome with the
    // kDoNotOpenWindow initial browser action.
    command_line->AppendSwitch(switches::kNoStartupWindow);
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

    Profile* profile = nullptr;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      profile = profile_manager->GetProfile(
          dest_path.Append(FILE_PATH_LITERAL("New Profile")));
      ASSERT_TRUE(profile);
    }
    DisableWelcomePages({profile});

    // Don't delete Profile too early.
    ScopedProfileKeepAlive profile_keep_alive(
        profile, ProfileKeepAliveOrigin::kBrowserWindow);

    // Open some urls in the browser.
    Browser* browser = Browser::Create(
        Browser::CreateParams(Browser::TYPE_NORMAL, profile, true));
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
    SessionStartupPref::SetStartupPref(profile, pref);
    profile->GetPrefs()->CommitPendingWrite();

    // Ensure the session ends with the profile created above.
    auto last_opened_profiles =
        g_browser_process->profile_manager()->GetLastOpenedProfiles();
    EXPECT_EQ(1u, last_opened_profiles.size());
    EXPECT_TRUE(base::Contains(last_opened_profiles, profile));
  }
};

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosWindowlessBrowserTest,
                       HandlesUncleanExit) {
  // Browser launch should be suppressed with the kNoStartupWindow switch.
  ASSERT_FALSE(browser());

  // Ensure we have an active profile for this test.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  auto* profile = profile_manager->GetLastUsedProfile();
  ASSERT_TRUE(profile);

  // Disable the profile picker and set the exit type to crashed.
  g_browser_process->local_state()->SetInteger(
      prefs::kBrowserProfilePickerAvailabilityOnStartup,
      static_cast<int>(ProfilePicker::AvailabilityOnStartup::kDisabled));
  ExitTypeService::GetInstanceForProfile(profile)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);

  // Opening a new window should suppress the profile picker and the crash
  // restore bubble should be showing.
  NewWindowSync(/*incognito=*/false, /*should_trigger_session_restore=*/true);

  EXPECT_FALSE(ProfilePicker::IsOpen());
  views::BubbleDialogDelegate* crash_bubble_delegate =
      SessionCrashedBubbleView::GetInstanceForTest();
  EXPECT_TRUE(crash_bubble_delegate);
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosWindowlessBrowserTest,
                       PRE_FullRestoreWithTwoProfiles) {
  // Simulate a full restore by creating the profiles in a PRE_ test.
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ASSERT_TRUE(embedded_test_server()->Start());

  // Create two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();

  Profile* profile1 = nullptr;
  Profile* profile2 = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
    ASSERT_TRUE(profile1);

    profile2 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
    ASSERT_TRUE(profile2);
  }
  DisableWelcomePages({profile1, profile2});

  // Don't delete Profiles too early.
  ScopedProfileKeepAlive profile1_keep_alive(
      profile1, ProfileKeepAliveOrigin::kBrowserWindow);
  ScopedProfileKeepAlive profile2_keep_alive(
      profile2, ProfileKeepAliveOrigin::kBrowserWindow);

  // Open some urls with the browsers, and close them.
  Browser* browser1 = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, profile1, true));
  chrome::NewTab(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1, embedded_test_server()->GetURL("/empty.html")));

  Browser* browser2 = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, profile2, true));
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
  SessionStartupPref::SetStartupPref(profile1, pref1);
  SessionStartupPref pref2(SessionStartupPref::URLS);
  pref2.urls = urls2;
  SessionStartupPref::SetStartupPref(profile2, pref2);

  profile1->GetPrefs()->CommitPendingWrite();
  profile2->GetPrefs()->CommitPendingWrite();

  // Ensure the session ends with the above two profiles.
  auto last_opened_profiles =
      g_browser_process->profile_manager()->GetLastOpenedProfiles();
  EXPECT_EQ(2u, last_opened_profiles.size());
  EXPECT_TRUE(base::Contains(last_opened_profiles, profile1));
  EXPECT_TRUE(base::Contains(last_opened_profiles, profile2));
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosWindowlessBrowserTest,
                       FullRestoreWithTwoProfiles) {
  // Browser launch should be suppressed with the kNoStartupWindow switch.
  ASSERT_FALSE(browser());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Open the two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();

  Profile* profile1 = nullptr;
  Profile* profile2 = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
    ASSERT_TRUE(profile1);

    profile2 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
    ASSERT_TRUE(profile2);
  }

  // The profiles to be restored should match those setup in the PRE_ test.
  auto last_opened_profiles =
      g_browser_process->profile_manager()->GetLastOpenedProfiles();
  EXPECT_EQ(2u, last_opened_profiles.size());
  EXPECT_TRUE(base::Contains(last_opened_profiles, profile1));
  EXPECT_TRUE(base::Contains(last_opened_profiles, profile2));

  // Trigger Lacros full restore.
  base::RunLoop run_loop;
  testing::SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 2);
  browser_service()->OpenForFullRestore(/*skip_crash_restore=*/true);
  run_loop.Run();

  // The startup URLs are ignored, and instead the last open sessions are
  // restored.
  EXPECT_TRUE(profile1->restored_last_session());
  EXPECT_TRUE(profile2->restored_last_session());

  Browser* new_browser = nullptr;
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile1));
  new_browser = chrome::FindBrowserWithProfile(profile1);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());

  ASSERT_EQ(1u, chrome::GetBrowserCount(profile2));
  new_browser = chrome::FindBrowserWithProfile(profile2);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/form.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosWindowlessBrowserTest,
                       PRE_FullRestoreDoNotSkipCrashRestore) {
  // Simulate a full restore by setting up a profile in a PRE_ test.
  SetupSingleProfileWithURLSPreference();
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosWindowlessBrowserTest,
                       FullRestoreDoNotSkipCrashRestore) {
  // Browser launch should be suppressed with the kNoStartupWindow switch.
  ASSERT_FALSE(browser());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Open the profile.
  base::FilePath dest_path = profile_manager->user_data_dir();

  Profile* profile = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile")));
    ASSERT_TRUE(profile);
  }

  // The profile to be restored should match the one in the PRE_ test.
  auto last_opened_profiles =
      g_browser_process->profile_manager()->GetLastOpenedProfiles();
  EXPECT_EQ(1u, last_opened_profiles.size());
  EXPECT_TRUE(base::Contains(last_opened_profiles, profile));

  // Disable the profile picker and set the exit type to crashed.
  g_browser_process->local_state()->SetInteger(
      prefs::kBrowserProfilePickerAvailabilityOnStartup,
      static_cast<int>(ProfilePicker::AvailabilityOnStartup::kDisabled));
  ExitTypeService::GetInstanceForProfile(profile)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);

  // Trigger Lacros full restore but do not skip crash restore prompts.
  browser_service()->OpenForFullRestore(/*skip_crash_restore=*/false);

  // The browser should not be restored but instead we should have the browser's
  // crash restore bubble prompt.
  Browser* new_browser = nullptr;
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile));
  new_browser = chrome::FindBrowserWithProfile(profile);
  ASSERT_TRUE(new_browser);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(content::WaitForLoadStop(tab_strip->GetWebContentsAt(0)));

  views::BubbleDialogDelegate* crash_bubble_delegate =
      SessionCrashedBubbleView::GetInstanceForTest();
  EXPECT_TRUE(crash_bubble_delegate);
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosWindowlessBrowserTest,
                       PRE_FullRestoreSkipCrashRestore) {
  // Simulate a full restore by setting up a profile in a PRE_ test.
  SetupSingleProfileWithURLSPreference();
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosWindowlessBrowserTest,
                       FullRestoreSkipCrashRestore) {
  // Browser launch should be suppressed with the kNoStartupWindow switch.
  ASSERT_FALSE(browser());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Open the profile.
  base::FilePath dest_path = profile_manager->user_data_dir();

  Profile* profile = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile")));
    ASSERT_TRUE(profile);
  }

  // The profile to be restored should match the one in the PRE_ test.
  auto last_opened_profiles =
      g_browser_process->profile_manager()->GetLastOpenedProfiles();
  EXPECT_EQ(1u, last_opened_profiles.size());
  EXPECT_TRUE(base::Contains(last_opened_profiles, profile));

  // Disable the profile picker and set the exit type to crashed.
  g_browser_process->local_state()->SetInteger(
      prefs::kBrowserProfilePickerAvailabilityOnStartup,
      static_cast<int>(ProfilePicker::AvailabilityOnStartup::kDisabled));
  ExitTypeService::GetInstanceForProfile(profile)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);

  // Trigger Lacros full restore and skip crash restore prompts.
  base::RunLoop run_loop;
  testing::SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 1);
  browser_service()->OpenForFullRestore(/*skip_crash_restore=*/true);
  run_loop.Run();

  // The browser should be restored (ignoring startup preference).
  Browser* new_browser = nullptr;
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile));
  new_browser = chrome::FindBrowserWithProfile(profile);
  ASSERT_TRUE(new_browser);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosWindowlessBrowserTest,
                       NewTab_OpensWindowWithSessionRestore) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  auto* profile =
      profile_manager->GetProfile(profile_manager->GetPrimaryUserProfilePath());
  DisableWelcomePages({profile});
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());

  // Set the startup pref to restore the last session.
  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile, pref);

  // Open a browser window with some URLs.
  auto* browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, profile, true));
  auto* tab_strip = browser->tab_strip_model();

  chrome::NewTab(browser);
  tab_strip->ActivateTabAt(0);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser, embedded_test_server()->GetURL("/title1.html")));

  chrome::NewTab(browser);
  tab_strip->ActivateTabAt(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser, embedded_test_server()->GetURL("/title2.html")));

  ASSERT_EQ(2, tab_strip->count());

  // Keep the browser process running while the browser is closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);

  // Close the browser and ensure there are no longer any open browser windows.
  CloseBrowserSynchronously(browser);
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());

  // Trigger a new tab with session restore.
  base::RunLoop run_loop;
  testing::SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 1);
  NewTabSync(/*should_trigger_session_restore=*/true);
  run_loop.Run();

  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
  auto* new_browser = chrome::FindBrowserWithProfile(profile);
  ASSERT_TRUE(new_browser);
  auto* new_tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(3, new_tab_strip->count());

  EXPECT_EQ("",  // The new tab.
            new_tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());
  EXPECT_EQ("/title1.html",
            new_tab_strip->GetWebContentsAt(1)->GetLastCommittedURL().path());
  EXPECT_EQ("/title2.html",
            new_tab_strip->GetWebContentsAt(2)->GetLastCommittedURL().path());

  // A second call to NewTab() ignores session restore and adds yet another new
  // tab to the existing browser.
  NewTabSync(/*should_trigger_session_restore=*/true);

  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
  ASSERT_EQ(4, new_tab_strip->count());
}

// Tests that requesting an incognito window when incognito mode is disallowed
// does not crash, and opens a regular window instead. Regression test for
// https://crbug.com/1314473
IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosBrowserTest,
                       NewWindow_IncognitoDisallowed) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* main_profile = profile_manager->GetProfileByPath(
      ProfileManager::GetPrimaryUserProfilePath());
  // Disallow incognito.
  IncognitoModePrefs::SetAvailability(
      main_profile->GetPrefs(), IncognitoModePrefs::Availability::kDisabled);
  // Request a new incognito window.
  NewWindowSync(/*incognito=*/true, /*should_trigger_session_restore=*/false);
  // A regular window opens instead.
  EXPECT_FALSE(ProfilePicker::IsOpen());
  Profile* profile = BrowserList::GetInstance()->GetLastActive()->profile();
  EXPECT_EQ(profile->GetPath(), main_profile->GetPath());
  EXPECT_FALSE(profile->IsOffTheRecord());
}

// Tests for non-syncing profiles.
class BrowserServiceLacrosNonSyncingProfilesBrowserTest
    : public BrowserServiceLacrosBrowserTest {
 public:
  // BrowserServiceLacrosBrowserTest:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    BrowserServiceLacrosBrowserTest::SetUpDefaultCommandLine(command_line);
    if (GetTestPreCount() == 0) {
      // The kNoStartupWindow is applied when launching lacros-chrome with the
      // kDoNotOpenWindow initial browser action.
      command_line->AppendSwitch(switches::kNoStartupWindow);

      // Show the FRE in these tests. We only disable the FRE for PRE_ tests
      // (with GetTestPreCount() == 1) as we need the general set up to run
      // and finish registering a signed in account with the primary profile. It
      // will then be available to the subsequent steps of the test.
      command_line->RemoveSwitch(switches::kNoFirstRun);
    }
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  // Start tracking the logged histograms from the beginning, since the FRE can
  // be triggered and completed before we enter the test body.
  base::HistogramTester histogram_tester_;

  profiles::testing::ScopedNonEnterpriseDomainSetterForTesting
      non_enterprise_domain_setter_;
};

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesBrowserTest,
                       PRE_NewWindow_OpensFirstRun) {
  // Dummy case to set up the primary profile.
  histogram_tester().ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", 0);
}
IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesBrowserTest,
                       NewWindow_OpensFirstRun) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_TRUE(ShouldOpenPrimaryProfileFirstRun(profile_manager->GetProfile(
      profile_manager->GetPrimaryUserProfilePath())));
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
  histogram_tester().ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", 0);

  base::RunLoop run_loop;
  browser_service()->NewWindow(
      /*incognito=*/false, /*should_trigger_session_restore=*/false,
      /*callback=*/run_loop.QuitClosure());
  profiles::testing::CompleteLacrosFirstRun(LoginUIService::ABORT_SYNC);

  run_loop.Run();

  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
  histogram_tester().ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint",
      LacrosFirstRunService::EntryPoint::kOther, 1);
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesBrowserTest,
                       PRE_NewWindow_OpensFirstRun_UiClose) {
  // Dummy case to set up the primary profile.
  histogram_tester().ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", 0);
}
IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesBrowserTest,
                       NewWindow_OpensFirstRun_UiClose) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_TRUE(ShouldOpenPrimaryProfileFirstRun(profile_manager->GetProfile(
      profile_manager->GetPrimaryUserProfilePath())));
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
  histogram_tester().ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", 0);

  base::RunLoop run_loop;
  browser_service()->NewWindow(
      /*incognito=*/false, /*should_trigger_session_restore=*/false,
      /*callback=*/run_loop.QuitClosure());
  profiles::testing::CompleteLacrosFirstRun(LoginUIService::UI_CLOSED);

  run_loop.Run();

  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
  histogram_tester().ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint",
      LacrosFirstRunService::EntryPoint::kOther, 1);
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesBrowserTest,
                       PRE_NewTab_OpensFirstRun) {
  // Dummy case to set up the primary profile.
  histogram_tester().ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", 0);
}
IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesBrowserTest,
                       NewTab_OpensFirstRun) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_TRUE(ShouldOpenPrimaryProfileFirstRun(profile_manager->GetProfile(
      profile_manager->GetPrimaryUserProfilePath())));
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
  histogram_tester().ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", 0);

  base::RunLoop run_loop;
  browser_service()->NewTab(
      /*should_trigger_session_restore=*/false,
      /*callback=*/run_loop.QuitClosure());
  profiles::testing::CompleteLacrosFirstRun(LoginUIService::ABORT_SYNC);

  run_loop.Run();

  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
  histogram_tester().ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint",
      LacrosFirstRunService::EntryPoint::kOther, 1);
}
