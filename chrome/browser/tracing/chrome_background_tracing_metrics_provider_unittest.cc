// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/chrome_background_tracing_metrics_provider.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/test/background_tracing_test_support.h"
#include "content/public/test/browser_task_environment.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/trace_log.pb.h"
#include "third_party/zlib/google/compression_utils.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
// "nogncheck" because of crbug.com/1125897.
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/metrics/chromeos_system_profile_provider.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace tracing {
namespace {

const char kDummyTrace[] = "Trace bytes as serialized proto";

class TestBackgroundTracingHelper
    : public content::BackgroundTracingManager::EnabledStateTestObserver {
 public:
  TestBackgroundTracingHelper() {
    content::AddBackgroundTracingEnabledStateObserverForTesting(this);
  }

  ~TestBackgroundTracingHelper() {
    content::RemoveBackgroundTracingEnabledStateObserverForTesting(this);
  }

  void OnTraceSaved() override { wait_for_trace_saved_.Quit(); }

  void WaitForTraceSaved() { wait_for_trace_saved_.Run(); }

 private:
  base::RunLoop wait_for_trace_saved_;
};

}  // namespace

class ChromeBackgroundTracingMetricsProviderTest : public testing::Test {
 public:
  ChromeBackgroundTracingMetricsProviderTest()
      : background_tracing_manager_(
            content::BackgroundTracingManager::CreateInstance()),
        local_state_(TestingBrowserProcess::GetGlobal()) {}

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::BackgroundTracingManager>
      background_tracing_manager_;
  ScopedTestingLocalState local_state_;
};

TEST_F(ChromeBackgroundTracingMetricsProviderTest, NoTraceData) {
  ChromeBackgroundTracingMetricsProvider provider(nullptr);
  ASSERT_FALSE(provider.HasIndependentMetrics());
}

TEST_F(ChromeBackgroundTracingMetricsProviderTest, UploadsTraceLog) {
  TestBackgroundTracingHelper background_tracing_helper;
  ChromeBackgroundTracingMetricsProvider provider(nullptr);
  EXPECT_FALSE(provider.HasIndependentMetrics());

  content::BackgroundTracingManager::GetInstance().SaveTraceForTesting(
      kDummyTrace, "test_scenario", "test_rule", base::Token::CreateRandom());
  background_tracing_helper.WaitForTraceSaved();

  EXPECT_TRUE(provider.HasIndependentMetrics());
  metrics::ChromeUserMetricsExtension uma_proto;
  uma_proto.set_client_id(100);
  uma_proto.set_session_id(15);

  base::RunLoop run_loop;
  provider.ProvideIndependentMetrics(
      base::DoNothing(), base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }),
      &uma_proto,
      /* snapshot_manager=*/nullptr);
  run_loop.Run();

  EXPECT_EQ(100u, uma_proto.client_id());
  EXPECT_EQ(15, uma_proto.session_id());
  ASSERT_EQ(1, uma_proto.trace_log_size());
  EXPECT_EQ(metrics::TraceLog::COMPRESSION_TYPE_ZLIB,
            uma_proto.trace_log(0).compression_type());
  std::string serialize_trace;
  ASSERT_TRUE(compression::GzipUncompress(uma_proto.trace_log(0).raw_data(),
                                          &serialize_trace));
  EXPECT_EQ(kDummyTrace, serialize_trace);

  EXPECT_FALSE(provider.HasIndependentMetrics());
}

TEST_F(ChromeBackgroundTracingMetricsProviderTest, HandleMissingTrace) {
  ChromeBackgroundTracingMetricsProvider provider(nullptr);
  EXPECT_FALSE(provider.HasIndependentMetrics());

  metrics::ChromeUserMetricsExtension uma_proto;
  uma_proto.set_client_id(100);
  uma_proto.set_session_id(15);
  provider.ProvideIndependentMetrics(
      base::DoNothing(),
      base::BindOnce([](bool success) { EXPECT_FALSE(success); }), &uma_proto,
      /* snapshot_manager=*/nullptr);

  EXPECT_EQ(100u, uma_proto.client_id());
  EXPECT_EQ(15, uma_proto.session_id());
  EXPECT_EQ(0, uma_proto.trace_log_size());
  EXPECT_FALSE(provider.HasIndependentMetrics());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ChromeBackgroundTracingMetricsProviderChromeOSTest
    : public ChromeBackgroundTracingMetricsProviderTest {
 public:
  // ChromeBackgroundTracingMetricsProviderTest:
  void SetUp() override {
    ChromeBackgroundTracingMetricsProviderTest::SetUp();

    // ChromeOSSystemProfileProvider needs the following to provide system
    // profile meta.
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::TpmManagerClient::InitializeFake();
    ash::DemoSession::SetDemoConfigForTesting(
        ash::DemoSession::DemoModeConfig::kNone);
    ash::LoginState::Initialize();
  }

  void TearDown() override {
    ChromeBackgroundTracingMetricsProviderTest::TearDown();

    ash::LoginState::Shutdown();
    ash::DemoSession::ResetDemoConfigForTesting();
    chromeos::TpmManagerClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }
};

TEST_F(ChromeBackgroundTracingMetricsProviderChromeOSTest, HardwareClass) {
  // Set a fake hardware class.
  constexpr char kFakeHardwareClass[] = "Fake hardware class";
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider;
  fake_statistics_provider.SetMachineStatistic("hardware_class",
                                               kFakeHardwareClass);

  auto system_profile_provider =
      std::make_unique<ChromeOSSystemProfileProvider>();
  ChromeBackgroundTracingMetricsProvider provider(
      system_profile_provider.get());
  provider.Init();

  // AsyncInit needs to happen to collect `hardware_class` etc.
  {
    base::RunLoop run_loop;
    base::RepeatingClosure barrier =
        base::BarrierClosure(2, run_loop.QuitWhenIdleClosure());
    provider.AsyncInit(barrier);
    system_profile_provider->AsyncInit(barrier);
    run_loop.Run();
  }

  TestBackgroundTracingHelper background_tracing_helper;
  // Fake a UMA collection for background tracing.
  content::BackgroundTracingManager::GetInstance().SaveTraceForTesting(
      kDummyTrace, "test_scenario", "test_rule", base::Token::CreateRandom());
  background_tracing_helper.WaitForTraceSaved();
  ASSERT_TRUE(provider.HasIndependentMetrics());

  metrics::ChromeUserMetricsExtension uma_proto;
  {
    base::RunLoop run_loop;
    provider.ProvideIndependentMetrics(
        base::DoNothing(), base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }),
        &uma_proto,
        /* snapshot_manager=*/nullptr);
    run_loop.Run();
  }

  // Verify `hardware_class` is collected correctly.
  ASSERT_EQ(1, uma_proto.trace_log_size());
  const metrics::SystemProfileProto& system_profile =
      uma_proto.system_profile();
  const metrics::SystemProfileProto::Hardware& hardware =
      system_profile.hardware();
  EXPECT_EQ(kFakeHardwareClass, hardware.full_hardware_class());

  EXPECT_FALSE(provider.HasIndependentMetrics());
}
#endif

}  // namespace tracing
