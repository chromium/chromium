// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/values_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/app_mode/app_session.h"
#include "chrome/browser/chromeos/app_mode/app_session_browser_window_handler.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/chromeos/app_mode/kiosk_session_plugin_handler_delegate.h"
#include "content/public/browser/plugin_service.h"
#endif

namespace chromeos {

namespace {

using ::chromeos::FakePowerManagerClient;

constexpr char kTestAppId[] = "aaaabbbbaaaabbbbaaaabbbbaaaabbbb";

#if BUILDFLAG(ENABLE_PLUGINS)
constexpr char16_t kPepperPluginName1[] = u"pepper_plugin_name1";
constexpr char16_t kPepperPluginName2[] = u"pepper_plugin_name2";
constexpr char16_t kBrowserPluginName[] = u"browser_plugin_name";
constexpr char kPepperPluginFilePath1[] = "/path/to/pepper_plugin1";
constexpr char kPepperPluginFilePath2[] = "/path/to/pepper_plugin2";
constexpr char kBrowserPluginFilePath[] = "/path/to/browser_plugin";
constexpr char kUnregisteredPluginFilePath[] = "/path/to/unregistered_plugin";
#endif  // BUILDFLAG(ENABLE_PLUGINS)

}  // namespace

class AppSessionTest : public testing::Test {
 public:
  explicit AppSessionTest(base::test::TaskEnvironment::TimeSource time_source =
                              base::test::TaskEnvironment::TimeSource::DEFAULT)
      : task_environment_{time_source},
        local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())) {}

  AppSessionTest(const AppSessionTest&) = delete;
  AppSessionTest& operator=(const AppSessionTest&) = delete;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  TestingPrefServiceSimple* local_state() { return local_state_->Get(); }

  TestingProfile* profile() { return &profile_; }

  base::HistogramTester* histogram() { return &histogram_; }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  std::unique_ptr<Browser> CreateBrowserWithTestWindow() {
    return CreateBrowserWithTestWindowForParams(
        Browser::CreateParams(&profile_, true));
  }

  // Simulate starting a web kiosk session.
  void StartWebKioskSession() {
    // Create the main kiosk browser window, which is normally auto-created when
    // a web kiosk session starts.
    web_kiosk_main_browser_ = CreateBrowserWithTestWindow();

    app_session_ = AppSession::CreateForTesting(
        base::DoNothing(), local_state(), {crash_path().value()});
    app_session_->InitForWebKiosk(web_kiosk_main_browser_.get());

    task_environment_.RunUntilIdle();
  }

  // Simulate starting a chrome app kiosk session.
  void StartChromeAppKioskSession() {
    app_session_ =
        std::make_unique<AppSession>(base::DoNothing(), local_state());
    app_session_->Init(&profile_, kTestAppId);
  }

  // Simulate opening a new browser window, and ensure it is automatically
  // closed.
  void OpenNewBrowserAndEnsureItIsClosed() {
    auto new_browser = CreateBrowserWithTestWindow();

    // Ensure it is closed.
    base::RunLoop loop;
    static_cast<TestBrowserWindow*>(new_browser->window())
        ->SetCloseCallback(
            base::BindLambdaForTesting([&loop]() { loop.Quit(); }));
    loop.Run();
  }

  void CloseMainBrowser() {
    // Close the main browser window.
    web_kiosk_main_browser_.reset();
  }

  bool IsSessionShuttingDown() const {
    return app_session_->is_shutting_down();
  }

  base::FilePath crash_path() const { return temp_dir_.GetPath(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ScopedTestingLocalState> local_state_;

  // Must outlive |app_session_|.
  TestingProfile profile_;
  // Main browser window created when launching a web kiosk app.
  // Could be nullptr if |StartWebKioskSession| function was not called.
  std::unique_ptr<Browser> web_kiosk_main_browser_;
  base::HistogramTester histogram_;
  std::unique_ptr<AppSession> app_session_;
};

class AppSessionTestMockTime : public AppSessionTest {
 public:
  AppSessionTestMockTime()
      : AppSessionTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(AppSessionTest, WebKioskTracksBrowserCreation) {
  {
    base::Value::Dict value;
    value.Set(kKioskSessionStartTime, base::TimeToValue(base::Time::Now()));

    local_state()->SetDict(prefs::kKioskMetrics, std::move(value));
  }

  StartWebKioskSession();
  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kWebStarted, 1);
  histogram()->ExpectTotalCount(kKioskSessionCountPerDayHistogram, 1);

  OpenNewBrowserAndEnsureItIsClosed();
  // The main browser window still exists, the kiosk session should not
  // shutdown.
  EXPECT_FALSE(IsSessionShuttingDown());
  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kOther, 1);
  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kSettingsPage, 0);
  // Opening a new browser should not be counted as a new session.
  histogram()->ExpectTotalCount(kKioskSessionCountPerDayHistogram, 1);

  CloseMainBrowser();
  EXPECT_TRUE(IsSessionShuttingDown());

  const base::Value::Dict& dict = local_state()->GetDict(prefs::kKioskMetrics);
  const base::Value::List* sessions_list =
      dict.FindList(kKioskSessionLastDayList);
  ASSERT_TRUE(sessions_list);
  EXPECT_EQ(1, sessions_list->size());

  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kStopped, 1);
  EXPECT_EQ(2, histogram()->GetAllSamples(kKioskSessionStateHistogram).size());

  histogram()->ExpectTotalCount(kKioskSessionDurationNormalHistogram, 1);
  histogram()->ExpectTotalCount(kKioskSessionDurationInDaysNormalHistogram, 0);
}

