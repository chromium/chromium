// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/page_capping_page_load_metrics_observer.h"

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/data_use_measurement/page_load_capping/chrome_page_load_capping_features.h"
#include "chrome/browser/data_use_measurement/page_load_capping/page_load_capping_blacklist.h"
#include "chrome/browser/data_use_measurement/page_load_capping/page_load_capping_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/infobars/mock_infobar_service.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_data.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_delegate.h"
#include "components/blacklist/opt_out_blacklist/opt_out_store.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {

const char kTestURL[] = "http://www.test.com";

// Test class that has default blacklisting behavior with no backing store.
class TestPageLoadCappingBlacklist : public PageLoadCappingBlacklist {
 public:
  explicit TestPageLoadCappingBlacklist(
      blacklist::OptOutBlacklistDelegate* delegate)
      : PageLoadCappingBlacklist(nullptr,
                                 base::DefaultClock::GetInstance(),
                                 delegate) {}
  ~TestPageLoadCappingBlacklist() override {}
};

}  // namespace

// Class that overrides WriteToSavings.
class TestPageCappingPageLoadMetricsObserver
    : public PageCappingPageLoadMetricsObserver {
 public:
  using SizeUpdateCallback = base::RepeatingCallback<void(int64_t)>;
  TestPageCappingPageLoadMetricsObserver(
      int64_t fuzzing_offset,
      PageLoadCappingBlacklist* blacklist,
      std::unique_ptr<base::SimpleTestTickClock> simple_test_tick_clock,
      const SizeUpdateCallback& callback)
      : fuzzing_offset_(fuzzing_offset),
        blacklist_(blacklist),
        simple_test_tick_clock_(std::move(simple_test_tick_clock)),
        size_callback_(callback) {
    SetTickClockForTesting(simple_test_tick_clock_.get());
  }
  ~TestPageCappingPageLoadMetricsObserver() override {}

  void WriteToSavings(int64_t bytes_saved) override {
    size_callback_.Run(bytes_saved);
  }

  int64_t GetFuzzingOffset() const override { return fuzzing_offset_; }

  PageLoadCappingBlacklist* GetPageLoadCappingBlacklist() const override {
    return blacklist_;
  }

 private:
  int64_t fuzzing_offset_;
  PageLoadCappingBlacklist* blacklist_;
  std::unique_ptr<base::SimpleTestTickClock> simple_test_tick_clock_;
  SizeUpdateCallback size_callback_;
};

class PageCappingObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness,
      public blacklist::OptOutBlacklistDelegate {
 public:
  PageCappingObserverTest()
      : test_blacklist_(std::make_unique<TestPageLoadCappingBlacklist>(this)) {}
  ~PageCappingObserverTest() override = default;

  void SetUpTest(bool enabled, std::map<std::string, std::string> params) {
    if (enabled) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          data_use_measurement::page_load_capping::features::
              kDetectingHeavyPages,
          params);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          data_use_measurement::page_load_capping::features::
              kDetectingHeavyPages);
    }

    MockInfoBarService::CreateForWebContents(web_contents());
    NavigateAndCommit(GURL(kTestURL));
  }

  size_t InfoBarCount() { return infobar_service()->infobar_count(); }

  void RemoveAllInfoBars() { infobar_service()->RemoveAllInfoBars(false); }

  InfoBarService* infobar_service() {
    return InfoBarService::FromWebContents(web_contents());
  }

  // Called from the observer when |WriteToSavings| is called.
  void UpdateSavings(int64_t savings) { savings_ += savings; }

  // Load a resource of size |bytes|.
  void SimulateBytes(int bytes) {
    std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
    auto resource_data_update =
        page_load_metrics::mojom::ResourceDataUpdate::New();
    resource_data_update->delta_bytes = bytes;
    resources.push_back(std::move(resource_data_update));
    SimulateResourceDataUseUpdate(resources);
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    auto observer = std::make_unique<TestPageCappingPageLoadMetricsObserver>(
        fuzzing_offset_, test_blacklist_.get(),
        std::make_unique<base::SimpleTestTickClock>(),
        base::BindRepeating(&PageCappingObserverTest::UpdateSavings,
                            base::Unretained(this)));
    observer_ = observer.get();
    // Keep the clock frozen.
    tracker->AddObserver(std::move(observer));
  }

  // blacklist::OptOutBlacklistDelegate:
  void OnNewBlacklistedHost(const std::string& host, base::Time time) override {
  }
  void OnUserBlacklistedStatusChange(bool blacklisted) override {}
  void OnBlacklistCleared(base::Time time) override {}

  base::test::ScopedFeatureList scoped_feature_list_;
  int64_t savings_ = 0;
  int64_t fuzzing_offset_ = 0;
  TestPageCappingPageLoadMetricsObserver* observer_;
  std::unique_ptr<TestPageLoadCappingBlacklist> test_blacklist_;
};

