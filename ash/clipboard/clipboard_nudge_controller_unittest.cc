// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_nudge_controller.h"

#include <array>
#include <memory>
#include <utility>

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_nudge_constants.h"
#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

namespace {

using crosapi::mojom::ClipboardHistoryControllerShowSource::kAccelerator;

constexpr char kClipboardNudgeId[] = "ClipboardContextualNudge";

// The array of all clipboard nudge types.
constexpr std::array<ClipboardNudgeType, ClipboardNudgeType::kMax + 1>
    kAllClipboardNudgeTypes = {ClipboardNudgeType::kOnboardingNudge,
                               ClipboardNudgeType::kZeroStateNudge,
                               ClipboardNudgeType::kScreenshotNotificationNudge,
                               ClipboardNudgeType::kDuplicateCopyNudge};

}  // namespace

class ClipboardNudgeControllerTestBase : public AshTestBase {
 public:
  ClipboardNudgeControllerTestBase()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  base::SimpleTestClock* clock() { return &test_clock_; }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    nudge_controller_ =
        Shell::Get()->clipboard_history_controller()->nudge_controller();
    nudge_controller_->OverrideClockForTesting(&test_clock_);
    test_clock_.Advance(base::Seconds(360));
  }

  void TearDown() override {
    nudge_controller_->ClearClockOverrideForTesting();
    AshTestBase::TearDown();
  }

  void ShowNudgeForType(ClipboardNudgeType nudge_type) {
    switch (nudge_type) {
      case ClipboardNudgeType::kOnboardingNudge:
      case ClipboardNudgeType::kZeroStateNudge:
      case ClipboardNudgeType::kDuplicateCopyNudge:
        nudge_controller_->ShowNudge(nudge_type);
        return;
      case ClipboardNudgeType::kScreenshotNotificationNudge:
        nudge_controller_->MarkScreenshotNotificationShown();
        return;
    }
  }

  // Returns an item with arbitrary data for simulating clipboard writes.
  ClipboardHistoryItem CreateItem() const {
    ClipboardHistoryItemBuilder builder;
    builder.SetText("text");
    return builder.Build();
  }

  bool IsClipboardNudgeShown() {
    return AnchoredNudgeManager::Get()->IsNudgeShown(kClipboardNudgeId);
  }

  base::HistogramTester& histograms() { return histograms_; }

  // Owned by ClipboardHistoryController.
  raw_ptr<ClipboardNudgeController, DanglingUntriaged> nudge_controller_;

 private:
  base::SimpleTestClock test_clock_;

  // Histogram value verifier.
  base::HistogramTester histograms_;
};

// A test to verify basic clipboard nudge behavior.
class ClipboardNudgeControllerTest : public ClipboardNudgeControllerTestBase {
 public:
  NudgeCatalogName GetClipboardNudgeCatalogName() {
    return Shell::Get()->anchored_nudge_manager()->GetNudgeCatalogNameForTest(
        kClipboardNudgeId);
  }
};

// Checks that clipboard state advances after the nudge controller is fed the
// right event type.
TEST_F(ClipboardNudgeControllerTest, ShouldShowNudgeAfterCorrectSequence) {
  EXPECT_EQ(ClipboardNudgeController::OnboardingState::kInit,
            nudge_controller_->onboarding_state_for_testing());
  // Checks that the first copy advances state as expected.
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  EXPECT_EQ(ClipboardNudgeController::OnboardingState::kFirstCopy,
            nudge_controller_->onboarding_state_for_testing());

  // Checks that the first paste advances state as expected.
  nudge_controller_->OnClipboardDataRead();
  EXPECT_EQ(ClipboardNudgeController::OnboardingState::kFirstPaste,
            nudge_controller_->onboarding_state_for_testing());

  // Checks that the second copy advances state as expected.
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  EXPECT_EQ(ClipboardNudgeController::OnboardingState::kSecondCopy,
            nudge_controller_->onboarding_state_for_testing());

  // Check that clipboard nudge has not yet been created.
  EXPECT_FALSE(IsClipboardNudgeShown());

  // Checks that the second paste resets state as expected.
  nudge_controller_->OnClipboardDataRead();
  EXPECT_EQ(ClipboardNudgeController::OnboardingState::kInit,
            nudge_controller_->onboarding_state_for_testing());

  // Check that clipboard nudge has been created.
  EXPECT_TRUE(IsClipboardNudgeShown());
}

