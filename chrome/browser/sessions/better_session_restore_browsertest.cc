// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_data_service.h"
#include "chrome/browser/sessions/session_data_service_factory.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/sessions/sessions_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "services/network/public/cpp/features.h"
#include "services/network/test/test_utils.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/scoped_nsautorelease_pool.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#endif

namespace {

const char kTestHeaders[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";

// We need to serve the test files so that PRE_Test and Test can access the same
// page using the same URL. In addition, perceived security origin of the page
// needs to stay the same, so e.g., redirecting the URL requests doesn't
// work. (If we used a test server, the PRE_Test and Test would have separate
// instances running on separate ports.)

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
class FakeBackgroundModeManager : public BackgroundModeManager {
 public:
  FakeBackgroundModeManager()
      : BackgroundModeManager(*base::CommandLine::ForCurrentProcess(),
                              &g_browser_process->profile_manager()
                                   ->GetProfileAttributesStorage()) {}

  void SetBackgroundModeActive(bool active) {
    background_mode_active_ = active;
  }

  bool IsBackgroundModeActive() override { return background_mode_active_; }

 private:
  bool background_mode_active_ = false;
};
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

}  // namespace

class BetterSessionRestoreTest : public InProcessBrowserTest {
 public:
  BetterSessionRestoreTest()
      : fake_server_address_("http://www.test.com/"),
        test_path_("session_restore/"),
        title_pass_(u"PASS"),
        title_storing_(u"STORING"),
        title_error_write_failed_(u"ERROR_WRITE_FAILED"),
        title_error_empty_(u"ERROR_EMPTY") {
    // Set up the URL request filtering.
    test_files_.push_back("common.js");
    test_files_.push_back("cookies.html");
    test_files_.push_back("local_storage.html");
    test_files_.push_back("post.html");
    test_files_.push_back("post_with_password.html");
    test_files_.push_back("session_cookies.html");
    test_files_.push_back("session_storage.html");
    test_files_.push_back("subdomain_cookies.html");

    // We are adding a URLLoaderInterceptor here, instead of in
    // SetUpOnMainThread(), because during a session restore the restored tab
    // comes up before SetUpOnMainThread().  Note that at this point, we do not
    // have a profile.
    url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindLambdaForTesting(
            [&](content::URLLoaderInterceptor::RequestParams* params) {
              std::string path = params->url_request.url.path();
              std::string path_prefix = std::string("/") + test_path_;
              for (auto& it : test_files_) {
                std::string file = path_prefix + it;
                if (path == file) {
                  std::string relative_path(
                      "chrome/test/data/session_restore/");
                  relative_path += it;
                  std::string headers(kTestHeaders);
                  content::URLLoaderInterceptor::WriteResponse(
                      relative_path, params->client.get(), &headers);

                  return true;
                }
              }
              if (path == path_prefix + "posted.php") {
                last_upload_bytes_.clear();
                last_upload_bytes_ =
                    network::GetUploadData(params->url_request);
                content::URLLoaderInterceptor::WriteResponse(
                    kTestHeaders,
                    "<html><head><title>PASS</title></head><body>Data posted"
                    "</body></html>",
                    params->client.get());
                return true;
              }

              return false;
            }));
  }

  BetterSessionRestoreTest(const BetterSessionRestoreTest&) = delete;
  BetterSessionRestoreTest& operator=(const BetterSessionRestoreTest&) = delete;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    crosapi::mojom::BrowserInitParamsPtr init_params =
        crosapi::mojom::BrowserInitParams::New();
    std::string device_account_email = "primaryaccount@gmail.com";
    account_manager::AccountKey key(
        signin::GetTestGaiaIdForEmail(device_account_email),
        ::account_manager::AccountType::kGaia);
    init_params->device_account =
        account_manager::ToMojoAccount({key, device_account_email});
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
    InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
  }
