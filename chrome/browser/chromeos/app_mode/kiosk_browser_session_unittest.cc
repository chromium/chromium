// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_browser_session.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_browser_window_handler.h"
#include "chrome/browser/chromeos/app_mode/kiosk_metrics_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/tabs/tab_activity_simulator.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_delegate_map.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/chromeos/app_mode/kiosk_session_plugin_handler_delegate.h"
#include "content/public/browser/plugin_service.h"
#endif  // BUILDFLAG(ENABLE_PLUGINS)

namespace chromeos {

namespace {

using ::chromeos::FakePowerManagerClient;

constexpr char kTestAppId[] = "aaaabbbbaaaabbbbaaaabbbbaaaabbbb";
constexpr char kTestWebAppName1[] = "test_web_app_name1";
constexpr char kTestWebAppName2[] = "test_web_app_name2";
constexpr char kTestUrl[] = "www.test.com";
constexpr base::TimeDelta kCloseBrowserTimeout = base::Seconds(2);

#if BUILDFLAG(ENABLE_PLUGINS)
constexpr char kBrowserPluginFilePath[] = "/path/to/browser_plugin";
#endif  // BUILDFLAG(ENABLE_PLUGINS)

// This class constructs and owns the `Browser` object. It assumes that the
// `Browser` uses a `TestBrowserWindow`. The class adds a default tab to the
// newly constructed browser and handles the closing lifecycle by registering a
// close callback on the `TestBrowserWindow`.
class FakeBrowser {
 public:
  explicit FakeBrowser(Browser::CreateParams params)
      : FakeBrowser(Browser::DeprecatedCreateOwnedForTesting(params)) {}

  explicit FakeBrowser(std::unique_ptr<Browser> browser)
      : browser_(std::move(browser)) {
    if (!browser_->is_type_picture_in_picture()) {
      // Add a tab to the browser to ensure that `CloseAllTabs()` works.
      // Note that tabs are not supported with PICTURE_IN_PICTURE windows.
      TabActivitySimulator().AddWebContentsAndNavigate(
          browser_->tab_strip_model(), GURL(kTestUrl));
    }
    static_cast<TestBrowserWindow*>(browser_->window())
        ->SetCloseCallback(base::BindOnce(&FakeBrowser::OnBrowserWindowClosed,
                                          weak_ptr_.GetWeakPtr()));
  }

  ~FakeBrowser() {
    if (browser_ && !browser_->tab_strip_model()->empty()) {
      // This is required to prevent a DCHECK crash in the destructor of
      // `Browser` if tabs remain open.
      browser_->tab_strip_model()->CloseAllTabs();
    }
  }

  [[nodiscard]] bool WaitForBrowserClose() { return closed_future_.Wait(); }
  bool IsClosed() { return closed_future_.IsReady(); }

  bool IsFullscreen() {
    return browser_->GetFeatures()
        .exclusive_access_manager()
        ->fullscreen_controller()
        ->IsFullscreenForBrowser();
  }

 private:
  void OnBrowserWindowClosed() {
    closed_future_.SetValue();
    // `TestBrowserWindow` does not destroy `Browser` when `Close()` is called,
    // but real browser window does. Call `RemoveBrowser` here to fake this
    // behavior.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeBrowser::RemoveBrowser, weak_ptr_.GetWeakPtr()));
  }

  void RemoveBrowser() { browser_.reset(); }

  base::test::TestFuture<void> closed_future_;
  std::unique_ptr<Browser> browser_;
  base::WeakPtrFactory<FakeBrowser> weak_ptr_{this};
};

// A test browser window that can toggle fullscreen state.
class FullscreenTestBrowserWindow : public TestBrowserWindow,
                                    ExclusiveAccessContext {
 public:
  explicit FullscreenTestBrowserWindow(TestingProfile* profile,
                                       bool fullscreen = false)
      : fullscreen_(fullscreen), profile_(profile) {}

  FullscreenTestBrowserWindow(const FullscreenTestBrowserWindow&) = delete;
  FullscreenTestBrowserWindow& operator=(const FullscreenTestBrowserWindow&) =
      delete;

  ~FullscreenTestBrowserWindow() override = default;

  // TestBrowserWindow:
  bool ShouldHideUIForFullscreen() const override { return fullscreen_; }
  bool IsFullscreen() const override { return fullscreen_; }
  void EnterFullscreen(const url::Origin& origin,
                       ExclusiveAccessBubbleType type,
                       FullscreenTabParams fullscreen_tab_params) override {
    fullscreen_ = true;
  }
  void ExitFullscreen() override { fullscreen_ = false; }
  bool IsToolbarShowing() const override { return false; }
  bool IsLocationBarVisible() const override { return true; }

  ExclusiveAccessContext* GetExclusiveAccessContext() override { return this; }

  // ExclusiveAccessContext:
  Profile* GetProfile() override { return profile_; }
  content::WebContents* GetWebContentsForExclusiveAccess() override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  void UpdateExclusiveAccessBubble(
      const ExclusiveAccessBubbleParams& params,
      ExclusiveAccessBubbleHideCallback first_hide_callback) override {}
  bool IsExclusiveAccessBubbleDisplayed() const override { return false; }
  void OnExclusiveAccessUserInput() override {}
  bool CanUserEnterFullscreen() const override { return false; }
  bool CanUserExitFullscreen() const override { return true; }

 private:
  bool fullscreen_ = false;
  raw_ptr<TestingProfile> profile_;
};

std::unique_ptr<FakeBrowser> CreateBrowserWithFullscreenTestWindowForParams(
    Browser::CreateParams params,
    TestingProfile* profile,
    bool is_main_browser = false) {
  // The main browser window for the kiosk is always fullscreen in the
  // production.
  auto window = std::make_unique<FullscreenTestBrowserWindow>(
      profile, /*fullscreen=*/is_main_browser);
  params.window = window.release();
  return std::make_unique<FakeBrowser>(params);
}

void EmulateDeviceReboot() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kFirstExecAfterBoot);
}

