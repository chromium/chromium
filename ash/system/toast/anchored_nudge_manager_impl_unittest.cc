// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/anchored_nudge_manager_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/scoped_anchored_nudge_pause.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/style/system_toast_style.h"
#include "ash/system/toast/anchored_nudge.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/i18n/rtl.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr NudgeCatalogName kTestCatalogName =
    NudgeCatalogName::kTestCatalogName;

constexpr char kFirstButtonPressed[] =
    "Ash.NotifierFramework.Nudge.FirstButtonPressed";
constexpr char kSecondButtonPressed[] =
    "Ash.NotifierFramework.Nudge.SecondButtonPressed";
constexpr char kNudgeShownCount[] = "Ash.NotifierFramework.Nudge.ShownCount";
constexpr char kNudgeTimeToActionWithin1m[] =
    "Ash.NotifierFramework.Nudge.TimeToAction.Within1m";
constexpr char kNudgeTimeToActionWithin1h[] =
    "Ash.NotifierFramework.Nudge.TimeToAction.Within1h";
constexpr char kNudgeTimeToActionWithinSession[] =
    "Ash.NotifierFramework.Nudge.TimeToAction.WithinSession";

void SetLockedState(bool locked) {
  SessionInfo info;
  info.state = locked ? session_manager::SessionState::LOCKED
                      : session_manager::SessionState::ACTIVE;
  Shell::Get()->session_controller()->SetSessionInfo(info);
}

}  // namespace

class AnchoredNudgeManagerImplTest : public AshTestBase {
 public:
  AnchoredNudgeManagerImplTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(features::kSystemNudgeV2);
  }

  AnchoredNudgeManagerImpl* anchored_nudge_manager() {
    return Shell::Get()->anchored_nudge_manager();
  }

  // Creates an `AnchoredNudgeData` object with only the required elements.
  AnchoredNudgeData CreateBaseNudgeData(
      const std::string& id,
      views::View* anchor_view,
      const std::u16string& body_text = std::u16string()) {
    return AnchoredNudgeData(id, kTestCatalogName, body_text, anchor_view);
  }

  void CancelNudge(const std::string& id) {
    anchored_nudge_manager()->Cancel(id);
  }

  std::map<std::string, raw_ptr<AnchoredNudge>> GetShownNudges() {
    return anchored_nudge_manager()->shown_nudges_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a nudge can be shown and its contents are properly sent.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_SingleNudge) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  const std::u16string text = u"text";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view, text);

  // Show a nudge.
  anchored_nudge_manager()->Show(nudge_data);

  // Ensure the nudge is visible and has set the provided contents.
  AnchoredNudge* nudge = GetShownNudges()[id];
  ASSERT_TRUE(nudge);
  EXPECT_TRUE(nudge->GetVisible());
  EXPECT_EQ(text, nudge->GetBodyText());
  EXPECT_EQ(anchor_view, nudge->GetAnchorView());

  // Ensure the nudge widget was not activated when shown.
  EXPECT_FALSE(nudge->GetWidget()->IsActive());

  // Cancel the nudge, expect it to be removed from the shown nudges map.
  CancelNudge(id);
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that two nudges can be shown on screen at the same time.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_TwoNudges) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  auto* contents_view =
      widget->SetContentsView(std::make_unique<views::View>());

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  const std::string id_2 = "id_2";
  auto* anchor_view_2 =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data_2 = CreateBaseNudgeData(id_2, anchor_view_2);

  // Show the first nudge, expect the first nudge shown.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);
  EXPECT_FALSE(GetShownNudges()[id_2]);

  // Show the second nudge, expect both nudges shown.
  anchored_nudge_manager()->Show(nudge_data_2);
  EXPECT_TRUE(GetShownNudges()[id]);
  EXPECT_TRUE(GetShownNudges()[id_2]);

  // Cancel the second nudge, expect the first nudge shown.
  CancelNudge(id_2);
  EXPECT_TRUE(GetShownNudges()[id]);
  EXPECT_FALSE(GetShownNudges()[id_2]);

  // Cancel the first nudge, expect no nudges shown.
  CancelNudge(id);
  EXPECT_FALSE(GetShownNudges()[id]);
  EXPECT_FALSE(GetShownNudges()[id_2]);
}

