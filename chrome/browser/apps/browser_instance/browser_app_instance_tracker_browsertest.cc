// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/browser_instance/browser_app_instance_tracker.h"

#include "ash/constants/web_app_id_constants.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_observer.h"
#include "chrome/browser/ash/system_web_apps/apps/crosh_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/constants.h"

// default implementation of RunTestOnMainThread() and TestBody()
#include "content/public/test/browser_test.h"

namespace {

constexpr char kTitle_A[] = "a.example.org";
// Generated from start URL "https://a.example.org/".
// See |web_app::GenerateAppId|.
constexpr char kAppId_A[] = "dhehpanpcmiafdmbldplnfenbijejdfe";

constexpr char kTitle_B[] = "b.example.org";
// Generated from start URL "https://b.example.org/".
constexpr char kAppId_B[] = "abhkhfladdfdlfmhaokoglcllbamaili";

uint64_t ToUint64(base::UnguessableToken id) {
  // test IDs have only low part set
  DCHECK(!id.GetHighForSerialization());
  return id.GetLowForSerialization();
}

base::UnguessableToken TestId(uint64_t id) {
  return base::UnguessableToken::CreateForTesting(0, id);
}

// Make test sequence easier to scan
constexpr bool kActive = true;
constexpr bool kInactive = false;
constexpr char kAppTab[] = "tab";
constexpr char kAppWindow[] = "window";
constexpr char kChromeWindow[] = "chrome";

struct TestInstance {
  static TestInstance Create(const std::string& name,
                             const apps::BrowserAppInstance& instance) {
    return {
        name,
        ToUint64(instance.id),
        instance.type == apps::BrowserAppInstance::Type::kAppTab ? kAppTab
                                                                 : kAppWindow,
        instance.app_id,
        instance.window,
        instance.title,
        instance.is_web_contents_active,
    };
  }
  static TestInstance Create(const std::string name,
                             const apps::BrowserWindowInstance& instance) {
    return {
        name,
        ToUint64(instance.id),
        kChromeWindow,
        /* app_id= */ "",
        instance.window,
        /* title= */ "",
        /* is_web_contents_active= */ false,
    };
  }
  static TestInstance Create(const apps::BrowserAppInstance* instance) {
    if (instance) {
      return Create("snapshot", *instance);
    }
    return {};
  }
  static TestInstance Create(const apps::BrowserWindowInstance* instance) {
    if (instance) {
      return Create("snapshot", *instance);
    }
    return {};
  }
  std::string name;
  uint64_t id;
  std::string type;
  std::string app_id;
  raw_ptr<aura::Window> window;
  std::string title;
  bool is_web_contents_active;
};

bool operator==(const TestInstance& e1, const TestInstance& e2) {
  return e1.name == e2.name && e1.id == e2.id && e1.type == e2.type &&
         e1.app_id == e2.app_id && e1.window == e2.window &&
         e1.title == e2.title &&
         e1.is_web_contents_active == e2.is_web_contents_active;
}

bool operator<(const TestInstance& e1, const TestInstance& e2) {
  return std::tie(e1.name, e1.id, e1.type, e1.app_id, e1.window, e1.title,
                  e1.is_web_contents_active) <
         std::tie(e2.name, e2.id, e2.type, e2.app_id, e2.window, e2.title,
                  e2.is_web_contents_active);
}

std::ostream& operator<<(std::ostream& os, const TestInstance& e) {
  if (e.name == "") {
    return os << "none";
  }
  return os << e.name << "(id=" << e.id << ",type=" << e.type
            << ",app_id=" << e.app_id << ", title='" << e.title << "'"
            << ", window=" << e.window
            << ", tab=" << (e.is_web_contents_active ? "active" : "inactive")
            << ")";
}

class Tracker : public apps::BrowserAppInstanceTracker {
 public:
  Tracker(Profile* profile, apps::AppRegistryCache& app_registry_cache)
      : apps::BrowserAppInstanceTracker(profile, app_registry_cache) {}

 private:
  base::UnguessableToken GenerateId() const override {
    return TestId(++last_id_);
  }

  mutable uint64_t last_id_{0};
};

class Recorder : public apps::BrowserAppInstanceObserver {
 public:
  explicit Recorder(apps::BrowserAppInstanceTracker& tracker)
      : tracker_(tracker) {
    tracker_->AddObserver(this);
  }

  ~Recorder() override { tracker_->RemoveObserver(this); }

  void OnBrowserWindowAdded(
      const apps::BrowserWindowInstance& instance) override {
    calls_.push_back(TestInstance::Create("added", instance));
  }

  void OnBrowserWindowUpdated(
      const apps::BrowserWindowInstance& instance) override {
    calls_.push_back(TestInstance::Create("updated", instance));
  }

  void OnBrowserWindowRemoved(
      const apps::BrowserWindowInstance& instance) override {
    calls_.push_back(TestInstance::Create("removed", instance));
  }

  void OnBrowserAppAdded(const apps::BrowserAppInstance& instance) override {
    calls_.push_back(TestInstance::Create("added", instance));
  }

  void OnBrowserAppUpdated(const apps::BrowserAppInstance& instance) override {
    calls_.push_back(TestInstance::Create("updated", instance));
  }