struct KioskSessionRestartTestCase {
  std::string test_name;
  bool run_with_reboot = false;
};

struct KioskSessionPowerManagerRequestRestartTestCase {
  power_manager::RequestRestartReason power_manager_reason;
  KioskSessionRestartReason restart_reason;
};

void CheckSessionRestartReasonHistogramDependingOnRebootStatus(
    bool run_with_reboot,
    const KioskSessionRestartReason& reasonWithoutReboot,
    const KioskSessionRestartReason& reasonWithReboot,
    const base::HistogramTester* histogram) {
  if (run_with_reboot) {
    histogram->ExpectBucketCount(kKioskSessionRestartReasonHistogram,
                                 reasonWithReboot, 1);
  } else {
    histogram->ExpectBucketCount(kKioskSessionRestartReasonHistogram,
                                 reasonWithoutReboot, 1);
  }
  histogram->ExpectTotalCount(kKioskSessionRestartReasonHistogram, 1);
}

class SystemWebAppWaiter {
 public:
  explicit SystemWebAppWaiter(
      ash::SystemWebAppManager* system_web_app_manager) {
    system_web_app_manager->on_apps_synchronized().Post(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          // Wait one execution loop for on_apps_synchronized() to be called on
          // all listeners.
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, run_loop_.QuitClosure());
        }));
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};


enum class KioskType { kChromeApp = 0, kWebApp = 1, kIwa = 2 };

}  // namespace

