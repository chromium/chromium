// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_nudge_controller.h"

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_nudge.h"
#include "ash/clipboard/clipboard_nudge_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {
namespace {

class NudgeWigetObserver : public views::WidgetObserver {
 public:
  NudgeWigetObserver(views::Widget* widget) : widget_(widget) {
    if (widget_)
      widget_->AddObserver(this);
  }

  ~NudgeWigetObserver() override { CleanupWidget(); }

  bool WidgetClosed() const { return !widget_; }

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override { CleanupWidget(); }

  void CleanupWidget() {
    if (widget_) {
      widget_->RemoveObserver(this);
      widget_ = nullptr;
    }
  }

 private:
  views::Widget* widget_;
};
}  // namespace

class ClipboardNudgeControllerTest : public AshTestBase {
 public:
  ClipboardNudgeControllerTest() = default;
  ClipboardNudgeControllerTest(const ClipboardNudgeControllerTest&) = delete;
  ClipboardNudgeControllerTest& operator=(const ClipboardNudgeControllerTest&) =
      delete;
  ~ClipboardNudgeControllerTest() override = default;

  base::SimpleTestClock* clock() { return &test_clock_; }

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kClipboardHistory,
         chromeos::features::kClipboardHistoryContextMenuNudge},
        {});
    AshTestBase::SetUp();
    nudge_controller_ =
        Shell::Get()->clipboard_history_controller()->nudge_controller();
    nudge_controller_->OverrideClockForTesting(&test_clock_);
    test_clock_.Advance(base::TimeDelta::FromSeconds(360));
  }

  void TearDown() override {
    nudge_controller_->ClearClockOverrideForTesting();
    AshTestBase::TearDown();
  }

  // Owned by ClipboardHistoryController.
  ClipboardNudgeController* nudge_controller_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::SimpleTestClock test_clock_;
};

// Checks that clipboard state advances after the nudge controller is fed the
// right event type.
TEST_F(ClipboardNudgeControllerTest, ShouldShowNudgeAfterCorrectSequence) {
  EXPECT_EQ(ClipboardState::kInit,
            nudge_controller_->GetClipboardStateForTesting());
  // Checks that the first copy advances state as expected.
  nudge_controller_->OnClipboardHistoryItemAdded(
      ClipboardHistoryItem(ui::ClipboardData()));
  EXPECT_EQ(ClipboardState::kFirstCopy,
            nudge_controller_->GetClipboardStateForTesting());

  // Checks that the first paste advances state as expected.
  nudge_controller_->OnClipboardDataRead();
  EXPECT_EQ(ClipboardState::kFirstPaste,
            nudge_controller_->GetClipboardStateForTesting());

  // Checks that the second copy advances state as expected.
  nudge_controller_->OnClipboardHistoryItemAdded(
      ClipboardHistoryItem(ui::ClipboardData()));
  EXPECT_EQ(ClipboardState::kSecondCopy,
            nudge_controller_->GetClipboardStateForTesting());

  // Check that clipbaord nudge has not yet been created.
  EXPECT_FALSE(nudge_controller_->GetClipboardNudgeForTesting());

  // Checks that the second paste resets state as expected.
  nudge_controller_->OnClipboardDataRead();
  EXPECT_EQ(ClipboardState::kInit,
            nudge_controller_->GetClipboardStateForTesting());

  // Check that clipbaord nudge has been created.
  EXPECT_TRUE(nudge_controller_->GetClipboardNudgeForTesting());
}

// Checks that the clipboard state does not advace if too much time passes
// during the copy paste sequence.
TEST_F(ClipboardNudgeControllerTest, NudgeTimeOut) {
  // Perform copy -> paste -> copy sequence.
  nudge_controller_->OnClipboardHistoryItemAdded(
      ClipboardHistoryItem(ui::ClipboardData()));
  nudge_controller_->OnClipboardDataRead();
  nudge_controller_->OnClipboardHistoryItemAdded(
      ClipboardHistoryItem(ui::ClipboardData()));

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
  nudge_controller_->OnClipboardHistoryItemAdded(
      ClipboardHistoryItem(ui::ClipboardData()));
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

  // Check that clipbaord nudge has not yet been created.
  EXPECT_FALSE(nudge_controller_->GetClipboardNudgeForTesting());

  // Check that HandleClipboardChanged() will advance nudge_controller's
  // ClipboardState.
  nudge_controller_->OnClipboardHistoryItemAdded(
      ClipboardHistoryItem(ui::ClipboardData()));
  EXPECT_EQ(ClipboardState::kSecondCopy,
            nudge_controller_->GetClipboardStateForTesting());
  nudge_controller_->OnClipboardDataRead();
  EXPECT_EQ(ClipboardState::kInit,
            nudge_controller_->GetClipboardStateForTesting());

  // Check that clipbaord nudge has been created.
  EXPECT_TRUE(nudge_controller_->GetClipboardNudgeForTesting());
}

