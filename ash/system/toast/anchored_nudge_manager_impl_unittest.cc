// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/anchored_nudge_manager_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/scoped_nudge_pause.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/style/system_toast_style.h"
#include "ash/system/toast/anchored_nudge.h"
#include "ash/system/toast/nudge_constants.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/i18n/rtl.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/manager/display_manager.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr NudgeCatalogName kTestCatalogName =
    NudgeCatalogName::kTestCatalogName;

constexpr char kPrimaryButtonPressed[] =
    "Ash.NotifierFramework.Nudge.PrimaryButtonPressed";
constexpr char kSecondaryButtonPressed[] =
    "Ash.NotifierFramework.Nudge.SecondaryButtonPressed";
constexpr char kNudgeShownCount[] = "Ash.NotifierFramework.Nudge.ShownCount";
constexpr char kNudgeTimeToActionWithin1m[] =
    "Ash.NotifierFramework.Nudge.TimeToAction.Within1m";
constexpr char kNudgeTimeToActionWithin1h[] =
    "Ash.NotifierFramework.Nudge.TimeToAction.Within1h";
constexpr char kNudgeTimeToActionWithinSession[] =
    "Ash.NotifierFramework.Nudge.TimeToAction.WithinSession";

constexpr base::TimeDelta kAnimationSettleDownDuration = base::Seconds(5);

void SetLockedState(bool locked) {
  SessionInfo info;
  info.state = locked ? session_manager::SessionState::LOCKED
                      : session_manager::SessionState::ACTIVE;
  Shell::Get()->session_controller()->SetSessionInfo(info);
}

AnchoredNudgeManagerImpl* GetAnchoredNudgeManager() {
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
  GetAnchoredNudgeManager()->Cancel(id);
}

AnchoredNudge* GetShownNudge(const std::string& id) {
  return GetAnchoredNudgeManager()->GetShownNudgeForTest(id);
}

const std::u16string& GetNudgeBodyText(const std::string& id) {
  return GetAnchoredNudgeManager()->GetNudgeBodyTextForTest(id);
}

views::LabelButton* GetNudgePrimaryButton(const std::string& id) {
  return GetAnchoredNudgeManager()->GetNudgePrimaryButtonForTest(id);
}

views::LabelButton* GetNudgeSecondaryButton(const std::string& id) {
  return GetAnchoredNudgeManager()->GetNudgeSecondaryButtonForTest(id);
}

AnchoredNudge* GetNudgeIfShown(const std::string& id) {
  return GetAnchoredNudgeManager()->GetNudgeIfShown(id);
}

void PressTab() {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
}

}  // namespace

class AnchoredNudgeManagerImplTest : public AshTestBase {
 public:
  AnchoredNudgeManagerImplTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

// Tests that a nudge can be shown and its contents are properly sent.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_SingleNudge) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  const std::u16string body_text = u"Body text";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view, body_text);

  // Show a nudge.
  GetAnchoredNudgeManager()->Show(nudge_data);

  // Ensure the nudge is visible and has set the provided contents.
  AnchoredNudge* nudge = GetShownNudge(id);
  ASSERT_TRUE(nudge);
  EXPECT_TRUE(nudge->GetVisible());
  EXPECT_EQ(anchor_view, nudge->GetAnchorView());

  // Ensure the nudge widget was not activated when shown.
  EXPECT_FALSE(nudge->GetWidget()->IsActive());

  // Cancel the nudge, expect it to be removed from the shown nudges map.
  CancelNudge(id);
  EXPECT_FALSE(GetShownNudge(id));
}

// Tests that two nudges can be shown on screen at the same time.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_TwoNudges) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  auto* contents_view =
      widget->SetContentsView(std::make_unique<views::View>());

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  const std::string id_2("id_2");
  auto* anchor_view_2 =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data_2 = CreateBaseNudgeData(id_2, anchor_view_2);

  // Show the first nudge, expect the first nudge shown.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));
  EXPECT_FALSE(GetShownNudge(id_2));

  // Show the second nudge, expect both nudges shown.
  GetAnchoredNudgeManager()->Show(nudge_data_2);
  EXPECT_TRUE(GetShownNudge(id));
  EXPECT_TRUE(GetShownNudge(id_2));

  // Cancel the second nudge, expect the first nudge shown.
  CancelNudge(id_2);
  EXPECT_TRUE(GetShownNudge(id));
  EXPECT_FALSE(GetShownNudge(id_2));

  // Cancel the first nudge, expect no nudges shown.
  CancelNudge(id);
  EXPECT_FALSE(GetShownNudge(id));
  EXPECT_FALSE(GetShownNudge(id_2));
}

