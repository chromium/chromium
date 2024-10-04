// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_window.h"

#include <stddef.h>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/account_id/account_id.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#error "This test verifies the Desktop implementation of Guest only."
#endif

namespace {

// Code related to history borrowed from:
// chrome/browser/history/history_browsertest.cc

// Note: WaitableEvent is not used for synchronization between the main thread
// and history backend thread because the history subsystem posts tasks back
// to the main thread. Had we tried to Signal an event in such a task
// and Wait for it on the main thread, the task would not run at all because
// the main thread would be blocked on the Wait call, resulting in a deadlock.

// A task to be scheduled on the history backend thread.
// Notifies the main thread after all history backend thread tasks have run.
class WaitForHistoryTask : public history::HistoryDBTask {
 public:
  explicit WaitForHistoryTask(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}
  WaitForHistoryTask(const WaitForHistoryTask&) = delete;
  WaitForHistoryTask& operator=(const WaitForHistoryTask&) = delete;

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    return true;
  }

  void DoneRunOnMainThread() override { std::move(quit_closure_).Run(); }

 private:
  ~WaitForHistoryTask() override = default;
  base::OnceClosure quit_closure_;
};

void WaitForHistoryBackendToRun(Profile* profile) {
  base::CancelableTaskTracker task_tracker;
  base::RunLoop loop;
  std::unique_ptr<history::HistoryDBTask> task(
      new WaitForHistoryTask(loop.QuitWhenIdleClosure()));
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  history->ScheduleDBTask(FROM_HERE, std::move(task), &task_tracker);
  loop.Run();
}

class EmptyAcceleratorHandler : public ui::AcceleratorProvider {
 public:
  // Don't handle accelerators.
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override {
    return false;
  }
};

}  // namespace

class ProfileWindowBrowserTest : public InProcessBrowserTest {
 public:
  ProfileWindowBrowserTest() = default;
  ProfileWindowBrowserTest(const ProfileWindowBrowserTest&) = delete;
  ProfileWindowBrowserTest& operator=(const ProfileWindowBrowserTest&) = delete;
  ~ProfileWindowBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, CountForNullBrowser) {
  EXPECT_EQ(size_t{0}, chrome::GetBrowserCount(nullptr));
  EXPECT_EQ(0, BrowserList::GetOffTheRecordBrowsersActiveForProfile(nullptr));
}

class ProfileWindowCountBrowserTest : public ProfileWindowBrowserTest,
                                      public testing::WithParamInterface<bool> {
 protected:
  ProfileWindowCountBrowserTest() = default;

  bool is_incognito() { return GetParam(); }

  int GetWindowCount() {
    return is_incognito()
               ? BrowserList::GetOffTheRecordBrowsersActiveForProfile(
                     browser()->profile())
               : BrowserList::GetGuestBrowserCount();
  }

  Browser* CreateGuestOrIncognitoBrowser() {
    Browser* new_browser;
    // When |profile_| is null this means no browsers have been created,
    // this is the first browser instance.
    if (!profile_) {
      new_browser = is_incognito()
                        ? CreateIncognitoBrowser(browser()->profile())
                        : CreateGuestBrowser();
      profile_ = new_browser->profile();
    } else {
      new_browser = CreateIncognitoBrowser(profile_);
    }

    return new_browser;
  }

 private:
  raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(ProfileWindowCountBrowserTest, CountProfileWindows) {
  EXPECT_EQ(0, GetWindowCount());

  // Create a browser and check the count.
  Browser* browser1 = CreateGuestOrIncognitoBrowser();
  EXPECT_EQ(1, GetWindowCount());

  // Create another browser and check the count.
  Browser* browser2 = CreateGuestOrIncognitoBrowser();
  EXPECT_EQ(2, GetWindowCount());

  // Close one browser and count.
  CloseBrowserSynchronously(browser2);
  EXPECT_EQ(1, GetWindowCount());

  // Close another browser and count.
  CloseBrowserSynchronously(browser1);
  EXPECT_EQ(0, GetWindowCount());
}

// |OpenDevToolsWindowSync| is slow on Linux Debug and can result in flacky test
// failure. See (crbug.com/1186994).
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_DevToolsWindowsNotCounted DISABLED_DevToolsWindowsNotCounted
#else
#define MAYBE_DevToolsWindowsNotCounted DevToolsWindowsNotCounted
#endif
IN_PROC_BROWSER_TEST_P(ProfileWindowCountBrowserTest,
                       MAYBE_DevToolsWindowsNotCounted) {
  Browser* browser = CreateGuestOrIncognitoBrowser();
  EXPECT_EQ(1, GetWindowCount());

  DevToolsWindow* devtools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser,
                                                    /*is_docked=*/true);
  EXPECT_EQ(1, GetWindowCount());
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);

  devtools_window = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser, /*is_docked=*/false);
  EXPECT_EQ(1, GetWindowCount());
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);

  EXPECT_EQ(1, GetWindowCount());
}