// TODO(b/271336749): split kiosk_browser_session_unittest.cc file into smaller
// test files.
template <typename KioskBrowserSessionParamType = KioskSessionRestartTestCase>
class KioskBrowserSessionBaseTest
    : public ::testing::TestWithParam<KioskBrowserSessionParamType> {
 public:
  KioskBrowserSessionBaseTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  KioskBrowserSessionBaseTest(const KioskBrowserSessionBaseTest&) = delete;
  KioskBrowserSessionBaseTest& operator=(const KioskBrowserSessionBaseTest&) =
      delete;

  static void SetUpTestSuite() {
    chromeos::PowerManagerClient::InitializeFake();
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ash_test_helper_.SetUp(ash::AshTestHelper::InitParams());
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile("test@user");
  }

  static void TearDownTestSuite() { chromeos::PowerManagerClient::Shutdown(); }

  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }

  TestingProfile* profile() { return profile_; }

  base::HistogramTester* histogram() { return &histogram_; }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  std::unique_ptr<FakeBrowser> CreateBrowserWithTestWindow() {
    return CreateBrowserWithFullscreenTestWindowForParams(
        Browser::CreateParams(profile(), true), profile());
  }

  std::unique_ptr<FakeBrowser> CreateBrowserForWebApp(
      const std::string& web_app_name,
      std::optional<Browser::Type> browser_type = std::nullopt) {
    Browser::CreateParams params = Browser::CreateParams::CreateForAppPopup(
        /*app_name=*/web_app_name, /*trusted_source=*/true,
        /*window_bounds=*/gfx::Rect(), /*profile=*/profile(),
        /*user_gesture=*/true);
    if (browser_type.has_value()) {
      params.type = browser_type.value();
    }
    return CreateBrowserWithFullscreenTestWindowForParams(params, profile());
  }

  // Create the main kiosk browser window, which is normally auto-created when a
  // web kiosk session starts.
  void CreateWebKioskMainBrowser(const std::string& web_app_name) {
    web_kiosk_main_browser_ = CreateBrowserWithFullscreenTestWindowForParams(
        Browser::CreateParams::CreateForApp(
            /*app_name=*/web_app_name, /*trusted_source=*/true,
            /*window_bounds=*/gfx::Rect(), /*profile=*/profile(),
            /*user_gesture=*/true),
        profile(), /*is_main_browser=*/true);
  }

  // Simulate starting a web kiosk session.
  void StartWebKioskSession(
      const std::string& web_app_name = kTestWebAppName1) {
    CreateWebKioskMainBrowser(web_app_name);

    kiosk_browser_session_ = KioskBrowserSession::CreateForTesting(
        profile(), base::DoNothing(), local_state(), {crash_path().value()});
    kiosk_browser_session_->InitForWebKiosk(web_app_name);

    task_environment_.RunUntilIdle();
  }

  // Simulate starting an IWA kiosk session.
  void StartIwaKioskSession(const std::string& iwa_name = kTestWebAppName1) {
    // IWAs are launched same as web apps, reusing web kiosk routines.
    CreateWebKioskMainBrowser(iwa_name);

    kiosk_browser_session_ = KioskBrowserSession::CreateForTesting(
        profile(), base::DoNothing(), local_state(), {crash_path().value()});
    kiosk_browser_session_->InitForIwaKiosk(iwa_name);
  }

  // Simulate starting a chrome app kiosk session.
  void StartChromeAppKioskSession() {
    kiosk_browser_session_ = std::make_unique<KioskBrowserSession>(
        profile(), base::DoNothing(), local_state());
    kiosk_browser_session_->InitForChromeAppKiosk(kTestAppId);
  }

  // Waits until `kiosk_browser_session_` handles creation of
  // `new_browser_window` and returns whether `new_browser_window` was asked to
  // close. In this case we will also ensure that `new_browser_window` was
  // automatically closed.
  bool DidSessionCloseNewWindow(FakeBrowser& new_browser) {
    // Wait until the new window is handled by `kiosk_browser_session_`.
    base::test::TestFuture<bool> is_handled;
    kiosk_browser_session_->SetOnHandleBrowserCallbackForTesting(
        is_handled.GetRepeatingCallback());
    bool is_closed_by_kiosk_session = is_handled.Get();

    if (is_closed_by_kiosk_session) {
      EXPECT_TRUE(new_browser.WaitForBrowserClose());
    }

    return is_closed_by_kiosk_session;
  }

  // Keeps ownership of the browser window while checking if the window is
  // closed automatically or not.
  bool DidSessionCloseNewWindow(std::unique_ptr<FakeBrowser> new_browser) {
    return DidSessionCloseNewWindow(*new_browser);
  }

  void CloseMainBrowser() {
    // Close the main browser window.
    web_kiosk_main_browser_.reset();
  }

  bool IsMainBrowserClosed() {
    return web_kiosk_main_browser_ == nullptr ||
           web_kiosk_main_browser_->IsClosed();
  }

  bool IsMainBrowserFullscreen() {
    return web_kiosk_main_browser_->IsFullscreen();
  }

  bool IsSessionShuttingDown() const {
    return kiosk_browser_session_->is_shutting_down();
  }

  void ResetKioskBrowserSession() { kiosk_browser_session_.reset(); }

  PrefService* GetPrefs() { return profile()->GetPrefs(); }

  KioskSessionPluginHandlerDelegate* GetPluginHandlerDelegate() {
    return kiosk_browser_session_->GetPluginHandlerDelegateForTesting();
  }

  base::FilePath crash_path() const { return temp_dir_.GetPath(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  ash::AshTestHelper ash_test_helper_;

  // `RenderViewHostTestEnabled` is required to make the navigation work that
  // happens in the tab added to `TestBrowserWindow` in `FakeBrowser`.
  content::RenderViewHostTestEnabler enabler_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> profile_;
  // Main browser window created when launching a web or IWA kiosk app.
  // Will be nullptr if `CreateWebKioskMainBrowser` function was not called.
  std::unique_ptr<FakeBrowser> web_kiosk_main_browser_;
  base::HistogramTester histogram_;
  std::unique_ptr<KioskBrowserSession> kiosk_browser_session_;
};

using KioskBrowserSessionTest = KioskBrowserSessionBaseTest<>;
using KioskBrowserSessionRestartReasonTest =
    KioskBrowserSessionBaseTest<KioskSessionRestartTestCase>;

TEST_F(KioskBrowserSessionTest, WebKioskTracksBrowserCreation) {
  local_state()->SetDict(
      prefs::kKioskMetrics,
      base::Value::Dict().Set(kKioskSessionStartTime,
                              base::TimeToValue(base::Time::Now())));

  StartWebKioskSession();
  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kWebStarted, 1);
  histogram()->ExpectTotalCount(kKioskSessionCountPerDayHistogram, 1);

  EXPECT_TRUE(DidSessionCloseNewWindow(CreateBrowserWithTestWindow()));

  // The main browser window still exists, the kiosk session should not
  // shutdown.
  EXPECT_FALSE(IsSessionShuttingDown());
  // Opening a new browser should not be counted as a new session.
  histogram()->ExpectTotalCount(kKioskSessionCountPerDayHistogram, 1);

  CloseMainBrowser();
  EXPECT_TRUE(IsSessionShuttingDown());

  const base::Value::Dict& dict = local_state()->GetDict(prefs::kKioskMetrics);
  const base::Value::List* sessions_list =
      dict.FindList(kKioskSessionLastDayList);
  ASSERT_TRUE(sessions_list);
  EXPECT_EQ(1u, sessions_list->size());

  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kStopped, 1);
  EXPECT_EQ(2u, histogram()->GetAllSamples(kKioskSessionStateHistogram).size());

  histogram()->ExpectTotalCount(kKioskSessionDurationNormalHistogram, 1);
  histogram()->ExpectTotalCount(kKioskSessionDurationInDaysNormalHistogram, 0);
}

TEST_F(KioskBrowserSessionTest, IwaKioskSessionState) {
  StartIwaKioskSession();
  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kIwaStarted, 1);
  histogram()->ExpectTotalCount(kKioskSessionCountPerDayHistogram, 1);
}

TEST_F(KioskBrowserSessionTest, ChromeAppKioskSessionState) {
  StartChromeAppKioskSession();
  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kStarted, 1);
  histogram()->ExpectTotalCount(kKioskSessionCountPerDayHistogram, 1);
}

TEST_F(KioskBrowserSessionTest, ChromeAppKioskTracksBrowserCreation) {
  StartChromeAppKioskSession();

  EXPECT_TRUE(DidSessionCloseNewWindow(CreateBrowserWithTestWindow()));
  // Closing the browser should not shutdown the ChromeApp kiosk session.
  EXPECT_FALSE(IsSessionShuttingDown());
  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kClosedRegularBrowser,
                                 1);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 1);

  const base::Value::Dict& dict = local_state()->GetDict(prefs::kKioskMetrics);
  const base::Value::List* sessions_list =
      dict.FindList(kKioskSessionLastDayList);
  ASSERT_TRUE(sessions_list);
  EXPECT_EQ(1u, sessions_list->size());

  // Emulate exiting kiosk session.
  ResetKioskBrowserSession();

  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kStopped, 1);
  EXPECT_EQ(2u, histogram()->GetAllSamples(kKioskSessionStateHistogram).size());

  histogram()->ExpectTotalCount(kKioskSessionDurationNormalHistogram, 1);
  histogram()->ExpectTotalCount(kKioskSessionDurationInDaysNormalHistogram, 0);
}