// Tests that a nudge with buttons can be shown, execute callbacks and dismiss
// the nudge when the button is pressed.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_WithButtons) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  const std::u16string primary_button_text = u"Primary";
  const std::u16string secondary_button_text = u"Secondary";
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Add a primary button with no callbacks.
  nudge_data.primary_button_text = primary_button_text;

  // Show the nudge.
  GetAnchoredNudgeManager()->Show(nudge_data);
  AnchoredNudge* nudge = GetShownNudge(id);

  // Ensure the nudge is visible and has set the provided contents.
  ASSERT_TRUE(nudge);
  ASSERT_TRUE(GetNudgePrimaryButton(id));
  EXPECT_FALSE(GetNudgeSecondaryButton(id));

  // Press the primary button, the nudge should have dismissed.
  LeftClickOn(GetNudgePrimaryButton(id));
  EXPECT_FALSE(GetShownNudge(id));
  histogram_tester.ExpectBucketCount(kPrimaryButtonPressed, kTestCatalogName,
                                     1);

  // Add callbacks for the primary button.
  bool primary_button_callback_ran = false;
  nudge_data.primary_button_callback = base::BindLambdaForTesting(
      [&primary_button_callback_ran] { primary_button_callback_ran = true; });

  // Show the nudge again.
  GetAnchoredNudgeManager()->Show(nudge_data);
  nudge = GetShownNudge(id);

  // Press the primary button, `primary_button_callback` should have executed,
  // and the nudge should have dismissed.
  LeftClickOn(GetNudgePrimaryButton(id));
  EXPECT_TRUE(primary_button_callback_ran);
  EXPECT_FALSE(GetShownNudge(id));
  histogram_tester.ExpectBucketCount(kPrimaryButtonPressed, kTestCatalogName,
                                     2);

  // Add a secondary button with no callbacks.
  nudge_data.secondary_button_text = secondary_button_text;

  // Show the nudge again, now with a secondary button.
  GetAnchoredNudgeManager()->Show(nudge_data);
  nudge = GetShownNudge(id);

  // Ensure the nudge has a secondary button.
  ASSERT_TRUE(GetNudgeSecondaryButton(id));

  // Press the secondary button, the nudge should have dismissed.
  LeftClickOn(GetNudgeSecondaryButton(id));
  EXPECT_FALSE(GetShownNudge(id));
  histogram_tester.ExpectBucketCount(kSecondaryButtonPressed, kTestCatalogName,
                                     1);

  // Add a callback for the secondary button.
  bool secondary_button_callback_ran = false;
  nudge_data.secondary_button_callback =
      base::BindLambdaForTesting([&secondary_button_callback_ran] {
        secondary_button_callback_ran = true;
      });

  // Show the nudge again.
  GetAnchoredNudgeManager()->Show(nudge_data);
  nudge = GetShownNudge(id);

  // Press the secondary button, `secondary_button_callback` should have
  // executed, and the nudge should have dismissed.
  LeftClickOn(GetNudgeSecondaryButton(id));
  EXPECT_TRUE(secondary_button_callback_ran);
  EXPECT_FALSE(GetShownNudge(id));
  histogram_tester.ExpectBucketCount(kSecondaryButtonPressed, kTestCatalogName,
                                     2);
}

// Tests that a nudge without an anchor view is shown on its default location.
TEST_F(AnchoredNudgeManagerImplTest, DefaultLocation) {
  Shelf* shelf = GetPrimaryShelf();
  display::Display primary_display = GetPrimaryDisplay();
  gfx::Rect display_bounds = primary_display.bounds();
  int shelf_size = ShelfConfig::Get()->shelf_size();
  gfx::Rect nudge_bounds;

  // Show nudge on its default location by not providing an anchor view.
  const std::string id("id");
  auto nudge_data = CreateBaseNudgeData(id, /*anchor_view=*/nullptr);
  GetAnchoredNudgeManager()->Show(nudge_data);

  // The nudge should be shown on the leading bottom corner of the work area,
  // which for LTR languages is the bottom-left.
  shelf->SetAlignment(ShelfAlignment::kBottom);
  nudge_bounds = GetShownNudge(id)->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  shelf->SetAlignment(ShelfAlignment::kLeft);
  nudge_bounds = GetShownNudge(id)->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x() + shelf_size);
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());

  shelf->SetAlignment(ShelfAlignment::kRight);
  nudge_bounds = GetShownNudge(id)->GetWidget()->GetWindowBoundsInScreen();
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
  const std::string id("id");
  auto nudge_data = CreateBaseNudgeData(id, /*anchor_view=*/nullptr);
  GetAnchoredNudgeManager()->Show(nudge_data);

  // The nudge should be shown on the leading bottom corner of the work area,
  // which for RTL languages is the bottom-right.
  shelf->SetAlignment(ShelfAlignment::kBottom);
  nudge_bounds = GetShownNudge(id)->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(nudge_bounds.right(), display_bounds.right());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  shelf->SetAlignment(ShelfAlignment::kLeft);
  nudge_bounds = GetShownNudge(id)->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(nudge_bounds.right(), display_bounds.right());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());

  shelf->SetAlignment(ShelfAlignment::kRight);
  nudge_bounds = GetShownNudge(id)->GetWidget()->GetWindowBoundsInScreen();
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
  display::Display primary_display = GetPrimaryDisplay();
  gfx::Rect display_bounds = primary_display.bounds();
  int shelf_size = ShelfConfig::Get()->shelf_size();
  gfx::Rect nudge_bounds;

  // Show nudge on its default location by not providing an anchor view.
  const std::string id("id");
  auto nudge_data = CreateBaseNudgeData(id, /*anchor_view=*/nullptr);
  GetAnchoredNudgeManager()->Show(nudge_data);

  // The nudge should be shown on the leading bottom corner of the work area.
  nudge_bounds = GetShownNudge(id)->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  // Test that the nudge updates its baseline when the hotseat is shown.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  nudge_bounds = GetShownNudge(id)->GetWidget()->GetWindowBoundsInScreen();
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
  const std::string id("id");
  auto nudge_data = CreateBaseNudgeData(id, /*anchor_view=*/nullptr);
  GetAnchoredNudgeManager()->Show(nudge_data);

  // The nudge should be shown on the leading bottom corner of the work area.
  nudge_bounds = GetShownNudge(id)->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(),
            display_bounds.bottom() - ShelfConfig::Get()->shelf_size());

  // Test that the nudge updates its baseline when the shelf hides itself.
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect()));
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  nudge_bounds = GetShownNudge(id)->GetWidget()->GetWindowBoundsInScreen();
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
  const std::string id("id");
  auto nudge_data = CreateBaseNudgeData(id, /*anchor_view=*/nullptr);
  GetAnchoredNudgeManager()->Show(nudge_data);

  // Since no zoom factor has been set on the display, it should be 1.
  const display::ManagedDisplayInfo& display =
      display_manager()->GetDisplayInfo(GetPrimaryDisplay().id());
  EXPECT_EQ(
      display_manager()->GetDisplayForId(display.id()).device_scale_factor(),
      1.f);

  // The nudge should be shown on the bottom-left of the work area.
  nudge_bounds = GetShownNudge(id)->GetWidget()->GetWindowBoundsInScreen();
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
  nudge_bounds = GetShownNudge(id)->GetWidget()->GetWindowBoundsInScreen();
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
  const std::string id("id");

  const std::u16string text = u"text";
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view, text);

  const std::u16string text_2 = u"text_2";
  auto* anchor_view_2 =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data_2 = CreateBaseNudgeData(id, anchor_view_2, text_2);

  // Show a nudge with some initial contents.
  GetAnchoredNudgeManager()->Show(nudge_data);

  // First nudge contents should be set.
  AnchoredNudge* nudge = GetShownNudge(id);
  ASSERT_TRUE(nudge);
  EXPECT_EQ(text, GetNudgeBodyText(id));
  EXPECT_EQ(anchor_view, nudge->GetAnchorView());

  // Attempt to show a nudge with different contents but with the same id.
  GetAnchoredNudgeManager()->Show(nudge_data_2);

  // The previous nudge should be cancelled and replaced with the new nudge.
  nudge = GetShownNudge(id);
  ASSERT_TRUE(nudge);
  EXPECT_EQ(text_2, GetNudgeBodyText(id));
  EXPECT_EQ(anchor_view_2, nudge->GetAnchorView());
}

