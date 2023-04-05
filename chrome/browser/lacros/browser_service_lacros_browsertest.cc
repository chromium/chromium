// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/chromeos/app_mode/app_session.h"
#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"
#include "chrome/browser/lacros/browser_service_lacros.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
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
#include "chrome/browser/ui/startup/first_run_service.h"
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
#include "components/policy/core/common/policy_pref_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"

using crosapi::mojom::BrowserInitParams;
using crosapi::mojom::BrowserInitParamsPtr;
using crosapi::mojom::CreationResult;
using crosapi::mojom::SessionType;

namespace {
constexpr char kNavigationUrl[] = "https://www.google.com/";
}  // namespace

class BrowserServiceLacrosBrowserTest : public InProcessBrowserTest {
 public:
  BrowserServiceLacrosBrowserTest(
      crosapi::mojom::SessionType session_type =
          crosapi::mojom::SessionType::kRegularSession)
      : session_type_(session_type) {}

  BrowserServiceLacrosBrowserTest(const BrowserServiceLacrosBrowserTest&) =
      delete;
  BrowserServiceLacrosBrowserTest& operator=(
      const BrowserServiceLacrosBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    browser_service_ = std::make_unique<BrowserServiceLacros>();
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    crosapi::mojom::BrowserInitParamsPtr init_params =
        chromeos::BrowserInitParams::GetForTests()->Clone();
    init_params->session_type = session_type_;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));

    InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
  }

  void CreateFullscreenWindow() {
    bool use_callback = false;
    browser_service()->NewFullscreenWindow(
        GURL(kNavigationUrl),
        display::Screen::GetScreen()->GetDisplayForNewWindows().id(),
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
    base::test::TestFuture<void> new_window_future;
    browser_service()->NewWindow(
        incognito, should_trigger_session_restore,
        display::Screen::GetScreen()->GetDisplayForNewWindows().id(),
        new_window_future.GetCallback());
    ASSERT_TRUE(new_window_future.Wait())
        << "NewWindow did not trigger the callback.";
  }

  void NewTabSync() {
    base::test::TestFuture<void> new_tab_future;
    browser_service()->NewTab(new_tab_future.GetCallback());
    ASSERT_TRUE(new_tab_future.Wait())
        << "NewTab did not trigger the callback.";
  }

  void LaunchSync() {
    base::test::TestFuture<void> launch_future;
    browser_service()->Launch(0, launch_future.GetCallback());
    ASSERT_TRUE(launch_future.Wait()) << "Launch did not trigger the callback.";
  }

  BrowserServiceLacros* browser_service() const {
    return browser_service_.get();
  }

 private:
  std::unique_ptr<BrowserServiceLacros> browser_service_;
  crosapi::mojom::SessionType session_type_;
};

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosBrowserTest, NewFullscreenWindow) {
  CreateFullscreenWindow();
  VerifyFullscreenWindow();
}

