// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_BROWSERTEST_UTIL_H_

#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/testing/sync_metrics_test_utils.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_test_helper.h"
#include "components/unified_consent/unified_consent_service.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_sample_collector.h"

namespace content {
class WebContents;
}  // namespace content

class PrivacyBudgetBrowserTestBaseWithTestRecorder
    : public PlatformBrowserTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  PrivacyBudgetBrowserTestBaseWithTestRecorder();
  ~PrivacyBudgetBrowserTestBaseWithTestRecorder() override;
  void SetUpOnMainThread() override;

  // Returns the reported surface keys which are among the expected keys.
  base::flat_set<uint64_t> GetReportedSurfaceKeys(
      std::vector<uint64_t> expected_keys);

  // Returns how many times a surface key was reported.
  int GetSurfaceKeyCount(uint64_t expected_key);

  ukm::TestUkmRecorder& recorder() { return *ukm_recorder_; }

  content::WebContents* web_contents();

  const std::string& FilePathXYZ();

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

// Test class that allows to enable UKM recording.
class PrivacyBudgetBrowserTestBaseWithUkmRecording : public SyncTest {
 public:
  PrivacyBudgetBrowserTestBaseWithUkmRecording();
  ~PrivacyBudgetBrowserTestBaseWithUkmRecording() override;

  static ukm::UkmService* ukm_service();

  static PrefService* local_state();

  content::WebContents* web_contents();

  bool EnableUkmRecording();

  bool DisableUkmRecording();

  void TearDown() override;

 private:
  bool is_metrics_reporting_enabled_ = false;
  std::unique_ptr<SyncServiceImplHarness> sync_test_harness_;
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_BROWSERTEST_UTIL_H_
