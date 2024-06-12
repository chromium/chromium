// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/aw_background_tracing_metrics_provider.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/background_tracing_test_support.h"
#include "content/public/test/browser_task_environment.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/trace_log.pb.h"

namespace tracing {
namespace {

const char kPlaceholderTrace[] = "Trace bytes as serialized proto";

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

class AwBackgroundTracingMetricsProviderTest : public testing::Test {
 public:
  AwBackgroundTracingMetricsProviderTest() {
    content::SetContentClient(&content_client_);
    content::SetBrowserClientForTesting(&browser_client_);
    background_tracing_manager_ =
        content::BackgroundTracingManager::CreateInstance();
  }

  ~AwBackgroundTracingMetricsProviderTest() override {
    content::SetBrowserClientForTesting(nullptr);
    content::SetContentClient(nullptr);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::ContentClient content_client_;
  content::ContentBrowserClient browser_client_;
  std::unique_ptr<content::BackgroundTracingManager>
      background_tracing_manager_;
};

TEST_F(AwBackgroundTracingMetricsProviderTest, NoTraceData) {
  AwBackgroundTracingMetricsProvider provider;
  ASSERT_FALSE(provider.HasIndependentMetrics());
}

TEST_F(AwBackgroundTracingMetricsProviderTest, UploadsTraceLog) {
  TestBackgroundTracingHelper background_tracing_helper;
  AwBackgroundTracingMetricsProvider provider;
  EXPECT_FALSE(provider.HasIndependentMetrics());

  content::BackgroundTracingManager::GetInstance().SaveTraceForTesting(
      kPlaceholderTrace, "test_scenario", "test_rule",
      base::Token::CreateRandom());
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
  EXPECT_NE(kPlaceholderTrace, uma_proto.trace_log(0).raw_data());

  EXPECT_FALSE(provider.HasIndependentMetrics());
}

TEST_F(AwBackgroundTracingMetricsProviderTest, HandlesOversizeTraceLog) {
  TestBackgroundTracingHelper background_tracing_helper;
  AwBackgroundTracingMetricsProvider provider;
  EXPECT_FALSE(provider.HasIndependentMetrics());

  std::string trace;
  constexpr int size = kCompressedUploadLimitBytes * 5;
  trace.resize(size);

  // Writing a random string to the trace makes it less likely to compress well
  // and fit into the upload limit.
  for (int i = 0; i < size; i++) {
    trace[i] = base::RandInt('a', 'z');
  }

  content::BackgroundTracingManager::GetInstance().SaveTraceForTesting(
      std::move(trace), "test_scenario", "test_rule",
      base::Token::CreateRandom());
  background_tracing_helper.WaitForTraceSaved();

  EXPECT_TRUE(provider.HasIndependentMetrics());
  metrics::ChromeUserMetricsExtension uma_proto;
  uma_proto.set_client_id(100);
  uma_proto.set_session_id(15);

  base::RunLoop run_loop;
  provider.ProvideIndependentMetrics(
      base::DoNothing(), base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_FALSE(success);
        run_loop.Quit();
      }),
      &uma_proto,
      /* snapshot_manager=*/nullptr);
  run_loop.Run();

  EXPECT_EQ(100u, uma_proto.client_id());
  EXPECT_EQ(15, uma_proto.session_id());
  EXPECT_EQ(0, uma_proto.trace_log_size());
  EXPECT_FALSE(provider.HasIndependentMetrics());
}

TEST_F(AwBackgroundTracingMetricsProviderTest, ClearsAppPackageName) {
  TestBackgroundTracingHelper background_tracing_helper;
  AwBackgroundTracingMetricsProvider provider;
  EXPECT_FALSE(provider.HasIndependentMetrics());

  content::BackgroundTracingManager::GetInstance().SaveTraceForTesting(
      kPlaceholderTrace, "test_scenario", "test_rule",
      base::Token::CreateRandom());
  background_tracing_helper.WaitForTraceSaved();

  EXPECT_TRUE(provider.HasIndependentMetrics());
  metrics::ChromeUserMetricsExtension uma_proto;

  uma_proto.mutable_system_profile()->set_app_package_name("my_app");
  base::RunLoop run_loop;
  provider.ProvideIndependentMetrics(
      base::DoNothing(), base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }),
      &uma_proto,
      /* snapshot_manager=*/nullptr);
  run_loop.Run();

  EXPECT_TRUE(uma_proto.system_profile().app_package_name().empty());
  ASSERT_EQ(1, uma_proto.trace_log_size());
  EXPECT_EQ(metrics::TraceLog::COMPRESSION_TYPE_ZLIB,
            uma_proto.trace_log(0).compression_type());
  EXPECT_NE(kPlaceholderTrace, uma_proto.trace_log(0).raw_data());

  EXPECT_FALSE(provider.HasIndependentMetrics());
}

TEST_F(AwBackgroundTracingMetricsProviderTest, HandlesMissingTrace) {
  AwBackgroundTracingMetricsProvider provider;
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

}  // namespace tracing