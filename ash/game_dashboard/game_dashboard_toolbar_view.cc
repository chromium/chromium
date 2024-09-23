// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_toolbar_view.h"

#include <memory>

#include "ash/app_list/app_list_util.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/constants/ash_features.h"
#include "ash/game_dashboard/game_dashboard_context.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/game_dashboard/game_dashboard_metrics.h"
#include "ash/game_dashboard/game_dashboard_utils.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/system_shadow.h"
#include "base/check.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_handler.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/highlight_border.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Corner radius of the toolbar view.
constexpr int kCornerRadius = 20;
// Horizontal inset for the border around the toolbar.
constexpr int kHorizontalInset = 4;
// Vertical inset for the border around the toolbar.
constexpr int kVerticalInset = 4;
// Padding between children in the toolbar.
constexpr int kBetweenChildSpacing = 4;

std::unique_ptr<IconButton> CreateIconButton(
    base::RepeatingClosure callback,
    const gfx::VectorIcon* icon,
    int view_id,
    const std::u16string& text,
    bool is_togglable,
    ui::ColorId icon_color = cros_tokens::kCrosSysOnSurface) {
  // TODO(b/290696780): Update logic so the toolbar can drag from icon buttons.
  auto button = std::make_unique<IconButton>(
      std::move(callback), IconButton::Type::kMedium, icon, text,
      /*is_togglable=*/is_togglable, /*has_border=*/true);
  button->SetID(view_id);
  button->SetIconColor(icon_color);
  button->SetBackgroundColor(SK_ColorTRANSPARENT);
  return button;
}

GameDashboardToolbarSnapLocation CalculateGameDashboardToolbarSnapLocation(
    const gfx::Point& toolbar_center_point,
    const gfx::Rect& game_window_screen_bounds) {
  const auto game_window_center = game_window_screen_bounds.CenterPoint();
  if (toolbar_center_point.x() < game_window_center.x()) {
    return toolbar_center_point.y() < game_window_center.y()
               ? GameDashboardToolbarSnapLocation::kTopLeft
               : GameDashboardToolbarSnapLocation::kBottomLeft;
  }
  return toolbar_center_point.y() < game_window_center.y()
             ? GameDashboardToolbarSnapLocation::kTopRight
             : GameDashboardToolbarSnapLocation::kBottomRight;
}

GameDashboardToolbarSnapLocation GetNextHorizontalSnapLocation(
    GameDashboardToolbarSnapLocation current,
    bool going_left) {
  switch (current) {
    case GameDashboardToolbarSnapLocation::kTopLeft:
      return going_left ? current : GameDashboardToolbarSnapLocation::kTopRight;
    case GameDashboardToolbarSnapLocation::kTopRight:
      return going_left ? GameDashboardToolbarSnapLocation::kTopLeft : current;
    case GameDashboardToolbarSnapLocation::kBottomLeft:
      return going_left ? current
                        : GameDashboardToolbarSnapLocation::kBottomRight;
    case GameDashboardToolbarSnapLocation::kBottomRight:
      return going_left ? GameDashboardToolbarSnapLocation::kBottomLeft
                        : current;
  }
}

GameDashboardToolbarSnapLocation GetNextVerticalSnapLocation(
    GameDashboardToolbarSnapLocation current,
    bool going_up) {
  switch (current) {
    case GameDashboardToolbarSnapLocation::kTopLeft:
      return going_up ? current : GameDashboardToolbarSnapLocation::kBottomLeft;
    case GameDashboardToolbarSnapLocation::kTopRight:
      return going_up ? current
                      : GameDashboardToolbarSnapLocation::kBottomRight;
    case GameDashboardToolbarSnapLocation::kBottomLeft:
      return going_up ? GameDashboardToolbarSnapLocation::kTopLeft : current;
    case GameDashboardToolbarSnapLocation::kBottomRight:
      return going_up ? GameDashboardToolbarSnapLocation::kTopRight : current;
  }
}

}  // namespace

// ToolbarDragHandler is an EventHandler that keeps track of touch and mouse
// input for the purposes of determining when dragging should occur. It also is
// responsible for passing along events to notify the toolbar when a button
// click has occurred.
class ToolbarDragHandler : public ui::EventHandler {
 public:
  explicit ToolbarDragHandler(GameDashboardToolbarView* toolbar_view)
      : toolbar_view_(toolbar_view) {}
  ~ToolbarDragHandler() override = default;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    const gfx::PointF event_location =
        capture_mode_util::GetEventScreenLocation(*event);