// Checks that the clipboard state does not advace if too much time passes
// during the copy paste sequence.
TEST_F(ClipboardNudgeControllerTest, NudgeTimeOut) {
  // Perform copy -> paste -> copy sequence.
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  nudge_controller_->OnClipboardDataRead();
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());

  // Advance time to cause the nudge timer to time out.
  clock()->Advance(kMaxTimeBetweenPaste);
  nudge_controller_->OnClipboardDataRead();

  // Paste event should reset clipboard state to `kFirstPaste`.
  EXPECT_EQ(ClipboardNudgeController::OnboardingState::kFirstPaste,
            nudge_controller_->onboarding_state_for_testing());
}

// Checks that multiple pastes refreshes the |kMaxTimeBetweenPaste| timer that
// determines whether too much time has passed to show the nudge.
TEST_F(ClipboardNudgeControllerTest, NudgeDoesNotTimeOutWithSparsePastes) {
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  nudge_controller_->OnClipboardDataRead();
  EXPECT_EQ(ClipboardNudgeController::OnboardingState::kFirstPaste,
            nudge_controller_->onboarding_state_for_testing());

  // Perform 5 pastes over 2.5*|kMaxTimeBetweenPaste|.
  for (int paste_cycle = 0; paste_cycle < 5; paste_cycle++) {
    SCOPED_TRACE("paste cycle " + base::NumberToString(paste_cycle));
    clock()->Advance(kMaxTimeBetweenPaste / 2);
    nudge_controller_->OnClipboardDataRead();
    EXPECT_EQ(ClipboardNudgeController::OnboardingState::kFirstPaste,
              nudge_controller_->onboarding_state_for_testing());
  }

  // Check that clipboard nudge has not yet been created.
  EXPECT_FALSE(IsClipboardNudgeShown());

  // Check that HandleClipboardChanged() will advance nudge_controller's
  // ClipboardState.
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  EXPECT_EQ(ClipboardNudgeController::OnboardingState::kSecondCopy,
            nudge_controller_->onboarding_state_for_testing());
  nudge_controller_->OnClipboardDataRead();
  EXPECT_EQ(ClipboardNudgeController::OnboardingState::kInit,
            nudge_controller_->onboarding_state_for_testing());

  // Check that clipboard nudge has been created.
  EXPECT_TRUE(IsClipboardNudgeShown());
}

// Checks that consecutive copy events does not advance the clipboard state.
TEST_F(ClipboardNudgeControllerTest, RepeatedCopyDoesNotAdvanceState) {
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  EXPECT_EQ(ClipboardNudgeController::OnboardingState::kFirstCopy,
            nudge_controller_->onboarding_state_for_testing());
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  EXPECT_EQ(ClipboardNudgeController::OnboardingState::kFirstCopy,
            nudge_controller_->onboarding_state_for_testing());
}

// Checks that consecutive paste events does not advance the clipboard state.
TEST_F(ClipboardNudgeControllerTest, RepeatedPasteDoesNotAdvanceState) {
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  EXPECT_EQ(ClipboardNudgeController::OnboardingState::kFirstCopy,
            nudge_controller_->onboarding_state_for_testing());
  nudge_controller_->OnClipboardDataRead();
  EXPECT_EQ(ClipboardNudgeController::OnboardingState::kFirstPaste,
            nudge_controller_->onboarding_state_for_testing());
  nudge_controller_->OnClipboardDataRead();
  EXPECT_EQ(ClipboardNudgeController::OnboardingState::kFirstPaste,
            nudge_controller_->onboarding_state_for_testing());
}