#endif

 protected:
  void SetUpOnMainThread() override {
    SessionServiceTestHelper helper(browser()->profile());
    helper.SetForceBrowserNotAliveWithNoWindows(true);
#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
    g_browser_process->set_background_mode_manager_for_test(
        std::unique_ptr<BackgroundModeManager>(new FakeBackgroundModeManager));
#endif  //  BUILDFLAG(ENABLE_BACKGROUND_MODE)
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  void StoreDataWithPage(const std::string& filename) {
    StoreDataWithPage(browser(), filename);
  }

  // This function succeeds if data for |filename| could be stored successfully.
  // It fails if data already exists or there is an error when writing it.
  void StoreDataWithPage(Browser* browser, const std::string& filename) {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::TitleWatcher title_watcher(web_contents, title_storing_);
    title_watcher.AlsoWaitForTitle(title_pass_);
    title_watcher.AlsoWaitForTitle(title_error_write_failed_);
    title_watcher.AlsoWaitForTitle(title_error_empty_);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser, GURL(fake_server_address_ + test_path_ + filename)));
    std::u16string final_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(title_storing_, final_title);
  }

  void NavigateAndCheckStoredData(const std::string& filename) {
    NavigateAndCheckStoredData(browser(), filename);
  }

  // This function succeeds if data for |filename| is still stored.
  void NavigateAndCheckStoredData(Browser* browser,
                                  const std::string& filename) {
    // Navigate to a page which has previously stored data; check that the
    // stored data can be accessed.
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::TitleWatcher title_watcher(web_contents, title_pass_);
    title_watcher.AlsoWaitForTitle(title_storing_);
    title_watcher.AlsoWaitForTitle(title_error_write_failed_);
    title_watcher.AlsoWaitForTitle(title_error_empty_);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser, GURL(fake_server_address_ + test_path_ + filename)));
    std::u16string final_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(title_pass_, final_title);
  }

  void CheckReloadedPageRestored() {
    CheckTitle(browser(), title_pass_);
  }

  void CheckReloadedPageRestored(Browser* browser) {
    CheckTitle(browser, title_pass_);
  }

  void CheckReloadedPageNotRestored() {
    CheckReloadedPageNotRestored(browser());
  }

  void CheckReloadedPageNotRestored(Browser* browser) {
    CheckTitle(browser, title_storing_);
  }

  void CheckTitle(Browser* browser, const std::u16string& expected_title) {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetWebContentsAt(0);
    content::TitleWatcher title_watcher(web_contents, expected_title);
    title_watcher.AlsoWaitForTitle(title_pass_);
    title_watcher.AlsoWaitForTitle(title_storing_);
    title_watcher.AlsoWaitForTitle(title_error_write_failed_);
    title_watcher.AlsoWaitForTitle(title_error_empty_);
    // It's possible that the title was already the right one before
    // title_watcher was created.
    std::u16string first_title = web_contents->GetTitle();
    if (first_title != title_pass_ &&
        first_title != title_storing_ &&
        first_title != title_error_write_failed_ &&
        first_title != title_error_empty_) {
      std::u16string final_title = title_watcher.WaitAndGetTitle();
      EXPECT_EQ(expected_title, final_title);
    } else {
      EXPECT_EQ(expected_title, first_title);
    }
  }

  // Did the last intercepted upload data contain |search_string|?
  // This method is not thread-safe.  It's called on the UI thread, though
  // the intercept takes place on the IO thread.  It must not be called while an
  // upload is in progress.
  bool DidLastUploadContain(const std::string& search_string) {
    return last_upload_bytes_.find(search_string) != std::string::npos;
  }

  void PostFormWithPage(const std::string& filename, bool password_present) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TitleWatcher title_watcher(web_contents, title_pass_);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(fake_server_address_ + test_path_ + filename)));
    std::u16string final_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(title_pass_, final_title);
    EXPECT_TRUE(DidLastUploadContain("posted-text"));
    EXPECT_TRUE(DidLastUploadContain("text-entered"));
    if (password_present) {
      EXPECT_TRUE(DidLastUploadContain("posted-password"));
      EXPECT_TRUE(DidLastUploadContain("password-entered"));
    }
  }

  void CheckFormRestored(bool text_present, bool password_present) {
    CheckFormRestored(browser(), text_present, password_present);
  }

  void CheckFormRestored(
      Browser* browser, bool text_present, bool password_present) {
    CheckReloadedPageRestored(browser);
    EXPECT_EQ(text_present, DidLastUploadContain("posted-text"));
    EXPECT_EQ(text_present, DidLastUploadContain("text-entered"));
    EXPECT_EQ(password_present, DidLastUploadContain("posted-password"));
    EXPECT_EQ(password_present, DidLastUploadContain("password-entered"));
  }

  virtual Browser* QuitBrowserAndRestore(Browser* browser,
                                         bool close_all_windows) {
    Profile* profile = browser->profile();

    ScopedKeepAlive test_keep_alive(KeepAliveOrigin::PANEL_VIEW,
                                    KeepAliveRestartOption::DISABLED);
    ScopedProfileKeepAlive test_profile_keep_alive(
        profile, ProfileKeepAliveOrigin::kBrowserWindow);

    // Close the browser.
    if (close_all_windows)
      CloseAllBrowsers();
    else
      CloseBrowserSynchronously(browser);

    SessionServiceTestHelper helper(profile);
    helper.SetForceBrowserNotAliveWithNoWindows(true);

    // Create a new window, which may trigger session restore.
    size_t count = BrowserList::GetInstance()->size();
    chrome::NewEmptyWindow(profile);
    if (count == BrowserList::GetInstance()->size())
      return ui_test_utils::WaitForBrowserToOpen();

    return BrowserList::GetInstance()->get(count);
  }

  std::string fake_server_address() {
    return fake_server_address_;
  }

  void SetSecureFakeServerAddress() {
    fake_server_address_ = "https://www.test.com/";
  }

  std::string test_path() { return test_path_; }

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  void EnableBackgroundMode() {
    static_cast<FakeBackgroundModeManager*>(
        g_browser_process->background_mode_manager())->
        SetBackgroundModeActive(true);
  }

  void DisableBackgroundMode() {
    static_cast<FakeBackgroundModeManager*>(
        g_browser_process->background_mode_manager())->
        SetBackgroundModeActive(false);
  }
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

  std::string fake_server_address_;

 private:
  std::string last_upload_bytes_;
  std::vector<std::string> test_files_;
  const std::string test_path_;
  const std::u16string title_pass_;
  const std::u16string title_storing_;
  const std::u16string title_error_write_failed_;
  const std::u16string title_error_empty_;

  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