// Tests that a nudge with buttons can be shown, execute callbacks and dismiss
// the nudge when the button is pressed.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_WithButtons) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  const std::u16string first_button_text = u"first";
  const std::u16string second_button_text = u"second";
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Add a first button with no callbacks.
  nudge_data.first_button_text = first_button_text;

  // Show the nudge.
  anchored_nudge_manager()->Show(nudge_data);
  AnchoredNudge* nudge = GetShownNudges()[id];

  // Ensure the nudge is visible and has set the provided contents.
  ASSERT_TRUE(nudge);
  ASSERT_TRUE(nudge->GetFirstButton());
  EXPECT_EQ(first_button_text, nudge->GetFirstButton()->GetText());
  EXPECT_FALSE(nudge->GetSecondButton());

  // Press the first button, the nudge should have dismissed.
  LeftClickOn(nudge->GetFirstButton());
  EXPECT_FALSE(GetShownNudges()[id]);
  histogram_tester.ExpectBucketCount(kFirstButtonPressed, kTestCatalogName, 1);

  // Add callbacks for the first button.
  bool first_button_callback_ran = false;
  nudge_data.first_button_callback = base::BindLambdaForTesting(
      [&first_button_callback_ran] { first_button_callback_ran = true; });

  // Show the nudge again.
  anchored_nudge_manager()->Show(nudge_data);
  nudge = GetShownNudges()[id];

  // Press the first button, `first_button_callback` should have executed, and
  // the nudge should have dismissed.
  LeftClickOn(nudge->GetFirstButton());
  EXPECT_TRUE(first_button_callback_ran);
  EXPECT_FALSE(GetShownNudges()[id]);
  histogram_tester.ExpectBucketCount(kFirstButtonPressed, kTestCatalogName, 2);

  // Add a second button with no callbacks.
  nudge_data.second_button_text = second_button_text;

  // Show the nudge again, now with a second button.
  anchored_nudge_manager()->Show(nudge_data);
  nudge = GetShownNudges()[id];

  // Ensure the nudge has a second button.
  ASSERT_TRUE(nudge->GetSecondButton());
  EXPECT_EQ(second_button_text, nudge->GetSecondButton()->GetText());

  // Press the second button, the nudge should have dismissed.
  LeftClickOn(nudge->GetSecondButton());
  EXPECT_FALSE(GetShownNudges()[id]);
  histogram_tester.ExpectBucketCount(kSecondButtonPressed, kTestCatalogName, 1);

  // Add a callback for the second button.
  bool second_button_callback_ran = false;
  nudge_data.second_button_callback = base::BindLambdaForTesting(
      [&second_button_callback_ran] { second_button_callback_ran = true; });

  // Show the nudge again.
  anchored_nudge_manager()->Show(nudge_data);
  nudge = GetShownNudges()[id];

  // Press the second button, `second_button_callback` should have executed, and
  // the nudge should have dismissed.
  LeftClickOn(nudge->GetSecondButton());
  EXPECT_TRUE(second_button_callback_ran);
  EXPECT_FALSE(GetShownNudges()[id]);
  histogram_tester.ExpectBucketCount(kSecondButtonPressed, kTestCatalogName, 2);
}