// Verifies that administrative events does not advance clipboard state.
TEST_F(ClipboardNudgeControllerTest, AdminWriteDoesNotAdvanceState) {
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  nudge_controller_->OnClipboardDataRead();
  EXPECT_EQ(ClipboardNudgeController::OnboardingState::kFirstPaste,
            nudge_controller_->onboarding_state_for_testing());

  auto data = std::make_unique<ui::ClipboardData>();
  data->set_text("test");
  // Write the data to the clipboard, clipboard state should not advance.
  ui::ClipboardNonBacked::GetForCurrentThread()->WriteClipboardData(
      std::move(data));
  EXPECT_EQ(ClipboardNudgeController::OnboardingState::kFirstPaste,
            nudge_controller_->onboarding_state_for_testing());
}

// Verifies that controller cleans up and closes an old nudge before displaying
// another one.
TEST_F(ClipboardNudgeControllerTest, ShowZeroStateNudgeAfterOngoingNudge) {
  ShowNudgeForType(ClipboardNudgeType::kOnboardingNudge);
  ASSERT_TRUE(IsClipboardNudgeShown());
  EXPECT_EQ(GetClipboardNudgeCatalogName(),
            NudgeCatalogName::kClipboardHistoryOnboarding);

  ShowNudgeForType(ClipboardNudgeType::kZeroStateNudge);
  ASSERT_TRUE(IsClipboardNudgeShown());
  EXPECT_EQ(GetClipboardNudgeCatalogName(),
            NudgeCatalogName::kClipboardHistoryZeroState);
}

// Asserts that all nudge metric related histograms start at 0.
TEST_F(ClipboardNudgeControllerTest, NudgeMetrics_StartAtZero) {
  histograms().ExpectTotalCount(kClipboardHistoryOnboardingNudgeOpenTime, 0);
  histograms().ExpectTotalCount(kClipboardHistoryOnboardingNudgePasteTime, 0);
  histograms().ExpectTotalCount(kClipboardHistoryZeroStateNudgeOpenTime, 0);
  histograms().ExpectTotalCount(kClipboardHistoryZeroStateNudgePasteTime, 0);
  histograms().ExpectTotalCount(kClipboardHistoryScreenshotNotificationOpenTime,
                                0);
  histograms().ExpectTotalCount(
      kClipboardHistoryScreenshotNotificationPasteTime, 0);
}

// Tests that nudge `TimeToAction` metric is logged after the nudge has been
// shown and clipboard history is opened through the accelerator.
TEST_F(ClipboardNudgeControllerTest, TimeToActionMetric) {
  constexpr char kNudgeShownCount[] = "Ash.NotifierFramework.Nudge.ShownCount";
  constexpr char kNudgeTimeToActionWithin1m[] =
      "Ash.NotifierFramework.Nudge.TimeToAction.Within1m";
  constexpr char kNudgeTimeToActionWithin1h[] =
      "Ash.NotifierFramework.Nudge.TimeToAction.Within1h";

  constexpr NudgeCatalogName kOnboardingCatalogName =
      NudgeCatalogName::kClipboardHistoryOnboarding;
  constexpr NudgeCatalogName kZeroStateCatalogName =
      NudgeCatalogName::kClipboardHistoryZeroState;

  ShowNudgeForType(ClipboardNudgeType::kOnboardingNudge);
  task_environment()->FastForwardBy(base::Hours(1));
  ShowNudgeForType(ClipboardNudgeType::kZeroStateNudge);

  // Open clipboard history through the accelerator after one hour of having
  // shown the onboarding nudge, and right after showing the zero state nudge.
  nudge_controller_->OnClipboardHistoryMenuShown(
      /*show_source=*/crosapi::mojom::ClipboardHistoryControllerShowSource::
          kAccelerator);

  histograms().ExpectTotalCount(kNudgeShownCount, 2);
  histograms().ExpectBucketCount(kNudgeTimeToActionWithin1m,
                                 kZeroStateCatalogName, 1);
  histograms().ExpectBucketCount(kNudgeTimeToActionWithin1h,
                                 kOnboardingCatalogName, 1);
}

