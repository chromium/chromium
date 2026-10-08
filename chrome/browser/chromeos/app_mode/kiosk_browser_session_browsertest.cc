// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_browser_session.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_browser_window_handler.h"
#include "chrome/browser/chromeos/app_mode/kiosk_metrics_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"
#include "chromeos/ash/components/browser_delegate/browser_delegate.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace chromeos {

namespace {

constexpr char kTestAppId[] = "aaaabbbbaaaabbbbaaaabbbbaaaabbbb";
constexpr char kTestWebAppId1[] = "test_web_app_id1";
constexpr char kTestWebAppId2[] = "test_web_app_id2";

enum class KioskType { kChromeApp = 0, kWebApp = 1, kIwa = 2 };

BrowserWindowInterface* CreateBrowserWithParams(
    BrowserWindowCreateParams params) {
  BrowserWindowInterface* browser = CreateBrowserWindow(std::move(params));
  if (browser->GetType() !=
      BrowserWindowInterface::Type::TYPE_PICTURE_IN_PICTURE) {
    ash::BrowserController::GetInstance()->GetDelegate(browser)->AddTab(
        GURL(url::kAboutBlankURL), /*index=*/std::nullopt,
        ash::BrowserDelegate::TabDisposition::kForeground,
        ui::PAGE_TRANSITION_TYPED);
  }
  return browser;
}

}  // namespace

class KioskBrowserSessionBrowserTest : public InProcessBrowserTest {
 public:
  KioskBrowserSessionBrowserTest() { set_exit_when_last_browser_closes(false); }
  KioskBrowserSessionBrowserTest(const KioskBrowserSessionBrowserTest&) =
      delete;
  KioskBrowserSessionBrowserTest& operator=(
      const KioskBrowserSessionBrowserTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kNoStartupWindow);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Remove `kLoginUser` after login has completed so `KioskMetricsService`
    // does not treat sessions as restored after a crash unless a test
    // explicitly sets `kLoginUser`.
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        ash::switches::kLoginUser);
  }

  void TearDownOnMainThread() override {
    kiosk_browser_session_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  PrefService* local_state() { return g_browser_process->local_state(); }

  Profile* profile() { return ProfileManager::GetActiveUserProfile(); }

  base::HistogramTester* histogram() { return &histogram_; }

  BrowserWindowInterface* CreateBrowser() {
    return CreateBrowserWithParams(
        BrowserWindowCreateParams(profile(), /*from_user_gesture=*/true));
  }

  BrowserWindowInterface* CreateBrowserForWebApp(
      const std::string& web_app_id,
      std::optional<BrowserWindowInterface::Type> browser_type = std::nullopt) {
    BrowserWindowCreateParams params =
        BrowserWindowCreateParams::CreateForAppPopup(
            /*app_name=*/web_app::GenerateApplicationNameFromAppId(web_app_id),
            /*trusted_source=*/true,
            /*window_bounds=*/gfx::Rect(), /*profile=*/profile(),
            /*user_gesture=*/true);
    if (browser_type.has_value()) {
      params.type = browser_type.value();
    }
    return CreateBrowserWithParams(std::move(params));
  }

  void CreateWebKioskMainBrowser(const std::string& web_app_id) {
    BrowserWindowInterface* browser =
        CreateBrowserWithParams(BrowserWindowCreateParams::CreateForApp(
            /*app_name=*/web_app::GenerateApplicationNameFromAppId(web_app_id),
            /*trusted_source=*/true,
            /*window_bounds=*/gfx::Rect(), /*profile=*/profile(),
            /*user_gesture=*/true));
    web_kiosk_main_browser_ = browser->GetWeakPtr();
    ash::BrowserController::GetInstance()->GetDelegate(browser)->SetFullscreen(
        true);
  }

  void StartWebKioskSession(const std::string& web_app_id = kTestWebAppId1) {
    CreateWebKioskMainBrowser(web_app_id);

    kiosk_browser_session_ = KioskBrowserSession::CreateForTesting(
        local_state(), profile(), base::DoNothing(), {crash_path().value()});
    kiosk_browser_session_->InitForWebKiosk(web_app_id);

    content::RunAllTasksUntilIdle();
  }

  void StartIwaKioskSession(const std::string& iwa_id = kTestWebAppId1) {
    CreateWebKioskMainBrowser(iwa_id);

    kiosk_browser_session_ = KioskBrowserSession::CreateForTesting(
        local_state(), profile(), base::DoNothing(), {crash_path().value()});
    kiosk_browser_session_->InitForIwaKiosk(iwa_id);

    content::RunAllTasksUntilIdle();
  }

  void StartChromeAppKioskSession() {
    kiosk_browser_session_ = std::make_unique<KioskBrowserSession>(
        local_state(), profile(), base::DoNothing());
    kiosk_browser_session_->InitForChromeAppKiosk(kTestAppId);
  }

  bool DidSessionCloseNewWindow(BrowserWindowInterface* new_browser) {
    ui_test_utils::BrowserDestroyedObserver destroyed_observer(new_browser);
    base::test::TestFuture<bool> is_handled;
    kiosk_browser_session_->SetOnHandleBrowserCallbackForTesting(
        is_handled.GetRepeatingCallback());
    bool is_closed_by_kiosk_session = is_handled.Get();

    if (is_closed_by_kiosk_session) {
      destroyed_observer.Wait();
    }

    return is_closed_by_kiosk_session;
  }

  void CloseMainBrowser() {
    if (web_kiosk_main_browser_) {
      CloseBrowserSynchronously(web_kiosk_main_browser_.get());
    }
  }

  bool IsMainBrowserClosed() const { return !web_kiosk_main_browser_; }

  bool IsMainBrowserFullscreen() const {
    return ash::BrowserController::GetInstance()
        ->GetDelegate(web_kiosk_main_browser_.get())
        ->IsFullscreen();
  }

  bool IsSessionShuttingDown() const {
    return kiosk_browser_session_->is_shutting_down();
  }

  void ResetKioskBrowserSession() { kiosk_browser_session_.reset(); }

  PrefService* GetPrefs() { return profile()->GetPrefs(); }

  base::FilePath crash_path() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
  base::WeakPtr<BrowserWindowInterface> web_kiosk_main_browser_;
  base::HistogramTester histogram_;
  std::unique_ptr<KioskBrowserSession> kiosk_browser_session_;
};