// Tests that a nudge without an anchor view is shown on its default location.
TEST_F(AnchoredNudgeManagerImplTest, DefaultLocation) {
  Shelf* shelf = GetPrimaryShelf();
  display::Display primary_display = GetPrimaryDisplay();
  gfx::Rect display_bounds = primary_display.bounds();
  int shelf_size = ShelfConfig::Get()->shelf_size();
  gfx::Rect nudge_bounds;

  // Show nudge on its default location by not providing an anchor view.
  const std::string id = "id";
  auto nudge_data = CreateBaseNudgeData(id, /*anchor_view=*/nullptr);
  anchored_nudge_manager()->Show(nudge_data);

  // The nudge should be shown on the leading bottom corner of the work area,
  // which for LTR languages is the bottom-left.
  shelf->SetAlignment(ShelfAlignment::kBottom);
  nudge_bounds = GetShownNudges()[id]->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  shelf->SetAlignment(ShelfAlignment::kLeft);
  nudge_bounds = GetShownNudges()[id]->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x() + shelf_size);
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());

  shelf->SetAlignment(ShelfAlignment::kRight);
  nudge_bounds = GetShownNudges()[id]->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());
}

// Tests that a nudge without an anchor view is placed on the right on RTL.
TEST_F(AnchoredNudgeManagerImplTest, DefaultLocation_WithRTL) {
  Shelf* shelf = GetPrimaryShelf();
  display::Display primary_display = GetPrimaryDisplay();
  gfx::Rect display_bounds = primary_display.bounds();
  int shelf_size = ShelfConfig::Get()->shelf_size();
  gfx::Rect nudge_bounds;

  // Turn on RTL mode.
  base::i18n::SetRTLForTesting(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::i18n::IsRTL());

  // Show nudge on its default location by not providing an anchor view.
  const std::string id = "id";
  auto nudge_data = CreateBaseNudgeData(id, /*anchor_view=*/nullptr);
  anchored_nudge_manager()->Show(nudge_data);

  // The nudge should be shown on the leading bottom corner of the work area,
  // which for RTL languages is the bottom-right.
  shelf->SetAlignment(ShelfAlignment::kBottom);
  nudge_bounds = GetShownNudges()[id]->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(nudge_bounds.right(), display_bounds.right());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  shelf->SetAlignment(ShelfAlignment::kLeft);
  nudge_bounds = GetShownNudges()[id]->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(nudge_bounds.right(), display_bounds.right());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());

  shelf->SetAlignment(ShelfAlignment::kRight);
  nudge_bounds = GetShownNudges()[id]->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(nudge_bounds.right(), display_bounds.right() - shelf_size);
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());

  // Turn off RTL mode.
  base::i18n::SetRTLForTesting(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(base::i18n::IsRTL());
}

// Tests that a nudge without an anchor view updates its baseline based on the
// current hotseat state.
TEST_F(AnchoredNudgeManagerImplTest, DefaultLocation_WithHotseatShown) {
  Shelf* shelf = GetPrimaryShelf();
  HotseatWidget* hotseat = shelf->hotseat_widget();
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  display::Display primary_display = GetPrimaryDisplay();
  gfx::Rect display_bounds = primary_display.bounds();
  int shelf_size = ShelfConfig::Get()->shelf_size();
  gfx::Rect nudge_bounds;

  // Show nudge on its default location by not providing an anchor view.
  const std::string id = "id";
  auto nudge_data = CreateBaseNudgeData(id, /*anchor_view=*/nullptr);
  anchored_nudge_manager()->Show(nudge_data);

  // The nudge should be shown on the leading bottom corner of the work area.
  nudge_bounds = GetShownNudges()[id]->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  // Test that the nudge updates its baseline when the hotseat is shown.
  tablet_mode_controller->SetEnabledForTest(true);
  nudge_bounds = GetShownNudges()[id]->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(hotseat->state(), HotseatState::kShownHomeLauncher);
  EXPECT_EQ(nudge_bounds.bottom(),
            hotseat->CalculateHotseatYInScreen(hotseat->state()));
}

