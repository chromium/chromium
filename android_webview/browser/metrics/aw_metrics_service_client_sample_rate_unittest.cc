// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "base/metrics/user_metrics.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_switches.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_content_client_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

using InstallerPackageType = AwMetricsServiceClient::InstallerPackageType;

namespace {
class AwMetricsServiceClientSampleRateTestDelegate
    : public AwMetricsServiceClient::Delegate {
  void RegisterAdditionalMetricsProviders(
      metrics::MetricsService* service) override {}
  void AddWebViewAppStateObserver(WebViewAppStateObserver* observer) override {}
  bool HasAwContentsEverCreated() const override { return false; }
};

class AwMetricsServiceTestClientForSampling : public AwMetricsServiceClient {
 public:
  explicit AwMetricsServiceTestClientForSampling(
      std::unique_ptr<Delegate> delegate)
      : AwMetricsServiceClient(std::move(delegate)) {}
  void SetUnfilteredSampleRatePerMille(int unfiltered_sample_rate_per_mille) {
    _unfiltered_sample_rate_per_mille = unfiltered_sample_rate_per_mille;
  }

  int GetUnfilteredSampleRatePerMille() const override {
    return _unfiltered_sample_rate_per_mille;
  }

 private:
  int _unfiltered_sample_rate_per_mille = 0;
};

class AwMetricsServiceClientSampleRateTest : public testing::Test {
 protected:
  AwMetricsServiceClientSampleRateTest()
      : task_runner_(new base::TestSimpleTaskRunner) {
    base::SetRecordActionTaskRunner(task_runner_);
    // Needed because RegisterMetricsProvidersAndInitState() checks for this.
    metrics::SubprocessMetricsProvider::CreateInstance();
  }

 private:
  // Needed since starting metrics reporting triggers code that uses content::
  // objects.
  content::TestContentClientInitializer test_content_initializer_;
  // Needed since starting metrics reporting triggers code that expects to be
  // running on the browser UI thread. Also needed for its FastForwardBy method.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
};

}  // namespace

TEST_F(AwMetricsServiceClientSampleRateTest, TestShouldSampleByClientUUID) {
  struct {
    const char* client_uuid;
    int sampling_rate_per_mille;
    bool expected_filtering;
  } test_cases[] = {
      {"a7a68d68-8ba3-486d-832b-a0cded65fea2", 990 /*CANARY*/, true},
      {"fa5f5bd4-aae7-4d94-ab84-69c8ca40f400", 990 /*CANARY*/, false},
      {"fa5f5bd4-aae7-4d94-ab84-69c8ca40f400", 20 /*STABLE*/, true},
      {"747dbd7a-7b48-4496-8043-88edf84e0ab3", 20 /*STABLE*/, false}};

  for (const auto& test : test_cases) {
    auto prefs = std::make_unique<TestingPrefServiceSimple>();
    AwMetricsServiceTestClientForSampling::RegisterMetricsPrefs(
        prefs->registry());
    prefs->SetString(metrics::prefs::kMetricsClientID, test.client_uuid);
    auto client = std::make_unique<AwMetricsServiceTestClientForSampling>(
        std::make_unique<AwMetricsServiceClientSampleRateTestDelegate>());
    client->SetHaveMetricsConsent(/*user_consent=*/true, /*app_consent=*/true);
    client->Initialize(prefs.get());
    client->SetUpMetricsDir();
    client->SetUnfilteredSampleRatePerMille(test.sampling_rate_per_mille);

    EXPECT_EQ(client->ShouldApplyMetricsFiltering(), test.expected_filtering);
  }
}

}  // namespace android_webview