    switch (event->type()) {
      case ui::EventType::kMousePressed:
        is_dragging_ = false;
        previous_location_in_screen_ = event_location;
        break;
      case ui::EventType::kMouseDragged:
        if (!is_dragging_) {
          // It's confirmed that the user is trying to drag rather than press a
          // button in the toolbar.
          is_dragging_ = true;
        }
        DCHECK(is_dragging_)
            << "Received OnMouseDragged event but the toolbar isn't dragging.";
        toolbar_view_->RepositionToolbar(GetOffset(event_location));
        previous_location_in_screen_ = event_location;
        break;
      case ui::EventType::kMouseReleased:
        if (!is_dragging_) {
          // Allow the toolbar to receive this event so it can handle any button
          // clicks.
          return;
        }

        // The toolbar was dragged, so consume this event to ensure the toolbar
        // button doesn't process any button clicks.
        is_dragging_ = false;
        toolbar_view_->EndDraggingToolbar(GetOffset(event_location));
        previous_location_in_screen_.SetPoint(0, 0);
        break;
      default:
        // Don't stop events from being received on any other mouse events.
        return;
    }

    event->StopPropagation();
    event->SetHandled();
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    const gfx::PointF event_location =
        capture_mode_util::GetEventScreenLocation(*event);

    switch (event->type()) {
      case ui::EventType::kGestureScrollBegin:
        is_dragging_ = true;
        previous_location_in_screen_ = event_location;
        break;
      case ui::EventType::kGestureScrollUpdate:
        DCHECK(is_dragging_)
            << "Received EventType::kGestureScrollUpdate event but the "
               "toolbar isn't dragging.";
        toolbar_view_->RepositionToolbar(GetOffset(event_location));
        previous_location_in_screen_ = event_location;
        break;
      case ui::EventType::kGestureEnd:
        if (!is_dragging_) {
          // Pass along event if it occurred outside of a dragging instance.
          return;
        }
        // Treat dragging `ui::EventType::kGestureEnd` events the same as
        // `ui::EventType::kGestureScrollEnd` events.
        [[fallthrough]];
      case ui::EventType::kGestureScrollEnd:
        DCHECK(is_dragging_) << "Attempting to call end drag logic but the "
                                "toolbar wasn't dragging. Event = "
                             << base::to_underlying(event->type());
        is_dragging_ = false;
        toolbar_view_->EndDraggingToolbar(GetOffset(event_location));
        previous_location_in_screen_.SetPoint(0, 0);
        break;
      default:
        // Don't stop events from being received on any other gesture events.
        return;
    }

    event->StopPropagation();
    event->SetHandled();
  }

 private:
  // Determines the offset from the current event and the previous event
  // location.
  gfx::Vector2d GetOffset(const gfx::PointF& event_location) const {
    return gfx::ToRoundedVector2d(event_location -
                                  previous_location_in_screen_);
  }

  // Allows this class to access `GameDashboardToolbarView` owned functions.
  const raw_ptr<GameDashboardToolbarView> toolbar_view_;

  // The location of the previous drag event in screen coordinates.
  gfx::PointF previous_location_in_screen_;

  // If the toolbar view is in the dragging state.
  bool is_dragging_ = false;
};

GameDashboardToolbarView::GameDashboardToolbarView(
    GameDashboardContext* context)
    : context_(context) {
  DCHECK(context_);
  DCHECK(context_->game_window());

  drag_handler_ = std::make_unique<ToolbarDragHandler>(this);
  AddPreTargetHandler(drag_handler_.get(), ui::EventTarget::Priority::kSystem);

  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetInsideBorderInsets(gfx::Insets::VH(kVerticalInset, kHorizontalInset));
  SetBetweenChildSpacing(kBetweenChildSpacing);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevatedOpaque, kCornerRadius));
  SetBorder(views::CreateThemedRoundedRectBorder(
      1, kCornerRadius, ui::ColorIds::kColorCrosSystemHighlightBorder));
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, SystemShadow::Type::kElevation12);
  shadow_->SetRoundedCornerRadius(kCornerRadius);

  AddShortcutTiles();
}

GameDashboardToolbarView::~GameDashboardToolbarView() = default;

void GameDashboardToolbarView::OnRecordingStarted(
    bool is_recording_game_window) {
  UpdateRecordGameButton(is_recording_game_window);
}