// Tests that a nudge is not created if its anchor view is not visible.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_InvisibleAnchorView) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Set anchor view visibility to false.
  anchor_view->SetVisible(false);

  // Attempt to show nudge.
  GetAnchoredNudgeManager()->Show(nudge_data);

  // Anchor view is not visible, the nudge should not be created.
  EXPECT_FALSE(GetShownNudge(id));
}

// Tests that a nudge is not created if its anchor view doesn't have a widget.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_AnchorViewWithoutWidget) {
  // Set up nudge data contents.
  const std::string id("id");
  auto contents_view = std::make_unique<views::View>();
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Attempt to show nudge.
  GetAnchoredNudgeManager()->Show(nudge_data);

  // Anchor view does not have a widget, the nudge should not be created.
  EXPECT_FALSE(GetShownNudge(id));
}

// Tests that a nudge is not created if its anchor view was deleted.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_DeletedAnchorView) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto contents_view = std::make_unique<views::View>();
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  widget->SetContentsView(contents_view.get());

  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Anchor view exists, the nudge should be created.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // Delete the anchor view, the nudge should not be created.
  contents_view->RemoveAllChildViews();
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_FALSE(GetShownNudge(id));
}

// Tests that a nudge will not be shown if a `ScopedNudgePause` exists, and even
// if the `scoped_anchored_nudge_pause` gets destroyed, the nudge is dismissed
// and will not be saved.
TEST_F(AnchoredNudgeManagerImplTest, ShowNudge_ScopedNudgePause) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Set up a `ScopedNudgePause`.
  auto scoped_anchored_nudge_pause =
      GetAnchoredNudgeManager()->CreateScopedPause();

  // Attempt to show nudge.
  GetAnchoredNudgeManager()->Show(nudge_data);

  // A `ScopedNudgePause` exists, the nudge should not be shown.
  EXPECT_FALSE(GetShownNudge(id));

  // Destroy the `ScopedNudgePause`, the nudge doesn't exist either.
  scoped_anchored_nudge_pause.reset();
  EXPECT_FALSE(GetShownNudge(id));
}

// Tests that if a `ScopedNudgePause` creates when a nudge is showing, the nudge
// will be dismissed immediately.
TEST_F(AnchoredNudgeManagerImplTest, CancelNudge_ScopedNudgePause) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // The nudge will be shown.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // After a `ScopedNudgePause` is created, the nudge will be closed
  // immediately.
  GetAnchoredNudgeManager()->CreateScopedPause();
  EXPECT_FALSE(GetShownNudge(id));
}