TEST_F(KioskBrowserSessionTest, ChromeAppKioskShouldClosePreexistingBrowsers) {
  std::unique_ptr<FakeBrowser> preexisting_browser =
      CreateBrowserWithTestWindow();

  StartChromeAppKioskSession();

  ASSERT_TRUE(preexisting_browser->WaitForBrowserClose());
}

TEST_F(KioskBrowserSessionTest, WebKioskShouldClosePreexistingBrowsers) {
  std::unique_ptr<FakeBrowser> preexisting_browser =
      CreateBrowserWithTestWindow();

  StartWebKioskSession();

  ASSERT_TRUE(preexisting_browser->WaitForBrowserClose());
  EXPECT_FALSE(IsMainBrowserClosed());
}

// Check that sessions list in local_state contains only sessions within the
// last 24h.
TEST_F(KioskBrowserSessionTest, WebKioskLastDaySessions) {
  // Setup local_state with 5 more kiosk sessions happened prior to the current
  // one: {now, 2,3,4,5 days ago}
  {
    auto session_list =
        base::Value::List().Append(base::TimeToValue(base::Time::Now()));

    const size_t kMaxDays = 4;
    for (size_t i = 0; i < kMaxDays; i++) {
      session_list.Append(
          base::TimeToValue(base::Time::Now() - base::Days(i + 2)));
    }

    local_state()->SetDict(
        prefs::kKioskMetrics,
        base::Value::Dict()
            .Set(kKioskSessionLastDayList, std::move(session_list))
            // Emulates previous session crashes.
            .Set(kKioskSessionStartTime,
                 base::TimeToValue(base::Time::Now() -
                                   2 * kKioskSessionDurationHistogramLimit)));
  }

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ash::switches::kLoginUser, "fake-user");

  base::FilePath crash_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(crash_path(), &crash_file));

  StartWebKioskSession();

  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kRestored, 1);
  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kCrashed, 1);
  histogram()->ExpectTotalCount(kKioskSessionDurationCrashedHistogram, 1);
  histogram()->ExpectTotalCount(kKioskSessionDurationInDaysCrashedHistogram, 1);
  histogram()->ExpectTotalCount(kKioskSessionCountPerDayHistogram, 1);

  CloseMainBrowser();
  EXPECT_TRUE(IsSessionShuttingDown());

  const base::Value::Dict& dict = local_state()->GetDict(prefs::kKioskMetrics);
  const base::Value::List* sessions_list =
      dict.FindList(kKioskSessionLastDayList);
  ASSERT_TRUE(sessions_list);
  // There should be only two kiosk sessions on the list:
  // the one that happened right before the current one and the current one.
  EXPECT_EQ(2u, sessions_list->size());
  for (const auto& time : *sessions_list) {
    EXPECT_LE(base::Time::Now() - base::ValueToTime(time).value(),
              base::Days(1));
  }

  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kStopped, 1);
  EXPECT_EQ(3u, histogram()->GetAllSamples(kKioskSessionStateHistogram).size());
  histogram()->ExpectTotalCount(kKioskSessionDurationNormalHistogram, 1);
  histogram()->ExpectTotalCount(kKioskSessionDurationInDaysNormalHistogram, 0);
}

TEST_F(KioskBrowserSessionTest, DoNotOpenSecondBrowserInWebKiosk) {
  StartWebKioskSession(kTestWebAppName1);

  EXPECT_TRUE(
      DidSessionCloseNewWindow(CreateBrowserForWebApp(kTestWebAppName1)));
}

TEST_F(KioskBrowserSessionTest, DoNotCrashIfBrowserClosedSuccessfully) {
  StartWebKioskSession(kTestWebAppName1);

  auto browser = CreateBrowserForWebApp(kTestWebAppName1);

  task_environment()->FastForwardBy(kCloseBrowserTimeout);
}

TEST_F(KioskBrowserSessionTest, OpenSecondBrowserInWebKioskIfAllowed) {
  GetPrefs()->SetBoolean(prefs::kNewWindowsInKioskAllowed, true);
  StartWebKioskSession(kTestWebAppName1);

  EXPECT_FALSE(
      DidSessionCloseNewWindow(CreateBrowserForWebApp(kTestWebAppName1)));
}

TEST_F(KioskBrowserSessionTest, EnsureSecondBrowserIsFullscreenInWebKiosk) {
  GetPrefs()->SetBoolean(prefs::kNewWindowsInKioskAllowed, true);
  StartWebKioskSession(kTestWebAppName1);
  EXPECT_TRUE(IsMainBrowserFullscreen());

  std::unique_ptr<FakeBrowser> second_browser =
      CreateBrowserForWebApp(kTestWebAppName1);
  DidSessionCloseNewWindow(*second_browser);

  EXPECT_TRUE(second_browser->IsFullscreen());
}

TEST_F(KioskBrowserSessionTest,
       DoNotOpenSecondBrowserInWebKioskIfTypeIsNotAppPopup) {
  const std::vector<Browser::Type> not_app_popup_browser_types = {
      Browser::Type::TYPE_NORMAL,
      Browser::Type::TYPE_POPUP,
      Browser::Type::TYPE_APP,
      Browser::Type::TYPE_DEVTOOLS,
      Browser::Type::TYPE_CUSTOM_TAB,
      Browser::Type::TYPE_PICTURE_IN_PICTURE,
  };

  GetPrefs()->SetBoolean(prefs::kNewWindowsInKioskAllowed, true);
  StartWebKioskSession(kTestWebAppName1);

  for (auto browser_type : not_app_popup_browser_types) {
    EXPECT_TRUE(DidSessionCloseNewWindow(
        CreateBrowserForWebApp(kTestWebAppName1, browser_type)));
  }
}