TEST_F(PageCappingObserverTest, ExperimentDisabled) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages);
  SetUpTest(false, {});

  // Load a resource slightly over 1 MB.
  // The InfoBar should not show even though the cap would be met because the
  // feature is disabled.
  SimulateBytes(1 * 1024 * 1024 + 10);

  EXPECT_EQ(0u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, DefaultThresholdNotMetNonMedia) {
  SetUpTest(true, {});

  // Load a resource slightly under 5 MB, the default page capping threshold.
  // The cap is not met, so the InfoBar should not show.
  SimulateBytes(5 * 1024 * 1024 - 10);

  EXPECT_EQ(0u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, DefaultThresholdMetNonMedia) {
  SetUpTest(true, {});

  // Load a resource slightly over 5 MB, the default page capping threshold.
  // The cap is not met, so the InfoBar should not show.
  SimulateBytes(5 * 1024 * 1024 + 10);

  EXPECT_EQ(1u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, DefaultThresholdNotMetMedia) {
  SetUpTest(true, {});

  SimulateMediaPlayed();

  // Load a resource slightly under 15 MB, the default media page capping
  // threshold. The cap is not met, so the InfoBar should not show.
  SimulateBytes(15 * 1024 * 1024 - 10);

  EXPECT_EQ(0u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, DefaultThresholdMetMedia) {
  SetUpTest(true, {});

  SimulateMediaPlayed();

  // Load a resource slightly over 15 MB, the default media page capping
  // threshold. The cap is not met, so the InfoBar should not show.
  SimulateBytes(15 * 1024 * 1024 + 10);

  EXPECT_EQ(1u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, NotEnoughForThreshold) {
  SetUpTest(true, {{"MediaPageCapMiB", "1"}, {"PageCapMiB", "1"}});

  // Load a resource slightly under 1 MB.
  // The cap is not met, so the InfoBar should not show.
  SimulateBytes(1 * 1024 * 1024 - 10);

  EXPECT_EQ(0u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, InfobarOnlyShownOnce) {
  SetUpTest(true, {{"MediaPageCapMiB", "1"}, {"PageCapMiB", "1"}});

  // Load a resource slightly over 1 MB.
  // This should trigger the InfoBar.
  SimulateBytes(1 * 1024 * 1024 + 10);
  EXPECT_EQ(1u, InfoBarCount());
  // The InfoBar is already being shown, so this should not trigger an InfoBar.
  SimulateBytes(10);
  EXPECT_EQ(1u, InfoBarCount());

  // Clear all InfoBars.
  RemoveAllInfoBars();
  // Verify the InfoBars are clear.
  EXPECT_EQ(0u, InfoBarCount());
  // This would trigger an InfoBar if one was not already shown from this
  // observer.
  SimulateBytes(10);
  EXPECT_EQ(0u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, MediaCap) {
  SetUpTest(true, {{"MediaPageCapMiB", "10"}, {"PageCapMiB", "1"}});

  // Show that media has played.
  SimulateMediaPlayed();

  // Load a resource slightly under 10 MB.
  // This should not trigger an InfoBar as the media cap is not met.
  SimulateBytes(10 * 1024 * 1024 - 10);
  EXPECT_EQ(0u, InfoBarCount());

  // Adding more data should now trigger the InfoBar.
  SimulateBytes(10);
  EXPECT_EQ(1u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, PageCap) {
  SetUpTest(true, {{"MediaPageCapMiB", "1"}, {"PageCapMiB", "10"}});

  // Load a resource slightly under 10 MB.
  // This should not trigger an InfoBar as the non-media cap is not met.
  SimulateBytes(10 * 1024 * 1024 - 10);
  EXPECT_EQ(0u, InfoBarCount());

  // Adding more data should now trigger the InfoBar.
  SimulateBytes(10);
  EXPECT_EQ(1u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, PageCappingTriggered) {
  SetUpTest(true, {{"MediaPageCapMiB", "1"}, {"PageCapMiB", "1"}});

  // Load a resource slightly over 1 MB.
  // This should trigger an InfoBar as the non-media cap is met.
  SimulateBytes(1 * 1024 * 1024 + 10);
  EXPECT_EQ(1u, InfoBarCount());

  // Verify the callback is called twice with appropriate bool values.
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());
  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WebContentsTester::For(web_contents())
                  ->GetPauseSubresourceLoadingCalled());
  EXPECT_EQ(1u, InfoBarCount());
  EXPECT_TRUE(observer_->IsPausedForTesting());

  content::WebContentsTester::For(web_contents())
      ->ResetPauseSubresourceLoadingCalled();
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());

  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());
  EXPECT_FALSE(observer_->IsPausedForTesting());
}

// Check that data savings works without a specific param. The estimated page
// size should be 1.5 the threshold.
TEST_F(PageCappingObserverTest, DataSavingsDefault) {
  SetUpTest(true, {{"MediaPageCapMiB", "1"}, {"PageCapMiB", "1"}});

  // Load a resource of 1 MB.
  // This should trigger an InfoBar as the non-media cap is met.
  SimulateBytes(1024 * 1024);
  EXPECT_EQ(1u, InfoBarCount());

  // Verify the callback is called twice with appropriate bool values.
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());
  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WebContentsTester::For(web_contents())
                  ->GetPauseSubresourceLoadingCalled());
  EXPECT_EQ(1u, InfoBarCount());
  EXPECT_TRUE(observer_->IsPausedForTesting());

  // This should cause savings to be written.
  SimulateAppEnterBackground();
  EXPECT_EQ(1024 * 1024 / 2, savings_);

  // Load a resource of size 1/4 MB.
  SimulateBytes(1024 * 1024 / 4);

  // Adding another resource and forcing savings to be written should reduce
  // total savings.
  SimulateAppEnterBackground();
  EXPECT_EQ(1024 * 1024 / 4, savings_);

  content::WebContentsTester::For(web_contents())
      ->ResetPauseSubresourceLoadingCalled();
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());

  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());
  EXPECT_FALSE(observer_->IsPausedForTesting());

  // Resuming the page should force savings to 0.
  NavigateToUntrackedUrl();
  EXPECT_EQ(0l, savings_);
}