class BrowserServiceLacrosKioskBrowserTest
    : public BrowserServiceLacrosBrowserTest {
 public:
  BrowserServiceLacrosKioskBrowserTest()
      : BrowserServiceLacrosBrowserTest(
            crosapi::mojom::SessionType::kWebKioskSession) {}
};

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosKioskBrowserTest,
                       BlockAdditionalWindowsInWebKiosk) {
  CreateFullscreenWindow();

  // The new window should be blocked in the web Kiosk session.
  const size_t browser_count = BrowserList::GetInstance()->size();
  CreateNewWindow();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(BrowserList::GetInstance()->size(), browser_count);
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosBrowserTest,
                       AllowAdditionalWindowsInRegularSession) {
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
  Profile& profile2 =
      profiles::testing::CreateProfileSync(profile_manager, path_profile2);
  // Open a browser window to make it the last used profile.
  chrome::NewEmptyWindow(&profile2);
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
  NewTabSync();
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_FALSE(ProfilePicker::IsOpen());
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  auto* main_profile = profile_manager->GetProfileByPath(
      profile_manager->GetPrimaryUserProfilePath());
  auto* browser = chrome::FindBrowserWithProfile(main_profile);
  auto* tab_strip = browser->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());

  // Consequent `NewTab()` should add a new tab to an existing browser.
  NewTabSync();
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
  Profile& profile2 =
      profiles::testing::CreateProfileSync(profile_manager, profile2_path);
  chrome::NewEmptyWindow(&profile2);
  ui_test_utils::WaitForBrowserToOpen();
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  auto* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());

  // `NewTab()` should add a tab to the main profile window;
  NewTabSync();
  EXPECT_EQ(2, tab_strip->count());

  chrome::CloseAllBrowsers();
  // Wait for two browsers to be closed.
  ui_test_utils::WaitForBrowserToClose();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());

  // `NewTab()` should open the profile picker.
  NewTabSync();
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

  // Open browser with session restore.
  base::test::TestFuture<void> restore_waiter_future;
  testing::SessionsRestoredWaiter restore_waiter(
      restore_waiter_future.GetCallback(), 1);
  LaunchSync();
  ASSERT_TRUE(restore_waiter_future.Wait())
      << "restore_waiter did not trigger the callback.";

  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
  auto* new_browser = chrome::FindBrowserWithProfile(profile);
  ASSERT_TRUE(new_browser);
  auto* new_tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(2, new_tab_strip->count());

  EXPECT_EQ("/title1.html",
            new_tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());
  EXPECT_EQ("/title2.html",
            new_tab_strip->GetWebContentsAt(1)->GetLastCommittedURL().path());

  // A second call to Launch() ignores session restore and adds a new tab to the
  // existing browser.
  LaunchSync();
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
  ASSERT_EQ(3, new_tab_strip->count());
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
      main_profile->GetPrefs(), policy::IncognitoModeAvailability::kDisabled);
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
  BrowserServiceLacrosNonSyncingProfilesBrowserTest(
      crosapi::mojom::SessionType session_type =
          crosapi::mojom::SessionType::kRegularSession)
      : BrowserServiceLacrosBrowserTest(session_type) {}

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

  Profile* GetPrimaryProfile() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    return profile_manager->GetProfile(
        profile_manager->GetPrimaryUserProfilePath());
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
  EXPECT_TRUE(ShouldOpenFirstRun(GetPrimaryProfile()));
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
  histogram_tester().ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", 0);

  base::test::TestFuture<void> new_window_future;
  browser_service()->NewWindow(
      /*incognito=*/false, /*should_trigger_session_restore=*/false,
      display::Screen::GetScreen()->GetDisplayForNewWindows().id(),
      /*callback=*/new_window_future.GetCallback());
  profiles::testing::CompleteLacrosFirstRun(LoginUIService::ABORT_SYNC);

  ASSERT_TRUE(new_window_future.Wait())
      << "NewWindow did not trigger the callback.";

  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
  histogram_tester().ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint",
      FirstRunService::EntryPoint::kOther, 1);
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesBrowserTest,
                       PRE_NewWindow_OpensFirstRun_UiClose) {
  // Dummy case to set up the primary profile.
  histogram_tester().ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", 0);
}
IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesBrowserTest,
                       NewWindow_OpensFirstRun_UiClose) {
  EXPECT_TRUE(ShouldOpenFirstRun(GetPrimaryProfile()));
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
  histogram_tester().ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", 0);

  base::test::TestFuture<void> new_window_future;
  browser_service()->NewWindow(
      /*incognito=*/false, /*should_trigger_session_restore=*/false,
      display::Screen::GetScreen()->GetDisplayForNewWindows().id(),
      /*callback=*/new_window_future.GetCallback());
  profiles::testing::CompleteLacrosFirstRun(LoginUIService::UI_CLOSED);

  ASSERT_TRUE(new_window_future.Wait())
      << "NewWindow did not trigger the callback.";

  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
  histogram_tester().ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint",
      FirstRunService::EntryPoint::kOther, 1);
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesBrowserTest,
                       PRE_NewTab_OpensFirstRun) {
  // Dummy case to set up the primary profile.
  histogram_tester().ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", 0);
}
IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesBrowserTest,
                       NewTab_OpensFirstRun) {
  EXPECT_TRUE(ShouldOpenFirstRun(GetPrimaryProfile()));
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
  histogram_tester().ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", 0);

  base::test::TestFuture<void> new_tab_future;
  browser_service()->NewTab(
      /*callback=*/new_tab_future.GetCallback());
  profiles::testing::CompleteLacrosFirstRun(LoginUIService::ABORT_SYNC);

  ASSERT_TRUE(new_tab_future.Wait()) << "NewTab did not trigger the callback.";

  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
  histogram_tester().ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint",
      FirstRunService::EntryPoint::kOther, 1);
}

class BrowserServiceLacrosNonSyncingProfilesGuestBrowserTest
    : public BrowserServiceLacrosNonSyncingProfilesBrowserTest {
 public:
  BrowserServiceLacrosNonSyncingProfilesGuestBrowserTest()
      : BrowserServiceLacrosNonSyncingProfilesBrowserTest(
            crosapi::mojom::SessionType::kGuestSession) {}
};

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesGuestBrowserTest,
                       PRE_NewWindow_OpensFirstRun) {
  // Dummy case to set up the primary profile.
  histogram_tester().ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", 0);
}
IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesGuestBrowserTest,
                       NewWindow_OpensFirstRun) {
  EXPECT_FALSE(ShouldOpenFirstRun(GetPrimaryProfile()));
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
  histogram_tester().ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", 0);

  NewWindowSync(/*incognito=*/false, /*should_trigger_session_restore=*/false);

  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
  histogram_tester().ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", 0);
}

class BrowserServiceLacrosNonSyncingProfilesWebKioskBrowserTest
    : public BrowserServiceLacrosNonSyncingProfilesBrowserTest {
 public:
  BrowserServiceLacrosNonSyncingProfilesWebKioskBrowserTest()
      : BrowserServiceLacrosNonSyncingProfilesBrowserTest(
            crosapi::mojom::SessionType::kWebKioskSession) {}
};

IN_PROC_BROWSER_TEST_F(
    BrowserServiceLacrosNonSyncingProfilesWebKioskBrowserTest,
    PRE_NewWindow_OpensFirstRun) {
  // Dummy case to set up the primary profile.
  histogram_tester().ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", 0);
}
IN_PROC_BROWSER_TEST_F(
    BrowserServiceLacrosNonSyncingProfilesWebKioskBrowserTest,
    NewWindow_OpensFirstRun) {
  EXPECT_FALSE(ShouldOpenFirstRun(GetPrimaryProfile()));
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
  histogram_tester().ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", 0);

  NewWindowSync(/*incognito=*/false, /*should_trigger_session_restore=*/false);

  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
  histogram_tester().ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint", 0);
}
