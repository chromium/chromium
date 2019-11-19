// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/background_tracing_metrics_provider.h"

#include "base/bind.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/trace_log.pb.h"

namespace tracing {
namespace {
const char kDummyTrace[] = "Trace bytes as serialized proto";
}  // namespace

class BackgroundTracingMetricsProviderTest : public testing::Test {
 public:
  BackgroundTracingMetricsProviderTest() = default;

  void SetUp() override {
    base::DictionaryValue dict;

    dict.SetString("mode", "REACTIVE_TRACING_MODE");
    dict.SetString("category", "BENCHMARK");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
      rules_dict->SetString("trigger_name", "test");
      rules_list->Append(std::move(rules_dict));
    }
    dict.Set("configs", std::move(rules_list));

    std::unique_ptr<content::BackgroundTracingConfig> config(
        content::BackgroundTracingConfig::FromDict(&dict));
    ASSERT_TRUE(config);

    ASSERT_TRUE(
        content::BackgroundTracingManager::GetInstance()->SetActiveScenario(
            std::move(config),
            base::BindRepeating([](std::unique_ptr<std::string>,
                                   content::BackgroundTracingManager::
                                       FinishedProcessingCallback) {}),
            content::BackgroundTracingManager::ANONYMIZE_DATA));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BackgroundTracingMetricsProviderTest, NoTraceData) {
  BackgroundTracingMetricsProvider provider;
  ASSERT_FALSE(provider.HasIndependentMetrics());
}

TEST_F(BackgroundTracingMetricsProviderTest, UploadsTraceLog) {
  BackgroundTracingMetricsProvider provider;
  EXPECT_FALSE(provider.HasIndependentMetrics());

  content::BackgroundTracingManager::GetInstance()->SetTraceToUploadForTesting(
      std::make_unique<std::string>(kDummyTrace));

  EXPECT_TRUE(provider.HasIndependentMetrics());
  metrics::ChromeUserMetricsExtension uma_proto;
  uma_proto.set_client_id(100);
  uma_proto.set_session_id(15);
  provider.ProvideIndependentMetrics(
      base::BindOnce([](bool success) { EXPECT_TRUE(success); }), &uma_proto,
      /* snapshot_manager=*/nullptr);

  EXPECT_EQ(100u, uma_proto.client_id());
  EXPECT_EQ(15, uma_proto.session_id());
  ASSERT_EQ(1, uma_proto.trace_log_size());
  EXPECT_EQ(kDummyTrace, uma_proto.trace_log(0).raw_data());

  EXPECT_FALSE(provider.HasIndependentMetrics());
}

TEST_F(BackgroundTracingMetricsProviderTest, HandleMissingTrace) {
  BackgroundTracingMetricsProvider provider;
  EXPECT_FALSE(provider.HasIndependentMetrics());

  content::BackgroundTracingManager::GetInstance()->SetTraceToUploadForTesting(
      std::make_unique<std::string>(kDummyTrace));
  EXPECT_TRUE(provider.HasIndependentMetrics());

  content::BackgroundTracingManager::GetInstance()->SetTraceToUploadForTesting(
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
