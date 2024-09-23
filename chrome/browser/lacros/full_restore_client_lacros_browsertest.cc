// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/full_restore_client_lacros.h"

#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"

namespace {

bool IsInterfaceAvailable() {
  chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
  return lacros_service &&
         lacros_service->IsAvailable<crosapi::mojom::FullRestore>();
}

}  // namespace

using FullRestoreClientLacrosBrowserTest = InProcessBrowserTest;

// Create a browser with 8 more tabs (9 total) in addition to the default
// browser which has 1 tab.
IN_PROC_BROWSER_TEST_F(FullRestoreClientLacrosBrowserTest, PRE_TestCallback) {
  if (!IsInterfaceAvailable()) {
    GTEST_SKIP() << "Unsupported Ash version.";
  }

  Browser* browser1 = CreateBrowser(ProfileManager::GetActiveUserProfile());

  // Add 8 more tabs to the new browser. We will test only 5 of the urls are
  // sent back to ash.
  GURL example_url("https://www.google.com/");
  for (int i = 0; i < 8; ++i) {
    content::TestNavigationObserver navigation_observer(example_url);
    navigation_observer.StartWatchingNewWebContents();
    chrome::AddTabAt(browser1, example_url, /*index=*/-1, /*foreground=*/true);
    navigation_observer.Wait();
  }

  // Add a third browser window with three tabs to another profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile& profile2 = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  Browser* browser2 = CreateBrowser(&profile2);
  for (int i = 0; i < 2; ++i) {
    content::TestNavigationObserver navigation_observer(example_url);
    navigation_observer.StartWatchingNewWebContents();
    chrome::AddTabAt(browser2, example_url, /*index=*/-1, /*foreground=*/true);
    navigation_observer.Wait();
  }

  EXPECT_EQ(3u, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(FullRestoreClientLacrosBrowserTest, TestCallback) {
  if (!IsInterfaceAvailable()) {
    GTEST_SKIP() << "Unsupported Ash version.";
  }

  FullRestoreClientLacros client;

  base::test::TestFuture<std::vector<crosapi::mojom::SessionWindowPtr>>
      test_future;
  client.GetSessionInformation(test_future.GetCallback());
  const std::vector<crosapi::mojom::SessionWindowPtr>& session_windows =
      test_future.Get();
  ASSERT_EQ(3u, session_windows.size());

  // All three windows have a valid window id.
  EXPECT_NE(0u, session_windows[0]->window_id);
  EXPECT_NE(0u, session_windows[1]->window_id);
  EXPECT_NE(0u, session_windows[2]->window_id);

  // One of the windows has 1 tab, one has 3 tabs and one has 9 tabs. We send
  // only 5 urls to ash though.
  EXPECT_TRUE(base::ranges::any_of(
      session_windows,
      [](const crosapi::mojom::SessionWindowPtr& session_window) {
        return session_window->tab_count == 1 &&
               session_window->urls.size() == 1u;
      }));
  EXPECT_TRUE(base::ranges::any_of(
      session_windows,
      [](const crosapi::mojom::SessionWindowPtr& session_window) {
        return session_window->tab_count == 3 &&
               session_window->urls.size() == 3u;
      }));
  EXPECT_TRUE(base::ranges::any_of(
      session_windows,
      [](const crosapi::mojom::SessionWindowPtr& session_window) {
        return session_window->tab_count == 9 &&
               session_window->urls.size() == 5u;
      }));
}
