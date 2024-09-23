// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_resizer.h"

#include <optional>

#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/frame_header.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/resize_utils.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

using ::chromeos::FrameHeader;

// Returns true for resize components along the right edge, where a drag in
// positive x will make the window larger.
bool IsRightEdge(int window_component) {
  return window_component == HTTOPRIGHT || window_component == HTRIGHT ||
         window_component == HTBOTTOMRIGHT || window_component == HTGROWBOX;
}

bool IsBottomEdge(int window_component) {
  return window_component == HTBOTTOMLEFT || window_component == HTBOTTOM ||
         window_component == HTBOTTOMRIGHT || window_component == HTGROWBOX;
}

// Convert |window_component| to the ResizeEdge used in
// gfx::SizeRectToAspectRatio().
gfx::ResizeEdge GetWindowResizeEdge(int window_component) {
  switch (window_component) {
    case HTBOTTOM:
      return gfx::ResizeEdge::kBottom;
    case HTTOP:
      return gfx::ResizeEdge::kTop;
    case HTLEFT:
      return gfx::ResizeEdge::kLeft;
    case HTRIGHT:
      return gfx::ResizeEdge::kRight;
    case HTTOPLEFT:
      return gfx::ResizeEdge::kTopLeft;
    case HTTOPRIGHT:
      return gfx::ResizeEdge::kTopRight;
    case HTBOTTOMLEFT:
      return gfx::ResizeEdge::kBottomLeft;
    case HTBOTTOMRIGHT:
      return gfx::ResizeEdge::kBottomRight;
    default:
      NOTREACHED();
  }
}

}  // namespace

// static
const int WindowResizer::kBoundsChange_None = 0;
// static
const int WindowResizer::kBoundsChange_Repositions = 1;
// static
const int WindowResizer::kBoundsChange_Resizes = 2;

// static
const int WindowResizer::kBoundsChangeDirection_None = 0;
// static
const int WindowResizer::kBoundsChangeDirection_Horizontal = 1;
// static
const int WindowResizer::kBoundsChangeDirection_Vertical = 2;

WindowResizer::WindowResizer(WindowState* window_state)
    : window_state_(window_state) {
  DCHECK(window_state_->drag_details());
}

WindowResizer::~WindowResizer() = default;

// static
int WindowResizer::GetBoundsChangeForWindowComponent(int component) {
  int bounds_change = WindowResizer::kBoundsChange_None;
  switch (component) {
    case HTTOPLEFT:
    case HTTOP:
    case HTTOPRIGHT:
    case HTLEFT:
    case HTBOTTOMLEFT:
      bounds_change |= WindowResizer::kBoundsChange_Repositions |
                       WindowResizer::kBoundsChange_Resizes;
      break;
    case HTCAPTION:
      bounds_change |= WindowResizer::kBoundsChange_Repositions;
      break;
    case HTRIGHT:
    case HTBOTTOMRIGHT:
    case HTBOTTOM:
    case HTGROWBOX:
      bounds_change |= WindowResizer::kBoundsChange_Resizes;
      break;
    default:
      break;
  }
  return bounds_change;
}

// static
int WindowResizer::GetPositionChangeDirectionForWindowComponent(
    int window_component) {
  int pos_change_direction = WindowResizer::kBoundsChangeDirection_None;
  switch (window_component) {
    case HTTOPLEFT:
    case HTBOTTOMRIGHT:
    case HTGROWBOX:
    case HTCAPTION:
      pos_change_direction |= WindowResizer::kBoundsChangeDirection_Horizontal |
                              WindowResizer::kBoundsChangeDirection_Vertical;
      break;
    case HTTOP:
    case HTTOPRIGHT:
    case HTBOTTOM:
      pos_change_direction |= WindowResizer::kBoundsChangeDirection_Vertical;
      break;
    case HTBOTTOMLEFT:
    case HTRIGHT:
    case HTLEFT:
      pos_change_direction |= WindowResizer::kBoundsChangeDirection_Horizontal;
      break;
    default:
      break;
  }
  return pos_change_direction;
}

aura::Window* WindowResizer::GetTarget() const {
  return window_state_ ? window_state_->window() : nullptr;
}