void GameDashboardToolbarView::OnRecordingEnded() {
  UpdateRecordGameButton(/*is_recording_game_window=*/false);
}

void GameDashboardToolbarView::RepositionToolbar(const gfx::Vector2d& offset) {
  // Verify toolbar isn't outside game window bounds.
  auto* widget = GetWidget();
  gfx::Rect current_bounds = widget->GetWindowBoundsInScreen();
  // TODO(b/295536243): Update offset to handle dragging outside game window.
  current_bounds.Offset(offset);
  capture_mode_util::AdjustBoundsWithinConfinedBounds(
      context_->game_window()->GetBoundsInScreen(), current_bounds);
  widget->SetBounds(current_bounds);
}

void GameDashboardToolbarView::EndDraggingToolbar(const gfx::Vector2d& offset) {
  RepositionToolbar(offset);
  context_->SetGameDashboardToolbarSnapLocation(
      CalculateGameDashboardToolbarSnapLocation(
          GetWidget()->GetWindowBoundsInScreen().CenterPoint(),
          context_->game_window()->GetBoundsInScreen()));
}

void GameDashboardToolbarView::UpdateViewForGameControls(
    ArcGameControlsFlag flags) {
  DCHECK(game_controls_button_);

  auto* widget = GetWidget();
  if (game_dashboard_utils::IsFlagSet(flags, ArcGameControlsFlag::kEdit)) {
    CHECK(widget);
    widget->Hide();
  } else {
    // Show the widget in an inactive state.
    if (widget) {
      // `widget` is null when this function is indirectly called from the
      // constructor.
      widget->ShowInactive();
    }

    // Update game_controls_button_.
    game_dashboard_utils::UpdateGameControlsHintButton(game_controls_button_,
                                                       flags);
  }
}

bool GameDashboardToolbarView::OnKeyPressed(const ui::KeyEvent& event) {
  const auto current_snap_location = context_->toolbar_snap_location();
  const auto event_key_code = event.key_code();
  switch (event_key_code) {
    case ui::VKEY_LEFT:
    case ui::VKEY_RIGHT:
      context_->SetGameDashboardToolbarSnapLocation(
          GetNextHorizontalSnapLocation(
              current_snap_location,
              /*going_left=*/event_key_code == ui::VKEY_LEFT));
      return true;
    case ui::VKEY_UP:
    case ui::VKEY_DOWN:
      context_->SetGameDashboardToolbarSnapLocation(GetNextVerticalSnapLocation(
          current_snap_location, /*going_up=*/event_key_code == ui::VKEY_UP));
      return true;
    default:
      return false;
  }
}

bool GameDashboardToolbarView::OnKeyReleased(const ui::KeyEvent& event) {
  return IsArrowKeyEvent(event);
}

void GameDashboardToolbarView::OnGamepadButtonPressed() {
  is_expanded_ = !is_expanded_;
  for (View* child : children()) {
    if (child != gamepad_button_) {
      child->SetVisible(is_expanded_);
    }
  }
  UpdateGamepadButtonTooltipText();
  RecordGameDashboardToolbarClickToExpandState(context_->app_id(),
                                               is_expanded_);
  context_->MaybeUpdateToolbarWidgetBounds();
}

void GameDashboardToolbarView::OnGameControlsButtonPressed() {
  auto* game_window = context_->game_window();
  const bool was_toggled = game_controls_button_->toggled();
  game_window->SetProperty(
      kArcGameControlsFlagsKey,
      game_dashboard_utils::UpdateFlag(
          game_window->GetProperty(kArcGameControlsFlagsKey),
          static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kHint),
          /*enable_flag=*/!was_toggled));
  RecordGameDashboardControlsHintToggleSource(
      context_->app_id(), GameDashboardMenu::kToolbar, !was_toggled);
}

void GameDashboardToolbarView::OnRecordButtonPressed() {
  context_->set_recording_from_main_menu(false);

  if (record_game_button_->toggled()) {
    CaptureModeController::Get()->EndVideoRecording(
        EndRecordingReason::kGameToolbarStopRecordingButton);
  } else {
    GameDashboardController::Get()->StartCaptureSession(context_);
  }
}

void GameDashboardToolbarView::OnScreenshotButtonPressed() {
  CaptureModeController::Get()->CaptureScreenshotOfGivenWindow(
      context_->game_window());

  RecordGameDashboardScreenshotTakeSource(context_->app_id(),
                                          GameDashboardMenu::kToolbar);
}

