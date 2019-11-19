// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_resizer.h"

#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/views/window/window_resize_utils.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// Returns true for resize components along the right edge, where a drag in
// positive x will make the window larger.
bool IsRightEdge(int window_component) {
  return window_component == HTTOPRIGHT || window_component == HTRIGHT ||
         window_component == HTBOTTOMRIGHT || window_component == HTGROWBOX;
}

// Convert |window_component| to the HitTest used in views::WindowResizeUtils.
views::HitTest GetWindowResizeHitTest(int window_component) {
  switch (window_component) {
    case HTBOTTOM:
      return views::HitTest::kBottom;
    case HTTOP:
      return views::HitTest::kTop;
    case HTLEFT:
      return views::HitTest::kLeft;
    case HTRIGHT:
      return views::HitTest::kRight;
    case HTTOPLEFT:
      return views::HitTest::kTopLeft;
    case HTTOPRIGHT:
      return views::HitTest::kTopRight;
    case HTBOTTOMLEFT:
      return views::HitTest::kBottomLeft;
    case HTBOTTOMRIGHT:
      return views::HitTest::kBottomRight;
    default:
      NOTREACHED();
      return views::HitTest::kBottomRight;
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
  recorder_ = ash::CreatePresentationTimeHistogramRecorder(
      GetTarget()->layer()->GetCompositor(),
      "Ash.InteractiveWindowResize.TimeToPresent",
      "Ash.InteractiveWindowResize.TimeToPresent.MaxLatency");
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
    const gfx::Point& passed_location) {
  if (!details().is_resizable)
    return details().initial_bounds_in_parent;

  gfx::Point location = passed_location;
  int delta_x = location.x() - details().initial_location_in_parent.x();
  int delta_y = location.y() - details().initial_location_in_parent.y();

  AdjustDeltaForTouchResize(&delta_x, &delta_y);

  // The minimize size constraint may limit how much we change the window
  // position.  For example, dragging the left edge to the right should stop
  // repositioning the window when the minimize size is reached.
  gfx::Size size = GetSizeForDrag(&delta_x, &delta_y);
  gfx::Point origin = GetOriginForDrag(delta_x, delta_y);
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
          new_bounds.Inset(0, 0, 0, new_bounds.bottom() - work_area.bottom());
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
    // When we might want to reposition a window which is also restored to its
    // previous size, to keep the cursor within the dragged window.
    if (!details().restore_bounds.IsEmpty()) {
      // However - it is not desirable to change the origin if the window would
      // be still hit by the cursor.
      if (details().initial_location_in_parent.x() >
          details().initial_bounds_in_parent.x() +
              details().restore_bounds.width())
        new_bounds.set_x(location.x() - details().restore_bounds.width() / 2);
    }

    // Make sure that |new_bounds| doesn't leave any of the displays.  Note that
    // the |work_area| above isn't good for this check since it is the work area
    // for the current display but the window can move to a different one.
    aura::Window* parent = GetTarget()->parent();
    gfx::Point passed_location_in_screen(passed_location);
    ::wm::ConvertPointToScreen(parent, &passed_location_in_screen);
    gfx::Rect near_passed_location(passed_location_in_screen, gfx::Size());
    // Use a pointer location (matching the logic in DragWindowResizer) to
    // calculate the target display after the drag.
    const display::Display& display =
        display::Screen::GetScreen()->GetDisplayMatching(near_passed_location);
    gfx::Rect screen_work_area = display.work_area();
    screen_work_area.Inset(kMinimumOnScreenArea, 0);
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

// static
bool WindowResizer::IsBottomEdge(int window_component) {
  return window_component == HTBOTTOMLEFT || window_component == HTBOTTOM ||
         window_component == HTBOTTOMRIGHT || window_component == HTGROWBOX;
}

void WindowResizer::SetBoundsDuringResize(const gfx::Rect& bounds) {
  aura::Window* window = GetTarget();
  DCHECK(window);
  auto ptr = weak_ptr_factory_.GetWeakPtr();
  const gfx::Rect original_bounds = window->bounds();
  window->SetBounds(bounds);

  // Resizer can be destroyed when a window is attached during tab dragging.
  // crbug.com/970911.
  if (!ptr)
    return;

  if (bounds.size() == original_bounds.size())
    return;
  recorder_->RequestNext();
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

gfx::Point WindowResizer::GetOriginForDrag(int delta_x, int delta_y) {
  gfx::Point origin = details().initial_bounds_in_parent.origin();
  if (details().bounds_change & kBoundsChange_Repositions) {
    int pos_change_direction = GetPositionChangeDirectionForWindowComponent(
        details().window_component);
    if (pos_change_direction & kBoundsChangeDirection_Horizontal)
      origin.Offset(delta_x, 0);
    if (pos_change_direction & kBoundsChangeDirection_Vertical)
      origin.Offset(0, delta_y);
  }
  return origin;
}

gfx::Size WindowResizer::GetSizeForDrag(int* delta_x, int* delta_y) {
  gfx::Size size = details().initial_bounds_in_parent.size();
  if (details().bounds_change & kBoundsChange_Resizes) {
    gfx::Size min_size = GetTarget()->delegate()
                             ? GetTarget()->delegate()->GetMinimumSize()
                             : gfx::Size();
    size.SetSize(GetWidthForDrag(min_size.width(), delta_x),
                 GetHeightForDrag(min_size.height(), delta_y));
  } else if (!details().restore_bounds.IsEmpty()) {
    size = details().restore_bounds.size();
  }
  return size;
}

int WindowResizer::GetWidthForDrag(int min_width, int* delta_x) {
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

int WindowResizer::GetHeightForDrag(int min_height, int* delta_y) {
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
  DCHECK(!max_size.IsEmpty());

  views::WindowResizeUtils::SizeMinMaxToAspectRatio(aspect_ratio, &min_size,
                                                    &max_size);
  views::WindowResizeUtils::SizeRectToAspectRatio(
      GetWindowResizeHitTest(details().window_component), aspect_ratio,
      min_size, max_size, new_bounds);
}

}  // namespace ash
