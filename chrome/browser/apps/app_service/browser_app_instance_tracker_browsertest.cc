// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_instance.h"
#include "chrome/browser/apps/app_service/browser_app_instance_observer.h"
#include "chrome/browser/apps/app_service/browser_app_instance_tracker.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/constants.h"

// default implementation of RunTestOnMainThread() and TestBody()
#include "content/public/test/browser_test.h"

namespace {

using extension_misc::kChromeAppId;

constexpr char kURL_A[] = "https://a.example.org";
constexpr char kTitle_A[] = "a.example.org";
// Generated from start URL "https://a.example.org/".
// See |web_app::GenerateAppId|.
constexpr char kAppId_A[] = "dhehpanpcmiafdmbldplnfenbijejdfe";

constexpr char kURL_B[] = "https://b.example.org";
constexpr char kTitle_B[] = "b.example.org";
// Generated from start URL "https://b.example.org/".
constexpr char kAppId_B[] = "abhkhfladdfdlfmhaokoglcllbamaili";

uint64_t ToUint64(apps::BrowserAppInstanceId id) {
  // test IDs have only low part set
  DCHECK(!id.GetHighForSerialization());
  return id.GetLowForSerialization();
}

apps::BrowserAppInstanceId TestId(uint64_t id) {
  return base::UnguessableToken::Deserialize(0, id);
}

struct TestInstance {
  static TestInstance Create(const std::string name,
                             const apps::BrowserAppInstance& instance) {
    return {
        name,
        ToUint64(instance.id),
        instance.type,
        instance.app_id,
        instance.window,
        instance.title.value_or(""),
        instance.is_browser_visible,
        instance.is_browser_active,
        instance.is_web_contents_active.value_or(false),
    };
  }
  static TestInstance Create(const apps::BrowserAppInstance* instance) {
    if (instance) {
      return Create("snapshot", *instance);
    }
    return {};
  }
  std::string name;
  uint64_t id;
  apps::BrowserAppInstance::Type type;
  std::string app_id;
  aura::Window* window;
  std::string title;
  bool is_browser_visible;
  bool is_browser_active;
  bool is_web_contents_active;
};

// Make test sequence easier to scan
constexpr bool kVisible = true;
constexpr bool kHidden = false;
constexpr bool kActive = true;
constexpr bool kInactive = false;
constexpr bool kIgnored = false;
constexpr auto kAppTab = apps::BrowserAppInstance::Type::kAppTab;
constexpr auto kAppWindow = apps::BrowserAppInstance::Type::kAppWindow;
constexpr auto kChromeWindow = apps::BrowserAppInstance::Type::kChromeWindow;

bool operator==(const TestInstance& e1, const TestInstance& e2) {
  return e1.name == e2.name && e1.id == e2.id && e1.type == e2.type &&
         e1.app_id == e2.app_id && e1.window == e2.window &&
         e1.title == e2.title &&
         e1.is_browser_visible == e2.is_browser_visible &&
         e1.is_browser_active == e2.is_browser_active &&
         e1.is_web_contents_active == e2.is_web_contents_active;
}

bool operator!=(const TestInstance& e1, const TestInstance& e2) {
  return !(e1 == e2);
}

std::ostream& operator<<(std::ostream& os, const TestInstance& e) {
  if (e.name == "") {
    return os << "none";
  }
  os << e.name << "(id=" << e.id << ",app_id=" << e.app_id << ",type=";
  switch (e.type) {
    case apps::BrowserAppInstance::Type::kAppTab:
      os << "kAppTab";
      break;
    case apps::BrowserAppInstance::Type::kAppWindow:
      os << "kAppWindow";
      break;
    case apps::BrowserAppInstance::Type::kChromeWindow:
      os << "kChromeWindow";
      break;
  }
  os << ", title='" << e.title << "'";
  os << ", window=" << e.window;
  os << ", browser=" << (e.is_browser_visible ? "visible" : "hidden");
  os << "," << (e.is_browser_active ? "active" : "inactive");
  os << ", tab=" << (e.is_web_contents_active ? "active" : "inactive");
  return os << ")";
}

class Tracker : public apps::BrowserAppInstanceTracker {
 public:
  Tracker(Profile* profile, apps::AppRegistryCache& app_registry_cache)
      : apps::BrowserAppInstanceTracker(profile, app_registry_cache) {}

 private:
  apps::BrowserAppInstanceId GenerateId() const override {
    return TestId(++last_id_);
  }

  mutable uint64_t last_id_{0};
};

class Recorder : public apps::BrowserAppInstanceObserver {
 public:
  explicit Recorder(apps::BrowserAppInstanceTracker& tracker)
      : tracker_(tracker) {
    tracker_.AddObserver(this);
  }