// A parameterized test base to verify metric recording for each type of nudge.
class ClipboardNudgeMetricTest
    : public ClipboardNudgeControllerTestBase,
      public testing::WithParamInterface<ClipboardNudgeType> {
 public:
  ClipboardNudgeMetricTest() {
    // Enable the clipboard history refresh feature if the duplicate copy nudge
    // is being tested.
    if (GetNudgeType() == ClipboardNudgeType::kDuplicateCopyNudge) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{chromeos::features::kClipboardHistoryRefresh,
                                chromeos::features::kJelly},
          /*disabled_features=*/{});
    }
  }

  ClipboardNudgeType GetNudgeType() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ClipboardNudgeMetricTest,
    /*nudge_type=*/testing::ValuesIn(kAllClipboardNudgeTypes));

// Tests that the metrics are recorded as expected when opening the standalone
// clipboard history menu after a nudge shown.
TEST_P(ClipboardNudgeMetricTest, ShowMenuAfterNudgeShown) {
  const ClipboardNudgeType type_being_tested = GetNudgeType();
  ShowNudgeForType(type_being_tested);

  constexpr int kFastForwardSeconds = 2;
  clock()->Advance(base::Seconds(kFastForwardSeconds));
  nudge_controller_->OnClipboardHistoryMenuShown(/*show_source=*/kAccelerator);

  // Only the metric that records the time delta between the nudge shown and
  // the menu opening for `type_being_tested` should be recorded.
  for (const auto nudge_type : kAllClipboardNudgeTypes) {
    histograms().ExpectBucketCount(GetMenuOpenTimeDeltaHistogram(nudge_type),
                                   kFastForwardSeconds,
                                   nudge_type == type_being_tested ? 1 : 0);
    histograms().ExpectTotalCount(
        GetClipboardHistoryPasteTimeDeltaHistogram(nudge_type), 0);
  }
}

// Tests that the metrics are recorded as expected when pasting the clipboard
// history data after a nudge shown.
TEST_P(ClipboardNudgeMetricTest, PasteAfterNudgeShown) {
  const ClipboardNudgeType type_being_tested = GetNudgeType();
  ShowNudgeForType(type_being_tested);

  constexpr int kFastForwardSeconds = 2;
  clock()->Advance(base::Seconds(kFastForwardSeconds));
  nudge_controller_->OnClipboardHistoryPasted();

  // Only the metric that records the time delta between the nudge shown and the
  // clipboard data paste for `type_being_tested` should be recorded.
  for (const auto nudge_type : kAllClipboardNudgeTypes) {
    histograms().ExpectTotalCount(GetMenuOpenTimeDeltaHistogram(nudge_type), 0);
    histograms().ExpectBucketCount(
        GetClipboardHistoryPasteTimeDeltaHistogram(nudge_type),
        kFastForwardSeconds, nudge_type == type_being_tested ? 1 : 0);
  }
}

// Tests that showing a nudge once allows at most one histogram entry to be
// recorded.
TEST_P(ClipboardNudgeMetricTest, LogOnceForSingleNudgeShown) {
  const ClipboardNudgeType type_being_tested = GetNudgeType();
  ShowNudgeForType(type_being_tested);

  constexpr int kFastForwardSeconds = 2;
  clock()->Advance(base::Seconds(kFastForwardSeconds));
  nudge_controller_->OnClipboardHistoryMenuShown(/*show_source=*/kAccelerator);
  nudge_controller_->OnClipboardHistoryPasted();

  for (const auto nudge_type : kAllClipboardNudgeTypes) {
    histograms().ExpectBucketCount(GetMenuOpenTimeDeltaHistogram(nudge_type),
                                   kFastForwardSeconds,
                                   nudge_type == type_being_tested ? 1 : 0);
    histograms().ExpectBucketCount(
        GetClipboardHistoryPasteTimeDeltaHistogram(nudge_type),
        kFastForwardSeconds, nudge_type == type_being_tested ? 1 : 0);
  }

  nudge_controller_->OnClipboardHistoryMenuShown(/*show_source=*/kAccelerator);
  nudge_controller_->OnClipboardHistoryPasted();

  // The metrics should not be recorded for the second menu shown or clipboard
  // data paste.
  for (const auto nudge_type : kAllClipboardNudgeTypes) {
    histograms().ExpectBucketCount(GetMenuOpenTimeDeltaHistogram(nudge_type),
                                   kFastForwardSeconds,
                                   nudge_type == type_being_tested ? 1 : 0);
    histograms().ExpectBucketCount(
        GetClipboardHistoryPasteTimeDeltaHistogram(nudge_type),
        kFastForwardSeconds, nudge_type == type_being_tested ? 1 : 0);
  }
}

