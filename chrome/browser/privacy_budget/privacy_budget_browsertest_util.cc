// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_browsertest_util.h"
#include "base/memory/raw_ptr.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

PrivacyBudgetBrowserTestBaseWithTestRecorder::
    PrivacyBudgetBrowserTestBaseWithTestRecorder() = default;
PrivacyBudgetBrowserTestBaseWithTestRecorder::
    ~PrivacyBudgetBrowserTestBaseWithTestRecorder() = default;

void PrivacyBudgetBrowserTestBaseWithTestRecorder::SetUpOnMainThread() {
  PlatformBrowserTest::SetUpOnMainThread();
  // Do an initial empty navigation then create the recorder to make sure we
  // start on a clean slate. This clears the platform differences in between
  // Android and Desktop.
  content::NavigateToURLBlockUntilNavigationsComplete(web_contents(),
                                                      GURL("about:blank"), 1);

  // Ensure that the actively sampled surfaces reported at browser startup go
  // through before we set up the test recorder.
  content::RunAllTasksUntilIdle();
  ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
}

base::flat_set<uint64_t>
PrivacyBudgetBrowserTestBaseWithTestRecorder::GetReportedSurfaceKeys(
    std::vector<uint64_t> expected_keys) {
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder_->GetMergedEntriesByName(
          ukm::builders::Identifiability::kEntryName);

  base::flat_set<uint64_t> reported_surface_keys;
  for (const auto& entry : merged_entries) {
    for (const auto& metric : entry.second->metrics) {
      if (base::Contains(expected_keys, metric.first))
        reported_surface_keys.insert(metric.first);
    }
  }
  return reported_surface_keys;
}

int PrivacyBudgetBrowserTestBaseWithTestRecorder::GetSurfaceKeyCount(
    uint64_t expected_key) {
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> entries =
      ukm_recorder_->GetEntriesByName(
          ukm::builders::Identifiability::kEntryName);

  int count = 0;
  for (const ukm::mojom::UkmEntry* entry : entries) {
    for (const auto& metric : entry->metrics) {
      if (expected_key == metric.first)
        count++;
    }
  }
  return count;
}

content::WebContents*
PrivacyBudgetBrowserTestBaseWithTestRecorder::web_contents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

const std::string& PrivacyBudgetBrowserTestBaseWithTestRecorder::FilePathXYZ() {
  return GetParam();
}

PrivacyBudgetBrowserTestBaseWithUkmRecording::
    PrivacyBudgetBrowserTestBaseWithUkmRecording()
    : SyncTest(SINGLE_CLIENT) {}

PrivacyBudgetBrowserTestBaseWithUkmRecording::
    ~PrivacyBudgetBrowserTestBaseWithUkmRecording() = default;

ukm::UkmService* PrivacyBudgetBrowserTestBaseWithUkmRecording::ukm_service() {
  return g_browser_process->GetMetricsServicesManager()->GetUkmService();
}

PrefService* PrivacyBudgetBrowserTestBaseWithUkmRecording::local_state() {
  return g_browser_process->local_state();
}

bool PrivacyBudgetBrowserTestBaseWithUkmRecording::EnableUkmRecording() {
  // 1. Enable sync.
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  sync_test_harness_ = metrics::test::InitializeProfileForSync(
      profile, GetFakeServer()->AsWeakPtr());
  EXPECT_TRUE(sync_test_harness_->SetupSync());

  // 2. Signal consent for UKM reporting.
  unified_consent::UnifiedConsentService* consent_service =
      UnifiedConsentServiceFactory::GetForProfile(profile);
  if (consent_service != nullptr)
    consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);

  // 3. Enable metrics reporting.
  is_metrics_reporting_enabled_ = true;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &is_metrics_reporting_enabled_);

  // UpdateUploadPermissions causes the MetricsServicesManager to look at the
  // consent signals and re-evaluate whether reporting should be enabled.
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);

  // The following sequence synchronously completes UkmService initialization
  // (if it wasn't initialized yet) and flushes any accumulated metrics.
  ukm::UkmTestHelper ukm_test_helper(ukm_service());
  ukm_test_helper.BuildAndStoreLog();
  std::unique_ptr<ukm::Report> report_to_discard =
      ukm_test_helper.GetUkmReport();

  ukm_service()->SetSamplingForTesting(1);
  return ukm::UkmTestHelper(ukm_service()).IsRecordingEnabled();
}

bool PrivacyBudgetBrowserTestBaseWithUkmRecording::DisableUkmRecording() {
  EXPECT_TRUE(is_metrics_reporting_enabled_)
      << "DisableUkmRecording() should only be called after "
         "EnableUkmRecording()";
  is_metrics_reporting_enabled_ = false;
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
  return !ukm::UkmTestHelper(ukm_service()).IsRecordingEnabled();
}

content::WebContents*
PrivacyBudgetBrowserTestBaseWithUkmRecording::web_contents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

void PrivacyBudgetBrowserTestBaseWithUkmRecording::TearDown() {
  if (is_metrics_reporting_enabled_) {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        nullptr);
  }
}