IN_PROC_BROWSER_TEST_F(KioskBrowserSessionBrowserTest,
                       WebKioskTracksBrowserCreation) {
  local_state()->SetDict(
      ash::prefs::kKioskMetrics,
      base::DictValue().Set(kKioskSessionStartTime,
                            base::TimeToValue(base::Time::Now())));

  StartWebKioskSession();
  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kWebStarted, 1);
  histogram()->ExpectTotalCount(kKioskSessionCountPerDayHistogram, 1);

  EXPECT_TRUE(DidSessionCloseNewWindow(CreateBrowser()));

  // The main browser window still exists, the kiosk session should not
  // shutdown.
  EXPECT_FALSE(IsSessionShuttingDown());
  // Opening a new browser should not be counted as a new session.
  histogram()->ExpectTotalCount(kKioskSessionCountPerDayHistogram, 1);

  CloseMainBrowser();
  EXPECT_TRUE(IsSessionShuttingDown());

  const base::DictValue& dict =
      local_state()->GetDict(ash::prefs::kKioskMetrics);
  const base::ListValue* sessions_list =
      dict.FindList(kKioskSessionLastDayList);
  ASSERT_TRUE(sessions_list);
  EXPECT_EQ(1u, sessions_list->size());

  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kStopped, 1);
  EXPECT_EQ(2u, histogram()->GetAllSamples(kKioskSessionStateHistogram).size());

  histogram()->ExpectTotalCount(kKioskSessionDurationNormalHistogram, 1);
  histogram()->ExpectTotalCount(kKioskSessionDurationInDaysNormalHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(KioskBrowserSessionBrowserTest,
                       ChromeAppKioskTracksBrowserCreation) {
  StartChromeAppKioskSession();

  EXPECT_TRUE(DidSessionCloseNewWindow(CreateBrowser()));
  // Closing the browser should not shutdown the ChromeApp kiosk session.
  EXPECT_FALSE(IsSessionShuttingDown());
  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kClosedRegularBrowser,
                                 1);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 1);

  const base::DictValue& dict =
      local_state()->GetDict(ash::prefs::kKioskMetrics);
  const base::ListValue* sessions_list =
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