TEST_F(AppSessionTest, ChromeAppKioskSessionState) {
  StartChromeAppKioskSession();
  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kStarted, 1);
  histogram()->ExpectTotalCount(kKioskSessionCountPerDayHistogram, 1);
}

TEST_F(AppSessionTest, ChromeAppKioskTracksBrowserCreation) {
  auto app_session =
      std::make_unique<AppSession>(base::DoNothing(), local_state());
  app_session->Init(profile(), kTestAppId);

  OpenNewBrowserAndEnsureItIsClosed();
  // Closing the browser should not shutdown the ChromeApp kiosk session.
  EXPECT_FALSE(app_session->is_shutting_down());
  histogram()->ExpectBucketCount(kKioskNewBrowserWindowHistogram,
                                 KioskBrowserWindowType::kOther, 1);
  histogram()->ExpectTotalCount(kKioskNewBrowserWindowHistogram, 1);

  const base::Value::Dict& dict = local_state()->GetDict(prefs::kKioskMetrics);
  const base::Value::List* sessions_list =
      dict.FindList(kKioskSessionLastDayList);
  ASSERT_TRUE(sessions_list);
  EXPECT_EQ(1, sessions_list->size());

  // Emulate exiting kiosk session.
  app_session.reset();

  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kStopped, 1);
  EXPECT_EQ(2, histogram()->GetAllSamples(kKioskSessionStateHistogram).size());

  histogram()->ExpectTotalCount(kKioskSessionDurationNormalHistogram, 1);
  histogram()->ExpectTotalCount(kKioskSessionDurationInDaysNormalHistogram, 0);
}

// Check that sessions list in local_state contains only sessions within the
// last 24h.
TEST_F(AppSessionTest, WebKioskLastDaySessions) {
  // Setup local_state with 5 more kiosk sessions happened prior to the current
  // one: {now, 2,3,4,5 days ago}
  {
    base::Value::List session_list;
    session_list.Append(base::TimeToValue(base::Time::Now()));

    const size_t kMaxDays = 4;
    for (size_t i = 0; i < kMaxDays; i++) {
      session_list.Append(
          base::TimeToValue(base::Time::Now() - base::Days(i + 2)));
    }

    base::Value::Dict value;
    value.Set(kKioskSessionLastDayList, std::move(session_list));
    value.Set(kKioskSessionStartTime,
              base::TimeToValue(base::Time::Now() -
                                2 * kKioskSessionDurationHistogramLimit));

    local_state()->SetDict(prefs::kKioskMetrics, std::move(value));
  }

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ash::switches::kLoginUser, "fake-user");

  base::FilePath crash_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(crash_path(), &crash_file));

  StartWebKioskSession();
  // We set |kKioskSessionStartTime| for previous session and did not clear them
  // up, so it emulates previous session crashes.
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
  EXPECT_EQ(2, sessions_list->size());
  for (const auto& time : *sessions_list) {
    EXPECT_LE(base::Time::Now() - base::ValueToTime(time).value(),
              base::Days(1));
  }

  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kStopped, 1);
  EXPECT_EQ(3, histogram()->GetAllSamples(kKioskSessionStateHistogram).size());
  histogram()->ExpectTotalCount(kKioskSessionDurationNormalHistogram, 1);
  histogram()->ExpectTotalCount(kKioskSessionDurationInDaysNormalHistogram, 0);
}

