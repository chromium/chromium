// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/chrome_background_tracing_metrics_provider.h"

#include <utility>

#include "base/bind.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/tracing/common/trace_startup_config.h"
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

class ChromeBackgroundTracingMetricsProviderTest : public testing::Test {
 public:
  ChromeBackgroundTracingMetricsProviderTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    base::Value dict(base::Value::Type::DICTIONARY);

    dict.SetStringKey("mode", "REACTIVE_TRACING_MODE");
    dict.SetStringKey("custom_categories",
                      tracing::TraceStartupConfig::kDefaultStartupCategories);

    base::Value rules_list(base::Value::Type::LIST);
    {
      base::Value rules_dict(base::Value::Type::DICTIONARY);
      rules_dict.SetStringKey("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
      rules_dict.SetStringKey("trigger_name", "test");
      rules_list.Append(std::move(rules_dict));
    }
    dict.SetKey("configs", std::move(rules_list));

    std::unique_ptr<content::BackgroundTracingConfig> config(
        content::BackgroundTracingConfig::FromDict(std::move(dict)));
    ASSERT_TRUE(config);

    ASSERT_TRUE(
        content::BackgroundTracingManager::GetInstance()->SetActiveScenario(
            std::move(config),
            content::BackgroundTracingManager::ANONYMIZE_DATA));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;
};

TEST_F(ChromeBackgroundTracingMetricsProviderTest, NoTraceData) {
  ChromeBackgroundTracingMetricsProvider provider;
  ASSERT_FALSE(provider.HasIndependentMetrics());
}

TEST_F(ChromeBackgroundTracingMetricsProviderTest, UploadsTraceLog) {
  ChromeBackgroundTracingMetricsProvider provider;
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

TEST_F(ChromeBackgroundTracingMetricsProviderTest, HandleMissingTrace) {
  ChromeBackgroundTracingMetricsProvider provider;
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
