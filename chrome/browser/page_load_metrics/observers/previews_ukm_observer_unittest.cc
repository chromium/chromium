// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/previews_ukm_observer.h"

#include <memory>
#include <unordered_map>

#include "base/base64.h"
#include "base/macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/previews/previews_content_util.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/ip_endpoint.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "third_party/blink/public/common/loader/previews_state.h"

namespace content {
class NavigationHandle;
}

namespace previews {

namespace {

const char kDefaultTestUrl[] = "https://www.google.com/";

class TestPreviewsUKMObserver : public PreviewsUKMObserver {
 public:
  TestPreviewsUKMObserver(
      blink::PreviewsState committed_state,
      blink::PreviewsState allowed_state,
      bool origin_opt_out_received,
      bool save_data_enabled,
      std::unordered_map<PreviewsType, PreviewsEligibilityReason>
          eligibility_reasons)
      : committed_state_(committed_state),
        allowed_state_(allowed_state),
        origin_opt_out_received_(origin_opt_out_received),
        save_data_enabled_(save_data_enabled),
        eligibility_reasons_(eligibility_reasons) {}

  ~TestPreviewsUKMObserver() override {}

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override {
    PreviewsUITabHelper* ui_tab_helper = PreviewsUITabHelper::FromWebContents(
        navigation_handle->GetWebContents());
    previews::PreviewsUserData* user_data =
        ui_tab_helper->CreatePreviewsUserDataForNavigationHandle(
            navigation_handle, 1u);

    user_data->set_allowed_previews_state(allowed_state_);
    user_data->set_committed_previews_state(committed_state_);
    user_data->SetCommittedPreviewsTypeForTesting(
        previews::GetMainFramePreviewsType(committed_state_));

    if (origin_opt_out_received_) {
      user_data->set_cache_control_no_transform_directive();
    }

    for (auto iter = eligibility_reasons_.begin();
         iter != eligibility_reasons_.end(); iter++) {
      user_data->SetEligibilityReasonForPreview(iter->first, iter->second);
    }

    return PreviewsUKMObserver::OnCommit(navigation_handle, source_id);
  }

 private:
  bool IsDataSaverEnabled(
      content::NavigationHandle* navigation_handle) const override {
    return save_data_enabled_;
  }

  blink::PreviewsState committed_state_;
  blink::PreviewsState allowed_state_;
  bool origin_opt_out_received_;
  const bool save_data_enabled_;
  std::unordered_map<PreviewsType, PreviewsEligibilityReason>
      eligibility_reasons_;

  DISALLOW_COPY_AND_ASSIGN(TestPreviewsUKMObserver);
};

class PreviewsUKMObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  PreviewsUKMObserverTest() {}
  ~PreviewsUKMObserverTest() override {}

  void RunTest(blink::PreviewsState committed_state,
               blink::PreviewsState allowed_state,
               bool origin_opt_out,
               bool save_data_enabled,
               std::unordered_map<PreviewsType, PreviewsEligibilityReason>
                   eligibility_reasons) {
    committed_state_ = committed_state;
    allowed_state_ = allowed_state;
    origin_opt_out_ = origin_opt_out;
    save_data_enabled_ = save_data_enabled;
    eligibility_reasons_ = eligibility_reasons;
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        GURL(kDefaultTestUrl), web_contents());

    navigation->Commit();
  }

  void ValidatePreviewsUKM(
      blink::PreviewsState expected_recorded_previews,
      int opt_out_value,
      bool origin_opt_out_expected,
      bool save_data_enabled_expected,
      bool previews_likely_expected,
      std::unordered_map<PreviewsType, PreviewsEligibilityReason>
          eligibility_reasons) {
    using UkmEntry = ukm::builders::Previews;
    auto entries =
        tester()->test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);
    if (expected_recorded_previews == 0 && opt_out_value == 0 &&
        !origin_opt_out_expected && !save_data_enabled_expected &&
        !previews_likely_expected) {
      EXPECT_EQ(0u, entries.size());
      return;
    }
    EXPECT_EQ(1u, entries.size());

    const auto* const entry = entries.front();
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(
        entry, GURL(kDefaultTestUrl));

    // Collect the set of recorded previews into a PreviewsState bitmask to
    // compare against the expected previews.
    blink::PreviewsState recorded_previews = 0;
    if (tester()->test_ukm_recorder().EntryHasMetric(
            entry, UkmEntry::kdefer_all_scriptName))
      recorded_previews |= blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON;
    EXPECT_EQ(expected_recorded_previews, recorded_previews);