class ContinueWhereILeftOffTest : public BetterSessionRestoreTest {
 public:
  ContinueWhereILeftOffTest() = default;

  ContinueWhereILeftOffTest(const ContinueWhereILeftOffTest&) = delete;
  ContinueWhereILeftOffTest& operator=(const ContinueWhereILeftOffTest&) =
      delete;

  void SetUpOnMainThread() override {
    BetterSessionRestoreTest::SetUpOnMainThread();
    SessionStartupPref::SetStartupPref(
        browser()->profile(), SessionStartupPref(SessionStartupPref::LAST));
  }

 protected:
  Browser* QuitBrowserAndRestore(Browser* browser,
                                 bool close_all_windows) override {
    SessionRestoreTestHelper session_restore_observer;
    Browser* new_browser = BetterSessionRestoreTest::QuitBrowserAndRestore(
        browser, close_all_windows);
    session_restore_observer.Wait();
    return new_browser;
  }
};

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, PRE_SessionCookies) {
  // Set the startup preference to "continue where I left off" and visit a page
  // which stores a session cookie.
  StoreDataWithPage("session_cookies.html");
  content::EnsureCookiesFlushed(browser()->profile());
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, SessionCookies) {
  // The browsing session will be continued; just wait for the page to reload
  // and check the stored data.
  CheckReloadedPageRestored();
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, PRE_SessionStorage) {
  StoreDataWithPage("session_storage.html");
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, SessionStorage) {
  CheckReloadedPageRestored();
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest,
                       PRE_PRE_LocalStorageClearedOnExit) {
  StoreDataWithPage("local_storage.html");
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest,
                       PRE_LocalStorageClearedOnExit) {
  // Normally localStorage is restored.
  CheckReloadedPageRestored();
  // ... but not if it's set to clear on exit.
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, LocalStorageClearedOnExit) {
  CheckReloadedPageNotRestored();
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest,
                       PRE_PRE_CookiesClearedOnExit) {
  StoreDataWithPage("cookies.html");
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, PRE_CookiesClearedOnExit) {
  // Normally cookies are restored.
  CheckReloadedPageRestored();
  // ... but not if the content setting is set to clear on exit.
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, CookiesClearedOnExit) {
  CheckReloadedPageNotRestored();
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, PRE_Post) {
  PostFormWithPage("post.html", false);
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, Post) {
  CheckFormRestored(true, false);
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, PRE_PostWithPassword) {
  PostFormWithPage("post_with_password.html", true);
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, PostWithPassword) {
  CheckReloadedPageRestored();
  // The form data contained passwords, so it's removed completely.
  CheckFormRestored(false, false);
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, SessionCookiesBrowserClose) {
  // Set the startup preference to "continue where I left off" and visit a page
  // which stores a session cookie.
  StoreDataWithPage("session_cookies.html");
  Browser* new_browser = QuitBrowserAndRestore(browser(), false);
  // The browsing session will be continued; just wait for the page to reload
  // and check the stored data.
  CheckReloadedPageRestored(new_browser);
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, PostBrowserClose) {
  PostFormWithPage("post.html", false);
  Browser* new_browser = QuitBrowserAndRestore(browser(), false);
  CheckFormRestored(new_browser, true, false);
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest,
                       PostWithPasswordBrowserClose) {
  PostFormWithPage("post_with_password.html", true);
  Browser* new_browser = QuitBrowserAndRestore(browser(), false);
  CheckReloadedPageRestored(new_browser);
  // The form data contained passwords, so it's removed completely.
  CheckFormRestored(new_browser, false, false);
}

