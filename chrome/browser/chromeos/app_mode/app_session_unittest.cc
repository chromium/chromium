// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/json/values_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/app_mode/app_session.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

using ::chromeos::FakePowerManagerClient;

constexpr char16_t kPepperPluginName1[] = u"pepper_plugin_name1";
constexpr char16_t kPepperPluginName2[] = u"pepper_plugin_name2";
constexpr char16_t kBrowserPluginName[] = u"browser_plugin_name";
constexpr char kPepperPluginFilePath1[] = "/path/to/pepper_plugin1";
constexpr char kPepperPluginFilePath2[] = "/path/to/pepper_plugin2";
constexpr char kBrowserPluginFilePath[] = "/path/to/browser_plugin";
constexpr char kUnregisteredPluginFilePath[] = "/path/to/unregistered_plugin";

}  // namespace

class AppSessionTest : public testing::Test {
 public:
  AppSessionTest()
      : local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())) {}

  AppSessionTest(const AppSessionTest&) = delete;
  AppSessionTest& operator=(const AppSessionTest&) = delete;

  TestingPrefServiceSimple* local_state() { return local_state_->Get(); }

  void TearDown() override { local_state()->RemoveUserPref(kKioskMetrics); }

  void WebKioskTracksBrowserCreationTest() {
    auto app_session =
        std::make_unique<AppSession>(base::DoNothing(), local_state());
    TestingProfile profile;

    Browser::CreateParams params(&profile, true);
    auto app_browser = CreateBrowserWithTestWindowForParams(params);

    app_session->InitForWebKiosk(app_browser.get());

    Browser::CreateParams another_params(&profile, true);
    auto another_browser = CreateBrowserWithTestWindowForParams(another_params);

    base::RunLoop loop;
    static_cast<TestBrowserWindow*>(another_browser->window())
        ->SetCloseCallback(
            base::BindLambdaForTesting([&loop]() { loop.Quit(); }));
    loop.Run();

    bool chrome_closed = false;
    app_session->SetAttemptUserExitForTesting(base::BindLambdaForTesting(
        [&chrome_closed]() { chrome_closed = true; }));

    app_browser.reset();
    ASSERT_TRUE(chrome_closed);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ScopedTestingLocalState> local_state_;
};

TEST_F(AppSessionTest, WebKioskTracksBrowserCreation) {
  base::HistogramTester histogram;

  WebKioskTracksBrowserCreationTest();

  const base::Value* value = local_state()->GetDictionary(kKioskMetrics);
  ASSERT_TRUE(value);
  const base::Value* sessions_list =
      value->FindListKey(kKioskSessionLastDayList);
  ASSERT_TRUE(sessions_list);
  EXPECT_EQ(1, sessions_list->GetIfList()->size());

  histogram.ExpectBucketCount(kKioskSessionStateHistogram,
                              KioskSessionState::kWebStarted, 1);
  histogram.ExpectBucketCount(kKioskSessionStateHistogram,
                              KioskSessionState::kStopped, 1);
  EXPECT_EQ(2, histogram.GetAllSamples(kKioskSessionStateHistogram).size());

  histogram.ExpectTotalCount(kKioskSessionDurationNormalHistogram, 1);
  histogram.ExpectTotalCount(kKioskSessionDurationInDaysNormalHistogram, 0);
  histogram.ExpectTotalCount(kKioskSessionCountPerDayHistogram, 1);
}

// Check that sessions list in local_state contains only sessions within the
// last 24h.
TEST_F(AppSessionTest, WebKioskLastDaySessions) {
  base::HistogramTester histogram;
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

    local_state()->SetDict(kKioskMetrics, std::move(value));
  }

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ash::switches::kLoginUser, "fake-user");

  WebKioskTracksBrowserCreationTest();

  const base::Value* value = local_state()->GetDictionary(kKioskMetrics);
  ASSERT_TRUE(value);
  const base::Value* sessions_list =
      value->FindListKey(kKioskSessionLastDayList);
  ASSERT_TRUE(sessions_list);
  // There should be only two kiosk sessions on the list:
  // the one that happened right before the current one and the current one.
  EXPECT_EQ(2, sessions_list->GetIfList()->size());
  for (const auto& time : *sessions_list->GetIfList()) {
    EXPECT_LE(base::Time::Now() - base::ValueToTime(time).value(),
              base::Days(1));
  }

  histogram.ExpectBucketCount(kKioskSessionStateHistogram,
                              KioskSessionState::kRestored, 1);
  histogram.ExpectBucketCount(kKioskSessionStateHistogram,
                              KioskSessionState::kCrashed, 1);
  histogram.ExpectBucketCount(kKioskSessionStateHistogram,
                              KioskSessionState::kStopped, 1);
  EXPECT_EQ(3, histogram.GetAllSamples(kKioskSessionStateHistogram).size());

  histogram.ExpectTotalCount(kKioskSessionDurationCrashedHistogram, 1);
  histogram.ExpectTotalCount(kKioskSessionDurationNormalHistogram, 1);

  histogram.ExpectTotalCount(kKioskSessionDurationInDaysCrashedHistogram, 1);
  histogram.ExpectTotalCount(kKioskSessionDurationInDaysNormalHistogram, 0);

  histogram.ExpectTotalCount(kKioskSessionCountPerDayHistogram, 1);
}

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

  // Create an app session.
  std::unique_ptr<AppSession> app_session = std::make_unique<AppSession>();
  KioskSessionPluginHandlerDelegate* delegate = app_session.get();

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
  base::HistogramTester histogram;
  // Create an app session.
  std::unique_ptr<AppSession> app_session = std::make_unique<AppSession>();
  KioskSessionPluginHandlerDelegate* delegate = app_session.get();

  // Create a fake power manager client.
  FakePowerManagerClient client;

  // Verified the number of restart calls.
  EXPECT_EQ(client.num_request_restart_calls(), 0);
  delegate->OnPluginCrashed(base::FilePath(kBrowserPluginFilePath));
  EXPECT_EQ(client.num_request_restart_calls(), 1);

  histogram.ExpectBucketCount(kKioskSessionStateHistogram,
                              KioskSessionState::kPluginCrashed, 1);
  EXPECT_EQ(1, histogram.GetAllSamples(kKioskSessionStateHistogram).size());

  histogram.ExpectTotalCount(kKioskSessionCountPerDayHistogram, 0);
}

TEST_F(AppSessionTest, OnPluginHung) {
  base::HistogramTester histogram;
  // Create an app session.
  std::unique_ptr<AppSession> app_session = std::make_unique<AppSession>();
  KioskSessionPluginHandlerDelegate* delegate = app_session.get();

  // Create a fake power manager client.
  FakePowerManagerClient::InitializeFake();

  // Only verify if this method can be called without error.
  delegate->OnPluginHung(std::set<int>());
  histogram.ExpectBucketCount(kKioskSessionStateHistogram,
                              KioskSessionState::kPluginHung, 1);
  EXPECT_EQ(1, histogram.GetAllSamples(kKioskSessionStateHistogram).size());

  histogram.ExpectTotalCount(kKioskSessionCountPerDayHistogram, 0);
}

}  // namespace chromeos