// Tests that a nudge sets the appropriate arrow when it's set to be anchored to
// the shelf, and updates its arrow whenever the shelf alignment changes.
TEST_F(AnchoredNudgeManagerImplTest, NudgeAnchoredToShelf) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Make the nudge set its arrow based on the shelf's position.
  nudge_data.anchored_to_shelf = true;

  // Set shelf alignment to the left.
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(shelf->alignment(), ShelfAlignment::kBottom);
  EXPECT_EQ(shelf->GetVisibilityState(), SHELF_VISIBLE);
  shelf->SetAlignment(ShelfAlignment::kLeft);

  // Show a nudge, expect its arrow to be aligned with left shelf.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));
  EXPECT_EQ(GetShownNudge(id)->arrow(),
            views::BubbleBorder::Arrow::LEFT_BOTTOM);

  // Cancel the nudge, and show a new nudge with bottom shelf alignment.
  GetAnchoredNudgeManager()->Cancel(id);
  shelf->SetAlignment(ShelfAlignment::kBottom);
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_EQ(GetShownNudge(id)->arrow(),
            views::BubbleBorder::Arrow::BOTTOM_RIGHT);

  // Change the shelf alignment to the right while the nudge is still open,
  // nudge arrow should be updated.
  shelf->SetAlignment(ShelfAlignment::kRight);
  EXPECT_EQ(GetShownNudge(id)->arrow(),
            views::BubbleBorder::Arrow::RIGHT_BOTTOM);

  // Cancel the nudge and create a new nudge with an arrow that is not
  // corner-anchored.
  GetAnchoredNudgeManager()->Cancel(id);
  nudge_data.arrow = views::BubbleBorder::Arrow::LEFT_CENTER;
  GetAnchoredNudgeManager()->Show(nudge_data);

  shelf->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_EQ(GetShownNudge(id)->arrow(),
            views::BubbleBorder::Arrow::LEFT_CENTER);
  shelf->SetAlignment(ShelfAlignment::kBottom);
  EXPECT_EQ(GetShownNudge(id)->arrow(),
            views::BubbleBorder::Arrow::BOTTOM_CENTER);
  shelf->SetAlignment(ShelfAlignment::kRight);
  EXPECT_EQ(GetShownNudge(id)->arrow(),
            views::BubbleBorder::Arrow::RIGHT_CENTER);
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
  const std::string id("id");
  auto* anchor_view = shelf->status_area_widget()->unified_system_tray();
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Make the nudge set its arrow based on the shelf's position.
  nudge_data.anchored_to_shelf = true;

  // Set shelf alignment to the left.
  shelf->SetAlignment(ShelfAlignment::kLeft);

  // Show a nudge, expect its arrow to be aligned with left shelf.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));
  EXPECT_EQ(views::BubbleBorder::Arrow::LEFT_BOTTOM,
            GetShownNudge(id)->arrow());

  // Test that changing the shelf alignment on the secondary display does not
  // affect the nudge's arrow, since the nudge lives in the primary display.
  secondary_root_window_controller->shelf()->SetAlignment(
      ShelfAlignment::kBottom);
  EXPECT_EQ(views::BubbleBorder::Arrow::LEFT_BOTTOM,
            GetShownNudge(id)->arrow());
}

// Tests that a nudge that is anchored to the shelf maintains the shelf visible
// while the nudge is being shown and the shelf is on auto-hide.
TEST_F(AnchoredNudgeManagerImplTest, NudgeAnchoredToShelf_ShelfDoesNotHide) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
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
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(shelf->IsVisible());

  // Cancel the nudge, `shelf` should be hidden again.
  GetAnchoredNudgeManager()->Cancel(id);
  EXPECT_FALSE(shelf->IsVisible());
}

// Tests that a nudge closes if its anchor view is made invisible.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_WhenAnchorViewIsHiding) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // Set the anchor view visibility to false, the nudge should have closed.
  anchor_view->SetVisible(false);
  EXPECT_FALSE(GetShownNudge(id));

  // Set the anchor view visibility to true, the nudge should not reappear.
  anchor_view->SetVisible(true);
  EXPECT_FALSE(GetShownNudge(id));
}

// Tests that a nudge closes if its anchor view is deleted.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_WhenAnchorViewIsDeleting) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");

  auto* contents_view =
      widget->SetContentsView(std::make_unique<views::View>());
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // Delete the anchor view, the nudge should have closed.
  contents_view->RemoveAllChildViews();
  EXPECT_FALSE(GetShownNudge(id));
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
  const std::string id("id");
  auto* anchor_view = secondary_root_window_controller->shelf()
                          ->status_area_widget()
                          ->unified_system_tray();
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show the nudge in the secondary display.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // Remove the secondary display, which deletes the anchor view.
  UpdateDisplay("800x700");

  // The anchor view was deleted, the nudge should have closed.
  EXPECT_FALSE(GetShownNudge(id));
}

// Tests that a nudge closes if its anchor view widget is hiding.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_WhenAnchorViewWidgetIsHiding) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // Hide the anchor view widget, the nudge should have closed.
  widget->Hide();
  EXPECT_FALSE(GetShownNudge(id));

  // Show the anchor view widget, the nudge should not reappear.
  widget->ShowInactive();
  EXPECT_FALSE(GetShownNudge(id));
}

// Tests that a nudge is properly destroyed on shutdown.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_OnShutdown) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // Nudge is left open, no crash.
}

// Tests that nudges expire after their dismiss timer reaches its end.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_WhenDismissTimerExpires) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge with default duration.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // The nudge should expire after `kNudgeDefaultDuration` has passed.
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeDefaultDuration + base::Seconds(1));
  EXPECT_FALSE(GetShownNudge(id));

  // Test that a nudge with medium duration lasts longer than
  // `kNudgeDefaultDuration` but expires after `kNudgeMediumDuration`.
  nudge_data.duration = NudgeDuration::kMediumDuration;
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeDefaultDuration + base::Seconds(1));
  EXPECT_TRUE(GetShownNudge(id));

  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeMediumDuration);
  EXPECT_FALSE(GetShownNudge(id));

  // Test that a nudge with long duration lasts longer than
  // `kNudgeMediumDuration` but expires after `kNudgeLongDuration`.
  nudge_data.duration = NudgeDuration::kLongDuration;
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeMediumDuration + base::Seconds(1));
  EXPECT_TRUE(GetShownNudge(id));

  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeLongDuration);
  EXPECT_FALSE(GetShownNudge(id));
}