gfx::Rect WindowResizer::CalculateBoundsForDrag(
    const gfx::PointF& passed_location) {
  if (!details().is_resizable)
    return details().initial_bounds_in_parent;

  gfx::PointF location = passed_location;
  int delta_x = location.x() - details().initial_location_in_parent.x();
  int delta_y = location.y() - details().initial_location_in_parent.y();

  AdjustDeltaForTouchResize(&delta_x, &delta_y);

  // The minimize size constraint may limit how much we change the window
  // position.  For example, dragging the left edge to the right should stop
  // repositioning the window when the minimize size is reached.
  const gfx::Size size = GetSizeForDrag(&delta_x, &delta_y);
  gfx::Point origin = GetOriginForDrag(delta_x, delta_y, passed_location);
  gfx::Rect new_bounds(origin, size);

  gfx::SizeF* aspect_ratio_size =
      GetTarget()->GetProperty(aura::client::kAspectRatio);
  if (details().bounds_change & kBoundsChange_Resizes && aspect_ratio_size &&
      !aspect_ratio_size->IsEmpty()) {
    float aspect_ratio =
        aspect_ratio_size->width() / aspect_ratio_size->height();
    CalculateBoundsWithAspectRatio(aspect_ratio, &new_bounds);
    return new_bounds;
  }

  // Sizing has to keep the result on the screen. Note that this correction
  // has to come first since it might have an impact on the origin as well as
  // on the size.
  if (details().bounds_change & kBoundsChange_Resizes) {
    gfx::Rect work_area = display::Screen::GetScreen()
                              ->GetDisplayNearestWindow(GetTarget())
                              .work_area();
    ::wm::ConvertRectFromScreen(GetTarget()->parent(), &work_area);
    if (details().size_change_direction & kBoundsChangeDirection_Horizontal) {
      if (IsRightEdge(details().window_component) &&
          new_bounds.right() < work_area.x() + kMinimumOnScreenArea) {
        int delta = work_area.x() + kMinimumOnScreenArea - new_bounds.right();
        new_bounds.set_width(new_bounds.width() + delta);
      } else if (new_bounds.x() > work_area.right() - kMinimumOnScreenArea) {
        int width =
            new_bounds.right() - work_area.right() + kMinimumOnScreenArea;
        new_bounds.set_x(work_area.right() - kMinimumOnScreenArea);
        new_bounds.set_width(width);
      }
    }
    if (details().size_change_direction & kBoundsChangeDirection_Vertical) {
      if (!IsBottomEdge(details().window_component) &&
          new_bounds.y() > work_area.bottom() - kMinimumOnScreenArea) {
        int height =
            new_bounds.bottom() - work_area.bottom() + kMinimumOnScreenArea;
        new_bounds.set_y(work_area.bottom() - kMinimumOnScreenArea);
        new_bounds.set_height(height);
      } else if (details().window_component == HTBOTTOM ||
                 details().window_component == HTBOTTOMRIGHT ||
                 details().window_component == HTBOTTOMLEFT) {
        // Update bottom edge to stay in the work area when we are resizing
        // by dragging the bottom edge or corners.
        if (new_bounds.bottom() > work_area.bottom())
          new_bounds.Inset(gfx::Insets::TLBR(
              0, 0, new_bounds.bottom() - work_area.bottom(), 0));
      }
    }
    if (details().bounds_change & kBoundsChange_Repositions &&
        new_bounds.y() < 0) {
      int delta = new_bounds.y();
      new_bounds.set_y(0);
      new_bounds.set_height(new_bounds.height() + delta);
    }
  }

  if (details().bounds_change & kBoundsChange_Repositions) {
    // Make sure that |new_bounds| doesn't leave any of the displays.  Note that
    // the |work_area| above isn't good for this check since it is the work area
    // for the current display but the window can move to a different one.
    aura::Window* parent = GetTarget()->parent();
    gfx::PointF passed_location_in_screen(passed_location);
    ::wm::ConvertPointToScreen(parent, &passed_location_in_screen);
    gfx::Rect near_passed_location(
        gfx::ToRoundedPoint(passed_location_in_screen), gfx::Size());
    // Use a pointer location (matching the logic in DragWindowResizer) to
    // calculate the target display after the drag.
    const display::Display& display =
        display::Screen::GetScreen()->GetDisplayMatching(near_passed_location);
    gfx::Rect screen_work_area = display.work_area();
    screen_work_area.Inset(gfx::Insets::VH(0, kMinimumOnScreenArea));
    gfx::Rect new_bounds_in_screen(new_bounds);
    ::wm::ConvertRectToScreen(parent, &new_bounds_in_screen);
    if (!screen_work_area.Intersects(new_bounds_in_screen)) {
      // Make sure that the x origin does not leave the current display.
      new_bounds_in_screen.set_x(std::max(
          screen_work_area.x() - new_bounds.width(),
          std::min(screen_work_area.right(), new_bounds_in_screen.x())));
      new_bounds = new_bounds_in_screen;
      ::wm::ConvertRectFromScreen(parent, &new_bounds);
    }
  }

  return new_bounds;
}

