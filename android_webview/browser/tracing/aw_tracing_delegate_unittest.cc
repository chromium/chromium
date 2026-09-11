// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/aw_tracing_delegate.h"

#include <memory>

#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_feature_list_creator.h"
#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/browser/metrics/aw_metrics_test_utils.h"
#include "base/check_deref.h"
#include "base/values.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/tracing/common/background_tracing_state_manager.h"
#include "components/tracing/common/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class AwTracingDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    AwFeatureListCreator* aw_feature_list_creator = new AwFeatureListCreator();
    aw_feature_list_creator->CreateLocalState();
    std::unique_ptr<AwContentBrowserClient> aw_content_browser_client =
        std::make_unique<AwContentBrowserClient>(aw_feature_list_creator);
    browser_process_ = new AwBrowserProcess(aw_content_browser_client.get());

    auto client = std::make_unique<TestMetricsServiceClient>();
    client->Initialize(browser_process_->local_state());
    AwMetricsServiceClient::SetInstance(std::move(client));

    delegate_ = std::make_unique<android_webview::AwTracingDelegate>(
        CHECK_DEREF(browser_process_->local_state()));
  }

  void TearDown() override {
    AwMetricsServiceClient::ClearInstanceForTesting();
    delete browser_process_;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<android_webview::AwBrowserProcess> browser_process_;
  std::unique_ptr<android_webview::AwTracingDelegate> delegate_;
};

TEST_F(AwTracingDelegateTest, IsRecordingAllowed) {
  EXPECT_TRUE(delegate_->IsRecordingAllowed(
      content::TracingDelegate::IsLocalScenario(true), base::TimeTicks::Now()));

  // Consent is not determined yet (early startup), so recording is allowed.
  EXPECT_TRUE(delegate_->IsRecordingAllowed(
      content::TracingDelegate::IsLocalScenario(false),
      base::TimeTicks::Now()));

  // Consent granted.
  AwMetricsServiceClient::GetInstance()->SetHaveMetricsConsent(
      /*user_consent=*/true, /*app_consent=*/true);
  EXPECT_TRUE(delegate_->IsRecordingAllowed(
      content::TracingDelegate::IsLocalScenario(false),
      base::TimeTicks::Now()));

  // Consent denied.
  AwMetricsServiceClient::GetInstance()->SetHaveMetricsConsent(
      /*user_consent=*/false, /*app_consent=*/false);
  EXPECT_FALSE(delegate_->IsRecordingAllowed(
      content::TracingDelegate::IsLocalScenario(false),
      base::TimeTicks::Now()));
}

}  // namespace android_webview