// Check that data savings works with a specific param. The estimated page size
// should be |PageTypicalLargePageMB|.
TEST_F(PageCappingObserverTest, DataSavingsParam) {
  SetUpTest(true, {{"MediaPageCapMiB", "1"},
                   {"PageCapMiB", "1"},
                   {"PageTypicalLargePageMiB", "2"}});

  // Load a resource of 1 MB.
  // This should trigger an InfoBar as the non-media cap is met.
  SimulateBytes(1024 * 1024);
  EXPECT_EQ(1u, InfoBarCount());

  // Verify the callback is called twice with appropriate bool values.
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());
  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WebContentsTester::For(web_contents())
                  ->GetPauseSubresourceLoadingCalled());
  EXPECT_EQ(1u, InfoBarCount());
  EXPECT_TRUE(observer_->IsPausedForTesting());

  // This should cause savings to be written.
  SimulateAppEnterBackground();
  EXPECT_EQ(1024 * 1024, savings_);

  // Load a resource of size 1/4 MB.
  SimulateBytes(1024 * 1024 / 4);

  // Adding another resource and forcing savings to be written should reduce
  // total savings.
  SimulateAppEnterBackground();
  EXPECT_EQ(1024 * 1024 * 3 / 4, savings_);

  content::WebContentsTester::For(web_contents())
      ->ResetPauseSubresourceLoadingCalled();
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());

  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());
  EXPECT_FALSE(observer_->IsPausedForTesting());

  // Resuming should make the savings 0.
  SimulateAppEnterBackground();
  EXPECT_EQ(0l, savings_);

  // Forcing savings to be written again should not change savings.
  NavigateToUntrackedUrl();
  EXPECT_EQ(0l, savings_);
}

