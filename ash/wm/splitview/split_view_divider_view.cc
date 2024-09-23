// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_divider_view.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The divider handler's default / enlarged corner radius.
constexpr int kDividerHandlerCornerRadius = 1;
constexpr int kDividerHandlerEnlargedCornerRadius = 2;

}  // namespace

// -----------------------------------------------------------------------------
// DividerHandlerView:

class DividerHandlerView : public views::View {
  METADATA_HEADER(DividerHandlerView, views::View)
 public:
  explicit DividerHandlerView(bool is_horizontal)
      : is_horizontal_(is_horizontal) {}
  DividerHandlerView(const DividerHandlerView&) = delete;
  DividerHandlerView& operator=(const DividerHandlerView&) = delete;
  ~DividerHandlerView() override = default;

  void set_is_horizontal(bool is_horizontal) { is_horizontal_ = is_horizontal; }

  // views::View:
  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return is_horizontal_ ? ui::mojom::CursorType::kColumnResize
                          : ui::mojom::CursorType::kRowResize;
  }

 private:
  bool is_horizontal_;
};

BEGIN_METADATA(DividerHandlerView)
END_METADATA

// -----------------------------------------------------------------------------
// SplitViewDividerView:

SplitViewDividerView::SplitViewDividerView(SplitViewDivider* divider)
    : divider_(divider) {
  const bool horizontal = IsLayoutHorizontal(divider_->GetRootWindow());
  handler_view_ =
      AddChildView(std::make_unique<DividerHandlerView>(horizontal));
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  SetPaintToLayer(ui::LAYER_TEXTURED);
  layer()->SetFillsBoundsOpaquely(false);

  SetBackground(
      views::CreateThemedSolidBackground(cros_tokens::kCrosSysSystemBase));

  SetBorder(std::make_unique<views::HighlightBorder>(
      /*corner_radius=*/0,
      views::HighlightBorder::Type::kHighlightBorderNoShadow));

  SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  set_allow_deactivate_on_esc(true);

  GetViewAccessibility().SetRole(ax::mojom::Role::kToolbar);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_SNAP_GROUP_DIVIDER_A11Y_NAME));
  GetViewAccessibility().SetDescription(l10n_util::GetStringUTF16(
      horizontal ? IDS_ASH_SNAP_GROUP_DIVIDER_A11Y_DESCRIPTION_HORIZONTAL
                 : IDS_ASH_SNAP_GROUP_DIVIDER_A11Y_DESCRIPTION_VERTICAL));
  TooltipTextChanged();

  views::FocusRing::Install(this);
}

SplitViewDividerView::~SplitViewDividerView() = default;

void SplitViewDividerView::OnDividerClosing() {
  divider_ = nullptr;
}

void SplitViewDividerView::SetHandlerBarVisible(bool visible) {
  handler_view_->SetVisible(visible);
}

void SplitViewDividerView::Layout(PassKey) {
  // There is no divider in clamshell split view unless the feature flag
  // `kSnapGroup` is enabled. If we are in clamshell mode without the feature
  // flag and params, then we must be transitioning from tablet mode, and the
  // divider will be destroyed and there is no need to update it.
  if (!divider_ || (!display::Screen::GetScreen()->InTabletMode() &&
                    !IsSnapGroupEnabledInClamshellMode())) {
    return;
  }

  SetBoundsRect(GetLocalBounds());
  RefreshDividerHandler();
}

void SplitViewDividerView::OnMouseEntered(const ui::MouseEvent& event) {
  divider_->EnlargeOrShrinkDivider(/*should_enlarge=*/true);

  gfx::Point screen_location = event.location();
  ConvertPointToScreen(this, &screen_location);
}

void SplitViewDividerView::OnMouseExited(const ui::MouseEvent& event) {
  gfx::Point screen_location = event.location();
  ConvertPointToScreen(this, &screen_location);

  if (handler_view_ &&
      !handler_view_->GetBoundsInScreen().Contains(screen_location)) {
    divider_->EnlargeOrShrinkDivider(/*should_enlarge=*/false);
  }
}