// Tests that a nudge without an anchor view updates its baseline when the shelf
// hides itself.
TEST_F(AnchoredNudgeManagerImplTest, DefaultLocation_WithAutoHideShelf) {
  Shelf* shelf = GetPrimaryShelf();
  display::Display primary_display = GetPrimaryDisplay();
  gfx::Rect display_bounds = primary_display.bounds();
  gfx::Rect nudge_bounds;

  // Show nudge on its default location by not providing an anchor view.
  const std::string id = "id";
  auto nudge_data = CreateBaseNudgeData(id, /*anchor_view=*/nullptr);
  anchored_nudge_manager()->Show(nudge_data);

  // The nudge should be shown on the leading bottom corner of the work area.
  nudge_bounds = GetShownNudges()[id]->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(),
            display_bounds.bottom() - ShelfConfig::Get()->shelf_size());

  // Test that the nudge updates its baseline when the shelf hides itself.
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect()));
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  nudge_bounds = GetShownNudges()[id]->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(nudge_bounds.bottom(),
            display_bounds.bottom() -
                ShelfConfig::Get()->hidden_shelf_in_screen_portion());
}

// Tests that a nudge updates its location after zooming in/out the UI.
TEST_F(AnchoredNudgeManagerImplTest, DefaultLocation_Zoom) {
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  gfx::Rect display_bounds;
  gfx::Rect nudge_bounds;

  // Show nudge on its default location by not providing an anchor view.
  const std::string id = "id";
  auto nudge_data = CreateBaseNudgeData(id, /*anchor_view=*/nullptr);
  anchored_nudge_manager()->Show(nudge_data);

  // Since no zoom factor has been set on the display, it should be 1.
  const display::ManagedDisplayInfo& display =
      display_manager()->GetDisplayInfo(GetPrimaryDisplay().id());
  EXPECT_EQ(
      display_manager()->GetDisplayForId(display.id()).device_scale_factor(),
      1.f);

  // The nudge should be shown on the bottom-left of the work area.
  nudge_bounds = GetShownNudges()[id]->GetWidget()->GetWindowBoundsInScreen();
  display_bounds = GetPrimaryDisplay().bounds();
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  // Set the device scale factor to 2.
  constexpr float zoom_factor = 2.0f;
  display_manager()->UpdateZoomFactor(display.id(), zoom_factor);
  EXPECT_EQ(
      display_manager()->GetDisplayForId(display.id()).device_scale_factor(),
      zoom_factor);

  // Nudge bounds should update accordingly.
  nudge_bounds = GetShownNudges()[id]->GetWidget()->GetWindowBoundsInScreen();
  display_bounds = GetPrimaryDisplay().bounds();
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);
}

// Tests that attempting to show a nudge with an `id` that's in use cancels
// the existing nudge and replaces it with a new nudge.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_NudgeWithIdAlreadyExists) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  auto* contents_view =
      widget->SetContentsView(std::make_unique<views::View>());

  // Set up nudge data contents.
  const std::string id = "id";

  const std::u16string text = u"text";
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view, text);

  const std::u16string text_2 = u"text_2";
  auto* anchor_view_2 =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data_2 = CreateBaseNudgeData(id, anchor_view_2, text_2);

  // Show a nudge with some initial contents.
  anchored_nudge_manager()->Show(nudge_data);

  // First nudge contents should be set.
  AnchoredNudge* nudge = GetShownNudges()[id];
  ASSERT_TRUE(nudge);
  EXPECT_EQ(text, nudge->GetBodyText());
  EXPECT_EQ(anchor_view, nudge->GetAnchorView());

  // Attempt to show a nudge with different contents but with the same id.
  anchored_nudge_manager()->Show(nudge_data_2);

  // The previous nudge should be cancelled and replaced with the new nudge.
  nudge = GetShownNudges()[id];
  ASSERT_TRUE(nudge);
  EXPECT_EQ(text_2, nudge->GetBodyText());
  EXPECT_EQ(anchor_view_2, nudge->GetAnchorView());
}