  void OnBrowserAppRemoved(const apps::BrowserAppInstance& instance) override {
    calls_.push_back(TestInstance::Create("removed", instance));
  }

  void Verify(const std::vector<TestInstance>& expected_calls) {
    EXPECT_EQ(calls_.size(), expected_calls.size());
    for (size_t i = 0; i < std::max(calls_.size(), expected_calls.size());
         ++i) {
      EXPECT_EQ(Get(calls_, i), Get(expected_calls, i)) << "call #" << i;
    }
  }

  void VerifyIgnoreOrder(const std::vector<TestInstance>& expected_calls) {
    EXPECT_EQ(calls_.size(), expected_calls.size());
    std::vector<TestInstance> expected_calls_copy(expected_calls);
    std::vector<TestInstance> calls_copy(calls_);
    std::sort(expected_calls_copy.begin(), expected_calls_copy.end());
    std::sort(calls_copy.begin(), calls_copy.end());
    for (size_t i = 0;
         i < std::max(calls_copy.size(), expected_calls_copy.size()); ++i) {
      EXPECT_EQ(Get(calls_copy, i), Get(expected_calls_copy, i))
          << "call #" << i;
    }
  }

 private:
  static const TestInstance Get(const std::vector<TestInstance>& calls,
                                size_t i) {
    if (i < calls.size()) {
      return calls[i];
    }
    return {};
  }

  const raw_ref<apps::BrowserAppInstanceTracker> tracker_;
  std::vector<TestInstance> calls_;
};

}  // namespace

class BrowserAppInstanceTrackerTest : public InProcessBrowserTest {
 protected:
  Browser* CreateBrowser() {
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    Browser::CreateParams params(profile, true /* user_gesture */);
    Browser* browser = Browser::Create(params);
    browser->window()->Show();
    return browser;
  }

  Browser* CreatePopupBrowser() {
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    Browser::CreateParams params(profile, true /* user_gesture */);
    params.type = Browser::TYPE_POPUP;
    Browser* browser = Browser::Create(params);
    browser->window()->Show();
    return browser;
  }

  Browser* CreateAppBrowser(const std::string& app_id) {
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    auto params = Browser::CreateParams::CreateForApp(
        "_crx_" + app_id, true /* trusted_source */,
        gfx::Rect(), /* window_bounts */
        profile, true /* user_gesture */);
    Browser* browser = Browser::Create(params);
    browser->window()->Show();
    return browser;
  }

  content::WebContents* NavigateAndWait(Browser* browser,
                                        const std::string& url,
                                        WindowOpenDisposition disposition) {
    NavigateParams params(browser, GURL(url),
                          ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    params.disposition = disposition;
    Navigate(&params);
    auto* contents = params.navigated_or_inserted_contents.get();
    DCHECK_EQ(chrome::FindBrowserWithTab(params.navigated_or_inserted_contents),
              browser);
    content::TestNavigationObserver observer(contents);
    observer.Wait();
    return contents;
  }

  void NavigateActiveTab(Browser* browser, const std::string& url) {
    NavigateAndWait(browser, url, WindowOpenDisposition::CURRENT_TAB);
  }

  content::WebContents* InsertBackgroundTab(Browser* browser,
                                            const std::string& url) {
    return NavigateAndWait(browser, url,
                           WindowOpenDisposition::NEW_BACKGROUND_TAB);
  }

  content::WebContents* InsertForegroundTab(Browser* browser,
                                            const std::string& url) {
    return NavigateAndWait(browser, url,
                           WindowOpenDisposition::NEW_FOREGROUND_TAB);
  }

  webapps::AppId InstallWebApp(
      const std::string& start_url,
      web_app::mojom::UserDisplayMode user_display_mode) {
    auto info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL(start_url));
    info->user_display_mode = user_display_mode;
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    auto app_id = web_app::test::InstallWebApp(profile, std::move(info));
    return app_id;
  }

  webapps::AppId InstallWebAppOpeningAsTab(const std::string& start_url) {
    return InstallWebApp(start_url, web_app::mojom::UserDisplayMode::kBrowser);
  }

  webapps::AppId InstallWebAppOpeningAsWindow(const std::string& start_url) {
    return InstallWebApp(start_url,
                         web_app::mojom::UserDisplayMode::kStandalone);
  }

  void UninstallWebApp(const webapps::AppId& app_id) {
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    web_app::test::UninstallWebApp(profile, app_id);
  }

  uint64_t GetId(content::WebContents* contents) {
    const auto* instance = tracker_->GetAppInstance(contents);
    return instance ? ToUint64(instance->id) : 0;
  }