TEST_F(KioskBrowserSessionTest,
       DoNotOpenSecondBrowserInWebKioskWithEmptyWebAppName) {
  GetPrefs()->SetBoolean(prefs::kNewWindowsInKioskAllowed, true);
  StartWebKioskSession();

  EXPECT_TRUE(DidSessionCloseNewWindow(CreateBrowserWithTestWindow()));
}

TEST_F(KioskBrowserSessionTest,
       DoNotOpenSecondBrowserInWebKioskWithDifferentWebAppName) {
  GetPrefs()->SetBoolean(prefs::kNewWindowsInKioskAllowed, true);
  StartWebKioskSession(kTestWebAppName1);

  EXPECT_TRUE(
      DidSessionCloseNewWindow(CreateBrowserForWebApp(kTestWebAppName2)));
}

TEST_F(KioskBrowserSessionTest, DoNotOpenSecondBrowserInChromeAppKiosk) {
  // This flag allows opening new windows only for the web kiosk session. For
  // chrome app kiosk we still should block all new browsers.
  GetPrefs()->SetBoolean(prefs::kNewWindowsInKioskAllowed, true);
  StartChromeAppKioskSession();

  EXPECT_TRUE(
      DidSessionCloseNewWindow(CreateBrowserForWebApp(kTestWebAppName2)));
}

TEST_F(KioskBrowserSessionTest, NewOpenedRegularBrowserMetrics) {
  GetPrefs()->SetBoolean(prefs::kNewWindowsInKioskAllowed, true);
  StartWebKioskSession(kTestWebAppName1);

  DidSessionCloseNewWindow(CreateBrowserForWebApp(kTestWebAppName1));

  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kOpenedRegularBrowser,
                                 1);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 1);
}

TEST_F(KioskBrowserSessionTest, NewClosedRegularBrowserMetrics) {
  GetPrefs()->SetBoolean(prefs::kNewWindowsInKioskAllowed, false);
  StartWebKioskSession(kTestWebAppName1);

  DidSessionCloseNewWindow(CreateBrowserForWebApp(kTestWebAppName1));

  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kClosedRegularBrowser,
                                 1);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 1);
}

TEST_F(KioskBrowserSessionTest,
       DoNotExitWebKioskSessionWhenSecondBrowserIsOpened) {
  GetPrefs()->SetBoolean(prefs::kNewWindowsInKioskAllowed, true);
  StartWebKioskSession();

  auto second_browser = CreateBrowserForWebApp(kTestWebAppName1);
  EXPECT_FALSE(DidSessionCloseNewWindow(*second_browser));

  CloseMainBrowser();
  EXPECT_FALSE(IsSessionShuttingDown());

  second_browser.reset();
  // Exit kiosk session when the last browser is closed.
  EXPECT_TRUE(IsSessionShuttingDown());
}

TEST_F(KioskBrowserSessionTest, InitialBrowserShouldBeHandledAsRegularBrowser) {
  GetPrefs()->SetBoolean(prefs::kNewWindowsInKioskAllowed, true);
  StartWebKioskSession();

  auto second_browser = CreateBrowserForWebApp(kTestWebAppName1);
  EXPECT_FALSE(DidSessionCloseNewWindow(*second_browser));

  second_browser.reset();
  EXPECT_FALSE(IsSessionShuttingDown());

  CloseMainBrowser();
  // Exit kiosk session when the last browser is closed.
  EXPECT_TRUE(IsSessionShuttingDown());
}

TEST_P(KioskBrowserSessionRestartReasonTest, StoppedMetric) {
  const KioskSessionRestartTestCase& test_config = GetParam();
  StartWebKioskSession();
  // Emulate exiting the kiosk session.
  CloseMainBrowser();
  EXPECT_TRUE(IsSessionShuttingDown());
  if (test_config.run_with_reboot) {
    EmulateDeviceReboot();
  }
  histogram()->ExpectTotalCount(kKioskSessionRestartReasonHistogram, 0);

  StartWebKioskSession();

  if (test_config.run_with_reboot) {
    histogram()->ExpectBucketCount(
        kKioskSessionRestartReasonHistogram,
        KioskSessionRestartReason::kStoppedWithReboot, 1);
  } else {
    histogram()->ExpectBucketCount(kKioskSessionRestartReasonHistogram,
                                   KioskSessionRestartReason::kStopped, 1);
  }
  histogram()->ExpectTotalCount(kKioskSessionRestartReasonHistogram, 1);
}

TEST_P(KioskBrowserSessionRestartReasonTest, CrashMetric) {
  const KioskSessionRestartTestCase& test_config = GetParam();
  // Setup `kKioskSessionStartTime` and add a file to the crash directory to
  // emulate previous kiosk session crash.
  local_state()->SetDict(
      prefs::kKioskMetrics,
      base::Value::Dict().Set(
          kKioskSessionStartTime,
          base::TimeToValue(base::Time::Now() - base::Hours(1))));
  base::FilePath crash_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(crash_path(), &crash_file));
  if (test_config.run_with_reboot) {
    EmulateDeviceReboot();
  }

  StartWebKioskSession();

  CheckSessionRestartReasonHistogramDependingOnRebootStatus(
      test_config.run_with_reboot, KioskSessionRestartReason::kCrashed,
      KioskSessionRestartReason::kCrashedWithReboot, histogram());
}

