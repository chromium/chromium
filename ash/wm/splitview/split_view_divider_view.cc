// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_divider_view.h"

#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "base/functional/callback_helpers.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The divider handler's default / enlarged corner radius.
constexpr int kDividerHandlerCornerRadius = 1;
constexpr int kDividerHandlerEnlargedCornerRadius = 2;

// Distance between the bottom / right edge of the feedback button and the
// bottom / right of the work area.
constexpr int kFeedbackButtonDistanceFromEdge = 58;

// Feedback button size.
constexpr gfx::Size kFeedbackButtonSize{40, 40};

// Feedback button icon size.
constexpr int kFeedbackButtonIconSize = 20;

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
    : divider_(divider),
      handler_view_(AddChildView(std::make_unique<DividerHandlerView>(
          IsLayoutHorizontal(divider_->divider_widget()->GetNativeWindow())))) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  SetPaintToLayer(ui::LAYER_TEXTURED);
  layer()->SetFillsBoundsOpaquely(false);

  SetBackground(
      views::CreateThemedSolidBackground(cros_tokens::kCrosSysSecondary));
  RefreshFeedbackButton(/*visible=*/false);
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
  RefreshDividerHandler(/*should_enlarge=*/false);
  RefreshFeedbackButtonBounds();
}

void SplitViewDividerView::OnMouseEntered(const ui::MouseEvent& event) {
  divider_->EnlargeOrShrinkDivider(/*should_enlarge=*/true);

  gfx::Point screen_location = event.location();
  ConvertPointToScreen(this, &screen_location);

  // Show `feedback_button_` on mouse entered.
  if (!feedback_button_ ||
      !feedback_button_->GetBoundsInScreen().Contains(screen_location)) {
    RefreshFeedbackButton(/*visible=*/true);
  }
}

void SplitViewDividerView::OnMouseExited(const ui::MouseEvent& event) {
  gfx::Point screen_location = event.location();
  ConvertPointToScreen(this, &screen_location);

  if (handler_view_ &&
      !handler_view_->GetBoundsInScreen().Contains(screen_location) &&
      (!feedback_button_ ||
       !feedback_button_->GetBoundsInScreen().Contains(screen_location))) {
    divider_->EnlargeOrShrinkDivider(/*should_enlarge=*/false);
  }

  // Hide `feedback_button_` on mouse exited.
  if (feedback_button_ &&
      !feedback_button_->GetBoundsInScreen().Contains(screen_location)) {
    RefreshFeedbackButton(/*visible=*/false);
  }
}

bool SplitViewDividerView::OnMousePressed(const ui::MouseEvent& event) {
  gfx::Point location(event.location());
  views::View::ConvertPointToScreen(this, &location);
  initial_mouse_event_location_ = location;
  return true;
}

bool SplitViewDividerView::OnMouseDragged(const ui::MouseEvent& event) {
  CHECK(divider_);
  divider_->EnlargeOrShrinkDivider(/*should_enlarge=*/true);
  RefreshFeedbackButton(/*visible=*/false);

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

  RefreshFeedbackButton(/*visible=*/true);
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
    case ui::ET_GESTURE_TAP:
      if (event->details().tap_count() == 2) {
        SwapWindows();
      }
      break;
    case ui::ET_GESTURE_TAP_DOWN:
      divider_->EnlargeOrShrinkDivider(/*should_enlarge=*/true);
      RefreshFeedbackButton(/*visible=*/true);
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
      divider_->EnlargeOrShrinkDivider(/*should_enlarge=*/false);
      RefreshFeedbackButton(/*visible=*/false);
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      StartResizing(location);
      RefreshFeedbackButton(/*visible=*/true);
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      divider_->ResizeWithDivider(location);
      RefreshFeedbackButton(/*visible=*/true);
      break;
    case ui::ET_GESTURE_SCROLL_END:
      divider_->EnlargeOrShrinkDivider(/*should_enlarge=*/false);
      RefreshFeedbackButton(/*visible=*/false);
      break;
    case ui::ET_GESTURE_END: {
      EndResizing(location, /*swap_windows=*/false);

      // `EndResizing()` may set `divider_` to nullptr and causing crash.
      if (divider_) {
        divider_->EnlargeOrShrinkDivider(/*should_enlarge=*/false);
      }
      RefreshFeedbackButton(/*visible=*/true);
      break;
    }

    default:
      break;
  }
  event->SetHandled();
}