void WindowResizer::SetBoundsDuringResize(const gfx::Rect& bounds) {
  aura::Window* window = GetTarget();
  DCHECK(window);

  auto ptr = weak_ptr_factory_.GetWeakPtr();
  const gfx::Size original_size = window->bounds().size();

  // Prepare to record presentation time (e.g. tracking Configure).
  if (recorder_)
    recorder_->PrepareToRecord();

  window->SetBounds(bounds);

  // Resizer can be destroyed when a window is attached during tab dragging.
  // crbug.com/970911.
  if (!ptr)
    return;

  // Using `window->bounds()` instead of `bounds` to check size change because
  // whether "window->SetBounds()" could reject a bounds change. And when that
  // happens, there might be no new frames presented on screen.
  if (window->bounds().size() == original_size)
    return;

  if (recorder_)
    recorder_->RequestNext();
}

void WindowResizer::SetTransformDuringResize(const gfx::Transform& transform) {
  aura::Window* window = GetTarget();
  DCHECK(window);

  const gfx::Transform original_transform = window->transform();

  // Prepare to record presentation time (e.g. tracking Configure).
  if (recorder_) {
    recorder_->PrepareToRecord();
  }

  window->SetTransform(transform);

  if (window->transform() == original_transform) {
    return;
  }

  if (recorder_) {
    recorder_->RequestNext();
  }
}

void WindowResizer::SetPresentationTimeRecorder(
    std::unique_ptr<PresentationTimeRecorder> recorder) {
  recorder_ = std::move(recorder);
}

void WindowResizer::AdjustDeltaForTouchResize(int* delta_x, int* delta_y) {
  if (details().source != ::wm::WINDOW_MOVE_SOURCE_TOUCH ||
      !(details().bounds_change & kBoundsChange_Resizes))
    return;

  if (details().size_change_direction & kBoundsChangeDirection_Horizontal) {
    if (IsRightEdge(details().window_component)) {
      *delta_x += details().initial_location_in_parent.x() -
                  details().initial_bounds_in_parent.right();
    } else {
      *delta_x += details().initial_location_in_parent.x() -
                  details().initial_bounds_in_parent.x();
    }
  }
  if (details().size_change_direction & kBoundsChangeDirection_Vertical) {
    if (IsBottomEdge(details().window_component)) {
      *delta_y += details().initial_location_in_parent.y() -
                  details().initial_bounds_in_parent.bottom();
    } else {
      *delta_y += details().initial_location_in_parent.y() -
                  details().initial_bounds_in_parent.y();
    }
  }
}

gfx::Point WindowResizer::GetOriginForDrag(int delta_x,
                                           int delta_y,
                                           const gfx::PointF& event_location) {
  gfx::Point origin = details().initial_bounds_in_parent.origin();
  if (!(details().bounds_change & kBoundsChange_Repositions))
    return origin;

  int pos_change_direction =
      GetPositionChangeDirectionForWindowComponent(details().window_component);
  if (pos_change_direction & kBoundsChangeDirection_Horizontal)
    origin.Offset(delta_x, 0);
  if (pos_change_direction & kBoundsChangeDirection_Vertical)
    origin.Offset(0, delta_y);

  // If the window gets repositioned and changes to it's restored bounds,
  // modify the origin so that the cursor remains within the dragged window.
  // The ratio of the new origin to the new location should match the ratio
  // from the initial origin to the initial location. Floated windows do not
  // change to their restore bounds while dragging, so we treat them as if they
  // had no restore bounds.
  const gfx::Rect restore_bounds = details().restore_bounds_in_parent;
  if (restore_bounds.IsEmpty() || window_state_->IsFloated())
    return origin;

  // The ratios that should match is the (drag location x - bounds origin x) /
  // bounds width.
  const float ratio =
      (details().initial_location_in_parent.x() -
       static_cast<float>(details().initial_bounds_in_parent.x())) /
      details().initial_bounds_in_parent.width();
  int new_origin_x =
      base::ClampRound(event_location.x() - ratio * restore_bounds.width());
  origin.set_x(new_origin_x);

  // Windows may not have a widget in tests.
  auto* widget = views::Widget::GetWidgetForNativeWindow(GetTarget());
  if (!widget)
    return origin;

  // |widget| may have a custom frame, |header| will be null in this case.
  auto* header = FrameHeader::Get(widget);
  if (!header)
    return origin;

  // Compute the available bounds based on the header local bounds. These bounds
  // are from the previous layout and do not match |restore_bounds| yet.
  gfx::Rect header_bounds = header->view()->GetLocalBounds();
  header_bounds.set_height(header->GetHeaderHeight());
  gfx::Rect available_bounds = header_bounds;
  auto* back_button = header->GetBackButton();
  auto* caption_button_container = header->caption_button_container();
  if (back_button)
    available_bounds.Subtract(back_button->bounds());
  if (caption_button_container)
    available_bounds.Subtract(caption_button_container->bounds());

  // Calculate the new expected available header left and right bounds. The new
  // header will still are |new_origin_x| with width |restore_bounds.width()|.
  // The available region subtracts the control buttons.
  const int header_left =
      new_origin_x + (available_bounds.x() - header_bounds.x());
  const int header_right = new_origin_x + restore_bounds.width() -
                           (header_bounds.right() - available_bounds.right());

  // If |event_location| x falls outside |available_bounds|, shift
  // |new_origin_x| so that the new window bounds will not land on the any of
  // the header buttons.
  int shift = 0;
  if (event_location.x() > header_right)
    shift = event_location.x() - header_right;
  else if (event_location.x() < header_left)
    shift = event_location.x() - header_left;
  new_origin_x += shift;

  origin.set_x(new_origin_x);
  return origin;
}