TEST_F(PageCappingObserverTest, DataSavingsHistogram) {
  SetUpTest(true, {{"MediaPageCapMiB", "1"},
                   {"PageCapMiB", "1"},
                   {"PageTypicalLargePageMiB", "2"}});
  base::HistogramTester histogram_tester;

  // Load a resource of 1 MB.
  // This should trigger an InfoBar as the non-media cap is met.
  SimulateBytes(1024 * 1024);

  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  // Savings is only recorded in OnComplete
  SimulateAppEnterBackground();
  histogram_tester.ExpectTotalCount("HeavyPageCapping.RecordedDataSavings", 0);

  NavigateToUntrackedUrl();
  histogram_tester.ExpectUniqueSample("HeavyPageCapping.RecordedDataSavings",
                                      1024, 1);
}

TEST_F(PageCappingObserverTest, DataSavingsHistogramWhenResumed) {
  SetUpTest(true, {{"MediaPageCapMiB", "1"},
                   {"PageCapMiB", "1"},
                   {"PageTypicalLargePageMiB", "2"}});
  base::HistogramTester histogram_tester;

  // Load a resource of 1 MB.
  // This should trigger an InfoBar as the non-media cap is met.
  SimulateBytes(1024 * 1024);

  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  NavigateToUntrackedUrl();
  histogram_tester.ExpectTotalCount("HeavyPageCapping.RecordedDataSavings", 0);
}

TEST_F(PageCappingObserverTest, UKMNotRecordedWhenNotTriggered) {
  SetUpTest(true, {{"MediaPageCapMiB", "1"},
                   {"PageCapMiB", "1"},
                   {"PageTypicalLargePageMiB", "2"}});
  base::HistogramTester histogram_tester;
  NavigateToUntrackedUrl();

  using UkmEntry = ukm::builders::PageLoadCapping;
  auto entries = test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);

  EXPECT_EQ(0u, entries.size());
}

TEST_F(PageCappingObserverTest, UKMRecordedInfoBarShown) {
  SetUpTest(true, {{"MediaPageCapMiB", "1"},
                   {"PageCapMiB", "1"},
                   {"PageTypicalLargePageMiB", "2"}});

  // Load a resource of 1 MB.
  // This should trigger an InfoBar as the non-media cap is met.
  SimulateBytes(1024 * 1024);

  NavigateToUntrackedUrl();

  using UkmEntry = ukm::builders::PageLoadCapping;
  auto entries = test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);

  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntrySourceHasUrl(entries[0], GURL(kTestURL));
  EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(entries[0],
                                                 UkmEntry::kFinalStateName));
  EXPECT_EQ(static_cast<int64_t>(1),
            *(test_ukm_recorder().GetEntryMetric(entries[0],
                                                 UkmEntry::kFinalStateName)));
  EXPECT_EQ(
      static_cast<int64_t>(
          PageCappingPageLoadMetricsObserver::PageCappingState::kInfoBarShown),
      *(test_ukm_recorder().GetEntryMetric(entries[0],
                                           UkmEntry::kFinalStateName)));
}

TEST_F(PageCappingObserverTest, UKMRecordedPaused) {
  SetUpTest(true, {{"MediaPageCapMiB", "1"},
                   {"PageCapMiB", "1"},
                   {"PageTypicalLargePageMiB", "2"}});

  // Load a resource of 1 MB.
  // This should trigger an InfoBar as the non-media cap is met.
  SimulateBytes(1024 * 1024);

  // Pause the page.
  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  NavigateToUntrackedUrl();
  using UkmEntry = ukm::builders::PageLoadCapping;
  auto entries = test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);

  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntrySourceHasUrl(entries[0], GURL(kTestURL));
  EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(entries[0],
                                                 UkmEntry::kFinalStateName));
  EXPECT_EQ(static_cast<int64_t>(2),
            *(test_ukm_recorder().GetEntryMetric(entries[0],
                                                 UkmEntry::kFinalStateName)));
  EXPECT_EQ(
      static_cast<int64_t>(
          PageCappingPageLoadMetricsObserver::PageCappingState::kPagePaused),
      *(test_ukm_recorder().GetEntryMetric(entries[0],
                                           UkmEntry::kFinalStateName)));
}