// Tests that a nudge is not created if its anchor view is not visible.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_InvisibleAnchorView) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Set anchor view visibility to false.
  anchor_view->SetVisible(false);

  // Attempt to show nudge.
  anchored_nudge_manager()->Show(nudge_data);

  // Anchor view is not visible, the nudge should not be created.
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that a nudge is not created if its anchor view doesn't have a widget.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_AnchorViewWithoutWidget) {
  // Set up nudge data contents.
  const std::string id = "id";
  auto contents_view = std::make_unique<views::View>();
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Attempt to show nudge.
  anchored_nudge_manager()->Show(nudge_data);

  // Anchor view does not have a widget, the nudge should not be created.
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that a nudge will not be shown if a `ScopedAnchoredNudgePause` exists,
// and even if the `scoped_anchored_nudge_pause` gets destroyed, the nudge is
// dismissed and will not be saved.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_ScopedAnchoredNudgePause) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Set up a `ScopedAnchoredNudgePause`.
  auto scoped_anchored_nudge_pause =
      anchored_nudge_manager()->CreateScopedPause();

  // Attempt to show nudge.
  anchored_nudge_manager()->Show(nudge_data);

  // A `ScopedAnchoredNudgePause` exists, the nudge should not be shown.
  EXPECT_FALSE(GetShownNudges()[id]);

  // Destroy the `ScopedAnchoredNudgePause`, the nudge doesn't exist either.
  scoped_anchored_nudge_pause.reset();
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that if a `ScopedAnchoredNudgePause` creates when a nudge is showing,
// the nudge will be dismissed immediately.
TEST_F(AnchoredNudgeManagerImplTest, CancelNudge_ScopedAnchoredNudgePause) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // The nudge will be shown.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // After a `ScopedAnchoredNudgePause` is created, the nudge will be cleared
  // immediately.
  anchored_nudge_manager()->CreateScopedPause();
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that a nudge sets the appropriate arrow when it's set to be anchored to
// the shelf, and updates its arrow whenever the shelf alignment changes.
TEST_F(AnchoredNudgeManagerImplTest, NudgeAnchoredToShelf) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Make the nudge set its arrow based on the shelf's position.
  nudge_data.anchored_to_shelf = true;

  // Set shelf alignment to the left.
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  shelf->SetAlignment(ShelfAlignment::kLeft);

  // Show a nudge, expect its arrow to be aligned with left shelf.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);
  EXPECT_EQ(views::BubbleBorder::Arrow::LEFT_CENTER,
            GetShownNudges()[id]->arrow());

  // Cancel the nudge, and show a new nudge with bottom shelf alignment.
  anchored_nudge_manager()->Cancel(id);
  shelf->SetAlignment(ShelfAlignment::kBottom);
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_EQ(views::BubbleBorder::Arrow::BOTTOM_CENTER,
            GetShownNudges()[id]->arrow());

  // Change the shelf alignment to the right while the nudge is still open,
  // nudge arrow should be updated.
  shelf->SetAlignment(ShelfAlignment::kRight);
  EXPECT_EQ(views::BubbleBorder::Arrow::RIGHT_CENTER,
            GetShownNudges()[id]->arrow());
}

// Tests that a nudge that is anchored to the shelf is not affected by shelf
// alignment changes of a display where the nudge does not exist.
TEST_F(AnchoredNudgeManagerImplTest,
       NudgeAnchoredToShelf_WithASecondaryDisplay) {
  // Add a secondary display.
  UpdateDisplay("800x700,800x700");
  RootWindowController* const secondary_root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id());
  Shelf* shelf = GetPrimaryShelf();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = shelf->status_area_widget()->unified_system_tray();
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Make the nudge set its arrow based on the shelf's position.
  nudge_data.anchored_to_shelf = true;

  // Set shelf alignment to the left.
  shelf->SetAlignment(ShelfAlignment::kLeft);

  // Show a nudge, expect its arrow to be aligned with left shelf.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);
  EXPECT_EQ(views::BubbleBorder::Arrow::LEFT_CENTER,
            GetShownNudges()[id]->arrow());

  // Test that changing the shelf alignment on the secondary display does not
  // affect the nudge's arrow, since the nudge lives in the primary display.
  secondary_root_window_controller->shelf()->SetAlignment(
      ShelfAlignment::kBottom);
  EXPECT_EQ(views::BubbleBorder::Arrow::LEFT_CENTER,
            GetShownNudges()[id]->arrow());
}

