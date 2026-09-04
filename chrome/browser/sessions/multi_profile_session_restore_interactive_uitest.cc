// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_CHROMEOS)
class MultiProfileSessionRestoreInteractiveUiTest
    : public InProcessBrowserTest {
 public:
  MultiProfileSessionRestoreInteractiveUiTest() = default;
  ~MultiProfileSessionRestoreInteractiveUiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    // Simulate Chrome having restarted (e.g., following an update,
    // chrome://restart, or OS restart manager). This injects
    // --restore-last-session into base::CommandLine::ForCurrentProcess().
    command_line->AppendSwitch(switches::kRestoreLastSession);
  }
};

// Tests that when Chrome starts with --restore-last-session, closing a
// profile's window and subsequently launching that profile from another
// active window respects the "Open the New Tab page" setting and does not
// restore closed tabs.
IN_PROC_BROWSER_TEST_F(MultiProfileSessionRestoreInteractiveUiTest,
                       CloseAndLaunchProfileWithDefaultNtpSetting) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // 1. Create Profile 2 as an existing (non-new) profile.
  base::FilePath profile_2_dir =
      profile_manager->user_data_dir().Append(FILE_PATH_LITERAL("Profile 2"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::CreateDirectory(profile_2_dir);
    base::WriteFile(profile_2_dir.Append(FILE_PATH_LITERAL("Preferences")),
                    "{}");
  }

  Profile& profile_2 =
      profiles::testing::CreateProfileSync(profile_manager, profile_2_dir);
  ASSERT_FALSE(profile_2.IsNewProfile());

  // Hold a ScopedProfileKeepAlive to prevent Profile 2 from being destroyed
  // when its window is closed while the browser process remains alive.
  ScopedProfileKeepAlive profile_keep_alive(
      &profile_2, ProfileKeepAliveOrigin::kBrowserWindow);

  // 2. Explicitly set Profile 2's On Startup pref to "Open the New Tab page".
  SessionStartupPref pref(SessionStartupPref::DEFAULT);
  SessionStartupPref::SetStartupPref(&profile_2, pref);

  // 3. Open a browser window for Profile 2.
  ui_test_utils::BrowserCreatedObserver created_observer_1;
  profiles::OpenBrowserWindowForProfile(base::DoNothing(),
                                        /*always_create=*/true,
                                        /*is_new_profile=*/false,
                                        /*open_command_line_urls=*/false,
                                        &profile_2);
  BrowserWindowInterface* browser_2 = created_observer_1.Wait();
  ASSERT_TRUE(browser_2);
  ASSERT_EQ(browser_2->GetProfile(), &profile_2);

  // 4. Open 3 distinct tabs in Profile 2.
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  const GURL url3 = embedded_test_server()->GetURL("/title3.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser_2, url1));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser_2, url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser_2, url3, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  ASSERT_EQ(3, browser_2->GetTabStripModel()->count());

  // 5. Close Profile 2's window while Profile 1 (browser()) remains open.
  {
    ui_test_utils::BrowserDestroyedObserver destroyed_observer(browser_2);
    browser_2->GetWindow()->Close();
    destroyed_observer.Wait();
  }

  // Verify Profile 2 now has 0 open windows, but the browser process is alive.
  EXPECT_EQ(0u, ProfileBrowserCollection::GetForProfile(&profile_2)->GetSize());
  EXPECT_EQ(1u, ProfileBrowserCollection::GetForProfile(browser()->GetProfile())
                    ->GetSize());

  // 6. Launch a window for Profile 2 from the running browser process
  // (simulating the user selecting Profile 2 from the profile avatar menu in
  // Profile 1).
  ui_test_utils::BrowserCreatedObserver created_observer_2;
  profiles::SwitchToProfile(profile_2.GetPath(), /*always_create=*/false,
                            base::DoNothing());
  BrowserWindowInterface* new_browser_2 = created_observer_2.Wait();
  ASSERT_TRUE(new_browser_2);

  // 7. Verify tab count:
  // Should open 1 clean tab (NTP) according to the profile's startup setting.
  EXPECT_EQ(1, new_browser_2->GetTabStripModel()->count())
      << "Launching a closed profile window unexpectedly restored "
      << (new_browser_2->tab_strip_model()->count() - 1)
      << " previous tabs despite On Startup being set to 'Open New Tab Page'.";
}

// Tests that when On Startup is set to "Open specific pages", launching a
// closed profile window opens only the specified URLs, without restoring
// closed tabs.
IN_PROC_BROWSER_TEST_F(MultiProfileSessionRestoreInteractiveUiTest,
                       CloseAndLaunchProfileWithSpecificUrlsSetting) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath profile_2_dir =
      profile_manager->user_data_dir().Append(FILE_PATH_LITERAL("Profile 2"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::CreateDirectory(profile_2_dir);
    base::WriteFile(profile_2_dir.Append(FILE_PATH_LITERAL("Preferences")),
                    "{}");
  }

  Profile& profile_2 =
      profiles::testing::CreateProfileSync(profile_manager, profile_2_dir);
  ASSERT_FALSE(profile_2.IsNewProfile());

  ScopedProfileKeepAlive profile_keep_alive(
      &profile_2, ProfileKeepAliveOrigin::kBrowserWindow);

  // Configure On Startup to open one specific page ("specific_url").
  const GURL specific_url = embedded_test_server()->GetURL("/simple.html");
  SessionStartupPref pref_urls(SessionStartupPref::URLS);
  pref_urls.urls.push_back(specific_url);
  SessionStartupPref::SetStartupPref(&profile_2, pref_urls);

  // Open window for Profile 2 and add 2 tabs.
  ui_test_utils::BrowserCreatedObserver created_observer_1;
  profiles::OpenBrowserWindowForProfile(base::DoNothing(),
                                        /*always_create=*/true,
                                        /*is_new_profile=*/false,
                                        /*open_command_line_urls=*/false,
                                        &profile_2);
  BrowserWindowInterface* browser_2 = created_observer_1.Wait();
  ASSERT_TRUE(browser_2);

  const GURL old_url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL old_url2 = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser_2, old_url1));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser_2, old_url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  ASSERT_EQ(2, browser_2->GetTabStripModel()->count());

  // Close Profile 2's window.
  {
    ui_test_utils::BrowserDestroyedObserver destroyed_observer(browser_2);
    browser_2->GetWindow()->Close();
    destroyed_observer.Wait();
  }
  EXPECT_EQ(0u, ProfileBrowserCollection::GetForProfile(&profile_2)->GetSize());

  // Launch a window for Profile 2.
  ui_test_utils::BrowserCreatedObserver created_observer_2;
  profiles::SwitchToProfile(profile_2.GetPath(), /*always_create=*/false,
                            base::DoNothing());
  BrowserWindowInterface* new_browser_2 = created_observer_2.Wait();
  ASSERT_TRUE(new_browser_2);

  // Should open 1 tab (the specific URL).
  EXPECT_EQ(1, new_browser_2->GetTabStripModel()->count())
      << "Launching a closed profile window unexpectedly restored "
      << (new_browser_2->GetTabStripModel()->count() - 1)
      << " previous tabs despite On Startup set to 'Open specific pages'.";
  EXPECT_EQ(specific_url, new_browser_2->GetTabStripModel()
                              ->GetActiveWebContents()
                              ->GetVisibleURL());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
