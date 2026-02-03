// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_TEST_UTILS_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_TEST_UTILS_H_

#include <memory>

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/test_simple_task_runner.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_content_client_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

// A dummy delegate for testing AwMetricsServiceClient. It stubs out methods
// that interact with the broader Android system or WebView application state,
// allowing tests to focus on the metrics service logic itself.
class AwMetricsServiceClientTestDelegate
    : public AwMetricsServiceClient::Delegate {
 public:
  AwMetricsServiceClientTestDelegate();
  ~AwMetricsServiceClientTestDelegate() override;

  // AwMetricsServiceClient::Delegate:
  void RegisterAdditionalMetricsProviders(
      metrics::MetricsService* service) override;
  void AddWebViewAppStateObserver(WebViewAppStateObserver* observer) override;
  bool HasAwContentsEverCreated() const override;
};

// A concrete implementation of AwMetricsServiceClient for testing purposes.
// It initializes with a test delegate and allows tests to instantiate a
// client without needing the full WebView environment.
class TestMetricsServiceClient : public AwMetricsServiceClient {
 public:
  TestMetricsServiceClient();
  ~TestMetricsServiceClient() override;
};

// Base class for tests that require a fully initialized AwMetricsServiceClient
// singleton. Handles task runner registration, prefs, and singleton lifecycle.
class AwMetricsTestBase : public testing::Test {
 public:
  AwMetricsTestBase();
  ~AwMetricsTestBase() override;

 protected:
  void SetUp() override;
  void TearDown() override;

  void ClearExternalExperiments();

  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> action_task_runner_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  raw_ptr<content::TestContentClientInitializer>
      test_content_client_initializer_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_TEST_UTILS_H_