// Tests that a nudge that is anchored to the shelf maintains the shelf visible
// while the nudge is being shown and the shelf is on auto-hide.
TEST_F(AnchoredNudgeManagerImplTest, NudgeAnchoredToShelf_ShelfDoesNotHide) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Make the nudge maintain the shelf visible while it is showing.
  nudge_data.anchored_to_shelf = true;

  // Verify `shelf` is initially visible.
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_TRUE(shelf->IsVisible());

  // Set `shelf` to always auto-hide, it should not be visible.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_FALSE(shelf->IsVisible());

  // Show the nudge, `shelf` should be made visible while nudge is showing.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(shelf->IsVisible());

  // Cancel the nudge, `shelf` should be hidden again.
  anchored_nudge_manager()->Cancel(id);
  EXPECT_FALSE(shelf->IsVisible());
}

// Tests that a nudge closes if its anchor view is made invisible.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_WhenAnchorViewIsHiding) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Set the anchor view visibility to false, the nudge should have closed.
  anchor_view->SetVisible(false);
  EXPECT_FALSE(GetShownNudges()[id]);

  // Set the anchor view visibility to true, the nudge should not reappear.
  anchor_view->SetVisible(true);
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that a nudge closes if its anchor view is deleted.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_WhenAnchorViewIsDeleting) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";

  auto* contents_view =
      widget->SetContentsView(std::make_unique<views::View>());
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Delete the anchor view, the nudge should have closed.
  contents_view->RemoveAllChildViews();
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that a nudge whose anchor view is a part of a secondary display closes
// when that display is removed.
TEST_F(AnchoredNudgeManagerImplTest,
       NudgeCloses_WhenAnchorViewIsDeletingOnSecondaryDisplay) {
  // Set up two displays.
  UpdateDisplay("800x700,800x700");
  RootWindowController* const secondary_root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id());

  // Set up nudge data contents. The anchor view is a child of the secondary
  // root window controller, so it will be deleted if the display is removed.
  const std::string id = "id";
  auto* anchor_view = secondary_root_window_controller->shelf()
                          ->status_area_widget()
                          ->unified_system_tray();
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show the nudge in the secondary display.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Remove the secondary display, which deletes the anchor view.
  UpdateDisplay("800x700");

  // The anchor view was deleted, the nudge should have closed.
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that a nudge is properly destroyed on shutdown.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_OnShutdown) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Nudge is left open, no crash.
}

// Tests that nudges expire after their dismiss timer reaches its end.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_WhenDismissTimerExpires) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge with default duration.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // The nudge should expire after `kNudgeDefaultDuration` has passed.
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeDefaultDuration + base::Seconds(1));
  EXPECT_FALSE(GetShownNudges()[id]);

  // Test that a nudge with long duration persists and lasts more than
  // `kNudgeDefaultDuration` but expires after `kNudgeLongDuration`.
  nudge_data.has_long_duration = true;
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeDefaultDuration + base::Seconds(1));
  EXPECT_TRUE(GetShownNudges()[id]);

  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeLongDuration);
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that nudges are destroyed on session state changes.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_OnSessionStateChanged) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Lock screen, nudge should have closed.
  SetLockedState(true);
  EXPECT_FALSE(GetShownNudges()[id]);

  // Show a nudge in the locked state.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Unlock screen, nudge should have closed.
  SetLockedState(false);
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that the dismiss timer is paused on hover so the nudge won't close.
TEST_F(AnchoredNudgeManagerImplTest, NudgePersistsOnHover) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  anchored_nudge_manager()->Show(nudge_data);
  AnchoredNudge* nudge = GetShownNudges()[id];
  EXPECT_TRUE(nudge);

  // Wait for half of the nudge's duration.
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeDefaultDuration / 2);

  // Hover on the nudge and wait for its full duration. It should persist.
  GetEventGenerator()->MoveMouseTo(
      GetShownNudges()[id]->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(nudge->IsMouseHovered());
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeDefaultDuration);
  EXPECT_TRUE(nudge);

  // Hover out of the nudge and wait its duration. It should be dismissed.
  GetEventGenerator()->MoveMouseTo(gfx::Point(-100, -100));
  EXPECT_FALSE(nudge->IsMouseHovered());
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeDefaultDuration / 2 + base::Seconds(1));
  EXPECT_FALSE(GetShownNudges()[id]);
}