// Tests that new histogram entries can be recorded each time a nudge is shown.
TEST_P(ClipboardNudgeMetricTest, LogTwiceAfterShowingTwice) {
  const ClipboardNudgeType nudge_type = GetNudgeType();
  ShowNudgeForType(nudge_type);

  constexpr int kFirstFastForwardSeconds = 2;
  clock()->Advance(base::Seconds(kFirstFastForwardSeconds));
  nudge_controller_->OnClipboardHistoryMenuShown(/*show_source=*/kAccelerator);
  nudge_controller_->OnClipboardHistoryPasted();

  ShowNudgeForType(nudge_type);

  // Perform menu shown and data paste but with a different fast forward time
  // delta.
  constexpr int kSecondFastForwardSeconds = 5;
  clock()->Advance(base::Seconds(kSecondFastForwardSeconds));
  nudge_controller_->OnClipboardHistoryMenuShown(/*show_source=*/kAccelerator);
  nudge_controller_->OnClipboardHistoryPasted();

  const char* menu_shown_time_delta_histogram =
      GetMenuOpenTimeDeltaHistogram(nudge_type);
  histograms().ExpectTotalCount(menu_shown_time_delta_histogram, 2);
  histograms().ExpectBucketCount(menu_shown_time_delta_histogram,
                                 kFirstFastForwardSeconds, 1);
  histograms().ExpectBucketCount(menu_shown_time_delta_histogram,
                                 kSecondFastForwardSeconds, 1);

  const char* paste_time_delta_histogram =
      GetClipboardHistoryPasteTimeDeltaHistogram(nudge_type);
  histograms().ExpectTotalCount(paste_time_delta_histogram, 2);
  histograms().ExpectBucketCount(paste_time_delta_histogram,
                                 kFirstFastForwardSeconds, 1);
  histograms().ExpectBucketCount(paste_time_delta_histogram,
                                 kSecondFastForwardSeconds, 1);
}

// Tests that showing the standalone clipboard history menu from a source other
// than the accelerator does not log any metrics, because the nudge only
// mentions the accelerator as an option for opening the menu.
TEST_P(ClipboardNudgeMetricTest, NotLogForNonAcceleratorMenuShown) {
  using crosapi::mojom::ClipboardHistoryControllerShowSource;

  const ClipboardNudgeType nudge_type = GetNudgeType();
  ShowNudgeForType(nudge_type);

  for (size_t i =
           static_cast<size_t>(ClipboardHistoryControllerShowSource::kMinValue);
       i < static_cast<size_t>(ClipboardHistoryControllerShowSource::kMaxValue);
       ++i) {
    auto show_source = static_cast<ClipboardHistoryControllerShowSource>(i);
    if (show_source != kAccelerator) {
      nudge_controller_->OnClipboardHistoryMenuShown(show_source);
    }
  }

  histograms().ExpectTotalCount(GetMenuOpenTimeDeltaHistogram(nudge_type), 0);
  histograms().ExpectTotalCount(
      GetClipboardHistoryPasteTimeDeltaHistogram(nudge_type), 0);
}