// Flaky on Mac: https://crbug.com/709504
#if BUILDFLAG(IS_MAC)
#define MAYBE_SessionCookiesCloseAllBrowsers \
  DISABLED_SessionCookiesCloseAllBrowsers
#else
#define MAYBE_SessionCookiesCloseAllBrowsers SessionCookiesCloseAllBrowsers
#endif
// Check that session cookies are cleared on a wrench menu quit.
IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest,
                       MAYBE_SessionCookiesCloseAllBrowsers) {
  // Set the startup preference to "continue where I left off" and visit a page
  // which stores a session cookie.
  StoreDataWithPage("session_cookies.html");
  Browser* new_browser = QuitBrowserAndRestore(browser(), true);
  // The browsing session will be continued; just wait for the page to reload
  // and check the stored data.
  CheckReloadedPageRestored(new_browser);
}

// Check that form data is restored after wrench menu quit.
IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, PostCloseAllBrowsers) {
  PostFormWithPage("post.html", false);
  Browser* new_browser = QuitBrowserAndRestore(browser(), true);
  CheckFormRestored(new_browser, true, false);
}

// Check that form data with a password field is cleared after wrench menu quit.
IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest,
                       PostWithPasswordCloseAllBrowsers) {
  PostFormWithPage("post_with_password.html", true);
  Browser* new_browser = QuitBrowserAndRestore(browser(), true);
  CheckReloadedPageRestored(new_browser);
  // The form data contained passwords, so it's removed completely.
  CheckFormRestored(new_browser, false, false);
}

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest,
                       CookiesClearedOnBrowserClose) {
  StoreDataWithPage("cookies.html");
  // Normally cookies are restored.
  Browser* new_browser = QuitBrowserAndRestore(browser(), false);
  CheckReloadedPageRestored(new_browser);
  // ... but not if the content setting is set to clear on exit.
  CookieSettingsFactory::GetForProfile(new_browser->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);

  EnableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, false);
    CheckReloadedPageNotRestored(new_browser);

  DisableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, false);
    CheckReloadedPageNotRestored(new_browser);
}