// Tests that attempting to cancel a nudge with an invalid `id` should not
// have any effects.
TEST_F(AnchoredNudgeManagerImplTest, CancelNudgeWhichDoesNotExist) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  const std::string id_2 = "id_2";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  anchored_nudge_manager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Attempt to cancel nudge with an `id` that does not exist. Should not have
  // any effect.
  CancelNudge(id_2);
  EXPECT_TRUE(GetShownNudges()[id]);

  // Cancel the shown nudge with its valid `id`.
  CancelNudge(id);
  EXPECT_FALSE(GetShownNudges()[id]);

  // Attempt to cancel the same nudge again. Should not have any effect.
  CancelNudge(id);
  EXPECT_FALSE(GetShownNudges()[id]);
}

TEST_F(AnchoredNudgeManagerImplTest, ShownCountMetric) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  histogram_tester.ExpectBucketCount(kNudgeShownCount, kTestCatalogName, 0);

  anchored_nudge_manager()->Show(nudge_data);
  histogram_tester.ExpectBucketCount(kNudgeShownCount, kTestCatalogName, 1);

  anchored_nudge_manager()->Show(nudge_data);
  anchored_nudge_manager()->Show(nudge_data);
  histogram_tester.ExpectBucketCount(kNudgeShownCount, kTestCatalogName, 3);
}

TEST_F(AnchoredNudgeManagerImplTest, TimeToActionMetric) {
  base::HistogramTester histogram_tester;
  anchored_nudge_manager()->ResetNudgeRegistryForTesting();
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id = "id";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Metric is not recorded if the nudge has not been shown.
  anchored_nudge_manager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1m,
                                     kTestCatalogName, 0);

  // Metric is recorded if the action is performed after the nudge was shown.
  anchored_nudge_manager()->Show(nudge_data);
  task_environment()->FastForwardBy(base::Seconds(1));
  anchored_nudge_manager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1m,
                                     kTestCatalogName, 1);

  // Metric is not recorded if the nudge action is performed again without
  // another nudge being shown.
  anchored_nudge_manager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1m,
                                     kTestCatalogName, 1);

  // Metric is recorded with the appropriate time range after showing nudge
  // again and waiting enough time to fall into the "Within1h" time bucket.
  anchored_nudge_manager()->Show(nudge_data);
  task_environment()->FastForwardBy(base::Minutes(2));
  anchored_nudge_manager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1h,
                                     kTestCatalogName, 1);

  // Metric is not recorded if the nudge action is performed again without
  // another nudge being shown.
  anchored_nudge_manager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1h,
                                     kTestCatalogName, 1);

  // Metric is recorded with the appropriate time range after showing nudge
  // again and waiting enough time to fall into the "WithinSession" time bucket.
  anchored_nudge_manager()->Show(nudge_data);
  task_environment()->FastForwardBy(base::Hours(2));
  anchored_nudge_manager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithinSession,
                                     kTestCatalogName, 1);

  // Metric is not recorded if the nudge action is performed again without
  // another nudge being shown.
  anchored_nudge_manager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithinSession,
                                     kTestCatalogName, 1);
}

}  // namespace ash