IN_PROC_BROWSER_TEST_F(KioskBrowserSessionBrowserTest,
                       ChromeAppKioskShouldClosePreexistingBrowsers) {
  BrowserWindowInterface* preexisting_browser = CreateBrowser();
  ui_test_utils::BrowserDestroyedObserver destroyed_observer(
      preexisting_browser);

  StartChromeAppKioskSession();

  destroyed_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(KioskBrowserSessionBrowserTest,
                       WebKioskShouldClosePreexistingBrowsers) {
  BrowserWindowInterface* preexisting_browser = CreateBrowser();
  ui_test_utils::BrowserDestroyedObserver destroyed_observer(
      preexisting_browser);

  StartWebKioskSession();

  destroyed_observer.Wait();
  EXPECT_FALSE(IsMainBrowserClosed());
}

// Check that sessions list in local_state contains only sessions within the
// last 24h.
IN_PROC_BROWSER_TEST_F(KioskBrowserSessionBrowserTest,
                       WebKioskLastDaySessions) {
  // Setup local_state with 5 more kiosk sessions happened prior to the current
  // one: {now, 2,3,4,5 days ago}
  {
    auto session_list =
        base::ListValue().Append(base::TimeToValue(base::Time::Now()));

    const size_t kMaxDays = 4;
    for (size_t i = 0; i < kMaxDays; i++) {
      session_list.Append(
          base::TimeToValue(base::Time::Now() - base::Days(i + 2)));
    }

    local_state()->SetDict(
        ash::prefs::kKioskMetrics,
        base::DictValue()
            .Set(kKioskSessionLastDayList, std::move(session_list))
            // Emulates previous session crashes.
            .Set(kKioskSessionStartTime,
                 base::TimeToValue(base::Time::Now() -
                                   2 * kKioskSessionDurationHistogramLimit)));
  }

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ash::switches::kLoginUser, "fake-user");

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath crash_file;
    ASSERT_TRUE(base::CreateTemporaryFileInDir(crash_path(), &crash_file));
  }

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

  const base::DictValue& dict =
      local_state()->GetDict(ash::prefs::kKioskMetrics);
  const base::ListValue* sessions_list =
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

IN_PROC_BROWSER_TEST_F(KioskBrowserSessionBrowserTest,
                       DoNotOpenSecondBrowserInWebKiosk) {
  StartWebKioskSession(kTestWebAppId1);

  EXPECT_TRUE(DidSessionCloseNewWindow(CreateBrowserForWebApp(kTestWebAppId1)));
}

IN_PROC_BROWSER_TEST_F(KioskBrowserSessionBrowserTest,
                       DoNotCrashIfBrowserClosedSuccessfully) {
  StartWebKioskSession(kTestWebAppId1);

  BrowserWindowInterface* browser = CreateBrowserForWebApp(kTestWebAppId1);
  ui_test_utils::BrowserDestroyedObserver(browser).Wait();
}

IN_PROC_BROWSER_TEST_F(KioskBrowserSessionBrowserTest,
                       OpenSecondBrowserInWebKioskIfAllowed) {
  GetPrefs()->SetBoolean(ash::prefs::kNewWindowsInKioskAllowed, true);
  StartWebKioskSession(kTestWebAppId1);

  EXPECT_FALSE(
      DidSessionCloseNewWindow(CreateBrowserForWebApp(kTestWebAppId1)));
}

IN_PROC_BROWSER_TEST_F(KioskBrowserSessionBrowserTest,
                       EnsureSecondBrowserIsFullscreenInWebKiosk) {
  GetPrefs()->SetBoolean(ash::prefs::kNewWindowsInKioskAllowed, true);
  StartWebKioskSession(kTestWebAppId1);
  EXPECT_TRUE(IsMainBrowserFullscreen());

  BrowserWindowInterface* second_browser =
      CreateBrowserForWebApp(kTestWebAppId1);
  DidSessionCloseNewWindow(second_browser);

  EXPECT_TRUE(ash::BrowserController::GetInstance()
                  ->GetDelegate(second_browser)
                  ->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(KioskBrowserSessionBrowserTest,
                       DoNotOpenSecondBrowserInWebKioskIfTypeIsNotAppPopup) {
  const std::vector<BrowserWindowInterface::Type> not_app_popup_browser_types =
      {
          BrowserWindowInterface::Type::TYPE_NORMAL,
          BrowserWindowInterface::Type::TYPE_POPUP,
          BrowserWindowInterface::Type::TYPE_APP,
          BrowserWindowInterface::Type::TYPE_DEVTOOLS,
          BrowserWindowInterface::Type::TYPE_PICTURE_IN_PICTURE,
      };

  GetPrefs()->SetBoolean(ash::prefs::kNewWindowsInKioskAllowed, true);
  StartWebKioskSession(kTestWebAppId1);

  for (auto browser_type : not_app_popup_browser_types) {
    EXPECT_TRUE(DidSessionCloseNewWindow(
        CreateBrowserForWebApp(kTestWebAppId1, browser_type)));
  }
}

IN_PROC_BROWSER_TEST_F(KioskBrowserSessionBrowserTest,
                       DoNotOpenSecondBrowserInWebKioskWithEmptyWebAppId) {
  GetPrefs()->SetBoolean(ash::prefs::kNewWindowsInKioskAllowed, true);
  StartWebKioskSession();

  EXPECT_TRUE(DidSessionCloseNewWindow(CreateBrowser()));
}

IN_PROC_BROWSER_TEST_F(KioskBrowserSessionBrowserTest,
                       DoNotOpenSecondBrowserInWebKioskWithDifferentWebAppId) {
  GetPrefs()->SetBoolean(ash::prefs::kNewWindowsInKioskAllowed, true);
  StartWebKioskSession(kTestWebAppId1);

  EXPECT_TRUE(DidSessionCloseNewWindow(CreateBrowserForWebApp(kTestWebAppId2)));
}

IN_PROC_BROWSER_TEST_F(KioskBrowserSessionBrowserTest,
                       DoNotOpenSecondBrowserInChromeAppKiosk) {
  // This flag allows opening new windows only for the web kiosk session. For
  // chrome app kiosk we still should block all new browsers.
  GetPrefs()->SetBoolean(ash::prefs::kNewWindowsInKioskAllowed, true);
  StartChromeAppKioskSession();

  EXPECT_TRUE(DidSessionCloseNewWindow(CreateBrowserForWebApp(kTestWebAppId2)));
}

IN_PROC_BROWSER_TEST_F(KioskBrowserSessionBrowserTest,
                       NewOpenedRegularBrowserMetrics) {
  GetPrefs()->SetBoolean(ash::prefs::kNewWindowsInKioskAllowed, true);
  StartWebKioskSession(kTestWebAppId1);

  DidSessionCloseNewWindow(CreateBrowserForWebApp(kTestWebAppId1));

  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kOpenedRegularBrowser,
                                 1);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 1);
}