TEST_F(AppSessionTestMockTime, PeriodicMetrics) {
  const char* const kPeriodicMetrics[] = {
      kKioskRamUsagePercentageHistogram, kKioskSwapUsagePercentageHistogram,
      kKioskDiskUsagePercentageHistogram, kKioskChromeProcessCountHistogram};
  StartWebKioskSession();

  task_environment()->FastForwardBy(kPeriodicMetricsInterval / 2);
  for (const char* metric : kPeriodicMetrics) {
    histogram()->ExpectTotalCount(metric, 0);
  }

  task_environment()->FastForwardBy(kPeriodicMetricsInterval / 2);
  for (const char* metric : kPeriodicMetrics) {
    histogram()->ExpectTotalCount(metric, 1);
  }
}

#if BUILDFLAG(ENABLE_PLUGINS)
TEST_F(AppSessionTest, ShouldHandlePlugin) {
  // Create an out-of-process pepper plugin.
  content::WebPluginInfo info1;
  info1.name = kPepperPluginName1;
  info1.path = base::FilePath(kPepperPluginFilePath1);
  info1.type = content::WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS;

  // Create an in-of-process pepper plugin.
  content::WebPluginInfo info2;
  info2.name = kPepperPluginName2;
  info2.path = base::FilePath(kPepperPluginFilePath2);
  info2.type = content::WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;

  // Create an in-of-process browser (non-pepper) plugin.
  content::WebPluginInfo info3;
  info3.name = kBrowserPluginName;
  info3.path = base::FilePath(kBrowserPluginFilePath);
  info3.type = content::WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN;

  // Register two pepper plugins.
  content::PluginService* service = content::PluginService::GetInstance();
  service->RegisterInternalPlugin(info1, true);
  service->RegisterInternalPlugin(info2, true);
  service->RegisterInternalPlugin(info3, true);
  service->Init();
  service->RefreshPlugins();

  // Force plugins to load and wait for completion.
  base::RunLoop run_loop;
  service->GetPlugins(base::BindOnce(
      [](base::OnceClosure callback,
         const std::vector<content::WebPluginInfo>& ignore) {
        std::move(callback).Run();
      },
      run_loop.QuitClosure()));
  run_loop.Run();

  AppSession app_session;
  KioskSessionPluginHandlerDelegate* delegate =
      app_session.GetPluginHandlerDelegateForTesting();

  // The app session should handle two pepper plugins.
  EXPECT_TRUE(
      delegate->ShouldHandlePlugin(base::FilePath(kPepperPluginFilePath1)));
  EXPECT_TRUE(
      delegate->ShouldHandlePlugin(base::FilePath(kPepperPluginFilePath2)));

  // The app session should not handle the browser plugin.
  EXPECT_FALSE(
      delegate->ShouldHandlePlugin(base::FilePath(kBrowserPluginFilePath)));

  // The app session should not handle the unregistered plugin.
  EXPECT_FALSE(delegate->ShouldHandlePlugin(
      base::FilePath(kUnregisteredPluginFilePath)));
}

TEST_F(AppSessionTest, OnPluginCrashed) {
  AppSession app_session;
  KioskSessionPluginHandlerDelegate* delegate =
      app_session.GetPluginHandlerDelegateForTesting();

  // Create a fake power manager client.
  FakePowerManagerClient client;

  // Verified the number of restart calls.
  EXPECT_EQ(client.num_request_restart_calls(), 0);
  delegate->OnPluginCrashed(base::FilePath(kBrowserPluginFilePath));
  EXPECT_EQ(client.num_request_restart_calls(), 1);

  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kPluginCrashed, 1);
  EXPECT_EQ(1, histogram()->GetAllSamples(kKioskSessionStateHistogram).size());

  histogram()->ExpectTotalCount(kKioskSessionCountPerDayHistogram, 0);
}

TEST_F(AppSessionTest, OnPluginHung) {
  AppSession app_session;
  KioskSessionPluginHandlerDelegate* delegate =
      app_session.GetPluginHandlerDelegateForTesting();

  // Create a fake power manager client.
  FakePowerManagerClient::InitializeFake();

  // Only verify if this method can be called without error.
  delegate->OnPluginHung(std::set<int>());
  histogram()->ExpectBucketCount(kKioskSessionStateHistogram,
                                 KioskSessionState::kPluginHung, 1);
  EXPECT_EQ(1, histogram()->GetAllSamples(kKioskSessionStateHistogram).size());

  histogram()->ExpectTotalCount(kKioskSessionCountPerDayHistogram, 0);
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

}  // namespace chromeos
