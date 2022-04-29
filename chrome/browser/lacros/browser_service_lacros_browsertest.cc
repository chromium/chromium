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
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/profile_ui_test_utils.h"
#include "chrome/browser/ui/startup/first_run_lacros.h"
#include "chrome/browser/ui/views/session_crashed_bubble_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

using crosapi::mojom::BrowserInitParams;
using crosapi::mojom::BrowserInitParamsPtr;
using crosapi::mojom::CreationResult;
using crosapi::mojom::SessionType;

namespace {

constexpr char kNavigationUrl[] = "https://www.google.com/";

// This class waits for a specified number of sessions to be restored.
class SessionsRestoredWaiter {
 public:
  explicit SessionsRestoredWaiter(base::OnceClosure quit_closure,
                                  int num_session_restores_expected);
  SessionsRestoredWaiter(const SessionsRestoredWaiter&) = delete;
  SessionsRestoredWaiter& operator=(const SessionsRestoredWaiter&) = delete;
  ~SessionsRestoredWaiter();

 private:
  // Callback for session restore notifications.
  void OnSessionRestoreDone(Profile* profile, int num_tabs_restored);

  // For automatically unsubscribing from callback-based notifications.
  base::CallbackListSubscription callback_subscription_;
  base::OnceClosure quit_closure_;
  int num_session_restores_expected_;
  int num_sessions_restored_ = 0;
};

SessionsRestoredWaiter::SessionsRestoredWaiter(
    base::OnceClosure quit_closure,
    int num_session_restores_expected)
    : quit_closure_(std::move(quit_closure)),
      num_session_restores_expected_(num_session_restores_expected) {
  callback_subscription_ = SessionRestore::RegisterOnSessionRestoredCallback(
      base::BindRepeating(&SessionsRestoredWaiter::OnSessionRestoreDone,
                          base::Unretained(this)));
}

SessionsRestoredWaiter::~SessionsRestoredWaiter() = default;

void SessionsRestoredWaiter::OnSessionRestoreDone(Profile* profile,
                                                  int num_tabs_restored) {
  if (++num_sessions_restored_ == num_session_restores_expected_)
    std::move(quit_closure_).Run();
}

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
    if (chromeos::BrowserInitParams::Get()->session_type ==
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
                       ProfilePickerOpensOnStartup) {
  Profile* main_profile = browser()->profile();

  // Create an additional profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath path_profile2 =
      profile_manager->user_data_dir().Append(FILE_PATH_LITERAL("Profile 2"));

  base::RunLoop run_loop;
  Profile* profile2;
  profile_manager->CreateProfileAsync(
      path_profile2, base::BindLambdaForTesting(
                         [&](Profile* profile, Profile::CreateStatus status) {
                           if (status == Profile::CREATE_STATUS_INITIALIZED) {
                             profile2 = profile;
                             run_loop.Quit();
                           }
                         }));
  run_loop.Run();
  // Open a browser window to make it the last used profile.
  chrome::NewEmptyWindow(profile2);
  ui_test_utils::WaitForBrowserToOpen();

  // Profile picker does _not_ open for incognito windows. Instead, the
  // incognito window for the last used profile is directly opened.
  base::RunLoop run_loop2;
  browser_service()->NewWindow(
      /*incognito=*/true, /*should_trigger_session_restore=*/false,
      /*callback=*/base::BindLambdaForTesting([&]() { run_loop2.Quit(); }));
  run_loop2.Run();
  EXPECT_FALSE(ProfilePicker::IsOpen());
  Profile* profile = BrowserList::GetInstance()->GetLastActive()->profile();
  // Main profile should be always used.
  EXPECT_EQ(profile->GetPath(), main_profile->GetPath());
  EXPECT_TRUE(profile->IsOffTheRecord());

  browser_service()->NewWindow(
      /*incognito=*/false, /*should_trigger_session_restore=*/false,
      /*callback=*/base::BindLambdaForTesting([]() {}));
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
  base::RunLoop run_loop;
  browser_service()->NewWindow(
      /*incognito=*/false, /*should_trigger_session_restore=*/true,
      /*callback=*/base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  run_loop.Run();

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

  // Lacros only supports syncing profiles for now.
  // TODO(crbug.com/1260291): Revisit this once non-syncing profiles are
  // allowed.
  ProfileAttributesStorage* attributes_storage =
      &profile_manager->GetProfileAttributesStorage();
  attributes_storage->GetProfileAttributesWithPath(profile1->GetPath())
      ->SetAuthInfo("gaia_id_1", u"email_1",
                    /*is_consented_primary_account=*/true);
  attributes_storage->GetProfileAttributesWithPath(profile2->GetPath())
      ->SetAuthInfo("gaia_id_2", u"email_2",
                    /*is_consented_primary_account=*/true);

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
  SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 2);
  browser_service()->OpenForFullRestore();
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
  base::RunLoop run_loop;
  browser_service()->NewWindow(
      /*incognito=*/true, /*should_trigger_session_restore=*/false,
      /*callback=*/base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  run_loop.Run();
  // A regular window opens instead.
  EXPECT_FALSE(ProfilePicker::IsOpen());
  Profile* profile = BrowserList::GetInstance()->GetLastActive()->profile();
  EXPECT_EQ(profile->GetPath(), main_profile->GetPath());
  EXPECT_FALSE(profile->IsOffTheRecord());
}

// Tests for lacros-chrome that require `LacrosNonSyncingProfiles` to be
// enabled.
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

 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kLacrosNonSyncingProfiles};
  profiles::testing::ScopedNonEnterpriseDomainSetterForTesting
      non_enterprise_domain_setter_;
};

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesBrowserTest,
                       PRE_NewWindow_OpensFirstRun) {
  // Dummy case to set up the primary profile.
}
IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesBrowserTest,
                       NewWindow_OpensFirstRun) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_TRUE(ShouldOpenPrimaryProfileFirstRun(profile_manager->GetProfile(
      profile_manager->GetPrimaryUserProfilePath())));
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());

  base::RunLoop run_loop;
  browser_service()->NewWindow(
      /*incognito=*/false, /*should_trigger_session_restore=*/false,
      /*callback=*/run_loop.QuitClosure());
  profiles::testing::CompleteLacrosFirstRun(LoginUIService::ABORT_SYNC);

  run_loop.Run();

  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesBrowserTest,
                       PRE_NewWindow_OpensFirstRun_UiClose) {
  // Dummy case to set up the primary profile.
}
IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesBrowserTest,
                       NewWindow_OpensFirstRun_UiClose) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_TRUE(ShouldOpenPrimaryProfileFirstRun(profile_manager->GetProfile(
      profile_manager->GetPrimaryUserProfilePath())));
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());

  base::RunLoop run_loop;
  browser_service()->NewWindow(
      /*incognito=*/false, /*should_trigger_session_restore=*/false,
      /*callback=*/run_loop.QuitClosure());
  profiles::testing::CompleteLacrosFirstRun(LoginUIService::UI_CLOSED);

  run_loop.Run();

  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesBrowserTest,
                       PRE_NewTab_OpensFirstRun) {
  // Dummy case to set up the primary profile.
}
IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosNonSyncingProfilesBrowserTest,
                       NewTab_OpensFirstRun) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_TRUE(ShouldOpenPrimaryProfileFirstRun(profile_manager->GetProfile(
      profile_manager->GetPrimaryUserProfilePath())));
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());

  base::RunLoop run_loop;
  browser_service()->NewTab(
      /*callback=*/run_loop.QuitClosure());
  profiles::testing::CompleteLacrosFirstRun(LoginUIService::ABORT_SYNC);

  run_loop.Run();

  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
}
