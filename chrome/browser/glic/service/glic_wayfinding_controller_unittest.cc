// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_wayfinding_controller.h"

#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_provider_metrics.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

using mojom::InvocationSource;

constexpr char kCueResult[] = "Glic.Wayfinding.CueResult";

class GlicWayfindingControllerTest : public testing::Test {
 protected:
  GlicWayfindingControllerTest() {
    prefs::RegisterProfilePrefs(pref_service_.registry());
  }

  InvocationSource FirstSource() {
    return static_cast<InvocationSource>(
        pref_service_.GetInteger(prefs::kGlicWayfindingFirstInvocationSource));
  }

  bool UsedSelfStartedOpen() {
    return pref_service_.GetBoolean(prefs::kGlicWayfindingUsedSelfStartedOpen);
  }

  void Invoke(InvocationSource source) { controller_.OnInvoke(source); }

  // Makes the profile eligible, the way a first right-click open would.
  void MakeEligible() { Invoke(InvocationSource::kWebContentsContextMenuPage); }

  void ExpectCueResult(WayfindingCueResult result) {
    base::HistogramTester histograms;
    controller_.OnUserLeftPanel(GlicMetricReporter(nullptr));
    histograms.ExpectUniqueSample(kCueResult, result, 1);
  }

  int cue_count() {
    return pref_service_.GetInteger(prefs::kGlicWayfindingCueCount);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histograms_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  GlicWayfindingController controller_{&pref_service_,
                                       /*was_opened_before=*/false};
};

TEST_F(GlicWayfindingControllerTest, RecordsFirstInvocationSource) {
  Invoke(InvocationSource::kWebContentsContextMenuPage);
  Invoke(InvocationSource::kThreeDotsMenu);
  EXPECT_EQ(FirstSource(), InvocationSource::kWebContentsContextMenuPage);
  EXPECT_FALSE(UsedSelfStartedOpen());
}

TEST_F(GlicWayfindingControllerTest, ExistingUserHasNoFirstInvocationSource) {
  GlicWayfindingController controller(&pref_service_,
                                      /*was_opened_before=*/true);
  controller.OnInvoke(InvocationSource::kWebContentsContextMenuPage);
  EXPECT_EQ(FirstSource(), InvocationSource::kUnsupported);
}

TEST_F(GlicWayfindingControllerTest, RecordsButtonOrHotkeyOpen) {
  for (InvocationSource source :
       {InvocationSource::kTopChromeButton, InvocationSource::kToolbarButton,
        InvocationSource::kNudge, InvocationSource::kOsHotkey}) {
    pref_service_.ClearPref(prefs::kGlicWayfindingUsedSelfStartedOpen);
    Invoke(source);
    EXPECT_TRUE(UsedSelfStartedOpen()) << "source=" << source;
  }
}

TEST_F(GlicWayfindingControllerTest, FirstOpenFromButtonIsNotEligible) {
  Invoke(InvocationSource::kTopChromeButton);
  ExpectCueResult(WayfindingCueResult::kNotEligible);
}

TEST_F(GlicWayfindingControllerTest, ExistingUserIsNotEligible) {
  GlicWayfindingController controller(&pref_service_,
                                      /*was_opened_before=*/true);
  controller.OnInvoke(InvocationSource::kWebContentsContextMenuPage);
  base::HistogramTester histograms;
  controller.OnUserLeftPanel(GlicMetricReporter(nullptr));
  histograms.ExpectUniqueSample(kCueResult, WayfindingCueResult::kNotEligible,
                                1);
}

TEST_F(GlicWayfindingControllerTest, LaterButtonOpenEndsEligibility) {
  MakeEligible();
  Invoke(InvocationSource::kTopChromeButton);
  ExpectCueResult(WayfindingCueResult::kNotEligible);
}

TEST_F(GlicWayfindingControllerTest, LaterOtherOpenKeepsEligibility) {
  MakeEligible();
  Invoke(InvocationSource::kThreeDotsMenu);
  ExpectCueResult(WayfindingCueResult::kWouldCue);
}

TEST_F(GlicWayfindingControllerTest, EligibleProfileWouldCue) {
  MakeEligible();
  controller_.OnUserLeftPanel(GlicMetricReporter(nullptr));
  histograms_.ExpectUniqueSample(kCueResult, WayfindingCueResult::kWouldCue, 1);
  EXPECT_EQ(cue_count(), 1);
}

TEST_F(GlicWayfindingControllerTest, AtMostOneCuePerDay) {
  MakeEligible();
  controller_.OnUserLeftPanel(GlicMetricReporter(nullptr));
  task_environment_.FastForwardBy(base::Hours(23));
  controller_.OnUserLeftPanel(GlicMetricReporter(nullptr));
  histograms_.ExpectBucketCount(
      kCueResult, WayfindingCueResult::kNotEnoughTimeBetweenCues, 1);
  EXPECT_EQ(cue_count(), 1);

  task_environment_.FastForwardBy(base::Hours(1));
  controller_.OnUserLeftPanel(GlicMetricReporter(nullptr));
  EXPECT_EQ(cue_count(), 2);
}

TEST_F(GlicWayfindingControllerTest, AtMostThreeCuesPerProfile) {
  MakeEligible();
  for (int i = 0; i < 4; ++i) {
    controller_.OnUserLeftPanel(GlicMetricReporter(nullptr));
    task_environment_.FastForwardBy(base::Days(1));
  }
  histograms_.ExpectBucketCount(kCueResult, WayfindingCueResult::kWouldCue, 3);
  histograms_.ExpectBucketCount(kCueResult,
                                WayfindingCueResult::kProfileCapReached, 1);
  EXPECT_EQ(cue_count(), 3);
}

}  // namespace
}  // namespace glic
