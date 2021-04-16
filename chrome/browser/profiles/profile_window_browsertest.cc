// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_window.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#error "This test verifies the Desktop implementation of Guest only."
#endif

namespace {

enum ProfileWindowType { INCOGNITO, GUEST, EPHEMERAL_GUEST };

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
  WaitForHistoryTask() = default;
  WaitForHistoryTask(const WaitForHistoryTask&) = delete;
  WaitForHistoryTask& operator=(const WaitForHistoryTask&) = delete;

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    return true;
  }

  void DoneRunOnMainThread() override {
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

 private:
  ~WaitForHistoryTask() override = default;
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

}  // namespace

class ProfileWindowBrowserTest : public InProcessBrowserTest {
 public:
  ProfileWindowBrowserTest() = default;
  ProfileWindowBrowserTest(const ProfileWindowBrowserTest&) = delete;
  ProfileWindowBrowserTest& operator=(const ProfileWindowBrowserTest&) = delete;
  ~ProfileWindowBrowserTest() override = default;
};

class ProfileWindowCountBrowserTest
    : public ProfileWindowBrowserTest,
      public testing::WithParamInterface<ProfileWindowType> {
 protected:
  ProfileWindowCountBrowserTest() {
    ProfileWindowType profile_type = GetParam();
    is_incognito_ = profile_type == ProfileWindowType::INCOGNITO;
    if (!is_incognito_)
      TestingProfile::SetScopedFeatureListForEphemeralGuestProfiles(
          scoped_feature_list_,
          profile_type == ProfileWindowType::EPHEMERAL_GUEST);
  }

  int GetWindowCount() {
    return is_incognito_ ? BrowserList::GetOffTheRecordBrowsersActiveForProfile(
                               browser()->profile())
                         : BrowserList::GetGuestBrowserCount();
  }

  Browser* CreateGuestOrIncognitoBrowser() {
    Browser* new_browser;
    // When |profile_| is null this means no browsers have been created,
    // this is the first browser instance.
    // |is_incognito_| is used to determine which browser type to open.
    if (!profile_) {
      new_browser = is_incognito_ ? CreateIncognitoBrowser(browser()->profile())
                                  : CreateGuestBrowser();
      profile_ = new_browser->profile();
    } else {
      if (profile_->IsEphemeralGuestProfile())
        new_browser = CreateBrowser(profile_);
      else
        new_browser = CreateIncognitoBrowser(profile_);
    }

    return new_browser;
  }

 private:
  bool is_incognito_;
  base::test::ScopedFeatureList scoped_feature_list_;
  Profile* profile_ = nullptr;
};

// TODO(crbug.com/1186994): Test is flaky on Linux Dbg.
#if defined(OS_LINUX) && !defined(NDEBUG)
#define MAYBE_CountProfileWindows DISABLED_CountProfileWindows
#else
#define MAYBE_CountProfileWindows CountProfileWindows
#endif
IN_PROC_BROWSER_TEST_P(ProfileWindowCountBrowserTest,
                       MAYBE_CountProfileWindows) {
  DCHECK_EQ(0, GetWindowCount());

  // Create a browser and check the count.
  Browser* browser1 = CreateGuestOrIncognitoBrowser();
  DCHECK_EQ(1, GetWindowCount());

  // Create another browser and check the count.
  Browser* browser2 = CreateGuestOrIncognitoBrowser();
  DCHECK_EQ(2, GetWindowCount());

  // Open a docked DevTool window and count.
  DevToolsWindow* devtools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser1, true);
  DCHECK_EQ(2, GetWindowCount());
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);

  // Open a detached DevTool window and count.
  devtools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser1, false);
  DCHECK_EQ(2, GetWindowCount());
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);

  // Close one browser and count.
  CloseBrowserSynchronously(browser2);
  DCHECK_EQ(1, GetWindowCount());

  // Close another browser and count.
  CloseBrowserSynchronously(browser1);
  DCHECK_EQ(0, GetWindowCount());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ProfileWindowCountBrowserTest,
                         testing::Values(ProfileWindowType::INCOGNITO,
                                         ProfileWindowType::GUEST,
                                         ProfileWindowType::EPHEMERAL_GUEST));

class GuestProfileWindowBrowserTest : public ProfileWindowBrowserTest,
                                      public testing::WithParamInterface<bool> {
 protected:
  GuestProfileWindowBrowserTest() {
    is_ephemeral_ = GetParam();

    // Change the value if Ephemeral is not supported.
    is_ephemeral_ &=
        TestingProfile::SetScopedFeatureListForEphemeralGuestProfiles(
            scoped_feature_list_, is_ephemeral_);
  }

  bool IsEphemeral() { return is_ephemeral_; }

 private:
  bool is_ephemeral_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GuestProfileWindowBrowserTest, OpenGuestBrowser) {
  EXPECT_TRUE(CreateGuestBrowser());
}

IN_PROC_BROWSER_TEST_P(GuestProfileWindowBrowserTest, GuestIsOffTheRecord) {
  Profile* guest_profile = CreateGuestBrowser()->profile();
  if (IsEphemeral())
    EXPECT_FALSE(guest_profile->IsOffTheRecord());
  else
    EXPECT_TRUE(guest_profile->IsOffTheRecord());
}

IN_PROC_BROWSER_TEST_P(GuestProfileWindowBrowserTest, GuestIgnoresHistory) {
  Browser* guest_browser = CreateGuestBrowser();

  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      guest_browser->profile(), ServiceAccessType::EXPLICIT_ACCESS));

  GURL test_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html")));

  ui_test_utils::NavigateToURL(guest_browser, test_url);
  WaitForHistoryBackendToRun(guest_browser->profile());

  std::vector<GURL> urls =
      ui_test_utils::HistoryEnumerator(guest_browser->profile()).urls();

  unsigned int expect_history =
      guest_browser->profile()->IsEphemeralGuestProfile() ? 1 : 0;
  ASSERT_EQ(expect_history, urls.size());
}