// Tests that nudge's default duration is updated to medium duration if the
// nudge has a long body text or a button.
TEST_F(AnchoredNudgeManagerImplTest, NudgeDefaultDurationIsUpdated) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  const std::u16string long_body_text =
      u"This is just a body text that has more than sixty characters.";
  const std::u16string primary_button_text = u"first";
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge with default duration.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // The nudge should expire after `kNudgeDefaultDuration`.
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeDefaultDuration + base::Seconds(1));
  EXPECT_FALSE(GetShownNudge(id));

  // Add a long body text and show the nudge again.
  ASSERT_GE(static_cast<int>(long_body_text.length()),
            AnchoredNudgeManagerImpl::kLongBodyTextLength);
  nudge_data.body_text = long_body_text;
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // The nudge should not expire after `kNudgeDefaultDuration` has passed, but
  // will expire after `kNudgeMediumDuration` since it has a long body text.
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeDefaultDuration + base::Seconds(1));
  EXPECT_TRUE(GetShownNudge(id));
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeMediumDuration);
  EXPECT_FALSE(GetShownNudge(id));

  // Clear body text, add a button and show the nudge again.
  nudge_data.body_text = std::u16string();
  nudge_data.primary_button_text = primary_button_text;
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // The nudge should not expire after `kNudgeDefaultDuration` has passed, but
  // will expire after `kNudgeMediumDuration` since it has a button.
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeDefaultDuration + base::Seconds(1));
  EXPECT_TRUE(GetShownNudge(id));
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeMediumDuration);
  EXPECT_FALSE(GetShownNudge(id));

  // Set the duration to long and show the nudge again.
  nudge_data.duration = NudgeDuration::kLongDuration;
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // Ensure the duration doesn't update back to medium if it was set to long.
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeMediumDuration + base::Seconds(1));
  EXPECT_TRUE(GetShownNudge(id));
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeLongDuration);
  EXPECT_FALSE(GetShownNudge(id));
}

// Tests that nudges are destroyed on session state changes.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_OnSessionStateChanged) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // Lock screen, nudge should have closed.
  SetLockedState(true);
  EXPECT_FALSE(GetShownNudge(id));

  // Show a nudge in the locked state.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // Unlock screen, nudge should have closed.
  SetLockedState(false);
  EXPECT_FALSE(GetShownNudge(id));
}

// Tests that the nudge widget closes after its hide animation is completed.
TEST_F(AnchoredNudgeManagerImplTest, NudgeCloses_OnHideAnimationComplete) {
  // Set animations to last a non-zero, faster than normal duration, since the
  // regular duration may last longer in tests and cause flakiness.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // Cancel will trigger the hide animation, the nudge should still exist.
  GetAnchoredNudgeManager()->Cancel(id);
  EXPECT_TRUE(GetShownNudge(id));

  // Attempt cancelling the nudge while hide animation is in progress, these
  // calls should be ignored.
  GetAnchoredNudgeManager()->Cancel(id);
  GetAnchoredNudgeManager()->Cancel(id);

  // Fast forward to complete hide animation, the nudge should have closed.
  task_environment()->FastForwardBy(kAnimationSettleDownDuration);
  ASSERT_FALSE(GetShownNudge(id));
}

TEST_F(AnchoredNudgeManagerImplTest, NudgeHideAnimationInterrupted_OnShutdown) {
  // Set animations to last their normal duration.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // Cancel will trigger the hide animation, the nudge should still exist.
  GetAnchoredNudgeManager()->Cancel(id);
  EXPECT_TRUE(GetShownNudge(id));

  // Nudge animation is interrupted on shutdown, no crash.
}

TEST_F(AnchoredNudgeManagerImplTest,
       NudgeHideAnimationInterrupted_OnNudgeReplaced) {
  // Set animations to last their normal duration.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // Cancel will trigger the hide animation, the nudge should still exist.
  GetAnchoredNudgeManager()->Cancel(id);
  EXPECT_TRUE(GetShownNudge(id));

  // Attempt showing the same nudge again immediately. The hide animation should
  // be interrupted, and the nudge will be replaced.
  GetAnchoredNudgeManager()->Show(nudge_data);
  task_environment()->FastForwardBy(kAnimationSettleDownDuration);
  EXPECT_TRUE(GetShownNudge(id));
}

TEST_F(AnchoredNudgeManagerImplTest,
       NudgeHideAnimationInterrupted_OnScopedPauseAdded) {
  // Set animations to last their normal duration.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // Cancel will trigger the hide animation, the nudge should still exist.
  GetAnchoredNudgeManager()->Cancel(id);
  EXPECT_TRUE(GetShownNudge(id));

  // Create a scoped nudge pause right after, which will close the nudge
  // immediately interrupting its hide animation.
  GetAnchoredNudgeManager()->CreateScopedPause();
  EXPECT_FALSE(GetShownNudge(id));
}

// Tests that the dismiss timer is paused on hover so the nudge won't close.
TEST_F(AnchoredNudgeManagerImplTest, NudgePersists_OnHover) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  GetAnchoredNudgeManager()->Show(nudge_data);
  AnchoredNudge* nudge = GetShownNudge(id);
  EXPECT_TRUE(nudge);

  // Wait for half of the nudge's duration.
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeDefaultDuration / 2);

  // Hover on the nudge and wait for its full duration. It should persist.
  GetEventGenerator()->MoveMouseTo(
      GetShownNudge(id)->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(nudge->IsMouseHovered());
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeDefaultDuration);
  EXPECT_TRUE(nudge);

  // Hover out of the nudge and wait its duration. It should be dismissed.
  GetEventGenerator()->MoveMouseTo(gfx::Point(-100, -100));
  EXPECT_FALSE(nudge->IsMouseHovered());
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeDefaultDuration / 2 + base::Seconds(1));
  EXPECT_FALSE(GetShownNudge(id));
}

