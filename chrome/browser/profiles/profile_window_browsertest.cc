// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_window.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
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
  WaitForHistoryTask() {}

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    return true;
  }

  void DoneRunOnMainThread() override {
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

 private:
  ~WaitForHistoryTask() override {}

  DISALLOW_COPY_AND_ASSIGN(WaitForHistoryTask);
};

void WaitForHistoryBackendToRun(Profile* profile) {
  base::CancelableTaskTracker task_tracker;
  std::unique_ptr<history::HistoryDBTask> task(new WaitForHistoryTask());
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  history->ScheduleDBTask(FROM_HERE, std::move(task), &task_tracker);
  content::RunMessageLoop();
}

class EmptyAcceleratorHandler : public ui::AcceleratorProvider {
 public:
  // Don't handle accelerators.
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override {
    return false;
  }
};

base::FilePath CreateTestingProfile(const std::string& name,
                                    const std::string& relative_path) {
  ProfileManager* manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage = manager->GetProfileAttributesStorage();
  size_t starting_number_of_profiles = storage.GetNumberOfProfiles();

  base::FilePath profile_path =
      manager->user_data_dir().AppendASCII(relative_path);
  storage.AddProfile(profile_path, base::ASCIIToUTF16(name), std::string(),
                     base::string16(), false, 0u, std::string(),
                     EmptyAccountId());

  EXPECT_EQ(starting_number_of_profiles + 1u, storage.GetNumberOfProfiles());
  return profile_path;
}

}  // namespace

class ProfileWindowBrowserTest : public InProcessBrowserTest {
 public:
  ProfileWindowBrowserTest() {}
  ~ProfileWindowBrowserTest() override {}

  Browser* OpenGuestBrowser();

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileWindowBrowserTest);
};

Browser* ProfileWindowBrowserTest::OpenGuestBrowser() {
  size_t num_browsers = BrowserList::GetInstance()->size();

  // Create a guest browser nicely. Using CreateProfile() and CreateBrowser()
  // does incomplete initialization that would lead to
  // SystemUrlRequestContextGetter being leaked.
  profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());
  ui_test_utils::WaitForBrowserToOpen();

  DCHECK_NE(static_cast<Profile*>(nullptr),
            g_browser_process->profile_manager()->GetProfileByPath(
                ProfileManager::GetGuestProfilePath()));
  EXPECT_EQ(num_browsers + 1, BrowserList::GetInstance()->size());

  Profile* guest = g_browser_process->profile_manager()->GetProfileByPath(
      ProfileManager::GetGuestProfilePath());
  Browser* browser = chrome::FindAnyBrowser(guest, true);
  EXPECT_TRUE(browser);

  // When |browser| closes a BrowsingDataRemover will be created and executed.
  // It needs a loaded TemplateUrlService or else it hangs on to a
  // CallbackList::Subscription forever.
  search_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(guest));

  return browser;
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, OpenGuestBrowser) {
  EXPECT_TRUE(OpenGuestBrowser());
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, GuestIsIncognito) {
  Browser* guest_browser = OpenGuestBrowser();
  EXPECT_TRUE(guest_browser->profile()->IsOffTheRecord());
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, GuestIgnoresHistory) {
  Browser* guest_browser = OpenGuestBrowser();

  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      guest_browser->profile(), ServiceAccessType::EXPLICIT_ACCESS));

  GURL test_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html")));

  ui_test_utils::NavigateToURL(guest_browser, test_url);
  WaitForHistoryBackendToRun(guest_browser->profile());

  std::vector<GURL> urls =
      ui_test_utils::HistoryEnumerator(guest_browser->profile()).urls();
  ASSERT_EQ(0U, urls.size());
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, GuestClearsCookies) {
  Browser* guest_browser = OpenGuestBrowser();
  Profile* guest_profile = guest_browser->profile();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/set-cookie?cookie1"));

  // Before navigation there are no cookies for the URL.
  std::string cookie = content::GetCookies(guest_profile, url);
  ASSERT_EQ("", cookie);

  // After navigation there is a cookie for the URL.
  ui_test_utils::NavigateToURL(guest_browser, url);
  cookie = content::GetCookies(guest_profile, url);
  EXPECT_EQ("cookie1", cookie);

  CloseBrowserSynchronously(guest_browser);

  // Closing the browser has removed the cookie.
  cookie = content::GetCookies(guest_profile, url);
  ASSERT_EQ("", cookie);
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, GuestCannotSignin) {
  Browser* guest_browser = OpenGuestBrowser();

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
  EXPECT_NE(-1, model_normal_profile.GetIndexOfCommandId(IDC_BOOKMARKS_MENU));

  // Guest browser has no bookmark menu.
  Browser* guest_browser = OpenGuestBrowser();
  AppMenuModel model_guest_profile(&accelerator_handler, guest_browser);
  EXPECT_EQ(-1, model_guest_profile.GetIndexOfCommandId(IDC_BOOKMARKS_MENU));
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, OpenBrowserWindowForProfile) {
  Profile* profile = browser()->profile();
  size_t num_browsers = BrowserList::GetInstance()->size();
  profiles::OpenBrowserWindowForProfile(
      ProfileManager::CreateCallback(), true, false, false, profile,
      Profile::CreateStatus::CREATE_STATUS_INITIALIZED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(num_browsers + 1, BrowserList::GetInstance()->size());
  EXPECT_FALSE(UserManager::IsShowing());
}

// TODO(crbug.com/935746): Test is flaky on Win and Linux.
#if defined(OS_LINUX) || defined(OS_WIN)
#define MAYBE_OpenBrowserWindowForProfileWithSigninRequired \
  DISABLED_OpenBrowserWindowForProfileWithSigninRequired
#else
#define MAYBE_OpenBrowserWindowForProfileWithSigninRequired \
  OpenBrowserWindowForProfileWithSigninRequired
#endif
IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest,
                       MAYBE_OpenBrowserWindowForProfileWithSigninRequired) {
  Profile* profile = browser()->profile();
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(g_browser_process->profile_manager()
                  ->GetProfileAttributesStorage()
                  .GetProfileAttributesWithPath(profile->GetPath(), &entry));
  entry->SetIsSigninRequired(true);
  size_t num_browsers = BrowserList::GetInstance()->size();
  base::RunLoop run_loop;
  UserManager::AddOnUserManagerShownCallbackForTesting(run_loop.QuitClosure());
  profiles::OpenBrowserWindowForProfile(
      ProfileManager::CreateCallback(), true, false, false, profile,
      Profile::CreateStatus::CREATE_STATUS_INITIALIZED);
  run_loop.Run();
  EXPECT_EQ(num_browsers, BrowserList::GetInstance()->size());
  EXPECT_TRUE(UserManager::IsShowing());
}

