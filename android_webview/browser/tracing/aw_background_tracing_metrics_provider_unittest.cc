// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/aw_background_tracing_metrics_provider.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/tracing/common/trace_startup_config.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/trace_log.pb.h"

namespace tracing {
namespace {
const char kDummyTrace[] = "Trace bytes as serialized proto";
}  // namespace

class AwBackgroundTracingMetricsProviderTest : public testing::Test {
 public:
  AwBackgroundTracingMetricsProviderTest() {
    content::SetContentClient(&content_client_);
    content::SetBrowserClientForTesting(&browser_client_);
  }

  ~AwBackgroundTracingMetricsProviderTest() override {
    content::SetBrowserClientForTesting(nullptr);
    content::SetContentClient(nullptr);
  }

  void SetUp() override {
    base::Value::Dict dict;

    dict.Set("mode", "REACTIVE_TRACING_MODE");
    dict.Set("custom_categories",
             tracing::TraceStartupConfig::kDefaultStartupCategories);

    base::Value::List rules_list;
    {
      base::Value::Dict rules_dict;
      rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
      rules_dict.Set("trigger_name", "test");
      rules_list.Append(std::move(rules_dict));
    }
    dict.Set("configs", std::move(rules_list));

    std::unique_ptr<content::BackgroundTracingConfig> config(
        content::BackgroundTracingConfig::FromDict(std::move(dict)));
    ASSERT_TRUE(config);

    ASSERT_TRUE(
        content::BackgroundTracingManager::GetInstance().SetActiveScenario(
            std::move(config),
            content::BackgroundTracingManager::ANONYMIZE_DATA));
  }

  void TearDown() override {
    content::BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::ContentClient content_client_;
  content::ContentBrowserClient browser_client_;
};

TEST_F(AwBackgroundTracingMetricsProviderTest, NoTraceData) {
  AwBackgroundTracingMetricsProvider provider;
  ASSERT_FALSE(provider.HasIndependentMetrics());
}

TEST_F(AwBackgroundTracingMetricsProviderTest, UploadsTraceLog) {
  AwBackgroundTracingMetricsProvider provider;
  EXPECT_FALSE(provider.HasIndependentMetrics());

  content::BackgroundTracingManager::GetInstance().SetTraceToUploadForTesting(
      std::make_unique<std::string>(kDummyTrace));

  EXPECT_TRUE(provider.HasIndependentMetrics());
  metrics::ChromeUserMetricsExtension uma_proto;
  uma_proto.set_client_id(100);
  uma_proto.set_session_id(15);

  base::RunLoop run_loop;
  provider.ProvideIndependentMetrics(
      base::BindLambdaForTesting([&run_loop](bool success) {
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
  EXPECT_NE(kDummyTrace, uma_proto.trace_log(0).raw_data());

  EXPECT_FALSE(provider.HasIndependentMetrics());
}

TEST_F(AwBackgroundTracingMetricsProviderTest, HandlesOversizeTraceLog) {
  AwBackgroundTracingMetricsProvider provider;
  EXPECT_FALSE(provider.HasIndependentMetrics());

  auto trace = std::make_unique<std::string>();
  constexpr int size = kCompressedUploadLimitBytes * 5;
  trace->resize(size);

  // Writing a random string to the trace makes it less likely to compress well
  // and fit into the upload limit.
  for (int i = 0; i < size; i++) {
    (*trace)[i] = base::RandInt('a', 'z');
  }

  content::BackgroundTracingManager::GetInstance().SetTraceToUploadForTesting(
      std::move(trace));

  EXPECT_TRUE(provider.HasIndependentMetrics());
  metrics::ChromeUserMetricsExtension uma_proto;
  uma_proto.set_client_id(100);
  uma_proto.set_session_id(15);

  base::RunLoop run_loop;
  provider.ProvideIndependentMetrics(
      base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_FALSE(success);
        run_loop.Quit();
      }),
      &uma_proto,
      /* snapshot_manager=*/nullptr);
  run_loop.Run();

  EXPECT_EQ(100u, uma_proto.client_id());
  EXPECT_EQ(15, uma_proto.session_id());
  EXPECT_EQ(1, uma_proto.trace_log_size());
  EXPECT_TRUE(uma_proto.trace_log(0).raw_data().empty());
  EXPECT_FALSE(provider.HasIndependentMetrics());
}

TEST_F(AwBackgroundTracingMetricsProviderTest, ClearsAppPackageName) {
  AwBackgroundTracingMetricsProvider provider;
  EXPECT_FALSE(provider.HasIndependentMetrics());

  content::BackgroundTracingManager::GetInstance().SetTraceToUploadForTesting(
      std::make_unique<std::string>(kDummyTrace));

  EXPECT_TRUE(provider.HasIndependentMetrics());
  metrics::ChromeUserMetricsExtension uma_proto;

  uma_proto.mutable_system_profile()->set_app_package_name("my_app");
  base::RunLoop run_loop;
  provider.ProvideIndependentMetrics(
      base::BindLambdaForTesting([&run_loop](bool success) {
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
  EXPECT_NE(kDummyTrace, uma_proto.trace_log(0).raw_data());

  EXPECT_FALSE(provider.HasIndependentMetrics());
}

TEST_F(AwBackgroundTracingMetricsProviderTest, HandlesMissingTrace) {
  AwBackgroundTracingMetricsProvider provider;
  EXPECT_FALSE(provider.HasIndependentMetrics());

  content::BackgroundTracingManager::GetInstance().SetTraceToUploadForTesting(
      std::make_unique<std::string>(kDummyTrace));
  EXPECT_TRUE(provider.HasIndependentMetrics());

  content::BackgroundTracingManager::GetInstance().SetTraceToUploadForTesting(
      nullptr);
  metrics::ChromeUserMetricsExtension uma_proto;
  uma_proto.set_client_id(100);
  uma_proto.set_session_id(15);
  provider.ProvideIndependentMetrics(
      base::BindOnce([](bool success) { EXPECT_FALSE(success); }), &uma_proto,
      /* snapshot_manager=*/nullptr);

  EXPECT_EQ(100u, uma_proto.client_id());
  EXPECT_EQ(15, uma_proto.session_id());
  EXPECT_EQ(0, uma_proto.trace_log_size());
  EXPECT_FALSE(provider.HasIndependentMetrics());
}

}  // namespace tracing