// Check that cookies are cleared on a wrench menu quit only if cookies are set
// to current session only, regardless of whether background mode is enabled.
IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest,
                       CookiesClearedOnCloseAllBrowsers) {
  StoreDataWithPage("cookies.html");
  // Normally cookies are restored.
  Browser* new_browser = QuitBrowserAndRestore(browser(), true);
  CheckReloadedPageRestored(new_browser);
  // ... but not if the content setting is set to clear on exit.
  CookieSettingsFactory::GetForProfile(new_browser->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  // ... even if background mode is active.
  EnableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, true);
  CheckReloadedPageNotRestored(new_browser);

  DisableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, true);
  CheckReloadedPageNotRestored(new_browser);
}

#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)
// ChromeOS does not override the SessionStartupPreference upon controlled
// system restart.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
class RestartTest : public BetterSessionRestoreTest {
 public:
  RestartTest() = default;

  RestartTest(const RestartTest&) = delete;
  RestartTest& operator=(const RestartTest&) = delete;

  ~RestartTest() override = default;

 protected:
  void Restart() {
    // Simulate restarting the browser, but let the test exit peacefully.
    for (Browser* browser : *BrowserList::GetInstance()) {
      browser->profile()->SaveSessionState();
      SessionDataServiceFactory::GetForProfile(browser->profile())
          ->SetForceKeepSessionState();
    }
    PrefService* pref_service = g_browser_process->local_state();
    pref_service->SetBoolean(prefs::kWasRestarted, true);
  }
};

IN_PROC_BROWSER_TEST_F(RestartTest, PRE_SessionCookies) {
  StoreDataWithPage("session_cookies.html");
  content::EnsureCookiesFlushed(browser()->profile());
  Restart();
}

IN_PROC_BROWSER_TEST_F(RestartTest, SessionCookies) {
  CheckReloadedPageRestored();
}

IN_PROC_BROWSER_TEST_F(RestartTest, PRE_SessionStorage) {
  StoreDataWithPage("session_storage.html");
  Restart();
}

IN_PROC_BROWSER_TEST_F(RestartTest, SessionStorage) {
  CheckReloadedPageRestored();
}

IN_PROC_BROWSER_TEST_F(RestartTest, PRE_LocalStorageClearedOnExit) {
  StoreDataWithPage("local_storage.html");
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  Restart();
}

IN_PROC_BROWSER_TEST_F(RestartTest, LocalStorageClearedOnExit) {
  CheckReloadedPageRestored();
}

IN_PROC_BROWSER_TEST_F(RestartTest, PRE_CookiesClearedOnExit) {
  StoreDataWithPage("cookies.html");
  content::EnsureCookiesFlushed(browser()->profile());
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  Restart();
}

IN_PROC_BROWSER_TEST_F(RestartTest, CookiesClearedOnExit) {
  CheckReloadedPageRestored();
}

IN_PROC_BROWSER_TEST_F(RestartTest, PRE_Post) {
  PostFormWithPage("post.html", false);
  Restart();
}

IN_PROC_BROWSER_TEST_F(RestartTest, Post) {
  CheckFormRestored(true, false);
}

IN_PROC_BROWSER_TEST_F(RestartTest, PRE_PostWithPassword) {
  PostFormWithPage("post_with_password.html", true);
  Restart();
}

IN_PROC_BROWSER_TEST_F(RestartTest, PostWithPassword) {
  // The form data contained passwords, so it's removed completely.
  CheckFormRestored(false, false);
}
#endif

// These tests ensure that the Better Session Restore features are not triggered
// when they shouldn't be.
class NoSessionRestoreTest : public BetterSessionRestoreTest {
 public:
  NoSessionRestoreTest() = default;

  NoSessionRestoreTest(const NoSessionRestoreTest&) = delete;
  NoSessionRestoreTest& operator=(const NoSessionRestoreTest&) = delete;