    EXPECT_EQ(opt_out_value != 0, tester()->test_ukm_recorder().EntryHasMetric(
                                      entry, UkmEntry::kopt_outName));
    if (opt_out_value != 0) {
      tester()->test_ukm_recorder().ExpectEntryMetric(
          entry, UkmEntry::kopt_outName, opt_out_value);
    }
    EXPECT_EQ(origin_opt_out_expected,
              tester()->test_ukm_recorder().EntryHasMetric(
                  entry, UkmEntry::korigin_opt_outName));
    EXPECT_EQ(save_data_enabled_expected,
              tester()->test_ukm_recorder().EntryHasMetric(
                  entry, UkmEntry::ksave_data_enabledName));
    EXPECT_EQ(previews_likely_expected,
              tester()->test_ukm_recorder().EntryHasMetric(
                  entry, UkmEntry::kpreviews_likelyName));

  }

  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();
    PreviewsUITabHelper::CreateForWebContents(web_contents());
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<TestPreviewsUKMObserver>(
        committed_state_, allowed_state_, origin_opt_out_, save_data_enabled_,
        eligibility_reasons_));
    // Data is only added to the first navigation after RunTest().
    committed_state_ = blink::PreviewsTypes::PREVIEWS_OFF;
    allowed_state_ = blink::PreviewsTypes::PREVIEWS_OFF;
    origin_opt_out_ = false;
    eligibility_reasons_.clear();
  }

 private:
  blink::PreviewsState committed_state_ = blink::PreviewsTypes::PREVIEWS_OFF;
  blink::PreviewsState allowed_state_ = blink::PreviewsTypes::PREVIEWS_OFF;
  bool origin_opt_out_ = false;
  bool save_data_enabled_ = false;
  std::unordered_map<PreviewsType, PreviewsEligibilityReason>
      eligibility_reasons_ = {};

  DISALLOW_COPY_AND_ASSIGN(PreviewsUKMObserverTest);
};

TEST_F(PreviewsUKMObserverTest, NoPreviewSeen) {
  RunTest(blink::PreviewsTypes::PREVIEWS_OFF /* committed_state */,
          blink::PreviewsTypes::PREVIEWS_UNSPECIFIED /* allowed_state */,
          false /* origin_opt_out */, false /* save_data_enabled */,
          {} /* eligibility_reasons */);
  tester()->NavigateToUntrackedUrl();

  ValidatePreviewsUKM(
      blink::PreviewsTypes::PREVIEWS_UNSPECIFIED, 0 /* opt_out_value */,
      false /* origin_opt_out_expected */,
      false /* save_data_enabled_expected */, false /* previews_likely */,
      {} /* eligibility_reasons */);
}

TEST_F(PreviewsUKMObserverTest, UntrackedPreviewTypeOptOut) {
  RunTest(blink::PreviewsTypes::PREVIEWS_OFF /* committed_state */,
          blink::PreviewsTypes::PREVIEWS_UNSPECIFIED /* allowed_state */,
          false /* origin_opt_out */, false /* save_data_enabled */,
          {} /* eligibility_reasons */);
  tester()->metrics_web_contents_observer()->BroadcastEventToObservers(
      PreviewsUITabHelper::OptOutEventKey());
  tester()->NavigateToUntrackedUrl();

  // Opt out should not be added since we don't track this type.
  ValidatePreviewsUKM(
      blink::PreviewsTypes::PREVIEWS_UNSPECIFIED, 0 /* opt_out_value */,
      false /* origin_opt_out_expected */,
      false /* save_data_enabled_expected */, false /* previews_likely */,
      {} /* eligibility_reasons */);
}


TEST_F(PreviewsUKMObserverTest, DeferAllScriptSeen) {
  RunTest(blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON /* committed_state */,
          blink::PreviewsTypes::PREVIEWS_UNSPECIFIED /* allowed_state */,
          false /* origin_opt_out */, false /* save_data_enabled */,
          {} /* eligibility_reasons */);

  tester()->NavigateToUntrackedUrl();

  ValidatePreviewsUKM(blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON,
                      0 /* opt_out_value */,
                      false /* origin_opt_out_expected */,
                      false /* save_data_enabled_expected */,
                      true /* previews_likely */, {} /* eligibility_reasons */);
}

TEST_F(PreviewsUKMObserverTest, DeferAllScriptOptOutChip) {
  RunTest(blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON /* committed_state */,
          blink::PreviewsTypes::PREVIEWS_UNSPECIFIED /* allowed_state */,
          false /* origin_opt_out */, false /* save_data_enabled */,
          {} /* eligibility_reasons */);

  tester()->metrics_web_contents_observer()->BroadcastEventToObservers(
      PreviewsUITabHelper::OptOutEventKey());
  tester()->NavigateToUntrackedUrl();

  ValidatePreviewsUKM(blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON,
                      2 /* opt_out_value */,
                      false /* origin_opt_out_expected */,
                      false /* save_data_enabled_expected */,
                      true /* previews_likely */, {} /* eligibility_reasons */);
}