class ClipboardHistoryRefreshNudgeTest
    : public ClipboardNudgeControllerTestBase,
      public testing::WithParamInterface</*enable_refresh=*/bool> {
 public:
  // Time deltas that are less/more than the minimum time interval required to
  // show a capped nudge.
  static constexpr base::TimeDelta kLessThanMinInterval =
      kCappedNudgeMinInterval - base::Hours(1);
  static constexpr base::TimeDelta kMoreThanMinInterval =
      kCappedNudgeMinInterval + base::Hours(1);

  ClipboardHistoryRefreshNudgeTest() {
    std::vector<base::test::FeatureRef> refresh_features = {
        chromeos::features::kClipboardHistoryRefresh,
        chromeos::features::kJelly};

    if (IsClipboardHistoryRefreshEnabled()) {
      scoped_feature_list_.InitWithFeatures(refresh_features, {});
    } else {
      scoped_feature_list_.InitWithFeatures({}, refresh_features);
    }
  }

  void HideClipboardNudge() {
    AnchoredNudgeManager::Get()->Cancel(kClipboardNudgeId);
  }

  bool IsClipboardHistoryRefreshEnabled() const { return GetParam(); }

  void WaitForClipboardWriteConfirmed() {
    EXPECT_TRUE(operation_confirmed_future_.Take());
  }

 private:
  // ClipboardNudgeControllerTestBase:
  void SetUp() override {
    ClipboardNudgeControllerTestBase::SetUp();
    Shell::Get()
        ->clipboard_history_controller()
        ->set_confirmed_operation_callback_for_test(
            operation_confirmed_future_.GetRepeatingCallback());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TestFuture<bool> operation_confirmed_future_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ClipboardHistoryRefreshNudgeTest,
                         /*enable_refresh=*/testing::Bool());

// Verifies that the duplicate copy nudge is able to show after the onboarding
// nudge hits the shown count limit.
TEST_P(ClipboardHistoryRefreshNudgeTest,
       DuplicateCopyAfterOnboardingNudgesHitLimit) {
  // Show the onboarding nudge three times.
  base::HistogramTester histogram_tester;
  ShowNudgeForType(kOnboardingNudge);
  clock()->Advance(kMoreThanMinInterval);

  ShowNudgeForType(kOnboardingNudge);
  clock()->Advance(kMoreThanMinInterval);

  ShowNudgeForType(kOnboardingNudge);
  clock()->Advance(kMoreThanMinInterval);
  HideClipboardNudge();
  histogram_tester.ExpectUniqueSample(kClipboardHistoryOnboardingNudgeShowCount,
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/3);

  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste).WriteText(u"text");
  WaitForClipboardWriteConfirmed();

  // Write duplicate text to clipboard history.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste).WriteText(u"text");
  WaitForClipboardWriteConfirmed();

  // Check the existence of the system nudge.
  EXPECT_EQ(IsClipboardNudgeShown(), IsClipboardHistoryRefreshEnabled());

  // Check the show count of the clipboard history duplicate copy nudge.
  histogram_tester.ExpectUniqueSample(
      kClipboardHistoryDuplicateCopyNudgeShowCount,
      /*sample=*/true,
      /*expected_bucket_count=*/IsClipboardHistoryRefreshEnabled() ? 1 : 0);
}

