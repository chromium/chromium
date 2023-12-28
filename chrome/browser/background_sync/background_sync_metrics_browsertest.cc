// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/background_sync/background_sync_metrics.h"

#include <memory>

#include "chrome/browser/background_sync/background_sync_delegate_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"
#include "url/origin.h"

class BackgroundSyncMetricsBrowserTest : public InProcessBrowserTest {
 public:
  BackgroundSyncMetricsBrowserTest() = default;

  BackgroundSyncMetricsBrowserTest(const BackgroundSyncMetricsBrowserTest&) =
      delete;
  BackgroundSyncMetricsBrowserTest& operator=(
      const BackgroundSyncMetricsBrowserTest&) = delete;

  ~BackgroundSyncMetricsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    Profile* profile = browser()->profile();

    background_sync_delegate_ =
        std::make_unique<BackgroundSyncDelegateImpl>(profile);
    background_sync_metrics_ = std::make_unique<BackgroundSyncMetrics>(
        background_sync_delegate_.get());

    recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    // Adds the URL to the history so that UKM events for this origin are
    // recorded.
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL url(embedded_test_server()->GetURL("/links.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }

 protected:
  void WaitForUkm() {
    base::RunLoop run_loop;
    background_sync_metrics_->ukm_event_recorded_for_testing_ =
        run_loop.QuitClosure();
    run_loop.Run();
  }

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> recorder_;
  std::unique_ptr<BackgroundSyncMetrics> background_sync_metrics_;
  std::unique_ptr<BackgroundSyncDelegateImpl> background_sync_delegate_;
};

IN_PROC_BROWSER_TEST_F(BackgroundSyncMetricsBrowserTest,
                       OneShotBackgroundSyncUkmEventsAreRecorded) {
  background_sync_metrics_->MaybeRecordOneShotSyncRegistrationEvent(
      url::Origin::Create(
          embedded_test_server()->base_url().DeprecatedGetOriginAsURL()),
      /* can_fire= */ true,
      /* is_reregistered= */ false);
  WaitForUkm();

  {
    auto entries = recorder_->GetEntriesByName(
        ukm::builders::BackgroundSyncRegistered::kEntryName);
    ASSERT_EQ(entries.size(), 1u);
    const auto* entry = entries[0].get();
    recorder_->ExpectEntryMetric(
        entry, ukm::builders::BackgroundSyncRegistered::kCanFireName, true);
    recorder_->ExpectEntryMetric(
        entry, ukm::builders::BackgroundSyncRegistered::kIsReregisteredName,
        false);
  }

  background_sync_metrics_->MaybeRecordOneShotSyncCompletionEvent(
      url::Origin::Create(
          embedded_test_server()->base_url().DeprecatedGetOriginAsURL()),
      /* status_code= */ blink::ServiceWorkerStatusCode::kOk,
      /* num_attempts= */ 2, /* max_attempts= */ 5);
  WaitForUkm();

  {
    auto entries = recorder_->GetEntriesByName(
        ukm::builders::BackgroundSyncCompleted::kEntryName);
    ASSERT_EQ(entries.size(), 1u);
    const auto* entry = entries[0].get();
    recorder_->ExpectEntryMetric(
        entry, ukm::builders::BackgroundSyncCompleted::kStatusName,
        static_cast<int64_t>(blink::ServiceWorkerStatusCode::kOk));
    recorder_->ExpectEntryMetric(
        entry, ukm::builders::BackgroundSyncCompleted::kNumAttemptsName, 2);
    recorder_->ExpectEntryMetric(
        entry, ukm::builders::BackgroundSyncCompleted::kMaxAttemptsName, 5);
  }
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncMetricsBrowserTest,
                       PeriodicBackgroundSyncUkmEventsAreRecorded) {
  background_sync_metrics_->MaybeRecordPeriodicSyncRegistrationEvent(
      url::Origin::Create(
          embedded_test_server()->base_url().DeprecatedGetOriginAsURL()),
      /* min_interval= */ 1000,
      /* is_reregistered= */ false);
  WaitForUkm();

  {
    auto entries = recorder_->GetEntriesByName(
        ukm::builders::PeriodicBackgroundSyncRegistered::kEntryName);
    ASSERT_EQ(entries.size(), 1u);
    const auto* entry = entries[0].get();
    recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::PeriodicBackgroundSyncRegistered::kMinIntervalMsName,
        ukm::GetExponentialBucketMin(1000, kUkmEventDataBucketSpacing));
    recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::PeriodicBackgroundSyncRegistered::kIsReregisteredName,
        false);
  }

  background_sync_metrics_->MaybeRecordPeriodicSyncEventCompletion(
      url::Origin::Create(
          embedded_test_server()->base_url().DeprecatedGetOriginAsURL()),
      /* status_code= */ blink::ServiceWorkerStatusCode::kOk,
      /* num_attempts= */ 2, /* max_attempts= */ 5);
  WaitForUkm();

  {
    auto entries = recorder_->GetEntriesByName(
        ukm::builders::PeriodicBackgroundSyncEventCompleted::kEntryName);
    ASSERT_EQ(entries.size(), 1u);
    const auto* entry = entries[0].get();
    recorder_->ExpectEntryMetric(
        entry, ukm::builders::PeriodicBackgroundSyncEventCompleted::kStatusName,
        static_cast<int64_t>(blink::ServiceWorkerStatusCode::kOk));
    recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::PeriodicBackgroundSyncEventCompleted::kNumAttemptsName,
        2);
    recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::PeriodicBackgroundSyncEventCompleted::kMaxAttemptsName,
        5);
  }
}