IN_PROC_BROWSER_TEST_P(GuestProfileWindowBrowserTest, GuestClearsCookies) {
  Browser* guest_browser = CreateGuestBrowser();
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

IN_PROC_BROWSER_TEST_P(GuestProfileWindowBrowserTest,
                       GuestClearsFindInPageCache) {
  Browser* guest_browser = CreateGuestBrowser();
  Profile* guest_profile = guest_browser->profile();

  std::u16string fip_text = u"first guest session search text";
  FindBarStateFactory::GetForBrowserContext(guest_profile)
      ->SetLastSearchText(fip_text);

  // Open a second guest window and close one. This should not affect the find
  // in page cache as the guest session hasn't been ended.
  profiles::FindOrCreateNewWindowForProfile(
      guest_profile, chrome::startup::IS_NOT_PROCESS_STARTUP,
      chrome::startup::IS_NOT_FIRST_RUN, true /*always_create*/);
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
  // For ephemeral Guest profiles, after closing the last Guest browser the
  // Guest profile is scheduled for deletion and is not considered a Guest
  // profile anymore. Therefore the next Guest window requires opening a new
  // browser and refreshing the profile object.
  if (IsEphemeral()) {
    guest_profile = CreateGuestBrowser()->profile();
  } else {
    profiles::FindOrCreateNewWindowForProfile(
        guest_profile, chrome::startup::IS_NOT_PROCESS_STARTUP,
        chrome::startup::IS_NOT_FIRST_RUN, true /*always_create*/);
  }
  EXPECT_EQ(std::u16string(),
            FindBarStateFactory::GetForBrowserContext(guest_profile)
                ->GetSearchPrepopulateText());
}

IN_PROC_BROWSER_TEST_P(GuestProfileWindowBrowserTest, GuestCannotSignin) {
  // TODO(https://crbug.com/1125474): Enable the test after identity manager is
  // updated for ephemeral Guest profiles.
  if (IsEphemeral())
    return;

  Browser* guest_browser = CreateGuestBrowser();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(guest_browser->profile());

  // Guest profiles can't sign in without a IdentityManager.
  ASSERT_FALSE(identity_manager);
}

IN_PROC_BROWSER_TEST_P(GuestProfileWindowBrowserTest,
                       GuestAppMenuLacksBookmarks) {
  EmptyAcceleratorHandler accelerator_handler;
  // Verify the normal browser has a bookmark menu.
  AppMenuModel model_normal_profile(&accelerator_handler, browser());
  model_normal_profile.Init();
  EXPECT_NE(-1, model_normal_profile.GetIndexOfCommandId(IDC_BOOKMARKS_MENU));

  // Guest browser has no bookmark menu.
  Browser* guest_browser = CreateGuestBrowser();
  AppMenuModel model_guest_profile(&accelerator_handler, guest_browser);
  EXPECT_EQ(-1, model_guest_profile.GetIndexOfCommandId(IDC_BOOKMARKS_MENU));
}

INSTANTIATE_TEST_SUITE_P(GuestProfileWindowBrowserTest,
                         GuestProfileWindowBrowserTest,
                         /*is_ephemeral=*/testing::Bool());

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, OpenBrowserWindowForProfile) {
  Profile* profile = browser()->profile();
  size_t num_browsers = BrowserList::GetInstance()->size();
  profiles::OpenBrowserWindowForProfile(
      ProfileManager::CreateCallback(), true, false, false, profile,
      Profile::CreateStatus::CREATE_STATUS_INITIALIZED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(num_browsers + 1, BrowserList::GetInstance()->size());
  EXPECT_FALSE(ProfilePicker::IsOpen());
}

// TODO(crbug.com/935746): Test is flaky on Win and Linux.
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_WIN)
#define MAYBE_OpenBrowserWindowForProfileWithSigninRequired \
  DISABLED_OpenBrowserWindowForProfileWithSigninRequired
#else
#define MAYBE_OpenBrowserWindowForProfileWithSigninRequired \
  OpenBrowserWindowForProfileWithSigninRequired
#endif
IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest,
                       MAYBE_OpenBrowserWindowForProfileWithSigninRequired) {
  Profile* profile = browser()->profile();
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->SetIsSigninRequired(true);
  size_t num_browsers = BrowserList::GetInstance()->size();
  base::RunLoop run_loop;
  ProfilePicker::AddOnProfilePickerOpenedCallbackForTesting(
      run_loop.QuitClosure());
  profiles::OpenBrowserWindowForProfile(
      ProfileManager::CreateCallback(), true, false, false, profile,
      Profile::CreateStatus::CREATE_STATUS_INITIALIZED);
  run_loop.Run();
  EXPECT_EQ(num_browsers, BrowserList::GetInstance()->size());
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

class ProfileWindowWebUIBrowserTest : public WebUIBrowserTest {
 public:
  void OnSystemProfileCreated(std::string* url_to_test,
                              base::OnceClosure quit_loop,
                              Profile* profile,
                              const std::string& url) {
    *url_to_test = url;
    std::move(quit_loop).Run();
  }

 private:
  void SetUpOnMainThread() override {
    WebUIBrowserTest::SetUpOnMainThread();
    AddLibrary(base::FilePath(
        FILE_PATH_LITERAL("profile_window_browsertest.js")));
  }
};