  ~Recorder() override { tracker_.RemoveObserver(this); }

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
    for (int i = 0; i < std::max(calls_.size(), expected_calls.size()); ++i) {
      EXPECT_EQ(Get(calls_, i), Get(expected_calls, i)) << "call #" << i;
    }
  }

 private:
  static const TestInstance Get(const std::vector<TestInstance>& calls, int i) {
    if (i < calls.size()) {
      return calls[i];
    }
    return {};
  }

  apps::BrowserAppInstanceTracker& tracker_;
  std::vector<TestInstance> calls_;
};

}  // namespace

class BrowserAppInstanceTrackerTest : public InProcessBrowserTest {
 public:
  BrowserAppInstanceTrackerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        apps::BrowserAppInstanceTracker::kEnabled);
  }

 protected:
  Browser* CreateBrowser() {
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    Browser::CreateParams params(profile, true /* user_gesture */);
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
    auto* contents = params.navigated_or_inserted_contents;
    DCHECK_EQ(chrome::FindBrowserWithWebContents(
                  params.navigated_or_inserted_contents),
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

  web_app::AppId InstallWebApp(const std::string& start_url,
                               blink::mojom::DisplayMode user_display_mode) {
    auto info = std::make_unique<WebApplicationInfo>();
    info->start_url = GURL(start_url);
    info->user_display_mode = user_display_mode;
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    auto app_id = web_app::test::InstallWebApp(profile, std::move(info));
    FlushAppService();
    return app_id;
  }

  web_app::AppId InstallWebAppOpeningAsTab(const std::string& start_url) {
    return InstallWebApp(start_url, blink::mojom::DisplayMode::kBrowser);
  }

  web_app::AppId InstallWebAppOpeningAsWindow(const std::string& start_url) {
    return InstallWebApp(start_url, blink::mojom::DisplayMode::kStandalone);
  }

  void UninstallWebApp(const web_app::AppId& app_id) {
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    web_app::test::UninstallWebApp(profile, app_id);
    FlushAppService();
  }

  void FlushAppService() {
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->FlushMojoCallsForTesting();
  }

  uint64_t GetId(content::WebContents* contents) {
    const auto* instance = tracker_->GetAppInstance(contents);
    return instance ? ToUint64(instance->id) : 0;
  }

  uint64_t GetId(Browser* browser) {
    const auto* instance = tracker_->GetChromeInstance(browser);
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
  base::test::ScopedFeatureList scoped_feature_list_;
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
    EXPECT_EQ(GetId(browser), 1);
    EXPECT_EQ(GetId(tab_app1), 2);
    recorder.Verify({
        {"added", 1, kChromeWindow, kChromeAppId, window, "", kVisible, kActive,
         kIgnored},
        {"added", 2, kAppTab, kAppId_A, window, "", kVisible, kActive, kActive},
        {"updated", 2, kAppTab, kAppId_A, window, kURL_A, kVisible, kActive,
         kActive},
        {"updated", 2, kAppTab, kAppId_A, window, kTitle_A, kVisible, kActive,
         kActive},
    });
  }

  // Open a second tab in foreground.
  {
    SCOPED_TRACE("insert a second foreground tab");
    Recorder recorder(*tracker_);

    tab_app2 = InsertForegroundTab(browser, "https://b.example.org");
    EXPECT_EQ(GetId(tab_app2), 3);
    recorder.Verify({
        {"updated", 2, kAppTab, kAppId_A, window, kTitle_A, kVisible, kActive,
         kInactive},
        {"added", 3, kAppTab, kAppId_B, window, "", kVisible, kActive, kActive},
        {"updated", 3, kAppTab, kAppId_B, window, kURL_B, kVisible, kActive,
         kActive},
        {"updated", 3, kAppTab, kAppId_B, window, kTitle_B, kVisible, kActive,
         kActive},
    });
  }

  // Open a third tab in foreground with no app.
  {
    SCOPED_TRACE("insert a third foreground tab without app");
    Recorder recorder(*tracker_);

    InsertForegroundTab(browser, "https://c.example.org");
    recorder.Verify({
        {"updated", 3, kAppTab, kAppId_B, window, kTitle_B, kVisible, kActive,
         kInactive},
    });
  }

  // Open two more tabs in foreground and close them.
  {
    SCOPED_TRACE("insert and close two more tabs");
    Recorder recorder(*tracker_);

    auto* tab_app3 = InsertForegroundTab(browser, "https://a.example.org");
    EXPECT_EQ(GetId(tab_app3), 4);
    auto* tab_app4 = InsertForegroundTab(browser, "https://b.example.org");
    EXPECT_EQ(GetId(tab_app4), 5);
    // Close in reverse order.
    int i = browser->tab_strip_model()->GetIndexOfWebContents(tab_app4);
    browser->tab_strip_model()->CloseWebContentsAt(
        i, TabStripModel::CLOSE_USER_GESTURE);
    i = browser->tab_strip_model()->GetIndexOfWebContents(tab_app3);
    browser->tab_strip_model()->CloseWebContentsAt(
        i, TabStripModel::CLOSE_USER_GESTURE);

    recorder.Verify({
        // tab 4 opened: no events for tab 3 as it has no app
        {"added", 4, kAppTab, kAppId_A, window, "", kVisible, kActive, kActive},
        {"updated", 4, kAppTab, kAppId_A, window, kURL_A, kVisible, kActive,
         kActive},
        {"updated", 4, kAppTab, kAppId_A, window, kTitle_A, kVisible, kActive,
         kActive},
        // tab 5 opened: tab 4 deactivates
        {"updated", 4, kAppTab, kAppId_A, window, kTitle_A, kVisible, kActive,
         kInactive},
        {"added", 5, kAppTab, kAppId_B, window, "", kVisible, kActive, kActive},
        {"updated", 5, kAppTab, kAppId_B, window, kURL_B, kVisible, kActive,
         kActive},
        {"updated", 5, kAppTab, kAppId_B, window, kTitle_B, kVisible, kActive,
         kActive},
        // tab 5 closed: tab 4 reactivates
        {"removed", 5, kAppTab, kAppId_B, window, kTitle_B, kVisible, kActive,
         kActive},
        {"updated", 4, kAppTab, kAppId_A, window, kTitle_A, kVisible, kActive,
         kActive},
        // tab closed: no events for tab 3 as it has no app
        {"removed", 4, kAppTab, kAppId_A, window, kTitle_A, kVisible, kActive,
         kActive},
    });
  }

  // Close the browser.
  {
    SCOPED_TRACE("close browser");
    Recorder recorder(*tracker_);

    browser->tab_strip_model()->CloseAllTabs();
    recorder.Verify({
        {"removed", 3, kAppTab, kAppId_B, window, kTitle_B, kVisible, kActive,
         kInactive},
        {"removed", 2, kAppTab, kAppId_A, window, kTitle_A, kVisible, kActive,
         kInactive},
        {"removed", 1, kChromeWindow, kChromeAppId, window, "", kVisible,
         kActive, kIgnored},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, ForegroundTabNavigate) {
  // Setup: one foreground tab with no app.
  auto* browser = CreateBrowser();
  auto* tab = InsertForegroundTab(browser, "https://c.example.org");
  auto* window = browser->window()->GetNativeWindow();
  EXPECT_EQ(GetId(browser), 1);
  EXPECT_EQ(GetId(tab), 0);

  // Navigate the foreground tab to app A.
  {
    SCOPED_TRACE("navigate tab to app A");
    Recorder recorder(*tracker_);

    NavigateActiveTab(browser, "https://a.example.org");
    EXPECT_EQ(GetId(tab), 2);
    recorder.Verify({
        {"added", 2, kAppTab, kAppId_A, window, kURL_A, kVisible, kActive,
         kActive},
        {"updated", 2, kAppTab, kAppId_A, window, kTitle_A, kVisible, kActive,
         kActive},
    });
  }

  // Navigate the foreground tab to app B.
  {
    SCOPED_TRACE("navigate tab to app B");
    Recorder recorder(*tracker_);

    NavigateActiveTab(browser, "https://b.example.org");
    EXPECT_EQ(GetId(tab), 3);
    recorder.Verify({
        {"removed", 2, kAppTab, kAppId_A, window, kTitle_A, kVisible, kActive,
         kActive},
        {"added", 3, kAppTab, kAppId_B, window, kURL_B, kVisible, kActive,
         kActive},
        {"updated", 3, kAppTab, kAppId_B, window, kTitle_B, kVisible, kActive,
         kActive},
    });
  }

  // Navigate the foreground tab to a different subdomain with no app.
  {
    SCOPED_TRACE("navigate tab from app B to a non-app subdomain");
    Recorder recorder(*tracker_);

    NavigateActiveTab(browser, "https://c.example.org");
    EXPECT_EQ(GetId(tab), 0);
    recorder.Verify({
        {"removed", 3, kAppTab, kAppId_B, window, kTitle_B, kVisible, kActive,
         kActive},
    });
  }

  // Navigate the foreground tab from a non-app subdomain to app B.
  {
    SCOPED_TRACE("navigate tab from a non-app subdomain to app B");
    Recorder recorder(*tracker_);

    NavigateActiveTab(browser, "https://b.example.org");
    EXPECT_EQ(GetId(tab), 4);
    recorder.Verify({
        {"added", 4, kAppTab, kAppId_B, window, kURL_B, kVisible, kActive,
         kActive},
        {"updated", 4, kAppTab, kAppId_B, window, kTitle_B, kVisible, kActive,
         kActive},
    });
  }

  // Navigate the foreground tab to a different domain with no app.
  {
    SCOPED_TRACE("navigate tab from app B to a non-app domain");
    Recorder recorder(*tracker_);

    NavigateActiveTab(browser, "https://example.com");
    EXPECT_EQ(GetId(tab), 0);
    recorder.Verify({
        {"removed", 4, kAppTab, kAppId_B, window, kTitle_B, kVisible, kActive,
         kActive},
    });
  }

  // Navigate the foreground tab from a non-app domain to app B.
  {
    SCOPED_TRACE("navigate tab from a non-app domain to app B");
    Recorder recorder(*tracker_);

    NavigateActiveTab(browser, "https://b.example.org");
    EXPECT_EQ(GetId(tab), 5);
    recorder.Verify({
        {"added", 5, kAppTab, kAppId_B, window, kURL_B, kVisible, kActive,
         kActive},
        {"updated", 5, kAppTab, kAppId_B, window, kTitle_B, kVisible, kActive,
         kActive},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, WindowedWebApp) {
  std::string app_id = InstallWebAppOpeningAsWindow("https://d.example.org");

  Browser* browser = nullptr;
  content::WebContents* tab = nullptr;
  aura::Window* window = nullptr;

  // Open app D in a window.(configured to open in a window).
  {
    SCOPED_TRACE("create a windowed app in a window");
    Recorder recorder(*tracker_);

    browser = CreateAppBrowser(app_id);
    tab = InsertForegroundTab(browser, "https://d.example.org");
    EXPECT_EQ(GetId(browser), 0);
    EXPECT_EQ(GetId(tab), 1);
    window = browser->window()->GetNativeWindow();
    recorder.Verify({
        {"added", 1, kAppWindow, app_id, window, "", kVisible, kActive,
         kActive},
        {"updated", 1, kAppWindow, app_id, window, "https://d.example.org",
         kVisible, kActive, kActive},
        {"updated", 1, kAppWindow, app_id, window, "d.example.org", kVisible,
         kActive, kActive},
    });
  }

  // Close the browser.
  {
    SCOPED_TRACE("close browser");
    Recorder recorder(*tracker_);

    browser->tab_strip_model()->CloseAllTabs();
    recorder.Verify({
        {"removed", 1, kAppWindow, app_id, window, "d.example.org", kVisible,
         kActive, kActive},
    });
  }

  // Open app A in a window (configured to open in a tab).
  {
    SCOPED_TRACE("create a tabbed app in a window");
    Recorder recorder(*tracker_);

    browser = CreateAppBrowser(kAppId_A);
    tab = InsertForegroundTab(browser, "https://a.example.org");
    EXPECT_EQ(GetId(tab), 2);
    window = browser->window()->GetNativeWindow();
    // When open in a window it's still an app, even if configured to open in a
    // tab.
    recorder.Verify({
        {"added", 2, kAppWindow, kAppId_A, window, "", kVisible, kActive,
         kActive},
        {"updated", 2, kAppWindow, kAppId_A, window, kURL_A, kVisible, kActive,
         kActive},
        {"updated", 2, kAppWindow, kAppId_A, window, kTitle_A, kVisible,
         kActive, kActive},
    });
  }

  // Close the browser.
  {
    SCOPED_TRACE("close browser");
    Recorder recorder(*tracker_);

    browser->tab_strip_model()->CloseAllTabs();
    recorder.Verify({
        {"removed", 2, kAppWindow, kAppId_A, window, kTitle_A, kVisible,
         kActive, kActive},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, SwitchTabs) {
  // Setup: one foreground tab and one background tab.
  auto* browser = CreateBrowser();
  auto* window = browser->window()->GetNativeWindow();
  auto* tab0 = InsertForegroundTab(browser, "https://a.example.org");
  EXPECT_EQ(GetId(browser), 1);
  EXPECT_EQ(GetId(tab0), 2);
  auto* tab1 = InsertForegroundTab(browser, "https://b.example.org");
  EXPECT_EQ(GetId(tab1), 3);
  InsertForegroundTab(browser, "https://c.example.org");

  // Switch tabs: no app -> app A
  {
    SCOPED_TRACE("switch tabs to app A");
    Recorder recorder(*tracker_);

    browser->tab_strip_model()->ActivateTabAt(0);
    recorder.Verify({
        {"updated", 2, kAppTab, kAppId_A, window, kTitle_A, kVisible, kActive,
         kActive},
    });
  }

  // Switch tabs: app A -> app B
  {
    SCOPED_TRACE("switch tabs to app B");
    Recorder recorder(*tracker_);

    browser->tab_strip_model()->ActivateTabAt(1);
    recorder.Verify({
        {"updated", 3, kAppTab, kAppId_B, window, kTitle_B, kVisible, kActive,
         kActive},
        {"updated", 2, kAppTab, kAppId_A, window, kTitle_A, kVisible, kActive,
         kInactive},
    });
  }

  // Switch tabs: app B -> no app
  {
    SCOPED_TRACE("switch tabs to no app");
    Recorder recorder(*tracker_);

    browser->tab_strip_model()->ActivateTabAt(2);
    recorder.Verify({
        {"updated", 3, kAppTab, kAppId_B, window, kTitle_B, kVisible, kActive,
         kInactive},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, WindowVisibility) {
  // Setup: one foreground tab and one background tab.
  auto* browser = CreateBrowser();
  auto* window = browser->window()->GetNativeWindow();
  auto* bg_tab = InsertForegroundTab(browser, "https://a.example.org");
  EXPECT_EQ(GetId(browser), 1);
  EXPECT_EQ(GetId(bg_tab), 2);
  auto* fg_tab = InsertForegroundTab(browser, "https://b.example.org");
  EXPECT_EQ(GetId(fg_tab), 3);
  InsertForegroundTab(browser, "https://c.example.org");
  // Prevent spurious deactivation events.
  browser->window()->Deactivate();

  // Hide the window.
  {
    SCOPED_TRACE("hide window");
    Recorder recorder(*tracker_);

    browser->window()->GetNativeWindow()->Hide();
    recorder.Verify({
        {"updated", 1, kChromeWindow, kChromeAppId, window, "", kHidden,
         kInactive, kIgnored},
        {"updated", 2, kAppTab, kAppId_A, window, kTitle_A, kHidden, kInactive,
         kInactive},
        {"updated", 3, kAppTab, kAppId_B, window, kTitle_B, kHidden, kInactive,
         kInactive},
    });
  }

  // Show the window.
  {
    SCOPED_TRACE("show window");
    Recorder recorder(*tracker_);

    browser->window()->GetNativeWindow()->Show();
    recorder.Verify({
        {"updated", 1, kChromeWindow, kChromeAppId, window, "", kVisible,
         kInactive, kIgnored},
        {"updated", 2, kAppTab, kAppId_A, window, kTitle_A, kVisible, kInactive,
         kInactive},
        {"updated", 3, kAppTab, kAppId_B, window, kTitle_B, kVisible, kInactive,
         kInactive},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, WindowActivation) {
  // Setup: two browsers with two tabs each.
  auto* browser1 = CreateBrowser();
  auto* window1 = browser1->window()->GetNativeWindow();
  auto* b1_tab1 = InsertForegroundTab(browser1, "https://a.example.org");
  auto* b1_tab2 = InsertForegroundTab(browser1, "https://c.example.org");
  auto* b1_tab3 = InsertForegroundTab(browser1, "https://b.example.org");
  EXPECT_EQ(GetId(browser1), 1);
  EXPECT_EQ(GetId(b1_tab1), 2);
  EXPECT_EQ(GetId(b1_tab2), 0);
  EXPECT_EQ(GetId(b1_tab3), 3);

  auto* browser2 = CreateBrowser();
  auto* window2 = browser2->window()->GetNativeWindow();
  auto* b2_tab1 = InsertForegroundTab(browser2, "https://a.example.org");
  auto* b2_tab2 = InsertForegroundTab(browser2, "https://c.example.org");
  auto* b2_tab3 = InsertForegroundTab(browser2, "https://b.example.org");
  EXPECT_EQ(GetId(browser2), 4);
  EXPECT_EQ(GetId(b2_tab1), 5);
  EXPECT_EQ(GetId(b2_tab2), 0);
  EXPECT_EQ(GetId(b2_tab3), 6);

  ASSERT_FALSE(browser1->window()->IsActive());
  ASSERT_TRUE(browser2->window()->IsActive());

  // Activate window 1.
  {
    SCOPED_TRACE("activate window 1");
    Recorder recorder(*tracker_);

    browser1->window()->Activate();
    recorder.Verify({
        // deactivated first
        {"updated", 4, kChromeWindow, kChromeAppId, window2, "", kVisible,
         kInactive, kIgnored},
        {"updated", 5, kAppTab, kAppId_A, window2, kTitle_A, kVisible,
         kInactive, kInactive},
        {"updated", 6, kAppTab, kAppId_B, window2, kTitle_B, kVisible,
         kInactive, kActive},
        // then activated
        {"updated", 1, kChromeWindow, kChromeAppId, window1, "", kVisible,
         kActive, kIgnored},
        {"updated", 2, kAppTab, kAppId_A, window1, kTitle_A, kVisible, kActive,
         kInactive},
        {"updated", 3, kAppTab, kAppId_B, window1, kTitle_B, kVisible, kActive,
         kActive},
    });
  }

  // Activate window 2.
  {
    SCOPED_TRACE("activate window 2");
    Recorder recorder(*tracker_);

    browser2->window()->Activate();
    recorder.Verify({
        // deactivated first
        {"updated", 1, kChromeWindow, kChromeAppId, window1, "", kVisible,
         kInactive, kIgnored},
        {"updated", 2, kAppTab, kAppId_A, window1, kTitle_A, kVisible,
         kInactive, kInactive},
        {"updated", 3, kAppTab, kAppId_B, window1, kTitle_B, kVisible,
         kInactive, kActive},
        // then activated
        {"updated", 4, kChromeWindow, kChromeAppId, window2, "", kVisible,
         kActive, kIgnored},
        {"updated", 5, kAppTab, kAppId_A, window2, kTitle_A, kVisible, kActive,
         kInactive},
        {"updated", 6, kAppTab, kAppId_B, window2, kTitle_B, kVisible, kActive,
         kActive},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, TabDrag) {
  // Setup: two browsers: one with two, another with three tabs.
  auto* browser1 = CreateBrowser();
  auto* window1 = browser1->window()->GetNativeWindow();
  auto* b1_tab1 = InsertForegroundTab(browser1, "https://a.example.org");
  auto* b1_tab2 = InsertForegroundTab(browser1, "https://b.example.org");
  EXPECT_EQ(GetId(browser1), 1);
  EXPECT_EQ(GetId(b1_tab1), 2);
  EXPECT_EQ(GetId(b1_tab2), 3);

  auto* browser2 = CreateBrowser();
  auto* window2 = browser2->window()->GetNativeWindow();
  auto* b2_tab1 = InsertForegroundTab(browser2, "https://a.example.org");
  EXPECT_EQ(GetId(browser2), 4);
  EXPECT_EQ(GetId(b2_tab1), 5);
  auto* b2_tab2 = InsertForegroundTab(browser2, "https://a.example.org");
  EXPECT_EQ(GetId(b2_tab2), 6);
  auto* b2_tab3 = InsertForegroundTab(browser2, "https://b.example.org");
  EXPECT_EQ(GetId(b2_tab3), 7);

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
  auto detached =
      browser2->tab_strip_model()->DetachWebContentsAtForInsertion(src_index);

  // Target browser window goes into foreground right before drop.
  browser1->window()->Activate();

  // Attach.
  int dst_index = browser1->tab_strip_model()->count();
  browser1->tab_strip_model()->InsertWebContentsAt(
      dst_index, std::move(detached), TabStripModel::ADD_ACTIVE);
  recorder.Verify({
      // background tab in the dragged-from browser gets activated when the
      // active tab is detached
      {"updated", 6, kAppTab, kAppId_A, window2, kTitle_A, kVisible, kActive,
       kActive},
      // dragged-from browser goes into background
      {"updated", 4, kChromeWindow, kChromeAppId, window2, "", kVisible,
       kInactive, kIgnored},
      {"updated", 5, kAppTab, kAppId_A, window2, kTitle_A, kVisible, kInactive,
       kInactive},
      {"updated", 6, kAppTab, kAppId_A, window2, kTitle_A, kVisible, kInactive,
       kActive},
      // dragged-into browser window goes into foreground
      {"updated", 1, kChromeWindow, kChromeAppId, window1, "", kVisible,
       kActive, kIgnored},
      {"updated", 2, kAppTab, kAppId_A, window1, kTitle_A, kVisible, kActive,
       kInactive},
      {"updated", 3, kAppTab, kAppId_B, window1, kTitle_B, kVisible, kActive,
       kActive},
      // previously foreground tab in the dragged-into browser goes into
      // background when the dragged tab is attached to the new browser
      {"updated", 3, kAppTab, kAppId_B, window1, kTitle_B, kVisible, kActive,
       kInactive},
      // dragged tab gets reparented and becomes active in the new browser
      {"updated", 7, kAppTab, kAppId_B, window1, kTitle_B, kVisible, kActive,
       kActive},
  });
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, MoveTabToAppWindow) {
  // Setup: a browser with two tabs. One tab opens a website that matches the
  // app configured to open in a window.
  std::string app_id = InstallWebAppOpeningAsWindow("https://d.example.org");

  auto* browser1 = CreateBrowser();
  auto* window1 = browser1->window()->GetNativeWindow();
  InsertForegroundTab(browser1, "https://c.example.org");
  auto* tab = InsertForegroundTab(browser1, "https://d.example.org");
  EXPECT_EQ(GetId(browser1), 1);
  EXPECT_EQ(GetId(tab), 0);
  ASSERT_TRUE(browser1->window()->IsActive());

  // Move the tab from the browser to the newly created app browser. This
  // simulates "open in window".
  SCOPED_TRACE("open in window");
  Recorder recorder(*tracker_);

  auto* browser2 = CreateAppBrowser(app_id);
  EXPECT_EQ(GetId(browser2), 0);
  auto* window2 = browser2->window()->GetNativeWindow();
  // Target app browser goes into foreground.
  browser2->window()->Activate();

  // Detach.
  int src_index = browser1->tab_strip_model()->GetIndexOfWebContents(tab);
  auto detached =
      browser1->tab_strip_model()->DetachWebContentsAtForInsertion(src_index);

  // Attach.
  int dst_index = browser2->tab_strip_model()->count();
  browser2->tab_strip_model()->InsertWebContentsAt(
      dst_index, std::move(detached), TabStripModel::ADD_ACTIVE);
  recorder.Verify({
      // source browser goes into background when app browser is created
      {"updated", 1, kChromeWindow, kChromeAppId, window1, "", kVisible,
       kInactive, kIgnored},
      // moved tab gets reparented and becomes an app in the new browser
      {"added", 2, kAppWindow, app_id, window2, "d.example.org", kVisible,
       kActive, kActive},
  });
}

IN_PROC_BROWSER_TEST_F(BrowserAppInstanceTrackerTest, Accessors) {
  // Setup: two regular browsers, and one app window browser.
  auto* browser1 = CreateBrowser();
  auto* window1 = browser1->window()->GetNativeWindow();
  auto* b1_tab1 = InsertForegroundTab(browser1, "https://a.example.org");
  EXPECT_EQ(GetId(browser1), 1);
  EXPECT_EQ(GetId(b1_tab1), 2);
  auto* b1_tab2 = InsertForegroundTab(browser1, "https://c.example.org");
  auto* b1_tab3 = InsertForegroundTab(browser1, "https://b.example.org");
  EXPECT_EQ(GetId(b1_tab3), 3);

  auto* browser2 = CreateBrowser();
  auto* window2 = browser2->window()->GetNativeWindow();
  auto* b2_tab1 = InsertForegroundTab(browser2, "https://c.example.org");
  auto* b2_tab2 = InsertForegroundTab(browser2, "https://b.example.org");
  EXPECT_EQ(GetId(browser2), 4);
  EXPECT_EQ(GetId(b2_tab2), 5);

  auto* browser3 = CreateAppBrowser(kAppId_B);
  auto* window3 = browser3->window()->GetNativeWindow();
  auto* b3_tab1 = InsertForegroundTab(browser3, "https://b.example.org");
  EXPECT_EQ(GetId(b3_tab1), 6);

  ASSERT_FALSE(browser1->window()->IsActive());
  ASSERT_FALSE(browser2->window()->IsActive());
  ASSERT_TRUE(browser3->window()->IsActive());

  auto* b1_app = tracker_->GetChromeInstance(browser1);
  auto* b1_tab1_app = tracker_->GetAppInstance(b1_tab1);
  auto* b1_tab2_app = tracker_->GetAppInstance(b1_tab2);
  auto* b1_tab3_app = tracker_->GetAppInstance(b1_tab3);

  auto* b2_app = tracker_->GetChromeInstance(browser2);
  auto* b2_tab1_app = tracker_->GetAppInstance(b2_tab1);
  auto* b2_tab2_app = tracker_->GetAppInstance(b2_tab2);

  auto* b3_app = tracker_->GetChromeInstance(browser3);
  auto* b3_tab1_app = tracker_->GetAppInstance(b3_tab1);

  EXPECT_EQ(TestInstance::Create(b1_app),
            (TestInstance{"snapshot", 1, kChromeWindow, kChromeAppId, window1,
                          "", kVisible, kInactive, kIgnored}));
  EXPECT_EQ(TestInstance::Create(b1_tab1_app),
            (TestInstance{"snapshot", 2, kAppTab, kAppId_A, window1, kTitle_A,
                          kVisible, kInactive, kInactive}));
  EXPECT_EQ(TestInstance::Create(b1_tab2_app), TestInstance{});
  EXPECT_EQ(TestInstance::Create(b1_tab3_app),
            (TestInstance{"snapshot", 3, kAppTab, kAppId_B, window1, kTitle_B,
                          kVisible, kInactive, kActive}));

  EXPECT_EQ(TestInstance::Create(b2_app),
            (TestInstance{"snapshot", 4, kChromeWindow, kChromeAppId, window2,
                          "", kVisible, kInactive, kIgnored}));
  EXPECT_EQ(TestInstance::Create(b2_tab1_app), TestInstance{});
  EXPECT_EQ(TestInstance::Create(b2_tab2_app),
            (TestInstance{"snapshot", 5, kAppTab, kAppId_B, window2, kTitle_B,
                          kVisible, kInactive, kActive}));

  EXPECT_EQ(TestInstance::Create(b3_app), TestInstance{});
  EXPECT_EQ(TestInstance::Create(b3_tab1_app),
            (TestInstance{"snapshot", 6, kAppWindow, kAppId_B, window3,
                          kTitle_B, kVisible, kActive, kActive}));

  EXPECT_EQ(tracker_->GetAppInstancesByAppId(kAppId_A),
            std::set<const apps::BrowserAppInstance*>{b1_tab1_app});
  EXPECT_EQ(tracker_->GetAppInstancesByAppId(kAppId_B),
            (std::set<const apps::BrowserAppInstance*>{b1_tab3_app, b2_tab2_app,
                                                       b3_tab1_app}));
  EXPECT_EQ(tracker_->GetAppInstancesByAppId(kChromeAppId),
            (std::set<const apps::BrowserAppInstance*>{b1_app, b2_app}));

  EXPECT_TRUE(tracker_->IsAppRunning(kAppId_A));
  EXPECT_TRUE(tracker_->IsAppRunning(kAppId_B));
  EXPECT_TRUE(tracker_->IsAppRunning(kChromeAppId));
  EXPECT_FALSE(tracker_->IsAppRunning("non-existent-app"));

  EXPECT_EQ(TestInstance::Create(tracker_->GetAppInstanceById(TestId(2))),
            (TestInstance{"snapshot", 2, kAppTab, kAppId_A, window1, kTitle_A,
                          kVisible, kInactive, kInactive}));
  EXPECT_EQ(TestInstance::Create(tracker_->GetAppInstanceById(TestId(10))),
            TestInstance{});

  // App A is closed, B and Chrome are still running.
  browser1->tab_strip_model()->CloseAllTabs();

  EXPECT_FALSE(tracker_->IsAppRunning(kAppId_A));
  EXPECT_TRUE(tracker_->IsAppRunning(kAppId_B));
  EXPECT_TRUE(tracker_->IsAppRunning(kChromeAppId));

  // App A and Chrome are closed, B is still running.
  browser2->tab_strip_model()->CloseAllTabs();

  EXPECT_FALSE(tracker_->IsAppRunning(kAppId_A));
  EXPECT_TRUE(tracker_->IsAppRunning(kAppId_B));
  EXPECT_FALSE(tracker_->IsAppRunning(kChromeAppId));

  // Everything is closed.
  browser3->tab_strip_model()->CloseAllTabs();

  EXPECT_FALSE(tracker_->IsAppRunning(kAppId_A));
  EXPECT_FALSE(tracker_->IsAppRunning(kAppId_B));
  EXPECT_FALSE(tracker_->IsAppRunning(kChromeAppId));
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

    EXPECT_EQ(GetId(tab1), 0);
    EXPECT_EQ(GetId(tab3), 0);
    app_id = InstallWebAppOpeningAsTab("https://c.example.org");
    EXPECT_EQ(GetId(tab1), 2);
    EXPECT_EQ(GetId(tab3), 3);
    recorder.Verify({
        {"added", 2, kAppTab, app_id, window1, title, kVisible, kActive,
         kInactive},
        {"added", 3, kAppTab, app_id, window1, title, kVisible, kActive,
         kActive},
    });
  }

  {
    SCOPED_TRACE("uninstall app");
    Recorder recorder(*tracker_);

    UninstallWebApp(app_id);
    EXPECT_EQ(GetId(tab1), 0);
    EXPECT_EQ(GetId(tab3), 0);
    recorder.Verify({
        {"removed", 2, kAppTab, app_id, window1, title, kVisible, kActive,
         kInactive},
        {"removed", 3, kAppTab, app_id, window1, title, kVisible, kActive,
         kActive},
    });
  }

  {
    SCOPED_TRACE("install app opening in a window");
    Recorder recorder(*tracker_);

    EXPECT_EQ(GetId(tab1), 0);
    EXPECT_EQ(GetId(tab3), 0);
    app_id = InstallWebAppOpeningAsWindow("https://c.example.org");
    // This has no effect: apps configured to open in a window aren't counted as
    // apps when opened in a tab.
    EXPECT_EQ(GetId(tab1), 0);
    EXPECT_EQ(GetId(tab3), 0);
    recorder.Verify({});
  }
}