INSTANTIATE_TEST_SUITE_P(All, ProfileWindowCountBrowserTest, testing::Bool());

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, OpenGuestBrowser) {
  EXPECT_TRUE(CreateGuestBrowser());
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, GuestIsOffTheRecord) {
  EXPECT_TRUE(CreateGuestBrowser()->profile()->IsOffTheRecord());
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, GuestIgnoresHistory) {
  Browser* guest_browser = CreateGuestBrowser();

  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      guest_browser->profile(), ServiceAccessType::EXPLICIT_ACCESS));

  GURL test_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html")));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(guest_browser, test_url));
  WaitForHistoryBackendToRun(guest_browser->profile());

  std::vector<GURL> urls =
      ui_test_utils::HistoryEnumerator(guest_browser->profile()).urls();

  ASSERT_EQ(0u, urls.size());
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, GuestClearsCookies) {
  Browser* guest_browser = CreateGuestBrowser();
  Profile* guest_profile = guest_browser->profile();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/set-cookie?cookie1"));

  // Before navigation there are no cookies for the URL.
  std::string cookie = content::GetCookies(guest_profile, url);
  ASSERT_EQ("", cookie);

  // After navigation there is a cookie for the URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(guest_browser, url));
  cookie = content::GetCookies(guest_profile, url);
  EXPECT_EQ("cookie1", cookie);

  CloseBrowserSynchronously(guest_browser);

  // Closing the browser has removed the cookie.
  cookie = content::GetCookies(guest_profile, url);
  ASSERT_EQ("", cookie);
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, GuestClearsFindInPageCache) {
  Browser* guest_browser = CreateGuestBrowser();
  Profile* guest_profile = guest_browser->profile();

  std::u16string fip_text = u"first guest session search text";
  FindBarStateFactory::GetForBrowserContext(guest_profile)
      ->SetLastSearchText(fip_text);

  // Open a second guest window and close one. This should not affect the find
  // in page cache as the guest session hasn't been ended.
  profiles::FindOrCreateNewWindowForProfile(
      guest_profile, chrome::startup::IsProcessStartup::kNo,
      chrome::startup::IsFirstRun::kNo, true /*always_create*/);
  CloseBrowserSynchronously(guest_browser);
  EXPECT_EQ(fip_text, FindBarStateFactory::GetForBrowserContext(guest_profile)
                          ->GetSearchPrepopulateText());

  // Close the remaining guest browser window.
  guest_browser = chrome::FindAnyBrowser(guest_profile, true);
  EXPECT_TRUE(guest_browser);
  CloseBrowserSynchronously(guest_browser);
  content::RunAllTasksUntilIdle();

  // Open a new guest browser window. Since this is a separate session, the find
  // in page text should have been cleared (along with all other browsing data).
  profiles::FindOrCreateNewWindowForProfile(
      guest_profile, chrome::startup::IsProcessStartup::kNo,
      chrome::startup::IsFirstRun::kNo, true /*always_create*/);

  EXPECT_EQ(std::u16string(),
            FindBarStateFactory::GetForBrowserContext(guest_profile)
                ->GetSearchPrepopulateText());
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, GuestCannotSignin) {
  Browser* guest_browser = CreateGuestBrowser();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(guest_browser->profile());

  // Guest profiles can't sign in without a IdentityManager.
  ASSERT_FALSE(identity_manager);
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, GuestAppMenuLacksBookmarks) {
  EmptyAcceleratorHandler accelerator_handler;
  // Verify the normal browser has a bookmark menu.
  AppMenuModel model_normal_profile(&accelerator_handler, browser());
  model_normal_profile.Init();
  EXPECT_TRUE(
      model_normal_profile.GetIndexOfCommandId(IDC_BOOKMARKS_MENU).has_value());

  // Guest browser has no bookmark menu.
  Browser* guest_browser = CreateGuestBrowser();
  AppMenuModel model_guest_profile(&accelerator_handler, guest_browser);
  EXPECT_FALSE(
      model_guest_profile.GetIndexOfCommandId(IDC_BOOKMARKS_MENU).has_value());
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, OpenBrowserWindowForProfile) {
  Profile* profile = browser()->profile();
  size_t num_browsers = BrowserList::GetInstance()->size();
  base::test::TestFuture<Browser*> future;
  profiles::OpenBrowserWindowForProfile(future.GetCallback(), true, false,
                                        false, profile);
  ASSERT_TRUE(future.Get());
  EXPECT_NE(browser(), future.Get());
  EXPECT_EQ(profile, future.Get()->profile());
  EXPECT_EQ(num_browsers + 1, BrowserList::GetInstance()->size());
  EXPECT_FALSE(ProfilePicker::IsOpen());
}

// Regression test for https://crbug.com/1433283
IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest,
                       OpenTwoBrowserWindowsForProfile) {
  Profile* profile = browser()->profile();
  size_t num_browsers = BrowserList::GetInstance()->size();
  base::test::TestFuture<Browser*> future;
  profiles::OpenBrowserWindowForProfile(future.GetCallback(), true, false,
                                        false, profile);
  CreateBrowser(profile);
  EXPECT_EQ(profile, future.Get()->profile());
  EXPECT_EQ(num_browsers + 2, BrowserList::GetInstance()->size());
  EXPECT_FALSE(ProfilePicker::IsOpen());
}

// TODO(crbug.com/41443527): Test is flaky on Win and Linux.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#define MAYBE_OpenBrowserWindowForProfileWithSigninRequired \
  DISABLED_OpenBrowserWindowForProfileWithSigninRequired
#else
#define MAYBE_OpenBrowserWindowForProfileWithSigninRequired \
  OpenBrowserWindowForProfileWithSigninRequired
#endif
IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest,
                       MAYBE_OpenBrowserWindowForProfileWithSigninRequired) {
  signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);
  Profile* profile = browser()->profile();
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->LockForceSigninProfile(true);
  size_t num_browsers = BrowserList::GetInstance()->size();
  base::RunLoop run_loop;
  ProfilePicker::AddOnProfilePickerOpenedCallbackForTesting(
      run_loop.QuitClosure());
  profiles::OpenBrowserWindowForProfile(base::OnceCallback<void(Browser*)>(),
                                        true, false, false, profile);
  run_loop.Run();
  EXPECT_EQ(num_browsers, BrowserList::GetInstance()->size());
  EXPECT_TRUE(ProfilePicker::IsOpen());
}