void GameDashboardToolbarView::AddShortcutTiles() {
  // The gamepad button should always be the first icon added to the toolbar.
  gamepad_button_ = AddChildView(CreateIconButton(
      base::BindRepeating(&GameDashboardToolbarView::OnGamepadButtonPressed,
                          base::Unretained(this)),
      &kGdToolbarIcon, base::to_underlying(ToolbarViewId::kGamepadButton),
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_TOOLBAR_TILE_BUTTON_TITLE),
      /*is_togglable=*/false, /*icon_color=*/cros_tokens::kCrosSysPrimary));
  gamepad_button_->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  UpdateGamepadButtonTooltipText();
  MayAddGameControlsTile();

  if (base::FeatureList::IsEnabled(
          features::kFeatureManagementGameDashboardRecordGame)) {
    record_game_button_ = AddChildView(CreateIconButton(
        base::BindRepeating(&GameDashboardToolbarView::OnRecordButtonPressed,
                            base::Unretained(this)),
        &kGdRecordGameIcon,
        base::to_underlying(ToolbarViewId::kScreenRecordButton),
        l10n_util::GetStringUTF16(
            IDS_ASH_GAME_DASHBOARD_RECORD_GAME_TILE_BUTTON_TITLE),
        /*is_togglable=*/true));
    record_game_button_->GetViewAccessibility().SetRole(
        ax::mojom::Role::kButton);
    record_game_button_->SetVectorIcon(kGdRecordGameIcon);
    record_game_button_->SetBackgroundToggledColor(cros_tokens::kCrosSysError);
    record_game_button_->SetToggledVectorIcon(kCaptureModeCircleStopIcon);
    record_game_button_->SetIconToggledColor(cros_tokens::kCrosSysOnError);
    UpdateRecordGameButton(
        GameDashboardController::Get()->active_recording_context() == context_);
  }

  auto* screenshot_button = AddChildView(CreateIconButton(
      base::BindRepeating(&GameDashboardToolbarView::OnScreenshotButtonPressed,
                          base::Unretained(this)),
      &kGdScreenshotIcon, base::to_underlying(ToolbarViewId::kScreenshotButton),
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_SCREENSHOT_TILE_BUTTON_TITLE),
      /*is_togglable=*/false));
  screenshot_button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_GAME_DASHBOARD_SCREENSHOT_TILE_BUTTON_TITLE));
}

void GameDashboardToolbarView::MayAddGameControlsTile() {
  auto flags =
      game_dashboard_utils::GetGameControlsFlag(context_->game_window());
  if (!flags) {
    return;
  }

  game_controls_button_ = AddChildView(CreateIconButton(
      base::BindRepeating(
          &GameDashboardToolbarView::OnGameControlsButtonPressed,
          base::Unretained(this)),
      /*icon=*/&kGdGameControlsIcon,
      base::to_underlying(ToolbarViewId::kGameControlsButton),
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_CONTROLS_TILE_BUTTON_TITLE),
      /*is_togglable=*/true));
  game_controls_button_->GetViewAccessibility().SetRole(
      ax::mojom::Role::kButton);

  UpdateViewForGameControls(*flags);
}

void GameDashboardToolbarView::UpdateRecordGameButton(
    bool is_recording_game_window) {
  if (!record_game_button_) {
    return;
  }

  record_game_button_->SetEnabled(
      is_recording_game_window ||
      CaptureModeController::Get()->can_start_new_recording());
  record_game_button_->SetToggled(is_recording_game_window);
  record_game_button_->SetTooltipText(l10n_util::GetStringUTF16(
      is_recording_game_window
          ? IDS_ASH_GAME_DASHBOARD_RECORD_GAME_TILE_TOOLTIPS_RECORD_STOP
          : IDS_ASH_GAME_DASHBOARD_RECORD_GAME_TILE_TOOLTIPS_RECORD_START));
}

void GameDashboardToolbarView::UpdateGamepadButtonTooltipText() {
  gamepad_button_->SetTooltipText(l10n_util::GetStringUTF16(
      is_expanded_
          ? IDS_ASH_GAME_DASHBOARD_TOOLBAR_TILE_TOOLTIPS_CLOSE_TOOLBAR
          : IDS_ASH_GAME_DASHBOARD_TOOLBAR_TILE_TOOLTIPS_OPEN_TOOLBAR));
}

BEGIN_METADATA(GameDashboardToolbarView)
END_METADATA

}  // namespace ash