  uint64_t GetId(Browser* browser) {
    if (const auto* instance = tracker_->GetAppInstance(browser)) {
      return ToUint64(instance->id);
    }
    const auto* instance = tracker_->GetBrowserWindowInstance(browser);
    return instance ? ToUint64(instance->id) : 0;
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    Profile* profile = ProfileManager::GetPrimaryUserProfile();

    tracker_ = std::make_unique<Tracker>(
        profile, apps::AppServiceProxyFactory::GetForProfile(profile)
                     ->AppRegistryCache());

    ASSERT_EQ(kAppId_A, InstallWebAppOpeningAsTab("https://a.example.org"));
    ASSERT_EQ(kAppId_B, InstallWebAppOpeningAsTab("https://b.example.org"));
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest ::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kNoStartupWindow);
  }

 protected:
  std::unique_ptr<Tracker> tracker_;
  const base::ProcessId pid_ = base::Process::Current().Pid();
};

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, InsertAndCloseTabs) {
  Browser* browser = nullptr;
  aura::Window* window = nullptr;
  content::WebContents* tab_app1 = nullptr;
  content::WebContents* tab_app2 = nullptr;

  // Open a foreground tab with a website.
  {
    SCOPED_TRACE("insert an initial foreground tab");
    Recorder recorder(*tracker_);

    browser = CreateBrowser();
    window = browser->window()->GetNativeWindow();
    tab_app1 = InsertForegroundTab(browser, "https://a.example.org");
    EXPECT_EQ(GetId(browser), 1u);
    EXPECT_EQ(GetId(tab_app1), 2u);
    recorder.Verify({
        {"added", 1, kChromeWindow, "", window, "", false},
        {"added", 2, kAppTab, kAppId_A, window, "", kActive},
        {"updated", 2, kAppTab, kAppId_A, window, kTitle_A, kActive},
    });
  }

  // Open a second tab in foreground.
  {
    SCOPED_TRACE("insert a second foreground tab");
    Recorder recorder(*tracker_);

    tab_app2 = InsertForegroundTab(browser, "https://b.example.org");
    EXPECT_EQ(GetId(tab_app2), 3u);
    recorder.Verify({
        {"updated", 2, kAppTab, kAppId_A, window, kTitle_A, kInactive},
        {"added", 3, kAppTab, kAppId_B, window, "", kActive},
        {"updated", 3, kAppTab, kAppId_B, window, kTitle_B, kActive},
    });
  }

  // Open a third tab in foreground with no app.
  {
    SCOPED_TRACE("insert a third foreground tab without app");
    Recorder recorder(*tracker_);

    InsertForegroundTab(browser, "https://c.example.org");
    recorder.Verify({
        {"updated", 3, kAppTab, kAppId_B, window, kTitle_B, kInactive},
    });
  }

  // Open two more tabs in foreground and close them.
  {
    SCOPED_TRACE("insert and close two more tabs");
    Recorder recorder(*tracker_);

    auto* tab_app3 = InsertForegroundTab(browser, "https://a.example.org");
    EXPECT_EQ(GetId(tab_app3), 4u);
    auto* tab_app4 = InsertForegroundTab(browser, "https://b.example.org");
    EXPECT_EQ(GetId(tab_app4), 5u);
    // Close in reverse order.
    int i = browser->tab_strip_model()->GetIndexOfWebContents(tab_app4);
    browser->tab_strip_model()->CloseWebContentsAt(
        i, TabCloseTypes::CLOSE_USER_GESTURE);
    i = browser->tab_strip_model()->GetIndexOfWebContents(tab_app3);
    browser->tab_strip_model()->CloseWebContentsAt(
        i, TabCloseTypes::CLOSE_USER_GESTURE);

    recorder.Verify({
        // tab 4 opened: no events for tab 3 as it has no app
        {"added", 4, kAppTab, kAppId_A, window, "", kActive},
        {"updated", 4, kAppTab, kAppId_A, window, kTitle_A, kActive},
        // tab 5 opened: tab 4 deactivates
        {"updated", 4, kAppTab, kAppId_A, window, kTitle_A, kInactive},
        {"added", 5, kAppTab, kAppId_B, window, "", kActive},
        {"updated", 5, kAppTab, kAppId_B, window, kTitle_B, kActive},
        // tab 5 closed: tab 4 reactivates
        {"removed", 5, kAppTab, kAppId_B, window, kTitle_B, kActive},
        {"updated", 4, kAppTab, kAppId_A, window, kTitle_A, kActive},
        // tab closed: no events for tab 3 as it has no app
        {"removed", 4, kAppTab, kAppId_A, window, kTitle_A, kActive},
    });
  }

  // Close the browser.
  {
    SCOPED_TRACE("close browser");
    Recorder recorder(*tracker_);

    browser->tab_strip_model()->CloseAllTabs();
    recorder.Verify({
        {"removed", 3, kAppTab, kAppId_B, window, kTitle_B, kInactive},
        {"removed", 2, kAppTab, kAppId_A, window, kTitle_A, kInactive},
        {"removed", 1, kChromeWindow, "", window, "", false},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, PopupBrowserWindow) {
  Browser* browser = nullptr;
  aura::Window* window = nullptr;

  {
    SCOPED_TRACE("open popup browser window");
    Recorder recorder(*tracker_);

    browser = CreatePopupBrowser();
    window = browser->window()->GetNativeWindow();
    InsertForegroundTab(browser, "https://c.example.org");

    recorder.Verify({
        {"added", 1, kChromeWindow, "", window, "", false},
    });
  }

  {
    SCOPED_TRACE("close popup browser window");
    Recorder recorder(*tracker_);

    browser->tab_strip_model()->CloseAllTabs();

    recorder.Verify({
        {"removed", 1, kChromeWindow, "", window, "", false},
    });
  }

  {
    // Happens when an app running in a browser tab opens a separate popup
    // window: it's not of type Browser::TYPE_APP_POPUP, but the window contains
    // an instance of this app.
    SCOPED_TRACE("open popup browser window with app tab");
    Recorder recorder(*tracker_);

    browser = CreatePopupBrowser();
    window = browser->window()->GetNativeWindow();
    InsertForegroundTab(browser, "https://a.example.org");

    recorder.Verify({
        {"added", 2, kChromeWindow, "", window, "", false},
        {"added", 3, kAppTab, kAppId_A, window, "", kActive},
        {"updated", 3, kAppTab, kAppId_A, window, kTitle_A, kActive},
    });
  }

  {
    SCOPED_TRACE("close popup browser window with app tab");
    Recorder recorder(*tracker_);

    browser->tab_strip_model()->CloseAllTabs();

    recorder.Verify({
        {"removed", 3, kAppTab, kAppId_A, window, kTitle_A, kActive},
        {"removed", 2, kChromeWindow, "", window, "", false},
    });
  }
}
// Broken on ChromeOS <https://crbug.com/1493240>
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DevtoolsWindow DISABLED_DevtoolsWindow
#else
#define MAYBE_DevtoolsWindow DevtoolsWindow
#endif
IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, MAYBE_DevtoolsWindow) {
  Browser* browser = CreateBrowser();
  InsertForegroundTab(browser, "https://c.example.org");
  aura::Window* window1 = browser->window()->GetNativeWindow();

  {
    SCOPED_TRACE("docked dev tools window");
    Recorder recorder(*tracker_);

    DevToolsWindow* dev_tools_window =
        DevToolsWindowTesting::OpenDevToolsWindowSync(browser,
                                                      /*is_docked=*/true);
    DevToolsWindowTesting::CloseDevToolsWindowSync(dev_tools_window);
    recorder.Verify({});
  }

  {
    SCOPED_TRACE("undocked dev tools window");
    Recorder recorder(*tracker_);

    DevToolsWindow* dev_tools_window =
        DevToolsWindowTesting::OpenDevToolsWindowSync(browser,
                                                      /*is_docked=*/false);
    aura::Window* window2 = DevToolsWindowTesting::Get(dev_tools_window)
                                ->browser()
                                ->window()
                                ->GetNativeWindow();
    DevToolsWindowTesting::CloseDevToolsWindowSync(dev_tools_window);

    recorder.Verify({
        // dev tools window opened
        {"added", 2, kChromeWindow, "", window2, "", false},
        {"updated", 1, kChromeWindow, "", window1, "", false},
        {"updated", 2, kChromeWindow, "", window2, "", false},
        // dev tools window closed
        {"updated", 2, kChromeWindow, "", window2, "", false},
        {"updated", 1, kChromeWindow, "", window1, "", false},
        {"removed", 2, kChromeWindow, "", window2, "", false},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, ForegroundTabNavigate) {
  // Setup: one foreground tab with no app.
  auto* browser = CreateBrowser();
  auto* tab = InsertForegroundTab(browser, "https://c.example.org");
  auto* window = browser->window()->GetNativeWindow();
  EXPECT_EQ(GetId(browser), 1u);
  EXPECT_EQ(GetId(tab), 0u);

  // Navigate the foreground tab to app A.
  {
    SCOPED_TRACE("navigate tab to app A");
    Recorder recorder(*tracker_);

    NavigateActiveTab(browser, "https://a.example.org");
    EXPECT_EQ(GetId(tab), 2u);
    recorder.Verify({
        {"added", 2, kAppTab, kAppId_A, window, kTitle_A, kActive},
    });
  }

  // Navigate the foreground tab to app B.
  {
    SCOPED_TRACE("navigate tab to app B");
    Recorder recorder(*tracker_);

    NavigateActiveTab(browser, "https://b.example.org");
    EXPECT_EQ(GetId(tab), 3u);
    recorder.Verify({
        {"removed", 2, kAppTab, kAppId_A, window, kTitle_A, kActive},
        {"added", 3, kAppTab, kAppId_B, window, kTitle_B, kActive},
    });
  }

  // Navigate the foreground tab to a different subdomain with no app.
  {
    SCOPED_TRACE("navigate tab from app B to a non-app subdomain");
    Recorder recorder(*tracker_);

    NavigateActiveTab(browser, "https://c.example.org");
    EXPECT_EQ(GetId(tab), 0u);
    recorder.Verify({
        {"removed", 3, kAppTab, kAppId_B, window, kTitle_B, kActive},
    });
  }

  // Navigate the foreground tab from a non-app subdomain to app B.
  {
    SCOPED_TRACE("navigate tab from a non-app subdomain to app B");
    Recorder recorder(*tracker_);

    NavigateActiveTab(browser, "https://b.example.org");
    EXPECT_EQ(GetId(tab), 4u);
    recorder.Verify({
        {"added", 4, kAppTab, kAppId_B, window, kTitle_B, kActive},
    });
  }

  // Navigate the foreground tab to a different domain with no app.
  {
    SCOPED_TRACE("navigate tab from app B to a non-app domain");
    Recorder recorder(*tracker_);

    NavigateActiveTab(browser, "https://example.com");
    EXPECT_EQ(GetId(tab), 0u);
    recorder.Verify({
        {"removed", 4, kAppTab, kAppId_B, window, kTitle_B, kActive},
    });
  }

  // Navigate the foreground tab from a non-app domain to app B.
  {
    SCOPED_TRACE("navigate tab from a non-app domain to app B");
    Recorder recorder(*tracker_);

    NavigateActiveTab(browser, "https://b.example.org");
    EXPECT_EQ(GetId(tab), 5u);
    recorder.Verify({
        {"added", 5, kAppTab, kAppId_B, window, kTitle_B, kActive},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, WindowedWebApp) {
  std::string app_id = InstallWebAppOpeningAsWindow("https://d.example.org");

  Browser* browser = nullptr;
  content::WebContents* tab = nullptr;
  aura::Window* window = nullptr;

  // Open app D in a window (configured to open in a window).
  {
    SCOPED_TRACE("create a windowed app in a window");
    Recorder recorder(*tracker_);

    browser = CreateAppBrowser(app_id);
    tab = InsertForegroundTab(browser, "https://d.example.org");
    EXPECT_EQ(GetId(browser), 1u);
    EXPECT_EQ(GetId(tab), 1u);
    window = browser->window()->GetNativeWindow();
    recorder.Verify({
        {"added", 1, kAppWindow, app_id, window, "", kActive},
        {"updated", 1, kAppWindow, app_id, window, "d.example.org", kActive},
    });
  }

  // Close the browser.
  {
    SCOPED_TRACE("close browser");
    Recorder recorder(*tracker_);

    browser->tab_strip_model()->CloseAllTabs();
    recorder.Verify({
        {"removed", 1, kAppWindow, app_id, window, "d.example.org", kActive},
    });
  }

  // Open app A in a window (configured to open in a tab).
  {
    SCOPED_TRACE("create a tabbed app in a window");
    Recorder recorder(*tracker_);

    browser = CreateAppBrowser(kAppId_A);
    tab = InsertForegroundTab(browser, "https://a.example.org");
    EXPECT_EQ(GetId(browser), 2u);
    EXPECT_EQ(GetId(tab), 2u);
    window = browser->window()->GetNativeWindow();
    // When open in a window it's still an app, even if configured to open in a
    // tab.
    recorder.Verify({
        {"added", 2, kAppWindow, kAppId_A, window, "", kActive},
        {"updated", 2, kAppWindow, kAppId_A, window, kTitle_A, kActive},
    });
  }

  // Close the browser.
  {
    SCOPED_TRACE("close browser");
    Recorder recorder(*tracker_);

    browser->tab_strip_model()->CloseAllTabs();
    recorder.Verify({
        {"removed", 2, kAppWindow, kAppId_A, window, kTitle_A, kActive},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, TabbedSystemWebApp) {
  // Make sure we can use crosh.
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(ash::SystemWebAppManager::Get(profile));
  ash::SystemWebAppManager::Get(profile)->InstallSystemAppsForTesting();

  Browser* browser = nullptr;
  aura::Window* window = nullptr;

  {
    SCOPED_TRACE("create an app window");
    Recorder recorder(*tracker_);

    // Open an app window (crosh) and insert a tab.
    browser = CreateAppBrowser(web_app::kCroshAppId);
    chrome::NewTab(browser);
    content::WebContents* tab = browser->tab_strip_model()->GetWebContentsAt(0);
    NavigateActiveTab(browser, chrome::kChromeUIUntrustedCroshURL);
    tab->UpdateTitleForEntry(tab->GetController().GetLastCommittedEntry(),
                             u"crosh1");

    // A window is added, both the window and the tab map to the same app
    // instance.
    EXPECT_EQ(GetId(browser), 1u);
    EXPECT_EQ(GetId(tab), 1u);
    window = browser->window()->GetNativeWindow();
    recorder.Verify({
        {"added", 1, kAppWindow, web_app::kCroshAppId, window, "", kActive},
        {"updated", 1, kAppWindow, web_app::kCroshAppId, window, "crosh1",
         kActive},
    });
  }

  {
    SCOPED_TRACE("add a second tab");
    Recorder recorder(*tracker_);

    // Add a second WebContents to the same app window.
    chrome::NewTab(browser);
    content::WebContents* tab = browser->tab_strip_model()->GetWebContentsAt(1);
    NavigateActiveTab(browser, chrome::kChromeUIUntrustedCroshURL);
    tab->UpdateTitleForEntry(tab->GetController().GetLastCommittedEntry(),
                             u"crosh2");

    // Only title of the existing app instance should be updated.
    EXPECT_EQ(GetId(tab), 1u);
    recorder.Verify({
        {"updated", 1, kAppWindow, web_app::kCroshAppId, window, "", kActive},
        {"updated", 1, kAppWindow, web_app::kCroshAppId, window, "crosh2",
         kActive},
    });
  }

  {
    SCOPED_TRACE("close browser");
    Recorder recorder(*tracker_);

    browser->tab_strip_model()->CloseAllTabs();

    // The app instance disappars with the window.
    recorder.Verify({
        {"removed", 1, kAppWindow, web_app::kCroshAppId, window, "crosh2",
         kActive},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, SwitchTabs) {
  // Setup: one foreground tab and one background tab.
  auto* browser = CreateBrowser();
  auto* window = browser->window()->GetNativeWindow();
  auto* tab0 = InsertForegroundTab(browser, "https://a.example.org");
  EXPECT_EQ(GetId(browser), 1u);
  EXPECT_EQ(GetId(tab0), 2u);
  auto* tab1 = InsertForegroundTab(browser, "https://b.example.org");
  EXPECT_EQ(GetId(tab1), 3u);
  InsertForegroundTab(browser, "https://c.example.org");

  // Switch tabs: no app -> app A
  {
    SCOPED_TRACE("switch tabs to app A");
    Recorder recorder(*tracker_);

    browser->tab_strip_model()->ActivateTabAt(0);
    recorder.Verify({
        {"updated", 2, kAppTab, kAppId_A, window, kTitle_A, kActive},
    });
  }

  // Switch tabs: app A -> app B
  {
    SCOPED_TRACE("switch tabs to app B");
    Recorder recorder(*tracker_);

    browser->tab_strip_model()->ActivateTabAt(1);
    recorder.Verify({
        {"updated", 3, kAppTab, kAppId_B, window, kTitle_B, kActive},
        {"updated", 2, kAppTab, kAppId_A, window, kTitle_A, kInactive},
    });
  }

  // Switch tabs: app B -> no app
  {
    SCOPED_TRACE("switch tabs to no app");
    Recorder recorder(*tracker_);

    browser->tab_strip_model()->ActivateTabAt(2);
    recorder.Verify({
        {"updated", 3, kAppTab, kAppId_B, window, kTitle_B, kInactive},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, TabDrag) {
  // Setup: two browsers: one with two, another with three tabs.
  auto* browser1 = CreateBrowser();
  auto* window1 = browser1->window()->GetNativeWindow();
  auto* b1_tab1 = InsertForegroundTab(browser1, "https://a.example.org");
  auto* b1_tab2 = InsertForegroundTab(browser1, "https://b.example.org");
  EXPECT_EQ(GetId(browser1), 1u);
  EXPECT_EQ(GetId(b1_tab1), 2u);
  EXPECT_EQ(GetId(b1_tab2), 3u);

  auto* browser2 = CreateBrowser();
  auto* window2 = browser2->window()->GetNativeWindow();
  auto* b2_tab1 = InsertForegroundTab(browser2, "https://a.example.org");
  EXPECT_EQ(GetId(browser2), 4u);
  EXPECT_EQ(GetId(b2_tab1), 5u);
  auto* b2_tab2 = InsertForegroundTab(browser2, "https://a.example.org");
  EXPECT_EQ(GetId(b2_tab2), 6u);
  auto* b2_tab3 = InsertForegroundTab(browser2, "https://b.example.org");
  EXPECT_EQ(GetId(b2_tab3), 7u);

  ASSERT_FALSE(browser1->window()->IsActive());
  ASSERT_TRUE(browser2->window()->IsActive());

  // Drag the active tab of browser 2 and rop it into the last position in
  // browser 1.
  SCOPED_TRACE("tab drag and drop");
  Recorder recorder(*tracker_);

  // We skip a step where a detached tab gets inserted into a temporary browser
  // but the sequence there is identical.

  // Detach.
  int src_index = browser2->tab_strip_model()->GetIndexOfWebContents(b2_tab3);
  std::unique_ptr<tabs::TabModel> detached_tab =
      browser2->tab_strip_model()->DetachTabAtForInsertion(src_index);

  // Target browser window goes into foreground right before drop.
  browser1->window()->Activate();

  // Attach.
  int dst_index = browser1->tab_strip_model()->count();
  browser1->tab_strip_model()->InsertDetachedTabAt(
      dst_index, std::move(detached_tab), AddTabTypes::ADD_ACTIVE);
  recorder.Verify({
      // background tab in the dragged-from browser gets activated when the
      // active tab is detached
      {"updated", 6, kAppTab, kAppId_A, window2, kTitle_A, kActive},
      // previously foreground tab in the dragged-into browser goes into
      // background when the dragged tab is attached to the new browser
      {"updated", 3, kAppTab, kAppId_B, window1, kTitle_B, kInactive},
      // dragged tab gets reparented and becomes active in the new browser
      {"updated", 7, kAppTab, kAppId_B, window1, kTitle_B, kActive},
  });
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, MoveTabToAppWindow) {
  // Setup: a browser with two tabs. One tab opens a website that matches the
  // app configured to open in a window.
  std::string app_id = InstallWebAppOpeningAsWindow("https://d.example.org");

  auto* browser1 = CreateBrowser();
  InsertForegroundTab(browser1, "https://c.example.org");
  auto* tab = InsertForegroundTab(browser1, "https://d.example.org");
  EXPECT_EQ(GetId(browser1), 1u);
  EXPECT_EQ(GetId(tab), 0u);
  ASSERT_TRUE(browser1->window()->IsActive());

  // Move the tab from the browser to the newly created app browser. This
  // simulates "open in window".
  SCOPED_TRACE("open in window");
  Recorder recorder(*tracker_);

  auto* browser2 = CreateAppBrowser(app_id);
  EXPECT_EQ(GetId(browser2), 0u);
  auto* window2 = browser2->window()->GetNativeWindow();
  // Target app browser goes into foreground.
  browser2->window()->Activate();

  // Detach.
  int src_index = browser1->tab_strip_model()->GetIndexOfWebContents(tab);
  std::unique_ptr<tabs::TabModel> detached_tab =
      browser1->tab_strip_model()->DetachTabAtForInsertion(src_index);

  // Attach.
  int dst_index = browser2->tab_strip_model()->count();
  browser2->tab_strip_model()->InsertDetachedTabAt(
      dst_index, std::move(detached_tab), AddTabTypes::ADD_ACTIVE);
  recorder.Verify({
      // moved tab gets reparented and becomes an app in the new browser
      {"added", 2, kAppWindow, app_id, window2, "d.example.org", kActive},
  });
}

// TODO(crbug.com/40772830): test tab replace (portals)

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, Accessors) {
  // Setup: two regular browsers, and one app window browser.
  auto* browser1 = CreateBrowser();
  auto* window1 = browser1->window()->GetNativeWindow();
  auto* b1_tab1 = InsertForegroundTab(browser1, "https://a.example.org");
  EXPECT_EQ(GetId(browser1), 1u);
  EXPECT_EQ(GetId(b1_tab1), 2u);
  auto* b1_tab2 = InsertForegroundTab(browser1, "https://c.example.org");
  auto* b1_tab3 = InsertForegroundTab(browser1, "https://b.example.org");
  EXPECT_EQ(GetId(b1_tab3), 3u);

  auto* browser2 = CreateBrowser();
  auto* window2 = browser2->window()->GetNativeWindow();
  auto* b2_tab1 = InsertForegroundTab(browser2, "https://c.example.org");
  auto* b2_tab2 = InsertForegroundTab(browser2, "https://b.example.org");
  EXPECT_EQ(GetId(browser2), 4u);
  EXPECT_EQ(GetId(b2_tab2), 5u);

  auto* browser3 = CreateAppBrowser(kAppId_B);
  auto* window3 = browser3->window()->GetNativeWindow();
  auto* b3_tab1 = InsertForegroundTab(browser3, "https://b.example.org");
  EXPECT_EQ(GetId(b3_tab1), 6u);

  ASSERT_FALSE(browser1->window()->IsActive());
  ASSERT_FALSE(browser2->window()->IsActive());
  ASSERT_TRUE(browser3->window()->IsActive());

  auto* b1_app = tracker_->GetBrowserWindowInstance(browser1);
  auto* b1_tab1_app = tracker_->GetAppInstance(b1_tab1);
  auto* b1_tab2_app = tracker_->GetAppInstance(b1_tab2);
  auto* b1_tab3_app = tracker_->GetAppInstance(b1_tab3);

  auto* b2_app = tracker_->GetBrowserWindowInstance(browser2);
  auto* b2_tab1_app = tracker_->GetAppInstance(b2_tab1);
  auto* b2_tab2_app = tracker_->GetAppInstance(b2_tab2);

  auto* b3_app = tracker_->GetAppInstance(browser3);
  auto* b3_tab1_app = tracker_->GetAppInstance(b3_tab1);

  EXPECT_EQ(
      TestInstance::Create(b1_app),
      (TestInstance{"snapshot", 1, kChromeWindow, "", window1, "", false}));
  EXPECT_EQ(TestInstance::Create(b1_tab1_app),
            (TestInstance{"snapshot", 2, kAppTab, kAppId_A, window1, kTitle_A,
                          kInactive}));
  EXPECT_EQ(TestInstance::Create(b1_tab2_app), TestInstance{});
  EXPECT_EQ(TestInstance::Create(b1_tab3_app),
            (TestInstance{"snapshot", 3, kAppTab, kAppId_B, window1, kTitle_B,
                          kActive}));

  EXPECT_EQ(
      TestInstance::Create(b2_app),
      (TestInstance{"snapshot", 4, kChromeWindow, "", window2, "", false}));
  EXPECT_EQ(TestInstance::Create(b2_tab1_app), TestInstance{});
  EXPECT_EQ(TestInstance::Create(b2_tab2_app),
            (TestInstance{"snapshot", 5, kAppTab, kAppId_B, window2, kTitle_B,
                          kActive}));

  // browser3 does not map to any browser window instance, but it maps to the
  // same app instance as the tab.
  EXPECT_EQ(TestInstance::Create(tracker_->GetBrowserWindowInstance(browser3)),
            TestInstance{});
  EXPECT_EQ(b3_app, b3_tab1_app);
  EXPECT_EQ(TestInstance::Create(b3_tab1_app),
            (TestInstance{"snapshot", 6, kAppWindow, kAppId_B, window3,
                          kTitle_B, kActive}));
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, AppInstall) {
  auto* browser1 = CreateBrowser();
  auto* window1 = browser1->window()->GetNativeWindow();
  auto* tab1 = InsertForegroundTab(browser1, "https://c.example.org");
  InsertForegroundTab(browser1, "https://d.example.org");
  auto* tab3 = InsertForegroundTab(browser1, "https://c.example.org");

  std::string app_id;
  std::string title = "c.example.org";
  {
    SCOPED_TRACE("install app opening in a tab");
    Recorder recorder(*tracker_);

    EXPECT_EQ(GetId(tab1), 0u);
    EXPECT_EQ(GetId(tab3), 0u);
    app_id = InstallWebAppOpeningAsTab("https://c.example.org");
    EXPECT_EQ(GetId(tab1), 2u);
    EXPECT_EQ(GetId(tab3), 3u);
    recorder.Verify({
        {"added", 2, kAppTab, app_id, window1, title, kInactive},
        {"added", 3, kAppTab, app_id, window1, title, kActive},
    });
  }

  {
    SCOPED_TRACE("uninstall app");
    Recorder recorder(*tracker_);

    UninstallWebApp(app_id);
    EXPECT_EQ(GetId(tab1), 0u);
    EXPECT_EQ(GetId(tab3), 0u);
    recorder.Verify({
        {"removed", 2, kAppTab, app_id, window1, title, kInactive},
        {"removed", 3, kAppTab, app_id, window1, title, kActive},
    });
  }

  {
    SCOPED_TRACE("install app opening in a window");
    Recorder recorder(*tracker_);

    EXPECT_EQ(GetId(tab1), 0u);
    EXPECT_EQ(GetId(tab3), 0u);
    app_id = InstallWebAppOpeningAsWindow("https://c.example.org");
    // This has no effect: apps configured to open in a window aren't counted as
    // apps when opened in a tab.
    EXPECT_EQ(GetId(tab1), 0u);
    EXPECT_EQ(GetId(tab3), 0u);
    recorder.Verify({});
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, ActivateTabInstance) {
  // Setup: a browser with 2 tabs (app A and a non-app tab).
  Browser* browser = nullptr;

  content::WebContents* web_contents_a;
  content::WebContents* web_contents_c;

  // Open app A in a tab.
  browser = CreateBrowser();
  web_contents_a = InsertForegroundTab(browser, "https://a.example.org");

  // Open a second tab with no app.
  web_contents_c = InsertForegroundTab(browser, "https://c.example.org");

  EXPECT_EQ(browser->tab_strip_model()->GetActiveWebContents(), web_contents_c);

  tracker_->ActivateTabInstance(tracker_->GetAppInstance(web_contents_a)->id);

  EXPECT_EQ(browser->tab_strip_model()->GetActiveWebContents(), web_contents_a);
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, StopInstancesOfApp) {
  // Setup: a browser with 4 tabs and an app window for app D. The browser
  // contains 2 tabs of app A, one tab of app B and one tab not associated with
  // an app.
  Browser* browser1 = nullptr;
  aura::Window* window1 = nullptr;
  Browser* browser2 = nullptr;
  aura::Window* window2 = nullptr;

  // Open two tabbed instances of app A and 1 instance of app B.
  browser1 = CreateBrowser();
  window1 = browser1->window()->GetNativeWindow();
  content::WebContents* tab;
  tab = InsertForegroundTab(browser1, "https://a.example.org");
  EXPECT_EQ(GetId(tab), 2u);
  tab = InsertForegroundTab(browser1, "https://a.example.org");
  EXPECT_EQ(GetId(tab), 3u);
  tab = InsertForegroundTab(browser1, "https://b.example.org");
  EXPECT_EQ(GetId(tab), 4u);

  // Open a fourth tab with no app.
  InsertForegroundTab(browser1, "https://c.example.org");

  // Open a windowed instance of app D.
  std::string app_d_id = InstallWebAppOpeningAsWindow("https://d.example.org");
  browser2 = CreateAppBrowser(app_d_id);
  window2 = browser2->window()->GetNativeWindow();
  tab = InsertForegroundTab(browser2, "https://d.example.org");
  EXPECT_EQ(GetId(tab), 5u);

  // Stop app A.
  {
    SCOPED_TRACE("close app A");
    Recorder recorder(*tracker_);

    tracker_->StopInstancesOfApp(kAppId_A);

    recorder.VerifyIgnoreOrder({
        {"removed", 3, kAppTab, kAppId_A, window1, kTitle_A, kInactive},
        {"removed", 2, kAppTab, kAppId_A, window1, kTitle_A, kInactive},
    });
  }

  // Stop app D.
  {
    SCOPED_TRACE("close app D");
    Recorder recorder(*tracker_);

    tracker_->StopInstancesOfApp(app_d_id);

    recorder.Verify({
        {"removed", 5, kAppWindow, app_d_id, window2, "d.example.org", kActive},
    });
  }

  // Stop app B.
  {
    SCOPED_TRACE("close app B");
    Recorder recorder(*tracker_);

    tracker_->StopInstancesOfApp(kAppId_B);

    recorder.Verify({
        {"removed", 4, kAppTab, kAppId_B, window1, kTitle_B, kInactive},
    });
  }
}