// Tests that the dismiss timer is paused when one of the nudge's children is
// focused so the nudge won't close.
TEST_F(AnchoredNudgeManagerImplTest, NudgePersists_OnFocus) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge with a button so the nudge has a focusable child.
  const std::string id("id");
  auto nudge_data = CreateBaseNudgeData(id, /*anchor_view=*/nullptr);
  nudge_data.primary_button_text = u"button";

  // Show a nudge with a button.
  GetAnchoredNudgeManager()->Show(nudge_data);
  ASSERT_TRUE(GetShownNudge(id));
  auto* button = GetNudgePrimaryButton(id);
  ASSERT_TRUE(button);

  // Focus on the nudge's button and wait for its full duration times two. It
  // should persist.
  button->RequestFocus();
  EXPECT_TRUE(button->HasFocus());
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeDefaultDuration * 2);
  EXPECT_TRUE(GetShownNudge(id));

  // Focus out of the nudge and wait its full duration times two. It should be
  // dismissed.
  button->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(button->HasFocus());
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeDefaultDuration * 2);
  EXPECT_FALSE(GetShownNudge(id));
}

// Tests that attempting to cancel a nudge with an invalid `id` should not
// have any effects.
TEST_F(AnchoredNudgeManagerImplTest, CancelNudgeWhichDoesNotExist) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  const std::string id_2("id_2");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Show a nudge.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));

  // Attempt to cancel nudge with an `id` that does not exist. Should not have
  // any effect.
  CancelNudge(id_2);
  EXPECT_TRUE(GetShownNudge(id));

  // Cancel the shown nudge with its valid `id`.
  CancelNudge(id);
  EXPECT_FALSE(GetShownNudge(id));

  // Attempt to cancel the same nudge again. Should not have any effect.
  CancelNudge(id);
  EXPECT_FALSE(GetShownNudge(id));
}

TEST_F(AnchoredNudgeManagerImplTest, ShownCountMetric) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  histogram_tester.ExpectBucketCount(kNudgeShownCount, kTestCatalogName, 0);

  GetAnchoredNudgeManager()->Show(nudge_data);
  histogram_tester.ExpectBucketCount(kNudgeShownCount, kTestCatalogName, 1);

  GetAnchoredNudgeManager()->Show(nudge_data);
  GetAnchoredNudgeManager()->Show(nudge_data);
  histogram_tester.ExpectBucketCount(kNudgeShownCount, kTestCatalogName, 3);
}

TEST_F(AnchoredNudgeManagerImplTest, TimeToActionMetric) {
  base::HistogramTester histogram_tester;
  GetAnchoredNudgeManager()->ResetNudgeRegistryForTesting();
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto* anchor_view = widget->SetContentsView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);

  // Metric is not recorded if the nudge has not been shown.
  GetAnchoredNudgeManager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1m,
                                     kTestCatalogName, 0);

  // Metric is recorded if the action is performed after the nudge was shown.
  GetAnchoredNudgeManager()->Show(nudge_data);
  task_environment()->FastForwardBy(base::Seconds(1));
  GetAnchoredNudgeManager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1m,
                                     kTestCatalogName, 1);

  // Metric is not recorded if the nudge action is performed again without
  // another nudge being shown.
  GetAnchoredNudgeManager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1m,
                                     kTestCatalogName, 1);

  // Metric is recorded with the appropriate time range after showing nudge
  // again and waiting enough time to fall into the "Within1h" time bucket.
  GetAnchoredNudgeManager()->Show(nudge_data);
  task_environment()->FastForwardBy(base::Minutes(2));
  GetAnchoredNudgeManager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1h,
                                     kTestCatalogName, 1);

  // Metric is not recorded if the nudge action is performed again without
  // another nudge being shown.
  GetAnchoredNudgeManager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithin1h,
                                     kTestCatalogName, 1);

  // Metric is recorded with the appropriate time range after showing nudge
  // again and waiting enough time to fall into the "WithinSession" time bucket.
  GetAnchoredNudgeManager()->Show(nudge_data);
  task_environment()->FastForwardBy(base::Hours(2));
  GetAnchoredNudgeManager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithinSession,
                                     kTestCatalogName, 1);

  // Metric is not recorded if the nudge action is performed again without
  // another nudge being shown.
  GetAnchoredNudgeManager()->MaybeRecordNudgeAction(kTestCatalogName);
  histogram_tester.ExpectBucketCount(kNudgeTimeToActionWithinSession,
                                     kTestCatalogName, 1);
}

// Tests that a nudge is parented to its anchor view, which has a widget.
TEST_F(AnchoredNudgeManagerImplTest, SetParent_AnchorViewWithWidget) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto contents_view = std::make_unique<views::View>();
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  widget->SetContentsView(contents_view.get());

  auto nudge_data = CreateBaseNudgeData(id, anchor_view);
  nudge_data.set_anchor_view_as_parent = true;

  // Anchor view exists, the nudge should be created and parented by it.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));
  EXPECT_EQ(GetNudgeIfShown(id)->GetWidget()->GetNativeWindow()->parent(),
            anchor_view->GetWidget()->GetNativeView()->parent());
}

// Tests that a nudge is not parented to its anchor view if
// `set_anchor_view_as_parent` is not set to true.
TEST_F(AnchoredNudgeManagerImplTest, NotSetParent_AnchorViewWithWidget) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up nudge data contents.
  const std::string id("id");
  auto contents_view = std::make_unique<views::View>();
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  widget->SetContentsView(contents_view.get());

  auto nudge_data = CreateBaseNudgeData(id, anchor_view);
  nudge_data.set_anchor_view_as_parent = false;

  // Anchor view exists, the nudge should be created but not parented by it.
  GetAnchoredNudgeManager()->Show(nudge_data);
  EXPECT_TRUE(GetShownNudge(id));
  EXPECT_NE(GetNudgeIfShown(id)->GetWidget()->GetNativeWindow()->parent(),
            anchor_view->GetWidget()->GetNativeView()->parent());
}