IN_PROC_BROWSER_TEST_F(KioskBrowserSessionBrowserTest,
                       NewClosedRegularBrowserMetrics) {
  GetPrefs()->SetBoolean(ash::prefs::kNewWindowsInKioskAllowed, false);
  StartWebKioskSession(kTestWebAppId1);

  DidSessionCloseNewWindow(CreateBrowserForWebApp(kTestWebAppId1));

  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kClosedRegularBrowser,
                                 1);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 1);
}

IN_PROC_BROWSER_TEST_F(KioskBrowserSessionBrowserTest,
                       DoNotExitWebKioskSessionWhenSecondBrowserIsOpened) {
  GetPrefs()->SetBoolean(ash::prefs::kNewWindowsInKioskAllowed, true);
  StartWebKioskSession();

  BrowserWindowInterface* second_browser =
      CreateBrowserForWebApp(kTestWebAppId1);
  EXPECT_FALSE(DidSessionCloseNewWindow(second_browser));

  CloseMainBrowser();
  EXPECT_FALSE(IsSessionShuttingDown());

  CloseBrowserSynchronously(second_browser);
  // Exit kiosk session when the last browser is closed.
  EXPECT_TRUE(IsSessionShuttingDown());
}

IN_PROC_BROWSER_TEST_F(KioskBrowserSessionBrowserTest,
                       InitialBrowserShouldBeHandledAsRegularBrowser) {
  GetPrefs()->SetBoolean(ash::prefs::kNewWindowsInKioskAllowed, true);
  StartWebKioskSession();

  BrowserWindowInterface* second_browser =
      CreateBrowserForWebApp(kTestWebAppId1);
  EXPECT_FALSE(DidSessionCloseNewWindow(second_browser));

  CloseBrowserSynchronously(second_browser);
  EXPECT_FALSE(IsSessionShuttingDown());

  CloseMainBrowser();
  // Exit kiosk session when the last browser is closed.
  EXPECT_TRUE(IsSessionShuttingDown());
}

