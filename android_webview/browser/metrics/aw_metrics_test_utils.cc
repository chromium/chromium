// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_test_utils.h"

#include "android_webview/browser/metrics/aw_metrics_service_accessor.h"
#include "base/metrics/user_metrics.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
namespace android_webview {

AwMetricsServiceClientTestDelegate::AwMetricsServiceClientTestDelegate() =
    default;
AwMetricsServiceClientTestDelegate::~AwMetricsServiceClientTestDelegate() =
    default;

void AwMetricsServiceClientTestDelegate::RegisterAdditionalMetricsProviders(
    metrics::MetricsService* service) {}
void AwMetricsServiceClientTestDelegate::AddWebViewAppStateObserver(
    WebViewAppStateObserver* observer) {}
bool AwMetricsServiceClientTestDelegate::HasAwContentsEverCreated() const {
  return false;
}

TestMetricsServiceClient::TestMetricsServiceClient()
    : AwMetricsServiceClient(
          std::make_unique<AwMetricsServiceClientTestDelegate>()) {}
TestMetricsServiceClient::~TestMetricsServiceClient() = default;

AwMetricsTestBase::AwMetricsTestBase() = default;
AwMetricsTestBase::~AwMetricsTestBase() = default;

void AwMetricsTestBase::SetUp() {
  action_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  base::SetRecordActionTaskRunner(action_task_runner_);

  test_content_client_initializer_ =
      new content::TestContentClientInitializer();
  metrics::SubprocessMetricsProvider::CreateInstance();

  prefs_ = std::make_unique<TestingPrefServiceSimple>();
  AwMetricsServiceClient::RegisterMetricsPrefs(prefs_->registry());

  auto client = std::make_unique<TestMetricsServiceClient>();
  client->Initialize(prefs_.get());
  AwMetricsServiceClient::SetInstance(std::move(client));
}

void AwMetricsTestBase::TearDown() {
  task_environment_.RunUntilIdle();
  ClearExternalExperiments();
  AwMetricsServiceClient::ClearInstanceForTesting();
  prefs_.reset();
  delete test_content_client_initializer_;
}

void AwMetricsTestBase::ClearExternalExperiments() {
  AwMetricsServiceAccessor::ClearAllExternalExperimentsForTesting();
}

}  // namespace android_webview