// Tests that a nudge is not created if its anchor view doesn't have a widget
// but `set_anchor_view_as_parent` is set to true.
TEST_F(AnchoredNudgeManagerImplTest, SetParent_AnchorViewWithoutWidget) {
  // Set up nudge data contents.
  const std::string id("id");
  auto contents_view = std::make_unique<views::View>();
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  auto nudge_data = CreateBaseNudgeData(id, anchor_view);
  nudge_data.set_anchor_view_as_parent = true;

  // Attempt to show nudge.
  GetAnchoredNudgeManager()->Show(nudge_data);

  // Anchor view does not have a widget, the nudge should not be created.
  EXPECT_FALSE(GetShownNudge(id));
}

// Tests that a nudge receives focus when it has buttons, and skips focus
// traversal when it has no buttons.
TEST_F(AnchoredNudgeManagerImplTest, FocusTraversable) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set widget contents.
  views::View* view1;
  views::View* view2;
  views::View* view3;
  widget->SetContentsView(
      views::Builder<views::FlexLayoutView>()
          .AddChildren(
              views::Builder<views::LabelButton>()
                  .CopyAddressTo(&view1)
                  .SetFocusBehavior(views::View::FocusBehavior::ALWAYS),
              views::Builder<views::LabelButton>()
                  .CopyAddressTo(&view2)
                  .SetFocusBehavior(views::View::FocusBehavior::ALWAYS),
              views::Builder<views::LabelButton>()
                  .CopyAddressTo(&view3)
                  .SetFocusBehavior(views::View::FocusBehavior::ALWAYS))
          .Build());

  // Setup a nudge without buttons and set `view2` as the anchor view.
  const std::string id("id");
  auto nudge_data = CreateBaseNudgeData(id, view2);
  GetAnchoredNudgeManager()->Show(nudge_data);

  // Focus traversal should skip the nudge since it has no buttons.
  PressTab();
  EXPECT_EQ(widget->GetFocusManager()->GetFocusedView(), view1);
  PressTab();
  EXPECT_EQ(widget->GetFocusManager()->GetFocusedView(), view2);
  PressTab();
  EXPECT_EQ(widget->GetFocusManager()->GetFocusedView(), view3);

  // When a nudge has buttons, focus traversal should go into them and cycle
  // until the nudge is dismissed.
  nudge_data.primary_button_text = u"button";
  nudge_data.secondary_button_text = u"button";
  GetAnchoredNudgeManager()->Show(nudge_data);
  PressTab();
  EXPECT_EQ(widget->GetFocusManager()->GetFocusedView(), view1);
  PressTab();
  EXPECT_EQ(widget->GetFocusManager()->GetFocusedView(), view2);
  PressTab();

  auto* nudge_widget = GetShownNudge(id)->GetWidget();
  EXPECT_EQ(nudge_widget->GetFocusManager()->GetFocusedView(),
            GetNudgeSecondaryButton(id));
  PressTab();
  EXPECT_EQ(nudge_widget->GetFocusManager()->GetFocusedView(),
            GetNudgePrimaryButton(id));
  PressTab();
  EXPECT_EQ(nudge_widget->GetFocusManager()->GetFocusedView(),
            GetNudgeSecondaryButton(id));

  GetAnchoredNudgeManager()->Cancel(id);
  EXPECT_EQ(widget->GetFocusManager()->GetFocusedView(), view2);
}

// Tests that a nudge is anchored at the bottom left corner of its anchor
// widget.
TEST_F(AnchoredNudgeManagerImplTest, AnchorInsideWidget_BottomLeft) {
  std::unique_ptr<views::Widget> anchor_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  anchor_widget->SetBounds(
      gfx::Rect(gfx::Point(100, 100), gfx::Size(300, 200)));

  // Set up nudge data contents.
  const std::string id("id");
  auto nudge_data = CreateBaseNudgeData(id, /*anchor_view=*/nullptr);
  nudge_data.anchor_widget = anchor_widget.get();
  nudge_data.arrow = views::BubbleBorder::Arrow::BOTTOM_LEFT;

  // `anchor_widget` exists, will anchor inside the widget.
  GetAnchoredNudgeManager()->Show(nudge_data);
  auto* nudge = GetShownNudge(id);
  EXPECT_TRUE(nudge);

  auto* nudge_widget = nudge->GetWidget();
  EXPECT_TRUE(nudge_widget);

  auto nudge_bounds = nudge_widget->GetWindowBoundsInScreen();
  auto anchor_widget_bounds = anchor_widget->GetWindowBoundsInScreen();

  // Compare the bounds alignment with the `kBubbleBorderInsets`.
  EXPECT_EQ(nudge_bounds.bottom_left(),
            anchor_widget_bounds.bottom_left() +
                gfx::Vector2d(kBubbleBorderInsets.left(),
                              -kBubbleBorderInsets.bottom()));
}

// Tests that a nudge is anchored at the bottom right corner of its anchor
// widget.
TEST_F(AnchoredNudgeManagerImplTest, AnchorInsideWidget_BottomRight) {
  std::unique_ptr<views::Widget> anchor_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  anchor_widget->SetBounds(
      gfx::Rect(gfx::Point(100, 100), gfx::Size(300, 200)));

  // Set up nudge data contents.
  const std::string id("id");
  auto nudge_data = CreateBaseNudgeData(id, /*anchor_view=*/nullptr);
  nudge_data.anchor_widget = anchor_widget.get();
  nudge_data.arrow = views::BubbleBorder::Arrow::BOTTOM_RIGHT;

  // `anchor_widget` exists, will anchor inside the widget.
  GetAnchoredNudgeManager()->Show(nudge_data);
  auto* nudge = GetShownNudge(id);
  EXPECT_TRUE(nudge);

  auto* nudge_widget = nudge->GetWidget();
  EXPECT_TRUE(nudge_widget);

  auto nudge_bounds = nudge_widget->GetWindowBoundsInScreen();
  auto anchor_widget_bounds = anchor_widget->GetWindowBoundsInScreen();

  // Compare the bounds alignment with the `kBubbleBorderInsets`.
  EXPECT_EQ(nudge_bounds.bottom_right(),
            anchor_widget_bounds.bottom_right() +
                gfx::Vector2d(-kBubbleBorderInsets.right(),
                              -kBubbleBorderInsets.bottom()));
}