TEST_F(PreviewsUKMObserverTest, OriginOptOut) {
  RunTest(blink::PreviewsTypes::PREVIEWS_OFF /* committed_state */,
          blink::PreviewsTypes::PREVIEWS_UNSPECIFIED /* allowed_state */,
          true /* origin_opt_out */, false /* save_data_enabled */,
          {} /* eligibility_reasons */);

  tester()->NavigateToUntrackedUrl();

  ValidatePreviewsUKM(blink::PreviewsTypes::PREVIEWS_UNSPECIFIED,
                      0 /* opt_out_value */, true /* origin_opt_out_expected */,
                      false /* save_data_enabled_expected */,
                      false /* previews_likely */,
                      {} /* eligibility_reasons */);
}

TEST_F(PreviewsUKMObserverTest, DataSaverEnabled) {
  RunTest(blink::PreviewsTypes::PREVIEWS_OFF /* committed_state */,
          blink::PreviewsTypes::PREVIEWS_UNSPECIFIED /* allowed_state */,
          false /* origin_opt_out */, true /* save_data_enabled */,
          {} /* eligibility_reasons */);

  tester()->NavigateToUntrackedUrl();

  ValidatePreviewsUKM(
      blink::PreviewsTypes::PREVIEWS_UNSPECIFIED, 0 /* opt_out_value */,
      false /* origin_opt_out_expected */,
      true /* save_data_enabled_expected */, false /* previews_likely */,
      {} /* eligibility_reasons */);
}

TEST_F(PreviewsUKMObserverTest, PreviewsLikelyNotSet_PostCommitDecision) {
  RunTest(blink::PreviewsTypes::PREVIEWS_OFF /* committed_state */,
          blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON /* allowed_state */,
          false /* origin_opt_out */, true /* save_data_enabled */,
          {} /* eligibility_reasons */);

  tester()->NavigateToUntrackedUrl();

  ValidatePreviewsUKM(
      blink::PreviewsTypes::PREVIEWS_UNSPECIFIED, 0 /* opt_out_value */,
      false /* origin_opt_out_expected */,
      true /* save_data_enabled_expected */, false /* previews_likely */,
      {} /* eligibility_reasons */);
}

TEST_F(PreviewsUKMObserverTest, PreviewsLikelyNotSet_PreviewsOff) {
  RunTest(blink::PreviewsTypes::PREVIEWS_OFF /* committed_state */,
          blink::PreviewsTypes::PREVIEWS_OFF /* allowed_state */,
          false /* origin_opt_out */, true /* save_data_enabled */,
          {} /* eligibility_reasons */);

  tester()->NavigateToUntrackedUrl();

  ValidatePreviewsUKM(
      blink::PreviewsTypes::PREVIEWS_UNSPECIFIED, 0 /* opt_out_value */,
      false /* origin_opt_out_expected */,
      true /* save_data_enabled_expected */, false /* previews_likely */,
      {} /* eligibility_reasons */);
}

TEST_F(PreviewsUKMObserverTest, CoinFlipResult_Holdback) {
  RunTest(blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON /* committed_state */,
          blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON /* allowed_state */,
          false /* origin_opt_out */, true /* save_data_enabled */,
          {} /* eligibility_reasons */);

  tester()->NavigateToUntrackedUrl();

  ValidatePreviewsUKM(blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON,
                      0 /* opt_out_value */,
                      false /* origin_opt_out_expected */,
                      true /* save_data_enabled_expected */,
                      true /* previews_likely */, {} /* eligibility_reasons */);
}

TEST_F(PreviewsUKMObserverTest, CoinFlipResult_Allowed) {
  RunTest(blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON /* committed_state */,
          blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON /* allowed_state */,
          false /* origin_opt_out */, true /* save_data_enabled */,
          {} /* eligibility_reasons */);

  tester()->NavigateToUntrackedUrl();

  ValidatePreviewsUKM(blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON,
                      0 /* opt_out_value */,
                      false /* origin_opt_out_expected */,
                      true /* save_data_enabled_expected */,
                      true /* previews_likely */, {} /* eligibility_reasons */);
}