TEST_P(KioskBrowserSessionRestartReasonTest, LocalStateWasNotSavedMetric) {
  const KioskSessionRestartTestCase& test_config = GetParam();
  // Setup `kKioskSessionStartTime` to emulate previous kiosk session stopped
  // correctly, but because of race condition, `kKioskSessionStartTime` was not
  // cleaned.
  local_state()->SetDict(
      prefs::kKioskMetrics,
      base::Value::Dict().Set(
          kKioskSessionStartTime,
          base::TimeToValue(base::Time::Now() - base::Hours(1))));
  if (test_config.run_with_reboot) {
    EmulateDeviceReboot();
  }

  StartWebKioskSession();

  CheckSessionRestartReasonHistogramDependingOnRebootStatus(
      test_config.run_with_reboot,
      KioskSessionRestartReason::kLocalStateWasNotSaved,
      KioskSessionRestartReason::kLocalStateWasNotSavedWithReboot, histogram());
}

#if BUILDFLAG(ENABLE_PLUGINS)
TEST_P(KioskBrowserSessionRestartReasonTest, PluginCrashedMetric) {
  const KioskSessionRestartTestCase& test_config = GetParam();
  StartWebKioskSession();

  KioskSessionPluginHandlerDelegate* delegate = GetPluginHandlerDelegate();
  delegate->OnPluginCrashed(base::FilePath(kBrowserPluginFilePath));

  // Emulate exiting the kiosk session.
  CloseMainBrowser();
  EXPECT_TRUE(IsSessionShuttingDown());
  if (test_config.run_with_reboot) {
    EmulateDeviceReboot();
  }
  histogram()->ExpectTotalCount(kKioskSessionRestartReasonHistogram, 0);

  StartWebKioskSession();

  CheckSessionRestartReasonHistogramDependingOnRebootStatus(
      test_config.run_with_reboot, KioskSessionRestartReason::kPluginCrashed,
      KioskSessionRestartReason::kPluginCrashedWithReboot, histogram());
}

TEST_P(KioskBrowserSessionRestartReasonTest, PluginHungMetric) {
  const KioskSessionRestartTestCase& test_config = GetParam();
  // Create a fake power manager client.
  // FakePowerManagerClient client;
  StartWebKioskSession();

  KioskSessionPluginHandlerDelegate* delegate = GetPluginHandlerDelegate();
  delegate->OnPluginHung(std::set<int>());

  // Emulate exiting the kiosk session.
  CloseMainBrowser();
  EXPECT_TRUE(IsSessionShuttingDown());
  if (test_config.run_with_reboot) {
    EmulateDeviceReboot();
  }
  histogram()->ExpectTotalCount(kKioskSessionRestartReasonHistogram, 0);

  StartWebKioskSession();

  CheckSessionRestartReasonHistogramDependingOnRebootStatus(
      test_config.run_with_reboot, KioskSessionRestartReason::kPluginHung,
      KioskSessionRestartReason::kPluginHungWithReboot, histogram());
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

INSTANTIATE_TEST_SUITE_P(
    KioskBrowserSessionRestartReasons,
    KioskBrowserSessionRestartReasonTest,
    testing::ValuesIn<KioskSessionRestartTestCase>({
        {/*test_name=*/"WithReboot", /*run_with_reboot=*/true},
        {/*test_name=*/"WithoutReboot", /*run_with_reboot=*/false},
    }),
    [](const testing::TestParamInfo<
        KioskBrowserSessionRestartReasonTest::ParamType>& info) {
      return info.param.test_name;
    });

TEST_F(KioskBrowserSessionRestartReasonTest, PowerManagerRequestRestart) {
  std::vector<KioskSessionPowerManagerRequestRestartTestCase> test_cases = {
      {/*power_manager_reason=*/power_manager::RequestRestartReason::
           REQUEST_RESTART_SCHEDULED_REBOOT_POLICY,
       /*restart_reason=*/KioskSessionRestartReason::kRebootPolicy},
      {/*power_manager_reason=*/power_manager::RequestRestartReason::
           REQUEST_RESTART_REMOTE_ACTION_REBOOT,
       /*restart_reason=*/KioskSessionRestartReason::kRemoteActionReboot},
      {/*power_manager_reason=*/power_manager::RequestRestartReason::
           REQUEST_RESTART_API,
       /*restart_reason=*/KioskSessionRestartReason::kRestartApi}};

  for (auto test_case : test_cases) {
    StartWebKioskSession();
    chromeos::FakePowerManagerClient::Get()->RequestRestart(
        test_case.power_manager_reason, "test reboot description");
    // Emulate exiting the kiosk session.
    CloseMainBrowser();
    EXPECT_TRUE(IsSessionShuttingDown());

    StartWebKioskSession();

    histogram()->ExpectBucketCount(kKioskSessionRestartReasonHistogram,
                                   test_case.restart_reason, 1);
  }
}

// Kiosk type agnostic test class. Runs all tests for web and chrome app kiosks.
class KioskBrowserSessionTroubleshootingTest
    : public KioskBrowserSessionBaseTest<KioskType> {
 public:
  void SetUpKioskSession() {
    switch (GetKioskType()) {
      case KioskType::kChromeApp:
        StartChromeAppKioskSession();
        break;
      case KioskType::kWebApp:
        StartWebKioskSession();
        break;
      case KioskType::kIwa:
        StartIwaKioskSession();
        break;
    }
  }

  void UpdateTroubleshootingToolsPolicy(bool enable) {
    GetPrefs()->SetBoolean(prefs::kKioskTroubleshootingToolsEnabled, enable);
  }

  std::unique_ptr<FakeBrowser> CreateDevToolsBrowserWithTestWindow() {
    auto params = Browser::CreateParams::CreateForDevTools(profile());

    auto test_window = std::make_unique<TestBrowserWindow>();
    params.window = test_window.release();

    return std::make_unique<FakeBrowser>(params);
  }

  std::unique_ptr<FakeBrowser> CreateRegularBrowserWithTestWindow() {
    return CreateBrowserWithTestWindowAndType(Browser::TYPE_NORMAL);
  }

  std::unique_ptr<FakeBrowser> CreateBrowserWithTestWindowAndType(
      Browser::Type type) {
    Browser::CreateParams params(profile(), /*user_gesture=*/true);
    params.type = type;
    return std::make_unique<FakeBrowser>(
        CreateBrowserWithTestWindowForParams(params));
  }

 private:
  KioskType GetKioskType() const { return GetParam(); }
};

TEST_P(KioskBrowserSessionTroubleshootingTest,
       EnableTroubleshootingToolsDuringSession) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  // Kiosk session should shoutdown only if policy is changed from enable to
  // disable.
  EXPECT_FALSE(IsSessionShuttingDown());

  UpdateTroubleshootingToolsPolicy(/*enable=*/false);
  EXPECT_TRUE(IsSessionShuttingDown());
}