// Tests that a nudge with an anchor widget is placed on the right on RTL.
TEST_F(AnchoredNudgeManagerImplTest, AnchorInsideWidget_WithRTL) {
  // Turn on RTL mode.
  base::i18n::SetRTLForTesting(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::i18n::IsRTL());

  std::unique_ptr<views::Widget> anchor_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  anchor_widget->SetBounds(
      gfx::Rect(gfx::Point(100, 100), gfx::Size(300, 200)));

  // Set up nudge data contents.
  const std::string id("id");
  auto nudge_data = CreateBaseNudgeData(id, /*anchor_view=*/nullptr);
  nudge_data.anchor_widget = anchor_widget.get();
  nudge_data.arrow = views::BubbleBorder::Arrow::BOTTOM_LEFT;

  // `anchor_widget` exists, will anchor inside the widget.
  GetAnchoredNudgeManager()->Show(nudge_data);
  auto* nudge = GetShownNudge(id);
  EXPECT_TRUE(nudge);

  auto* nudge_widget = nudge->GetWidget();
  EXPECT_TRUE(nudge_widget);

  auto nudge_bounds = nudge_widget->GetWindowBoundsInScreen();
  auto anchor_widget_bounds = anchor_widget->GetWindowBoundsInScreen();

  // Compare the bounds alignment with the `kBubbleBorderInsets`.
  // The nudge should be shown on the leading bottom corner of the
  // `anchor_widget`, which for RTL languages is the bottom-right.
  EXPECT_EQ(nudge_bounds.bottom_right(),
            anchor_widget_bounds.bottom_right() +
                gfx::Vector2d(-kBubbleBorderInsets.right(),
                              -kBubbleBorderInsets.bottom()));

  // Turn off RTL mode.
  base::i18n::SetRTLForTesting(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(base::i18n::IsRTL());
}

// Tests that a nudge is anchored at the bottom left corner of its anchor
// widget in the tablet mode.
TEST_F(AnchoredNudgeManagerImplTest, AnchorInsideWidget_TabletMode) {
  ash::TabletModeControllerTestApi().EnterTabletMode();

  std::unique_ptr<views::Widget> anchor_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  anchor_widget->SetBounds(
      gfx::Rect(gfx::Point(100, 100), gfx::Size(300, 200)));

  // Set up nudge data contents.
  const std::string id("id");
  auto nudge_data = CreateBaseNudgeData(id, /*anchor_view=*/nullptr);
  nudge_data.anchor_widget = anchor_widget.get();
  nudge_data.arrow = views::BubbleBorder::Arrow::BOTTOM_LEFT;

  // `anchor_widget` exists, will anchor inside the widget.
  GetAnchoredNudgeManager()->Show(nudge_data);
  auto* nudge = GetShownNudge(id);
  EXPECT_TRUE(nudge);

  auto* nudge_widget = nudge->GetWidget();
  EXPECT_TRUE(nudge_widget);

  auto nudge_bounds = nudge_widget->GetWindowBoundsInScreen();
  auto anchor_widget_bounds = anchor_widget->GetWindowBoundsInScreen();

  // Compare the bounds alignment with the `kBubbleBorderInsets`.
  EXPECT_EQ(nudge_bounds.bottom_left(),
            anchor_widget_bounds.bottom_left() +
                gfx::Vector2d(kBubbleBorderInsets.left(),
                              -kBubbleBorderInsets.bottom()));
}

// Tests that the nudge is closed when the anchor widget is closed.
TEST_F(AnchoredNudgeManagerImplTest, NudgeClosedWhenAnchorWidgetClosed) {
  std::unique_ptr<views::Widget> anchor_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  anchor_widget->SetBounds(
      gfx::Rect(gfx::Point(100, 100), gfx::Size(300, 200)));

  // Set up nudge data contents.
  const std::string id("id");
  auto nudge_data = CreateBaseNudgeData(id, /*anchor_view=*/nullptr);
  nudge_data.anchor_widget = anchor_widget.get();
  nudge_data.arrow = views::BubbleBorder::Arrow::BOTTOM_LEFT;

  // `anchor_widget` exists, will anchor inside the widget.
  GetAnchoredNudgeManager()->Show(nudge_data);
  auto* nudge = GetShownNudge(id);
  EXPECT_TRUE(nudge);

  auto* nudge_widget = nudge->GetWidget();
  EXPECT_TRUE(nudge_widget);

  auto nudge_bounds = nudge_widget->GetWindowBoundsInScreen();
  auto anchor_widget_bounds = anchor_widget->GetWindowBoundsInScreen();

  // Compare the bounds alignment with the `kBubbleBorderInsets`.
  EXPECT_EQ(nudge_bounds.bottom_left(),
            anchor_widget_bounds.bottom_left() +
                gfx::Vector2d(kBubbleBorderInsets.left(),
                              -kBubbleBorderInsets.bottom()));

  // Close the `anchor_widget` should close the nudge as well.
  anchor_widget->CloseNow();
  nudge = GetShownNudge(id);
  EXPECT_FALSE(nudge);
}

}  // namespace ash