TEST_F(PreviewsUKMObserverTest, LogPreviewsEligibilityReason_WithAllowed) {
  RunTest(blink::PreviewsTypes::PREVIEWS_OFF /* committed_state */,
          blink::PreviewsTypes::PREVIEWS_UNSPECIFIED /* allowed_state */,
          false /* origin_opt_out */, true /* save_data_enabled */,
          {// ALLOWED is equal to zero and should not be recorded.
           {PreviewsType::NOSCRIPT,
            PreviewsEligibilityReason::ALLOWED}} /* eligibility_reasons */);

  tester()->NavigateToUntrackedUrl();

  ValidatePreviewsUKM(
      blink::PreviewsTypes::PREVIEWS_UNSPECIFIED, 0 /* opt_out_value */,
      false /* origin_opt_out_expected */,
      true /* save_data_enabled_expected */, false /* previews_likely */,
      {} /* eligibility_reasons */);
}

TEST_F(PreviewsUKMObserverTest, LogPreviewsEligibilityReason_NoneAllowed) {
  RunTest(blink::PreviewsTypes::PREVIEWS_OFF /* committed_state */,
          blink::PreviewsTypes::PREVIEWS_UNSPECIFIED /* allowed_state */,
          false /* origin_opt_out */, true /* save_data_enabled */,
          {{PreviewsType::DEFER_ALL_SCRIPT,
            PreviewsEligibilityReason::
                BLOCKLIST_DATA_NOT_LOADED}} /* eligibility_reasons */);

  tester()->NavigateToUntrackedUrl();

  ValidatePreviewsUKM(
      blink::PreviewsTypes::PREVIEWS_UNSPECIFIED, 0 /* opt_out_value */,
      false /* origin_opt_out_expected */,
      true /* save_data_enabled_expected */, false /* previews_likely */,
      {{PreviewsType::DEFER_ALL_SCRIPT,
        PreviewsEligibilityReason::
            BLOCKLIST_DATA_NOT_LOADED}} /* eligibility_reasons */);
}

TEST_F(PreviewsUKMObserverTest, CheckReportingForHidden) {
  RunTest(blink::PreviewsTypes::PREVIEWS_OFF /* committed_state */,
          blink::PreviewsTypes::PREVIEWS_UNSPECIFIED /* allowed_state */,
          false /* origin_opt_out */, true /* save_data_enabled */,
          {} /* eligibility_reasons */);

  web_contents()->WasHidden();

  ValidatePreviewsUKM(
      blink::PreviewsTypes::PREVIEWS_UNSPECIFIED, 0 /* opt_out_value */,
      false /* origin_opt_out_expected */,
      true /* save_data_enabled_expected */, false /* previews_likely */,
      {} /* eligibility_reasons */);
}

TEST_F(PreviewsUKMObserverTest, CheckReportingForFlushMetrics) {
  RunTest(blink::PreviewsTypes::PREVIEWS_OFF /* committed_state */,
          blink::PreviewsTypes::PREVIEWS_UNSPECIFIED /* allowed_state */,
          false /* origin_opt_out */, true /* save_data_enabled */,
          {} /* eligibility_reasons */);

  tester()->SimulateAppEnterBackground();

  ValidatePreviewsUKM(
      blink::PreviewsTypes::PREVIEWS_UNSPECIFIED, 0 /* opt_out_value */,
      false /* origin_opt_out_expected */,
      true /* save_data_enabled_expected */, false /* previews_likely */,
      {} /* eligibility_reasons */);
}

TEST_F(PreviewsUKMObserverTest, TestPageEndReasonUMA) {
  base::HistogramTester histogram_tester;

  // No preview:
  RunTest(blink::PreviewsTypes::PREVIEWS_OFF /* committed_state */,
          blink::PreviewsTypes::PREVIEWS_UNSPECIFIED /* allowed_state */,
          false /* origin_opt_out */, false /* save_data_enabled */,
          {} /* eligibility_reasons */);
  tester()->NavigateToUntrackedUrl();
  histogram_tester.ExpectUniqueSample(
      "Previews.PageEndReason.None",
      page_load_metrics::PageEndReason::END_NEW_NAVIGATION, 1);
  // The top level metric is not recorded on a non-preview.
  histogram_tester.ExpectTotalCount("Previews.PageEndReason", 0);

  // Defer All Script:
  RunTest(blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON /* committed_state */,
          blink::PreviewsTypes::PREVIEWS_UNSPECIFIED /* allowed_state */,
          false /* origin_opt_out */, false /* save_data_enabled */,
          {} /* eligibility_reasons */);
  tester()->NavigateToUntrackedUrl();
  histogram_tester.ExpectUniqueSample(
      "Previews.PageEndReason.DeferAllScript",
      page_load_metrics::PageEndReason::END_NEW_NAVIGATION, 1);
  histogram_tester.ExpectBucketCount(
      "Previews.PageEndReason",
      page_load_metrics::PageEndReason::END_NEW_NAVIGATION, 1);
}

}  // namespace

}  // namespace previews
