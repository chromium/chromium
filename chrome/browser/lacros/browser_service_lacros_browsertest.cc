// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/chromeos/app_mode/app_session.h"
#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"
#include "chrome/browser/lacros/browser_service_lacros.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/views/session_crashed_bubble_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

using crosapi::mojom::BrowserInitParams;
using crosapi::mojom::BrowserInitParamsPtr;
using crosapi::mojom::CreationResult;
using crosapi::mojom::SessionType;

const char kNavigationUrl[] = "https://www.google.com/";

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
    chromeos::LacrosService::Get()->SetInitParamsForTests(
        std::move(init_params));
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
    if (chromeos::LacrosService::Get()->init_params()->session_type ==
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
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    // The kNoStartupWindow is applied when launching lacros-chrome with the
    // kDoNotOpenWindow initial browser action.
    command_line->AppendSwitch(switches::kNoStartupWindow);
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