TEST_P(KioskBrowserSessionTroubleshootingTest,
       EnableTroubleshootingToolsBeforeSessionStarted) {
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);
  SetUpKioskSession();

  UpdateTroubleshootingToolsPolicy(/*enable=*/false);
  EXPECT_TRUE(IsSessionShuttingDown());
}

TEST_P(KioskBrowserSessionTroubleshootingTest,
       MainBrowserShutdownAfterKioskTroubleshootingToolsDisabled) {
  GetPrefs()->SetBoolean(prefs::kKioskTroubleshootingToolsEnabled, true);

  SetUpKioskSession();

  GetPrefs()->SetBoolean(prefs::kKioskTroubleshootingToolsEnabled, false);

  EXPECT_TRUE(IsSessionShuttingDown());

  CloseMainBrowser();

  EXPECT_TRUE(IsSessionShuttingDown());
}

TEST_P(KioskBrowserSessionTroubleshootingTest,
       OpenDevToolsEnabledTroubleshootingTools) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  EXPECT_FALSE(DidSessionCloseNewWindow(CreateDevToolsBrowserWithTestWindow()));

  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kOpenedDevToolsBrowser,
                                 1);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 1);
}

TEST_P(KioskBrowserSessionTroubleshootingTest,
       CloseTroubleshootingToolsByDefault) {
  SetUpKioskSession();

  // Kiosk troubleshooting tools are disabled by default.
  EXPECT_TRUE(DidSessionCloseNewWindow(CreateDevToolsBrowserWithTestWindow()));
  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kClosedRegularBrowser,
                                 1);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 1);

  EXPECT_TRUE(DidSessionCloseNewWindow(CreateRegularBrowserWithTestWindow()));

  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kClosedRegularBrowser,
                                 2);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 2);
}

TEST_P(KioskBrowserSessionTroubleshootingTest,
       OpenDevToolsDisableTroubleshootingToolsDuringSession) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  // Kiosk session should shoutdown only if policy is changed from enable to
  // disable.
  EXPECT_FALSE(IsSessionShuttingDown());
  EXPECT_FALSE(DidSessionCloseNewWindow(CreateDevToolsBrowserWithTestWindow()));

  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kOpenedDevToolsBrowser,
                                 1);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 1);

  UpdateTroubleshootingToolsPolicy(/*enable=*/false);
  EXPECT_TRUE(IsSessionShuttingDown());
}

TEST_P(KioskBrowserSessionTroubleshootingTest,
       OpenNewWindowEnabledTroubleshootingTools) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  std::unique_ptr<FakeBrowser> normal_browser =
      CreateRegularBrowserWithTestWindow();
  EXPECT_FALSE(DidSessionCloseNewWindow(*normal_browser));

  histogram()->ExpectBucketCount(
      kKioskNewBrowserWindowHistogram,
      KioskBrowserWindowType::kOpenedTroubleshootingNormalBrowser, 1);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 1);
}

TEST_P(KioskBrowserSessionTroubleshootingTest,
       CloseNewWindowDisabledTroubleshootingTools) {
  UpdateTroubleshootingToolsPolicy(/*enable=*/false);
  SetUpKioskSession();

  EXPECT_TRUE(DidSessionCloseNewWindow(CreateRegularBrowserWithTestWindow()));

  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kClosedRegularBrowser,
                                 1);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 1);
}

TEST_P(KioskBrowserSessionTroubleshootingTest,
       OnlyAllowRegularBrowserAndDevToolsAsTroubleshootingBrowsers) {
  const std::vector<Browser::Type> should_be_closed_browser_types = {
      Browser::Type::TYPE_POPUP,        Browser::Type::TYPE_APP,
      Browser::Type::TYPE_APP_POPUP,
      Browser::Type::TYPE_CUSTOM_TAB,
      Browser::TYPE_PICTURE_IN_PICTURE,
  };
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  for (Browser::Type type : should_be_closed_browser_types) {
    EXPECT_TRUE(
        DidSessionCloseNewWindow(CreateBrowserWithTestWindowAndType(type)));
  }

  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kClosedRegularBrowser,
                                 should_be_closed_browser_types.size());
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram,
                                should_be_closed_browser_types.size());
}

INSTANTIATE_TEST_SUITE_P(KioskBrowserSessionTroubleshootingTools,
                         KioskBrowserSessionTroubleshootingTest,
                         ::testing::Values(KioskType::kChromeApp,
                                           KioskType::kWebApp,
                                           KioskType::kIwa));