class KioskBrowserSessionTroubleshootingBrowserTest
    : public KioskBrowserSessionBrowserTest,
      public ::testing::WithParamInterface<KioskType> {
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
    GetPrefs()->SetBoolean(ash::prefs::kKioskTroubleshootingToolsEnabled,
                           enable);
  }

  BrowserWindowInterface* CreateDevToolsBrowser() {
    return CreateBrowserWithParams(
        BrowserWindowCreateParams::CreateForDevTools(profile()));
  }

  BrowserWindowInterface* CreateRegularBrowser() {
    return CreateBrowserWithType(BrowserWindowInterface::Type::TYPE_NORMAL);
  }

  BrowserWindowInterface* CreateBrowserWithType(
      BrowserWindowInterface::Type type) {
    if (type == BrowserWindowInterface::Type::TYPE_APP ||
        type == BrowserWindowInterface::Type::TYPE_APP_POPUP) {
      return CreateBrowserForWebApp(kTestWebAppId1, type);
    }
    BrowserWindowCreateParams params(profile(), /*from_user_gesture=*/true);
    params.type = type;
    return CreateBrowserWithParams(std::move(params));
  }

 private:
  KioskType GetKioskType() const { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(
    KioskBrowserSessionTroubleshootingBrowserTest,
    MainBrowserShutdownAfterKioskTroubleshootingToolsDisabled) {
  GetPrefs()->SetBoolean(ash::prefs::kKioskTroubleshootingToolsEnabled, true);

  SetUpKioskSession();

  GetPrefs()->SetBoolean(ash::prefs::kKioskTroubleshootingToolsEnabled, false);

  EXPECT_TRUE(IsSessionShuttingDown());

  CloseMainBrowser();

  EXPECT_TRUE(IsSessionShuttingDown());
}

IN_PROC_BROWSER_TEST_P(KioskBrowserSessionTroubleshootingBrowserTest,
                       OpenDevToolsEnabledTroubleshootingTools) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  EXPECT_FALSE(DidSessionCloseNewWindow(CreateDevToolsBrowser()));

  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kOpenedDevToolsBrowser,
                                 1);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 1);
}

IN_PROC_BROWSER_TEST_P(KioskBrowserSessionTroubleshootingBrowserTest,
                       CloseTroubleshootingToolsByDefault) {
  SetUpKioskSession();

  // Kiosk troubleshooting tools are disabled by default.
  EXPECT_TRUE(DidSessionCloseNewWindow(CreateDevToolsBrowser()));
  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kClosedRegularBrowser,
                                 1);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 1);

  EXPECT_TRUE(DidSessionCloseNewWindow(CreateRegularBrowser()));

  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kClosedRegularBrowser,
                                 2);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 2);
}

IN_PROC_BROWSER_TEST_P(KioskBrowserSessionTroubleshootingBrowserTest,
                       OpenDevToolsDisableTroubleshootingToolsDuringSession) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  // Kiosk session should shutdown only if policy is changed from enable to
  // disable.
  EXPECT_FALSE(IsSessionShuttingDown());
  EXPECT_FALSE(DidSessionCloseNewWindow(CreateDevToolsBrowser()));

  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kOpenedDevToolsBrowser,
                                 1);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 1);

  UpdateTroubleshootingToolsPolicy(/*enable=*/false);
  EXPECT_TRUE(IsSessionShuttingDown());
}

IN_PROC_BROWSER_TEST_P(KioskBrowserSessionTroubleshootingBrowserTest,
                       OpenNewWindowEnabledTroubleshootingTools) {
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  EXPECT_FALSE(DidSessionCloseNewWindow(CreateRegularBrowser()));

  histogram()->ExpectBucketCount(
      kKioskNewBrowserWindowHistogram,
      KioskBrowserWindowType::kOpenedTroubleshootingNormalBrowser, 1);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 1);
}

IN_PROC_BROWSER_TEST_P(KioskBrowserSessionTroubleshootingBrowserTest,
                       CloseNewWindowDisabledTroubleshootingTools) {
  UpdateTroubleshootingToolsPolicy(/*enable=*/false);
  SetUpKioskSession();

  EXPECT_TRUE(DidSessionCloseNewWindow(CreateRegularBrowser()));

  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kClosedRegularBrowser,
                                 1);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 1);
}

IN_PROC_BROWSER_TEST_P(
    KioskBrowserSessionTroubleshootingBrowserTest,
    OnlyAllowRegularBrowserAndDevToolsAsTroubleshootingBrowsers) {
  const std::vector<BrowserWindowInterface::Type>
      should_be_closed_browser_types = {
          BrowserWindowInterface::Type::TYPE_POPUP,
          BrowserWindowInterface::Type::TYPE_APP,
          BrowserWindowInterface::Type::TYPE_APP_POPUP,
          BrowserWindowInterface::Type::TYPE_PICTURE_IN_PICTURE,
      };
  SetUpKioskSession();
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  for (BrowserWindowInterface::Type type : should_be_closed_browser_types) {
    EXPECT_TRUE(DidSessionCloseNewWindow(CreateBrowserWithType(type)));
  }

  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kClosedRegularBrowser,
                                 should_be_closed_browser_types.size());
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram,
                                should_be_closed_browser_types.size());
}

INSTANTIATE_TEST_SUITE_P(KioskBrowserSessionTroubleshootingTools,
                         KioskBrowserSessionTroubleshootingBrowserTest,
                         ::testing::Values(KioskType::kChromeApp,
                                           KioskType::kWebApp,
                                           KioskType::kIwa));

}  // namespace chromeos