  void SetUpOnMainThread() override {
    BetterSessionRestoreTest::SetUpOnMainThread();
    SessionStartupPref::SetStartupPref(
        browser()->profile(), SessionStartupPref(SessionStartupPref::DEFAULT));
  }
};

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, PRE_SessionCookies) {
  StoreDataWithPage("session_cookies.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, SessionCookies) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(std::string(url::kAboutBlankURL), web_contents->GetURL().spec());
  // When we navigate to the page again, it doens't see the data previously
  // stored.
  StoreDataWithPage("session_cookies.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, PRE_SessionStorage) {
  StoreDataWithPage("session_storage.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, SessionStorage) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(std::string(url::kAboutBlankURL), web_contents->GetURL().spec());
  StoreDataWithPage("session_storage.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest,
                       PRE_PRE_LocalStorageClearedOnExit) {
  StoreDataWithPage("local_storage.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, PRE_LocalStorageClearedOnExit) {
  // Normally localStorage is persisted.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(std::string(url::kAboutBlankURL), web_contents->GetURL().spec());
  NavigateAndCheckStoredData("local_storage.html");
  // ... but not if it's set to clear on exit.
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, LocalStorageClearedOnExit) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(std::string(url::kAboutBlankURL), web_contents->GetURL().spec());
  StoreDataWithPage("local_storage.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, PRE_PRE_CookiesClearedOnExit) {
  StoreDataWithPage("cookies.html");
  content::EnsureCookiesFlushed(browser()->profile());
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, PRE_CookiesClearedOnExit) {
  // Normally cookies are restored.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(std::string(url::kAboutBlankURL), web_contents->GetURL().spec());
  NavigateAndCheckStoredData("cookies.html");
  // ... but not if the content setting is set to clear on exit.
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, CookiesClearedOnExit) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(std::string(url::kAboutBlankURL), web_contents->GetURL().spec());
  StoreDataWithPage("cookies.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, PRE_CookiesClearedOnStartup) {
  // Normally cookies are restored.
  StoreDataWithPage("cookies.html");
  content::EnsureCookiesFlushed(browser()->profile());
  // ... but not if the content setting is set to clear on exit.
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);

  // Disable cookie and storage deletion on shutdown to simulate the
  // process being killed before cleanup is finished.
  browser()->profile()->SaveSessionState();
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, CookiesClearedOnStartup) {
  // Check that the deletion is performed on startup instead.
  StoreDataWithPage("cookies.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, PRE_LocalStorageClearedOnStartup) {
  // Normally localStorage is persisted.
  StoreDataWithPage("local_storage.html");
  // ... but not if it's set to clear on exit.
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);

  // Disable cookie and storage deletion on shutdown to simulate the
  // process being killed before cleanup is finished.
  browser()->profile()->SaveSessionState();
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, LocalStorageClearedOnStartup) {
  // Check that the deletion is performed on startup instead.
  StoreDataWithPage("local_storage.html");
}

class NoSessionRestoreTestWithStartupDeletionDisabled
    : public NoSessionRestoreTest {
 public:
  NoSessionRestoreTestWithStartupDeletionDisabled() {
    feature_list_.InitAndDisableFeature(kDeleteSessionOnlyDataOnStartup);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTestWithStartupDeletionDisabled,
                       PRE_CookiesClearedOnStartup) {
  // Normally cookies are restored.
  StoreDataWithPage("cookies.html");
  content::EnsureCookiesFlushed(browser()->profile());
  // ... but not if the content setting is set to clear on exit.
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);

  // Disable cookie and storage deletion handling on shutdown to simulate the
  // process being killed before cleanup is finished.
  browser()->profile()->SaveSessionState();
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTestWithStartupDeletionDisabled,
                       CookiesClearedOnStartup) {
  // Check that the deletion is not performed when the feature is disabled.
  NavigateAndCheckStoredData("cookies.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTestWithStartupDeletionDisabled,
                       PRE_LocalStorageClearedOnStartup) {
  // Normally localStorage is persisted.
  StoreDataWithPage("local_storage.html");
  // ... but not if it's set to clear on exit.
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);

  // Disable cookie and storage deletion handling on shutdown to simulate the
  // process being killed before cleanup is finished.
  browser()->profile()->SaveSessionState();
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTestWithStartupDeletionDisabled,
                       LocalStorageClearedOnStartup) {
  // Check that the deletion is not performed when the feature is disabled.
  NavigateAndCheckStoredData("local_storage.html");
}

// Tests that session cookies are not cleared when only a popup window is open.
IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest,
                       SessionCookiesBrowserCloseWithPopupOpen) {
  StoreDataWithPage("session_cookies.html");
  Browser* popup = Browser::Create(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true));
  popup->window()->Show();
  Browser* new_browser = QuitBrowserAndRestore(browser(), false);
  NavigateAndCheckStoredData(new_browser, "session_cookies.html");
}

// Tests that session cookies are cleared if the last window to close is a
// popup.
IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest,
                       SessionCookiesBrowserClosePopupLast) {
  StoreDataWithPage("session_cookies.html");
  Browser* popup = Browser::Create(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true));
  popup->window()->Show();
  CloseBrowserSynchronously(browser());
  Browser* new_browser = QuitBrowserAndRestore(popup, false);
  if (browser_defaults::kBrowserAliveWithNoWindows)
    NavigateAndCheckStoredData(new_browser, "session_cookies.html");
  else
    StoreDataWithPage(new_browser, "session_cookies.html");
}

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
// Check that cookies are cleared on a wrench menu quit only if cookies are set
// to current session only, regardless of whether background mode is enabled.
IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest,
                       SubdomainCookiesClearedOnCloseAllBrowsers) {
  StoreDataWithPage("subdomain_cookies.html");

  // Normally cookies are restored.
  Browser* new_browser = QuitBrowserAndRestore(browser(), true);
  NavigateAndCheckStoredData(new_browser, "subdomain_cookies.html");

  // ... but not if the content setting is set to clear on exit.
  auto cookie_settings =
      CookieSettingsFactory::GetForProfile(new_browser->profile());
  cookie_settings->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  cookie_settings->SetCookieSetting(GURL("http://www.test.com"),
                                    CONTENT_SETTING_SESSION_ONLY);

  // Cookie for .test.com is created on www.test.com and deleted on shutdown.
  new_browser = QuitBrowserAndRestore(new_browser, true);
  StoreDataWithPage(new_browser, "subdomain_cookies.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, CookiesClearedOnBrowserClose) {
  StoreDataWithPage("cookies.html");

  // Normally cookies are restored.
  Browser* new_browser = QuitBrowserAndRestore(browser(), false);
  NavigateAndCheckStoredData(new_browser, "cookies.html");

  // ... but not if the content setting is set to clear on exit.
  CookieSettingsFactory::GetForProfile(new_browser->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);

  EnableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, false);
    StoreDataWithPage(new_browser, "cookies.html");
  DisableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, false);
    StoreDataWithPage(new_browser, "cookies.html");
}