bool SplitViewDividerView::OnMousePressed(const ui::MouseEvent& event) {
  gfx::Point location(event.location());
  views::View::ConvertPointToScreen(this, &location);
  initial_mouse_event_location_ = location;

  divider_->EnlargeOrShrinkDivider(/*should_enlarge=*/true);

  return true;
}

bool SplitViewDividerView::OnMouseDragged(const ui::MouseEvent& event) {
  CHECK(divider_);
  divider_->EnlargeOrShrinkDivider(/*should_enlarge=*/true);

  if (!mouse_move_started_) {
    // If this is the first mouse drag event, start the resize and reset
    // `mouse_move_started_`.
    DCHECK_NE(initial_mouse_event_location_, gfx::Point());
    mouse_move_started_ = true;
    StartResizing(initial_mouse_event_location_);
    return true;
  }

  // Else continue with the resize.
  gfx::Point location(event.location());
  views::View::ConvertPointToScreen(this, &location);
  divider_->ResizeWithDivider(location);
  return true;
}

void SplitViewDividerView::OnMouseReleased(const ui::MouseEvent& event) {
  gfx::Point location(event.location());
  views::View::ConvertPointToScreen(this, &location);
  initial_mouse_event_location_ = gfx::Point();
  mouse_move_started_ = false;
  EndResizing(location, /*swap_windows=*/event.GetClickCount() == 2);
}

void SplitViewDividerView::OnGestureEvent(ui::GestureEvent* event) {
  CHECK(divider_);
  if (event->IsSynthesized()) {
    // When `divider_` is destroyed, closing the widget can cause a window
    // visibility change which will cancel active touches and dispatch a
    // synthetic touch event.
    return;
  }

  gfx::Point location(event->location());
  views::View::ConvertPointToScreen(this, &location);
  switch (event->type()) {
    case ui::EventType::kGestureTap:
      if (event->details().tap_count() == 2) {
        SwapWindows();
      }
      break;
    case ui::EventType::kGestureTapDown:
      divider_->EnlargeOrShrinkDivider(/*should_enlarge=*/true);
      break;
    case ui::EventType::kGestureTapCancel:
      divider_->EnlargeOrShrinkDivider(/*should_enlarge=*/false);
      break;
    case ui::EventType::kGestureScrollBegin:
      StartResizing(location);
      break;
    case ui::EventType::kGestureScrollUpdate:
      divider_->ResizeWithDivider(location);
      break;
    case ui::EventType::kGestureScrollEnd:
      divider_->EnlargeOrShrinkDivider(/*should_enlarge=*/false);
      break;
    case ui::EventType::kGestureEnd: {
      EndResizing(location, /*swap_windows=*/false);

      // `EndResizing()` may set `divider_` to nullptr and causing crash.
      if (divider_) {
        divider_->EnlargeOrShrinkDivider(/*should_enlarge=*/false);
      }
      break;
    }

    default:
      break;
  }
  event->SetHandled();
}

ui::Cursor SplitViewDividerView::GetCursor(const ui::MouseEvent& event) {
  return IsLayoutHorizontal(divider_->GetDividerWindow())
             ? ui::mojom::CursorType::kColumnResize
             : ui::mojom::CursorType::kRowResize;
}

void SplitViewDividerView::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::EventType::kKeyPressed) {
    return;
  }
  const bool horizontal = IsLayoutHorizontal(divider_->GetRootWindow());
  const auto key_code = event->key_code();
  switch (key_code) {
    case ui::VKEY_LEFT:
    case ui::VKEY_RIGHT:
      if (horizontal) {
        ResizeOnKeyEvent(/*left_or_top=*/key_code == ui::VKEY_LEFT, horizontal);
      }
      break;
    case ui::VKEY_UP:
    case ui::VKEY_DOWN:
      if (!horizontal) {
        ResizeOnKeyEvent(/*left_or_top=*/key_code == ui::VKEY_UP, horizontal);
      }
      break;
    default:
      break;
  }
}

