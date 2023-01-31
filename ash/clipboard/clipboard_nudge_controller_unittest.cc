// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_nudge_controller.h"

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_nudge.h"
#include "ash/clipboard/clipboard_nudge_constants.h"
#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

namespace {

using crosapi::mojom::ClipboardHistoryControllerShowSource::kAccelerator;

}  // namespace

class ClipboardNudgeControllerTest : public AshTestBase {
 public:
  ClipboardNudgeControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ClipboardNudgeControllerTest(const ClipboardNudgeControllerTest&) = delete;
  ClipboardNudgeControllerTest& operator=(const ClipboardNudgeControllerTest&) =
      delete;
  ~ClipboardNudgeControllerTest() override = default;

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

  // Owned by ClipboardHistoryController.
  ClipboardNudgeController* nudge_controller_;

  void ShowNudgeForType(ClipboardNudgeType nudge_type) {
    switch (nudge_type) {
      case ClipboardNudgeType::kOnboardingNudge:
      case ClipboardNudgeType::kZeroStateNudge:
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

  base::HistogramTester& histograms() { return histograms_; }

 private:
  base::SimpleTestClock test_clock_;
  // Histogram value verifier.
  base::HistogramTester histograms_;
};

// Checks that clipboard state advances after the nudge controller is fed the
// right event type.
TEST_F(ClipboardNudgeControllerTest, ShouldShowNudgeAfterCorrectSequence) {
  EXPECT_EQ(ClipboardState::kInit,
            nudge_controller_->GetClipboardStateForTesting());
  // Checks that the first copy advances state as expected.
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  EXPECT_EQ(ClipboardState::kFirstCopy,
            nudge_controller_->GetClipboardStateForTesting());

  // Checks that the first paste advances state as expected.
  nudge_controller_->OnClipboardDataRead();
  EXPECT_EQ(ClipboardState::kFirstPaste,
            nudge_controller_->GetClipboardStateForTesting());

  // Checks that the second copy advances state as expected.
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  EXPECT_EQ(ClipboardState::kSecondCopy,
            nudge_controller_->GetClipboardStateForTesting());

  // Check that clipboard nudge has not yet been created.
  EXPECT_FALSE(nudge_controller_->GetSystemNudgeForTesting());

  // Checks that the second paste resets state as expected.
  nudge_controller_->OnClipboardDataRead();
  EXPECT_EQ(ClipboardState::kInit,
            nudge_controller_->GetClipboardStateForTesting());

  // Check that clipboard nudge has been created.
  EXPECT_TRUE(nudge_controller_->GetSystemNudgeForTesting());
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

  // Paste event should reset clipboard state to |kFirstPaste| instead of to
  // |kShouldShowNudge|.
  EXPECT_NE(ClipboardState::kShouldShowNudge,
            nudge_controller_->GetClipboardStateForTesting());
  EXPECT_EQ(ClipboardState::kFirstPaste,
            nudge_controller_->GetClipboardStateForTesting());
}

// Checks that multiple pastes refreshes the |kMaxTimeBetweenPaste| timer that
// determines whether too much time has passed to show the nudge.
TEST_F(ClipboardNudgeControllerTest, NudgeDoesNotTimeOutWithSparsePastes) {
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  nudge_controller_->OnClipboardDataRead();
  EXPECT_EQ(ClipboardState::kFirstPaste,
            nudge_controller_->GetClipboardStateForTesting());

  // Perform 5 pastes over 2.5*|kMaxTimeBetweenPaste|.
  for (int paste_cycle = 0; paste_cycle < 5; paste_cycle++) {
    SCOPED_TRACE("paste cycle " + base::NumberToString(paste_cycle));
    clock()->Advance(kMaxTimeBetweenPaste / 2);
    nudge_controller_->OnClipboardDataRead();
    EXPECT_EQ(ClipboardState::kFirstPaste,
              nudge_controller_->GetClipboardStateForTesting());
  }

  // Check that clipboard nudge has not yet been created.
  EXPECT_FALSE(nudge_controller_->GetSystemNudgeForTesting());

  // Check that HandleClipboardChanged() will advance nudge_controller's
  // ClipboardState.
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  EXPECT_EQ(ClipboardState::kSecondCopy,
            nudge_controller_->GetClipboardStateForTesting());
  nudge_controller_->OnClipboardDataRead();
  EXPECT_EQ(ClipboardState::kInit,
            nudge_controller_->GetClipboardStateForTesting());

  // Check that clipboard nudge has been created.
  EXPECT_TRUE(nudge_controller_->GetSystemNudgeForTesting());
}

// Checks that consecutive copy events does not advance the clipboard state.
TEST_F(ClipboardNudgeControllerTest, RepeatedCopyDoesNotAdvanceState) {
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  EXPECT_EQ(ClipboardState::kFirstCopy,
            nudge_controller_->GetClipboardStateForTesting());
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  EXPECT_EQ(ClipboardState::kFirstCopy,
            nudge_controller_->GetClipboardStateForTesting());
}

// Checks that consecutive paste events does not advance the clipboard state.
TEST_F(ClipboardNudgeControllerTest, RepeatedPasteDoesNotAdvanceState) {
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  EXPECT_EQ(ClipboardState::kFirstCopy,
            nudge_controller_->GetClipboardStateForTesting());
  nudge_controller_->OnClipboardDataRead();
  EXPECT_EQ(ClipboardState::kFirstPaste,
            nudge_controller_->GetClipboardStateForTesting());
  nudge_controller_->OnClipboardDataRead();
  EXPECT_EQ(ClipboardState::kFirstPaste,
            nudge_controller_->GetClipboardStateForTesting());
}

// Verifies that administrative events does not advance clipboard state.
TEST_F(ClipboardNudgeControllerTest, AdminWriteDoesNotAdvanceState) {
  nudge_controller_->OnClipboardHistoryItemAdded(CreateItem());
  nudge_controller_->OnClipboardDataRead();
  EXPECT_EQ(ClipboardState::kFirstPaste,
            nudge_controller_->GetClipboardStateForTesting());

  auto data = std::make_unique<ui::ClipboardData>();
  data->set_text("test");
  // Write the data to the clipboard, clipboard state should not advance.
  ui::ClipboardNonBacked::GetForCurrentThread()->WriteClipboardData(
      std::move(data));
  EXPECT_EQ(ClipboardState::kFirstPaste,
            nudge_controller_->GetClipboardStateForTesting());
}

// Verifies that controller cleans up and closes an old nudge before displaying
// another one.
TEST_F(ClipboardNudgeControllerTest, ShowZeroStateNudgeAfterOngoingNudge) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  ShowNudgeForType(ClipboardNudgeType::kOnboardingNudge);

  ClipboardNudge* nudge = static_cast<ClipboardNudge*>(
      nudge_controller_->GetSystemNudgeForTesting());
  views::Widget* nudge_widget = nudge->widget();
  ASSERT_TRUE(nudge_widget->GetLayer()->GetAnimator()->is_animating());
  EXPECT_FALSE(nudge_widget->IsClosed());
  EXPECT_EQ(ClipboardNudgeType::kOnboardingNudge, nudge->nudge_type());

  ShowNudgeForType(ClipboardNudgeType::kZeroStateNudge);
  // Verify the old nudge widget was closed.
  EXPECT_TRUE(nudge_widget->IsClosed());

  nudge = static_cast<ClipboardNudge*>(
      nudge_controller_->GetSystemNudgeForTesting());
  EXPECT_FALSE(nudge->widget()->IsClosed());
  EXPECT_EQ(ClipboardNudgeType::kZeroStateNudge, nudge->nudge_type());
}

// Verifies that the nudge cleans up properly during shutdown while it is
// animating to hide.
TEST_F(ClipboardNudgeControllerTest, NudgeClosingDuringShutdown) {
  ShowNudgeForType(ClipboardNudgeType::kOnboardingNudge);

  ClipboardNudge* nudge = static_cast<ClipboardNudge*>(
      nudge_controller_->GetSystemNudgeForTesting());
  views::Widget* nudge_widget = nudge->widget();
  EXPECT_FALSE(nudge_widget->IsClosed());
  EXPECT_EQ(ClipboardNudgeType::kOnboardingNudge, nudge->nudge_type());

  // Slow down the duration of the nudge
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

  nudge_controller_->FireHideNudgeTimerForTesting();
  ASSERT_TRUE(nudge_widget->GetLayer()->GetAnimator()->is_animating());
}

// Asserts that all nudge metric related histograms start at 0.
TEST_F(ClipboardNudgeControllerTest, NudgeMetrics_StartAtZero) {
  histograms().ExpectTotalCount(kOnboardingNudge_OpenTime, 0);
  histograms().ExpectTotalCount(kOnboardingNudge_PasteTime, 0);
  histograms().ExpectTotalCount(kZeroStateNudge_OpenTime, 0);
  histograms().ExpectTotalCount(kZeroStateNudge_PasteTime, 0);
  histograms().ExpectTotalCount(kScreenshotNotification_OpenTime, 0);
  histograms().ExpectTotalCount(kScreenshotNotification_PasteTime, 0);
}

// Tests that opening the clipboard history after showing the nudges logs only
// the metrics for the open time histograms. For this test, we only verify the
// number of emits, but not the timing mechanism.
TEST_F(ClipboardNudgeControllerTest, ShowMenuAfterNudges_LogsOpenNudgeMetrics) {
  ShowNudgeForType(ClipboardNudgeType::kOnboardingNudge);
  ShowNudgeForType(ClipboardNudgeType::kZeroStateNudge);
  ShowNudgeForType(ClipboardNudgeType::kScreenshotNotificationNudge);
  nudge_controller_->OnClipboardHistoryMenuShown(/*show_source=*/kAccelerator);

  histograms().ExpectTotalCount(kOnboardingNudge_OpenTime, 1);
  histograms().ExpectTotalCount(kZeroStateNudge_OpenTime, 1);
  histograms().ExpectTotalCount(kScreenshotNotification_OpenTime, 1);
  histograms().ExpectTotalCount(kOnboardingNudge_PasteTime, 0);
  histograms().ExpectTotalCount(kZeroStateNudge_PasteTime, 0);
  histograms().ExpectTotalCount(kScreenshotNotification_PasteTime, 0);
}

// Tests that pasting something from the clipboard history after showing the
// nudges only the metrics for the paste time histograms. For this test, we only
// verify the number of emits, but not the timing mechanism.
TEST_F(ClipboardNudgeControllerTest, PasteAfterNudges_LogsPasteNudgeMetrics) {
  ShowNudgeForType(ClipboardNudgeType::kOnboardingNudge);
  ShowNudgeForType(ClipboardNudgeType::kZeroStateNudge);
  ShowNudgeForType(ClipboardNudgeType::kScreenshotNotificationNudge);
  nudge_controller_->OnClipboardHistoryMenuShown(/*show_source=*/kAccelerator);
  nudge_controller_->OnClipboardHistoryPasted();

  histograms().ExpectTotalCount(kOnboardingNudge_OpenTime, 1);
  histograms().ExpectTotalCount(kZeroStateNudge_OpenTime, 1);
  histograms().ExpectTotalCount(kScreenshotNotification_OpenTime, 1);
  histograms().ExpectTotalCount(kOnboardingNudge_PasteTime, 1);
  histograms().ExpectTotalCount(kZeroStateNudge_PasteTime, 1);
  histograms().ExpectTotalCount(kScreenshotNotification_PasteTime, 1);
}

// Tests that the onboarding nudge being shown only logs the metrics for the
// onboarding nudge histograms.
TEST_F(ClipboardNudgeControllerTest, OnboardingNudge_DoesNotLogOtherMetrics) {
  ShowNudgeForType(ClipboardNudgeType::kOnboardingNudge);
  nudge_controller_->OnClipboardHistoryMenuShown(/*show_source=*/kAccelerator);
  nudge_controller_->OnClipboardHistoryPasted();

  histograms().ExpectTotalCount(kOnboardingNudge_OpenTime, 1);
  histograms().ExpectTotalCount(kZeroStateNudge_OpenTime, 0);
  histograms().ExpectTotalCount(kScreenshotNotification_OpenTime, 0);
  histograms().ExpectTotalCount(kOnboardingNudge_PasteTime, 1);
  histograms().ExpectTotalCount(kZeroStateNudge_PasteTime, 0);
  histograms().ExpectTotalCount(kScreenshotNotification_PasteTime, 0);
}

// Tests that the zero state nudge being shown only logs the metrics for the
// zero state nudge histograms.
TEST_F(ClipboardNudgeControllerTest, ZeroStateNudge_DoesNotLogOtherMetrics) {
  ShowNudgeForType(ClipboardNudgeType::kZeroStateNudge);
  nudge_controller_->OnClipboardHistoryMenuShown(/*show_source=*/kAccelerator);
  nudge_controller_->OnClipboardHistoryPasted();

  histograms().ExpectTotalCount(kOnboardingNudge_OpenTime, 0);
  histograms().ExpectTotalCount(kZeroStateNudge_OpenTime, 1);
  histograms().ExpectTotalCount(kScreenshotNotification_OpenTime, 0);
  histograms().ExpectTotalCount(kOnboardingNudge_PasteTime, 0);
  histograms().ExpectTotalCount(kZeroStateNudge_PasteTime, 1);
  histograms().ExpectTotalCount(kScreenshotNotification_PasteTime, 0);
}

// Tests that the screenshot notification nudge being shown only logs the
// metrics for the screenshot notification nudge histograms.
TEST_F(ClipboardNudgeControllerTest,
       ScreenshotNotification_DoesNotLogOtherMetrics) {
  ShowNudgeForType(ClipboardNudgeType::kScreenshotNotificationNudge);
  nudge_controller_->OnClipboardHistoryMenuShown(/*show_source=*/kAccelerator);
  nudge_controller_->OnClipboardHistoryPasted();

  histograms().ExpectTotalCount(kOnboardingNudge_OpenTime, 0);
  histograms().ExpectTotalCount(kZeroStateNudge_OpenTime, 0);
  histograms().ExpectTotalCount(kScreenshotNotification_OpenTime, 1);
  histograms().ExpectTotalCount(kOnboardingNudge_PasteTime, 0);
  histograms().ExpectTotalCount(kZeroStateNudge_PasteTime, 0);
  histograms().ExpectTotalCount(kScreenshotNotification_PasteTime, 1);
}

// Tests that nudge metrics will not log multiple times if the nudges are not
// shown before.
TEST_F(ClipboardNudgeControllerTest, SecondTimeAction_DoesNotLogNudgeMetrics) {
  ShowNudgeForType(ClipboardNudgeType::kOnboardingNudge);
  ShowNudgeForType(ClipboardNudgeType::kZeroStateNudge);
  ShowNudgeForType(ClipboardNudgeType::kScreenshotNotificationNudge);
  nudge_controller_->OnClipboardHistoryMenuShown(/*show_source=*/kAccelerator);
  nudge_controller_->OnClipboardHistoryPasted();
  nudge_controller_->OnClipboardHistoryMenuShown(/*show_source=*/kAccelerator);
  nudge_controller_->OnClipboardHistoryPasted();

  histograms().ExpectTotalCount(kOnboardingNudge_OpenTime, 1);
  histograms().ExpectTotalCount(kZeroStateNudge_OpenTime, 1);
  histograms().ExpectTotalCount(kScreenshotNotification_OpenTime, 1);
  histograms().ExpectTotalCount(kOnboardingNudge_PasteTime, 1);
  histograms().ExpectTotalCount(kZeroStateNudge_PasteTime, 1);
  histograms().ExpectTotalCount(kScreenshotNotification_PasteTime, 1);
}

// Tests that nudge metrics can log more times as the nudges are shown before.
TEST_F(ClipboardNudgeControllerTest, ShowNudgeTwice_LogsMetricsTwoTimes) {
  ShowNudgeForType(ClipboardNudgeType::kOnboardingNudge);
  ShowNudgeForType(ClipboardNudgeType::kZeroStateNudge);
  ShowNudgeForType(ClipboardNudgeType::kScreenshotNotificationNudge);
  nudge_controller_->OnClipboardHistoryMenuShown(/*show_source=*/kAccelerator);
  nudge_controller_->OnClipboardHistoryPasted();
  ShowNudgeForType(ClipboardNudgeType::kOnboardingNudge);
  ShowNudgeForType(ClipboardNudgeType::kZeroStateNudge);
  ShowNudgeForType(ClipboardNudgeType::kScreenshotNotificationNudge);
  nudge_controller_->OnClipboardHistoryMenuShown(/*show_source=*/kAccelerator);
  nudge_controller_->OnClipboardHistoryPasted();

  histograms().ExpectTotalCount(kOnboardingNudge_OpenTime, 2);
  histograms().ExpectTotalCount(kZeroStateNudge_OpenTime, 2);
  histograms().ExpectTotalCount(kScreenshotNotification_OpenTime, 2);
  histograms().ExpectTotalCount(kOnboardingNudge_PasteTime, 2);
  histograms().ExpectTotalCount(kZeroStateNudge_PasteTime, 2);
  histograms().ExpectTotalCount(kScreenshotNotification_PasteTime, 2);
}

// Tests that showing clipboard history from a source other than the accelerator
// does not log any metrics, because the nudge only mentions the accelerator as
// an option for opening the menu.
TEST_F(ClipboardNudgeControllerTest,
       NonAcceleratorShowMenuAfterNudges_DoesNotLogNudgeMetrics) {
  using crosapi::mojom::ClipboardHistoryControllerShowSource;

  ShowNudgeForType(ClipboardNudgeType::kOnboardingNudge);
  ShowNudgeForType(ClipboardNudgeType::kZeroStateNudge);
  ShowNudgeForType(ClipboardNudgeType::kScreenshotNotificationNudge);
  for (size_t i =
           static_cast<size_t>(ClipboardHistoryControllerShowSource::kMinValue);
       i < static_cast<size_t>(ClipboardHistoryControllerShowSource::kMaxValue);
       ++i) {
    auto show_source = static_cast<ClipboardHistoryControllerShowSource>(i);
    if (show_source != kAccelerator) {
      nudge_controller_->OnClipboardHistoryMenuShown(show_source);
    }
  }

  histograms().ExpectTotalCount(kOnboardingNudge_OpenTime, 0);
  histograms().ExpectTotalCount(kZeroStateNudge_OpenTime, 0);
  histograms().ExpectTotalCount(kScreenshotNotification_OpenTime, 0);
  histograms().ExpectTotalCount(kOnboardingNudge_PasteTime, 0);
  histograms().ExpectTotalCount(kZeroStateNudge_PasteTime, 0);
  histograms().ExpectTotalCount(kScreenshotNotification_PasteTime, 0);
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

}  // namespace ash