TEST_F(PageCappingObserverTest, UKMRecordedResumed) {
  SetUpTest(true, {{"MediaPageCapMiB", "1"},
                   {"PageCapMiB", "1"},
                   {"PageTypicalLargePageMiB", "2"}});

  // Load a resource of 1 MB.
  // This should trigger an InfoBar as the non-media cap is met.
  SimulateBytes(1024 * 1024);

  // Pause then resume.
  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  NavigateToUntrackedUrl();
  using UkmEntry = ukm::builders::PageLoadCapping;
  auto entries = test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);

  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntrySourceHasUrl(entries[0], GURL(kTestURL));
  EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(entries[0],
                                                 UkmEntry::kFinalStateName));
  EXPECT_EQ(static_cast<int64_t>(3),
            *(test_ukm_recorder().GetEntryMetric(entries[0],
                                                 UkmEntry::kFinalStateName)));
  EXPECT_EQ(
      static_cast<int64_t>(
          PageCappingPageLoadMetricsObserver::PageCappingState::kPageResumed),
      *(test_ukm_recorder().GetEntryMetric(entries[0],
                                           UkmEntry::kFinalStateName)));
}

TEST_F(PageCappingObserverTest, FuzzingOffset) {
  fuzzing_offset_ = 1;
  SetUpTest(true, {{"MediaPageCapMiB", "1"},
                   {"PageCapMiB", "1"},
                   {"PageTypicalLargePageMiB", "2"}});

  // Load a resource of 1 MB.
  // This should not trigger an InfoBar as the non-media cap is not met.
  SimulateBytes(1024 * 1024);

  EXPECT_EQ(0u, InfoBarCount());

  // Load a resource of 1 byte.
  // This should trigger an InfoBar as the non-media cap is met.
  SimulateBytes(1);

  EXPECT_EQ(1u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, NullBlacklistBlocksInfoBar) {
  test_blacklist_ = nullptr;
  SetUpTest(true, {{"MediaPageCapMiB", "1"},
                   {"PageCapMiB", "1"},
                   {"PageTypicalLargePageMiB", "2"}});
  base::HistogramTester histogram_tester;

  // Load a resource of 1 MB.
  // This should be enough to trigger an InfoBar (when ignoring the blacklist).
  SimulateBytes(1024 * 1024);

  // Because the blacklist is null (as in the case of incognito profiles), the
  // InfoBar should not be shown.
  EXPECT_EQ(0u, InfoBarCount());

  histogram_tester.ExpectTotalCount("HeavyPageCapping.BlacklistReason", 0);
}

TEST_F(PageCappingObserverTest, BlacklistOnTwoOptOuts) {
  SetUpTest(true, {{"MediaPageCapMiB", "1"},
                   {"PageCapMiB", "1"},
                   {"PageTypicalLargePageMiB", "2"}});
  base::HistogramTester histogram_tester;

  test_blacklist_->AddEntry(GURL(kTestURL).host(), true, 0);
  test_blacklist_->AddEntry(GURL(kTestURL).host(), true, 0);

  // Verify the blacklist is reporting not allowed.
  std::vector<blacklist::BlacklistReason> passed_reasons;
  auto blacklist_reason = test_blacklist_->IsLoadedAndAllowed(
      GURL(kTestURL).host(), 0, false, &passed_reasons);
  EXPECT_NE(blacklist::BlacklistReason::kAllowed, blacklist_reason);

  // Load a resource of 1 MB.
  // This should trigger an InfoBar as the non-media cap is met.
  SimulateBytes(1024 * 1024);

  // Because the blacklist is in an opted out state, the InfoBar should not be
  // shown.
  EXPECT_EQ(0u, InfoBarCount());

  histogram_tester.ExpectUniqueSample("HeavyPageCapping.BlacklistReason",
                                      static_cast<int>(blacklist_reason), 1);
}

TEST_F(PageCappingObserverTest, IgnoringInfoBarTriggersOptOut) {
  SetUpTest(true, {{"MediaPageCapMiB", "1"},
                   {"PageCapMiB", "1"},
                   {"PageTypicalLargePageMiB", "2"}});

  test_blacklist_->AddEntry(GURL(kTestURL).host(), true, 0);

  std::vector<blacklist::BlacklistReason> passed_reasons;
  auto blacklist_reason = test_blacklist_->IsLoadedAndAllowed(
      GURL(kTestURL).host(), 0, false, &passed_reasons);
  EXPECT_EQ(blacklist::BlacklistReason::kAllowed, blacklist_reason);

  // Load a resource of 1 MB.
  // This should trigger an InfoBar as the non-media cap is met.
  SimulateBytes(1024 * 1024);

  // Blacklist is in allowed state, so an InfoBar is allowed to be shown.
  EXPECT_EQ(1u, InfoBarCount());

  // Trigger an opt out reporting event.
  NavigateToUntrackedUrl();

  // Check that the opt out caused a blacklisted state.
  blacklist_reason = test_blacklist_->IsLoadedAndAllowed(
      GURL(kTestURL).host(), 0, false, &passed_reasons);
  EXPECT_NE(blacklist::BlacklistReason::kAllowed, blacklist_reason);
}

TEST_F(PageCappingObserverTest, PausedInfoBarTriggersNonOptOut) {
  SetUpTest(true, {{"MediaPageCapMiB", "1"},
                   {"PageCapMiB", "1"},
                   {"PageTypicalLargePageMiB", "2"}});

  test_blacklist_->AddEntry(GURL(kTestURL).host(), true, 0);

  std::vector<blacklist::BlacklistReason> passed_reasons;
  auto blacklist_reason = test_blacklist_->IsLoadedAndAllowed(
      GURL(kTestURL).host(), 0, false, &passed_reasons);
  EXPECT_EQ(blacklist::BlacklistReason::kAllowed, blacklist_reason);

  // Load a resource of 1 MB.
  // This should trigger an InfoBar as the non-media cap is met.
  SimulateBytes(1024 * 1024);

  // Blacklist is in allowed state, so an InfoBar is allowed to be shown.
  EXPECT_EQ(1u, InfoBarCount());

  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  // Trigger a non-opt out reporting event.
  NavigateToUntrackedUrl();

  // Check that the non-opt out did not cause a blacklisted state.
  blacklist_reason = test_blacklist_->IsLoadedAndAllowed(
      GURL(kTestURL).host(), 0, false, &passed_reasons);
  EXPECT_EQ(blacklist::BlacklistReason::kAllowed, blacklist_reason);
}

TEST_F(PageCappingObserverTest, ResumedInfoBarTriggersOptOut) {
  SetUpTest(true, {{"MediaPageCapMiB", "1"},
                   {"PageCapMiB", "1"},
                   {"PageTypicalLargePageMiB", "2"}});

  test_blacklist_->AddEntry(GURL(kTestURL).host(), true, 0);

  std::vector<blacklist::BlacklistReason> passed_reasons;
  auto blacklist_reason = test_blacklist_->IsLoadedAndAllowed(
      GURL(kTestURL).host(), 0, false, &passed_reasons);
  EXPECT_EQ(blacklist::BlacklistReason::kAllowed, blacklist_reason);

  // Load a resource of 1 MB.
  // This should not trigger an InfoBar as the blacklist rules should prevent
  // it.
  SimulateBytes(1024 * 1024);

  // Blacklist is in allowed state, so an InfoBar is allowed to be shown.
  EXPECT_EQ(1u, InfoBarCount());

  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  // Trigger an opt out reporting event.
  NavigateToUntrackedUrl();

  // Check that the opt out caused a blacklisted state.
  blacklist_reason = test_blacklist_->IsLoadedAndAllowed(
      GURL(kTestURL).host(), 0, false, &passed_reasons);
  EXPECT_NE(blacklist::BlacklistReason::kAllowed, blacklist_reason);
}