bool SplitViewDividerView::DoesIntersectRect(const views::View* target,
                                             const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  return true;
}

views::View* SplitViewDividerView::GetDefaultFocusableChild() {
  return this;
}

void SplitViewDividerView::OnFocus() {
  views::AccessiblePaneView::OnFocus();

  // Explicitly set the bounds to repaint the focus ring.
  const gfx::Rect focus_bounds = GetLocalBounds();
  views::FocusRing::Get(this)->SetBoundsRect(focus_bounds);
}

gfx::Rect SplitViewDividerView::GetHandlerViewBoundsInScreenForTesting() const {
  return handler_view_->GetBoundsInScreen();
}

void SplitViewDividerView::SwapWindows() {
  CHECK(divider_);
  divider_->SwapWindows();
}

void SplitViewDividerView::StartResizing(gfx::Point location) {
  CHECK(divider_);
  divider_->StartResizeWithDivider(location);
}

void SplitViewDividerView::EndResizing(gfx::Point location, bool swap_windows) {
  CHECK(divider_);
  // `EndResizeWithDivider()` may cause this view to be destroyed.
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  divider_->EndResizeWithDivider(location);
  if (!weak_ptr) {
    return;
  }

  if (swap_windows) {
    SwapWindows();
  }
}

void SplitViewDividerView::ResizeOnKeyEvent(bool left_or_top, bool horizontal) {
  CHECK(divider_);
  base::RecordAction(base::UserMetricsAction("SnapGroups_ResizeViaKeyboard"));
  const gfx::Point start_location(GetBoundsInScreen().CenterPoint());
  StartResizing(start_location);
  const int distance = left_or_top ? -kSplitViewDividerResizeDistance
                                   : kSplitViewDividerResizeDistance;
  const gfx::Point location(
      start_location +
      gfx::Vector2d(horizontal ? distance : 0, horizontal ? 0 : distance));
  divider_->ResizeWithDivider(location);
  EndResizing(location, /*swap_windows=*/false);
  const AccessibilityAlert alert =
      horizontal ? (left_or_top ? AccessibilityAlert::SNAP_GROUP_RESIZE_LEFT
                                : AccessibilityAlert::SNAP_GROUP_RESIZE_RIGHT)
                 : (left_or_top ? AccessibilityAlert::SNAP_GROUP_RESIZE_UP
                                : AccessibilityAlert::SNAP_GROUP_RESIZE_DOWN);
  Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(alert);
}

void SplitViewDividerView::RefreshDividerHandler() {
  CHECK(divider_);

  const gfx::Rect divider_bounds = bounds();
  const bool is_horizontal = IsLayoutHorizontal(divider_->GetRootWindow());
  handler_view_->set_is_horizontal(is_horizontal);
  const int divider_short_length =
      is_horizontal ? divider_bounds.width() : divider_bounds.height();
  const bool should_enlarge =
      divider_short_length == kSplitviewDividerEnlargedShortSideLength;
  const gfx::Point divider_center = divider_bounds.CenterPoint();
  const int handler_short_side = should_enlarge
                                     ? kDividerHandlerEnlargedShortSideLength
                                     : kDividerHandlerShortSideLength;
  const int handler_long_side = should_enlarge
                                    ? kDividerHandlerEnlargedLongSideLength
                                    : kDividerHandlerLongSideLength;
  if (is_horizontal) {
    handler_view_->SetBounds(divider_center.x() - handler_short_side / 2,
                             divider_center.y() - handler_long_side / 2,
                             handler_short_side, handler_long_side);
  } else {
    handler_view_->SetBounds(divider_center.x() - handler_long_side / 2,
                             divider_center.y() - handler_short_side / 2,
                             handler_long_side, handler_short_side);
  }

  handler_view_->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysOutline, should_enlarge
                                        ? kDividerHandlerEnlargedCornerRadius
                                        : kDividerHandlerCornerRadius));
  handler_view_->SetVisible(true);
}

BEGIN_METADATA(SplitViewDividerView)
END_METADATA

}  // namespace ash
