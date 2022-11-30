// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/background_tracing_field_trial.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"

#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::string kTrialName = "BackgroundWebviewTracing";
const std::string kGroupName = "BackgroundWebviewTracing1";
const char kTestConfigReactiveMode[] = R"(
  {
    "scenario_name": "BackgroundTracing",
    "configs": [
      {
        "rule": "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE",
        "histogram_name": "Android.WebView.LoadUrl.UrlScheme",
        "histogram_lower_value": 0
      }
    ],
    "mode": "REACTIVE_TRACING_MODE"
  }
)";

const char kTestConfigPreemptiveMode[] = R"(
  {
    "scenario_name": "BackgroundTracing",
    "custom_categories": "toplevel",
    "configs": [
      {
        "rule": "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE",
        "histogram_name": "Android.WebView.LoadUrl.UrlScheme",
        "histogram_lower_value": 0
      }
    ],
    "mode": "PREEMPTIVE_TRACING_MODE"
  }
)";

class BackgroundTracingTest : public testing::Test {
 public:
  BackgroundTracingTest() {
    content::SetContentClient(&content_client_);
    content::SetBrowserClientForTesting(&browser_client_);
  }

  void TearDown() override {
    content::BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
    content::SetBrowserClientForTesting(nullptr);
    content::SetContentClient(nullptr);
    base::FieldTrialParamAssociator::GetInstance()->ClearParamsForTesting(
        kTrialName, kGroupName);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::ContentClient content_client_;
  content::ContentBrowserClient browser_client_;
};

}  // namespace

TEST_F(BackgroundTracingTest, ReactiveConfigSystemSetup) {
  base::FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
      kTrialName, kGroupName, {{"config", kTestConfigReactiveMode}});
  base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);

  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());

  android_webview::MaybeSetupSystemTracing();

  // Config (reactive) and method call (system) mismatch, nothing should be set
  // up.
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

TEST_F(BackgroundTracingTest, ReactiveConfigWebViewOnlySetup) {
  base::FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
      kTrialName, kGroupName, {{"config", kTestConfigReactiveMode}});
  base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);

  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());

  android_webview::MaybeSetupWebViewOnlyTracing();

  // Config (reactive) and method call (webview-only) match.
  EXPECT_TRUE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}

TEST_F(BackgroundTracingTest, PreemptiveConfigSystemSetup) {
  base::FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
      kTrialName, kGroupName, {{"config", kTestConfigPreemptiveMode}});
  base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);

  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());

  android_webview::MaybeSetupSystemTracing();

  // Config (preemptive) and method call (system) mismatch, nothing should be
  // set up.
  EXPECT_FALSE(
      content::BackgroundTracingManager::GetInstance().HasActiveScenario());
}