class ProfileWindowWebUIBrowserTest : public WebUIBrowserTest {
 public:
  void OnSystemProfileCreated(std::string* url_to_test,
                              const base::Closure& quit_loop,
                              Profile* profile,
                              const std::string& url) {
    *url_to_test = url;
    quit_loop.Run();
  }

 private:
  void SetUpOnMainThread() override {
    WebUIBrowserTest::SetUpOnMainThread();
    AddLibrary(base::FilePath(
        FILE_PATH_LITERAL("profile_window_browsertest.js")));
  }
};

IN_PROC_BROWSER_TEST_F(ProfileWindowWebUIBrowserTest,
                       UserManagerFocusSingleProfile) {
  std::string url_to_test;
  base::RunLoop run_loop;
  profiles::CreateSystemProfileForUserManager(
      browser()->profile()->GetPath(),
      profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION,
      base::Bind(&ProfileWindowWebUIBrowserTest::OnSystemProfileCreated,
                 base::Unretained(this),
                 &url_to_test,
                 run_loop.QuitClosure()));
  run_loop.Run();

  ui_test_utils::NavigateToURL(browser(), GURL(url_to_test));
  EXPECT_TRUE(RunJavascriptTest("testNoPodFocused"));
}

// This test is flaky, see https://crbug.com/611619.
IN_PROC_BROWSER_TEST_F(ProfileWindowWebUIBrowserTest,
                       DISABLED_UserManagerFocusMultipleProfiles) {
  // The profile names are meant to sort differently by ICU collation and by
  // naive sorting. See crbug/596280.
  base::FilePath expected_path = CreateTestingProfile("#abc", "Profile 1");
  CreateTestingProfile("?abc", "Profile 2");

  std::string url_to_test;
  base::RunLoop run_loop;
  profiles::CreateSystemProfileForUserManager(
      expected_path,
      profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION,
      base::Bind(&ProfileWindowWebUIBrowserTest::OnSystemProfileCreated,
                 base::Unretained(this),
                 &url_to_test,
                 run_loop.QuitClosure()));
  run_loop.Run();

  ui_test_utils::NavigateToURL(browser(), GURL(url_to_test));
  EXPECT_TRUE(RunJavascriptTest("testPodFocused",
                                base::Value(expected_path.AsUTF8Unsafe())));
}