gfx::Size WindowResizer::GetSizeForDrag(int* delta_x, int* delta_y) const {
  gfx::Size size = details().initial_bounds_in_parent.size();
  if (details().bounds_change & kBoundsChange_Resizes) {
    const gfx::Size min_size = GetTarget()->delegate()
                                   ? GetTarget()->delegate()->GetMinimumSize()
                                   : gfx::Size();
    size.SetSize(GetWidthForDrag(min_size.width(), delta_x),
                 GetHeightForDrag(min_size.height(), delta_y));
  } else if (!details().restore_bounds_in_parent.IsEmpty() &&
             !window_state_->IsFloated()) {
    // Floated windows remain the same size while dragging regardless of
    // restored bounds.
    size = details().restore_bounds_in_parent.size();
  }
  return size;
}

int WindowResizer::GetWidthForDrag(int min_width, int* delta_x) const {
  int width = details().initial_bounds_in_parent.width();
  if (details().size_change_direction & kBoundsChangeDirection_Horizontal) {
    // Along the right edge, positive delta_x increases the window size.
    int x_multiplier = IsRightEdge(details().window_component) ? 1 : -1;
    width += x_multiplier * (*delta_x);

    // Ensure we don't shrink past the minimum width and clamp delta_x
    // for the window origin computation.
    if (width < min_width) {
      width = min_width;
      *delta_x = -x_multiplier *
                 (details().initial_bounds_in_parent.width() - min_width);
    }

    // And don't let the window go bigger than the display.
    int max_width = display::Screen::GetScreen()
                        ->GetDisplayNearestWindow(GetTarget())
                        .bounds()
                        .width();
    gfx::Size max_size = GetTarget()->delegate()
                             ? GetTarget()->delegate()->GetMaximumSize()
                             : gfx::Size();
    if (max_size.width() != 0)
      max_width = std::min(max_width, max_size.width());
    if (width > max_width) {
      width = max_width;
      *delta_x = -x_multiplier *
                 (details().initial_bounds_in_parent.width() - max_width);
    }
  }
  return width;
}

int WindowResizer::GetHeightForDrag(int min_height, int* delta_y) const {
  int height = details().initial_bounds_in_parent.height();
  if (details().size_change_direction & kBoundsChangeDirection_Vertical) {
    // Along the bottom edge, positive delta_y increases the window size.
    int y_multiplier = IsBottomEdge(details().window_component) ? 1 : -1;
    height += y_multiplier * (*delta_y);

    // Ensure we don't shrink past the minimum height and clamp delta_y
    // for the window origin computation.
    if (height < min_height) {
      height = min_height;
      *delta_y = -y_multiplier *
                 (details().initial_bounds_in_parent.height() - min_height);
    }

    // And don't let the window go bigger than the display.
    int max_height = display::Screen::GetScreen()
                         ->GetDisplayNearestWindow(GetTarget())
                         .bounds()
                         .height();
    gfx::Size max_size = GetTarget()->delegate()
                             ? GetTarget()->delegate()->GetMaximumSize()
                             : gfx::Size();
    if (max_size.height() != 0)
      max_height = std::min(max_height, max_size.height());
    if (height > max_height) {
      height = max_height;
      *delta_y = -y_multiplier *
                 (details().initial_bounds_in_parent.height() - max_height);
    }
  }
  return height;
}

void WindowResizer::CalculateBoundsWithAspectRatio(float aspect_ratio,
                                                   gfx::Rect* new_bounds) {
  gfx::Size min_size = GetTarget()->delegate()
                           ? GetTarget()->delegate()->GetMinimumSize()
                           : gfx::Size();
  gfx::Size max_size = GetTarget()->delegate()
                           ? GetTarget()->delegate()->GetMaximumSize()
                           : gfx::Size();
  DCHECK(!min_size.IsEmpty());

  // gfx::SizeRectToAspectRatio expects std::nullopt when there is no limit, but
  // GetMaximumSize() returns 0x0 when there is no limit.
  auto max_size_opt = !max_size.IsEmpty()
                          ? std::make_optional<gfx::Size>(max_size)
                          : std::nullopt;

  gfx::SizeRectToAspectRatio(GetWindowResizeEdge(details().window_component),
                             aspect_ratio, min_size, max_size_opt, new_bounds);
}

}  // namespace ash