ui::Cursor SplitViewDividerView::GetCursor(const ui::MouseEvent& event) {
  return IsLayoutHorizontal(divider_->divider_widget()->GetNativeWindow())
             ? ui::mojom::CursorType::kColumnResize
             : ui::mojom::CursorType::kRowResize;
}

bool SplitViewDividerView::DoesIntersectRect(const views::View* target,
                                             const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  return true;
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

void SplitViewDividerView::RefreshDividerHandler(bool should_enlarge) {
  CHECK(divider_);

  const gfx::Point divider_center = bounds().CenterPoint();
  const int handler_short_side = should_enlarge
                                     ? kDividerHandlerEnlargedShortSideLength
                                     : kDividerHandlerShortSideLength;
  const int handler_long_side = should_enlarge
                                    ? kDividerHandlerEnlargedLongSideLength
                                    : kDividerHandlerLongSideLength;
  const bool is_horizontal = IsLayoutHorizontal(divider_->GetRootWindow());
  handler_view_->set_is_horizontal(is_horizontal);
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
      cros_tokens::kCrosSysOnSecondary,
      should_enlarge ? kDividerHandlerEnlargedCornerRadius
                     : kDividerHandlerCornerRadius));
  handler_view_->SetVisible(true);
}

void SplitViewDividerView::RefreshFeedbackButton(bool visible) {
  if (!IsSnapGroupEnabledInClamshellMode()) {
    return;
  }

  if (!feedback_button_) {
    feedback_button_ = AddChildView(std::make_unique<IconButton>(
        base::BindRepeating(&SplitViewDividerView::OnFeedbackButtonPressed,
                            base::Unretained(this)),
        IconButton::Type::kMediumFloating, &kFeedbackIcon,
        IDS_ASH_SNAP_GROUP_SEND_FEEDBACK,
        /*is_togglable=*/false,
        /*has_border=*/false));
    feedback_button_->SetPaintToLayer();
    feedback_button_->layer()->SetFillsBoundsOpaquely(false);
    feedback_button_->SetPreferredSize(kFeedbackButtonSize);
    feedback_button_->SetIconSize(kFeedbackButtonIconSize);
    feedback_button_->SetIconColor(cros_tokens::kCrosSysOnSecondary);
    feedback_button_->SetVisible(true);
    feedback_button_->SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSecondary, kFeedbackButtonSize.height() / 2.f));
  }

  feedback_button_->SetVisible(visible);
  RefreshFeedbackButtonBounds();
}

void SplitViewDividerView::RefreshFeedbackButtonBounds() {
  if (feedback_button_ && feedback_button_->GetVisible()) {
    const gfx::Size feedback_button_size = feedback_button_->GetPreferredSize();
    gfx::Rect feedback_button_bounds = gfx::Rect();
    if (IsLayoutHorizontal(divider_->GetRootWindow())) {
      feedback_button_bounds = gfx::Rect(
          (width() - feedback_button_size.width()) / 2.f,
          height() - feedback_button_size.height() -
              kFeedbackButtonDistanceFromEdge,
          feedback_button_size.width(), feedback_button_size.height());
    } else {
      feedback_button_bounds = gfx::Rect(
          width() - feedback_button_size.width() / 2.f -
              kFeedbackButtonDistanceFromEdge,
          height() - feedback_button_size.height() / 2,
          feedback_button_size.width(), feedback_button_size.height());
    }
    feedback_button_->SetBoundsRect(feedback_button_bounds);
  }
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

void SplitViewDividerView::OnFeedbackButtonPressed() {
  Shell::Get()->shell_delegate()->OpenFeedbackDialog(
      /*source=*/ShellDelegate::FeedbackSource::kSnapGroups,
      /*description_template=*/std::string(),
      /*category_tag=*/"FromSnapGroups");
}

BEGIN_METADATA(SplitViewDividerView)
END_METADATA

}  // namespace ash