// Checks that consecutive copy events does not advance the clipboard state.
TEST_F(ClipboardNudgeControllerTest, RepeatedCopyDoesNotAdvanceState) {
  nudge_controller_->OnClipboardHistoryItemAdded(
      ClipboardHistoryItem(ui::ClipboardData()));
  EXPECT_EQ(ClipboardState::kFirstCopy,
            nudge_controller_->GetClipboardStateForTesting());
  nudge_controller_->OnClipboardHistoryItemAdded(
      ClipboardHistoryItem(ui::ClipboardData()));
  EXPECT_EQ(ClipboardState::kFirstCopy,
            nudge_controller_->GetClipboardStateForTesting());
}

// Checks that consecutive paste events does not advance the clipboard state.
TEST_F(ClipboardNudgeControllerTest, RepeatedPasteDoesNotAdvanceState) {
  nudge_controller_->OnClipboardHistoryItemAdded(
      ClipboardHistoryItem(ui::ClipboardData()));
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
  nudge_controller_->OnClipboardHistoryItemAdded(
      ClipboardHistoryItem(ui::ClipboardData()));
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

// Verifies that the context menu new feature badge for the clipboard option
// only shows |kContextMenuBadgeShowLimit| times.
TEST_F(ClipboardNudgeControllerTest, ShowNewFeatureNudge) {
  ASSERT_TRUE(nudge_controller_->ShouldShowNewFeatureBadge());

  // Mark the badge shown |kContextMenuBadgeShowLimit| - 1 times.
  // Should expect to keep showing the badge
  for (int i = 0; i < kContextMenuBadgeShowLimit - 1; ++i) {
    nudge_controller_->MarkNewFeatureBadgeShown();
    EXPECT_TRUE(nudge_controller_->ShouldShowNewFeatureBadge());
  }
  // Marking the badge as shown |kContextMenuBadgeShowLimit| times should not
  // expect to show the badge the next time.
  nudge_controller_->MarkNewFeatureBadgeShown();
  EXPECT_FALSE(nudge_controller_->ShouldShowNewFeatureBadge());
}

// Verifies that controller cleans up and closes an old nudge before displaying
// another one.
TEST_F(ClipboardNudgeControllerTest, ShowZeroStateNudgeAfterOngoingNudge) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  nudge_controller_->ShowNudge(ClipboardNudgeType::kOnboardingNudge);

  ClipboardNudge* nudge = nudge_controller_->GetClipboardNudgeForTesting();
  views::Widget* nudge_widget = nudge->widget();
  ASSERT_TRUE(nudge_widget->GetLayer()->GetAnimator()->is_animating());
  EXPECT_FALSE(nudge_widget->IsClosed());
  EXPECT_EQ(ClipboardNudgeType::kOnboardingNudge, nudge->nudge_type());

  NudgeWigetObserver widget_close_observer(nudge_widget);

  nudge_controller_->ShowNudge(ClipboardNudgeType::kZeroStateNudge);
  // Verify the old nudge widget was closed.
  EXPECT_TRUE(widget_close_observer.WidgetClosed());

  nudge = nudge_controller_->GetClipboardNudgeForTesting();
  EXPECT_FALSE(nudge->widget()->IsClosed());
  EXPECT_EQ(ClipboardNudgeType::kZeroStateNudge, nudge->nudge_type());
}

// Verifies that the nudge cleans up properly during shutdown while it is
// animating to hide.
TEST_F(ClipboardNudgeControllerTest, NudgeClosingDuringShutdown) {
  nudge_controller_->ShowNudge(ClipboardNudgeType::kOnboardingNudge);

  ClipboardNudge* nudge = nudge_controller_->GetClipboardNudgeForTesting();
  views::Widget* nudge_widget = nudge->widget();
  EXPECT_FALSE(nudge_widget->IsClosed());
  EXPECT_EQ(ClipboardNudgeType::kOnboardingNudge, nudge->nudge_type());

  // Slow down the duration of the nudge
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

  nudge_controller_->FireHideNudgeTimerForTesting();
  ASSERT_TRUE(nudge_widget->GetLayer()->GetAnimator()->is_animating());
}

}  // namespace ash