// Check that session cookies are cleared on a wrench menu quit.
IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, SessionCookiesCloseAllBrowsers) {
  StoreDataWithPage("session_cookies.html");
  EnableBackgroundMode();
  Browser* new_browser = QuitBrowserAndRestore(browser(), true);
  StoreDataWithPage(new_browser, "session_cookies.html");
  DisableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, true);
  StoreDataWithPage(new_browser, "session_cookies.html");
}

// Check that cookies are cleared on a wrench menu quit only if cookies are set
// to current session only, regardless of whether background mode is enabled.
IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, CookiesClearedOnCloseAllBrowsers) {
  StoreDataWithPage("cookies.html");

  // Normally cookies are restored.
  Browser* new_browser = QuitBrowserAndRestore(browser(), true);
  NavigateAndCheckStoredData(new_browser, "cookies.html");

  // ... but not if the content setting is set to clear on exit.
  CookieSettingsFactory::GetForProfile(new_browser->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  // ... even if background mode is active.
  EnableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, true);
  StoreDataWithPage(new_browser, "cookies.html");
  DisableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, true);
  StoreDataWithPage(new_browser, "cookies.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, SessionCookiesBrowserClose) {
  StoreDataWithPage("session_cookies.html");
  EnableBackgroundMode();
  Browser* new_browser = QuitBrowserAndRestore(browser(), false);
  if (browser_defaults::kBrowserAliveWithNoWindows)
    NavigateAndCheckStoredData(new_browser, "session_cookies.html");
  else
    StoreDataWithPage(new_browser, "session_cookies.html");
  DisableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, false);
  if (browser_defaults::kBrowserAliveWithNoWindows)
    NavigateAndCheckStoredData(new_browser, "session_cookies.html");
  else
    StoreDataWithPage(new_browser, "session_cookies.html");
}

#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)