class FakeNewWindowDelegate : public ash::TestNewWindowDelegate {
 public:
  FakeNewWindowDelegate() = default;
  ~FakeNewWindowDelegate() override = default;

  void NewWindow(bool incognito, bool should_trigger_session_restore) override {
    new_window_called_ = true;
  }

  void NewTab() override { new_tab_called_ = true; }

  void ShowTaskManager() override { task_manager_called_ = true; }

  void OpenFeedbackPage(FeedbackSource source,
                        const std::string& description_template) override {
    open_feedback_page_called_ = true;
  }

  bool is_new_window_called() const { return new_window_called_; }
  bool is_new_tab_called() const { return new_tab_called_; }
  bool is_task_manager_called() const { return task_manager_called_; }
  bool is_open_feedback_page_called() const {
    return open_feedback_page_called_;
  }

 private:
  bool new_window_called_ = false;
  bool new_tab_called_ = false;
  bool task_manager_called_ = false;
  bool open_feedback_page_called_ = false;
};

// Tests actions after pressing troubleshooting shortcuts. Runs all tests for
// web and chrome app kiosks.
class KioskBrowserSessionTroubleshootingShortcutsTest
    : public KioskBrowserSessionTroubleshootingTest {
 public:
  static bool ProcessInController(const ui::Accelerator& accelerator) {
    return ash::Shell::Get()->accelerator_controller()->Process(accelerator);
  }

  void SetUp() override {
    KioskBrowserSessionTroubleshootingTest::SetUp();

    ash::SessionInfo info;
    info.is_running_in_app_mode = true;
    info.state = session_manager::SessionState::ACTIVE;
    ash::Shell::Get()->session_controller()->SetSessionInfo(info);
  }

  bool IsOverviewToggled() const {
    ash::OverviewController* overview_controller =
        ash::Shell::Get()->overview_controller();
    return overview_controller->InOverviewSession();
  }

  bool IsNewWindowCalled() const {
    return fake_new_window_delegate_.is_new_window_called();
  }

  bool IsNewTabCalled() const {
    return fake_new_window_delegate_.is_new_tab_called();
  }

  bool IsTaskManagerCalled() const {
    return fake_new_window_delegate_.is_task_manager_called();
  }

  bool IsOpenFeedbackPageCalled() const {
    return fake_new_window_delegate_.is_open_feedback_page_called();
  }

 protected:
  ui::Accelerator new_window_accelerator =
      ui::Accelerator(ui::VKEY_N, ui::EF_CONTROL_DOWN);
  ui::Accelerator task_manager_accelerator =
      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN);
  ui::Accelerator open_feedback_page_accelerator =
      ui::Accelerator(ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  ui::Accelerator toggle_overview_accelerator =
      ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE);

 private:
  FakeNewWindowDelegate fake_new_window_delegate_;
};

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       NewWindowShortcutEnabled) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  ProcessInController(new_window_accelerator);
  EXPECT_TRUE(IsNewWindowCalled());
}

// Just confirm that other shortcuts (e.g. new tab) do not work.
TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       NewTabShortcutIsNoAction) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  ProcessInController(ui::Accelerator(ui::VKEY_T, ui::EF_CONTROL_DOWN));
  EXPECT_FALSE(IsNewTabCalled());
  EXPECT_FALSE(IsNewWindowCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       NewWindowShortcutNoActionByDefault) {
  SetUpKioskSession();

  ProcessInController(new_window_accelerator);
  EXPECT_FALSE(IsNewWindowCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       NewWindowShortcutNoActionIfPolicyDisabled) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/false);

  ProcessInController(new_window_accelerator);
  EXPECT_FALSE(IsNewWindowCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       TaskManagerShortcutEnabled) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  ProcessInController(task_manager_accelerator);
  EXPECT_TRUE(IsTaskManagerCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       TaskManagerShortcutNoActionByDefault) {
  SetUpKioskSession();

  ProcessInController(task_manager_accelerator);
  EXPECT_FALSE(IsTaskManagerCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       TaskManagerShortcutNoActionIfPolicyDisabled) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/false);

  ProcessInController(task_manager_accelerator);
  EXPECT_FALSE(IsTaskManagerCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       OpenFeedbackPageShortcutEnabled) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  ProcessInController(open_feedback_page_accelerator);
  EXPECT_TRUE(IsOpenFeedbackPageCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       OpenFeedbackPageShortcutNoActionByDefault) {
  SetUpKioskSession();

  ProcessInController(open_feedback_page_accelerator);
  EXPECT_FALSE(IsOpenFeedbackPageCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       OpenFeedbackPageShortcutNoActionIfPolicyDisabled) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/false);

  ProcessInController(open_feedback_page_accelerator);
  EXPECT_FALSE(IsOpenFeedbackPageCalled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       ToggleOverviewShortcutEnabled) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  ProcessInController(toggle_overview_accelerator);
  EXPECT_TRUE(IsOverviewToggled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       ToggleOverviewShortcutNoActionByDefault) {
  SetUpKioskSession();

  ProcessInController(toggle_overview_accelerator);
  EXPECT_FALSE(IsOverviewToggled());
}

TEST_P(KioskBrowserSessionTroubleshootingShortcutsTest,
       ToggleOverviewShortcutNoActionIfPolicyDisabled) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/false);

  ProcessInController(toggle_overview_accelerator);
  EXPECT_FALSE(IsOverviewToggled());
}

INSTANTIATE_TEST_SUITE_P(KioskBrowserSessionTroubleshootingShortcuts,
                         KioskBrowserSessionTroubleshootingShortcutsTest,
                         ::testing::Values(KioskType::kChromeApp,
                                           KioskType::kWebApp,
                                           KioskType::kIwa));

}  // namespace chromeos