// Verifies the show count limit of the duplicate copy nudge.
TEST_P(ClipboardHistoryRefreshNudgeTest, DuplicateCopyShowCountLimit) {
  base::HistogramTester histogram_tester;
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste).WriteText(u"text");
  WaitForClipboardWriteConfirmed();

  // Write duplicate text to clipboard history.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste).WriteText(u"text");
  WaitForClipboardWriteConfirmed();

  // Write duplicate text to clipboard history for the second time. Advance by
  // enough time that a capped nudge is able to show.
  clock()->Advance(kMoreThanMinInterval);
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste).WriteText(u"text");
  WaitForClipboardWriteConfirmed();

  // Write duplicate text to clipboard history for the third time.
  clock()->Advance(kMoreThanMinInterval);
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste).WriteText(u"text");
  WaitForClipboardWriteConfirmed();

  // Check the show count of the clipboard history duplicate copy nudge.
  histogram_tester.ExpectUniqueSample(
      kClipboardHistoryDuplicateCopyNudgeShowCount,
      /*sample=*/true,
      /*expected_bucket_count=*/IsClipboardHistoryRefreshEnabled() ? 3 : 0);

  // Write duplicate text to clipboard history for the forth time.
  HideClipboardNudge();
  clock()->Advance(kMoreThanMinInterval);
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste).WriteText(u"text");
  WaitForClipboardWriteConfirmed();

  // Check that the duplicate copy nudge does NOT show if the clipboard history
  // refresh feature is enabled. Because the show count has hit the limit.
  EXPECT_FALSE(IsClipboardNudgeShown());
  histogram_tester.ExpectUniqueSample(
      kClipboardHistoryDuplicateCopyNudgeShowCount,
      /*sample=*/true,
      /*expected_bucket_count=*/IsClipboardHistoryRefreshEnabled() ? 3 : 0);
}

// Ensure that the duplicate copy nudge is not shown too soon after the previous
// showing.
TEST_P(ClipboardHistoryRefreshNudgeTest, DuplicateCopyTimeIntervalThreshold) {
  base::HistogramTester histogram_tester;
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste).WriteText(u"text");
  WaitForClipboardWriteConfirmed();

  // Write duplicate text to clipboard history.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste).WriteText(u"text");
  WaitForClipboardWriteConfirmed();
  histogram_tester.ExpectUniqueSample(
      kClipboardHistoryDuplicateCopyNudgeShowCount,
      /*sample=*/true,
      /*expected_bucket_count=*/IsClipboardHistoryRefreshEnabled() ? 1 : 0);
  HideClipboardNudge();

  // Advance the time with an interval less than the threshold.
  clock()->Advance(kLessThanMinInterval);

  // Write duplicate text to clipboard history again.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste).WriteText(u"text");
  WaitForClipboardWriteConfirmed();

  // Check that the duplicate copy nudge does NOT show.
  EXPECT_FALSE(IsClipboardNudgeShown());

  // Check the show count of the clipboard history duplicate copy nudge.
  histogram_tester.ExpectUniqueSample(
      kClipboardHistoryDuplicateCopyNudgeShowCount,
      /*sample=*/true,
      /*expected_bucket_count=*/IsClipboardHistoryRefreshEnabled() ? 1 : 0);
}

// Checks that showing onboarding nudges after duplicate copy works as expected.
TEST_P(ClipboardHistoryRefreshNudgeTest,
       ShowOnboardingNudgeAfterDuplicateCopy) {
  base::HistogramTester histogram_tester;
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste).WriteText(u"text");
  WaitForClipboardWriteConfirmed();

  // Write duplicate text to clipboard history.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste).WriteText(u"text");
  WaitForClipboardWriteConfirmed();
  histogram_tester.ExpectUniqueSample(
      kClipboardHistoryDuplicateCopyNudgeShowCount,
      /*sample=*/true,
      /*expected_bucket_count=*/IsClipboardHistoryRefreshEnabled() ? 1 : 0);
  histogram_tester.ExpectUniqueSample(kClipboardHistoryOnboardingNudgeShowCount,
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/0);
  HideClipboardNudge();

  // Advance the time with an interval less than the threshold.
  clock()->Advance(kLessThanMinInterval);

  // Trigger the onboarding nudge by subsequent copies and pastes.
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  nudge_controller_->OnClipboardDataRead();
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  nudge_controller_->OnClipboardDataRead();

  // The onboarding nudge should NOT show if the duplicate copy nudge showed
  // before due to the time interval threshold.
  histogram_tester.ExpectUniqueSample(
      kClipboardHistoryOnboardingNudgeShowCount,
      /*sample=*/true,
      /*expected_bucket_count=*/IsClipboardHistoryRefreshEnabled() ? 0 : 1);
}

}  // namespace ash
