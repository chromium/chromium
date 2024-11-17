// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/cursor/cursor_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/paint/paint_canvas.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_rep_default.h"

namespace ash {
namespace {

// Amount of time without cursor movement before entering stationary state.
const int kStationaryDelayMs = 500;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// CursorView, public:

CursorView::~CursorView() = default;

// static
views::UniqueWidgetPtr CursorView::Create(const gfx::Point& initial_location,
                                          aura::Window* container) {
  CursorView* cursor_view = new CursorView(initial_location);
  auto widget = FastInkView::CreateWidgetWithContents(
      base::WrapUnique(cursor_view), container);

  // Initialize after FaskInkHost is set on `cursor_view` and it is attached to
  // a widget. So that it could get `buffer_to_screen_transform_`.
  cursor_view->Init();

  return widget;
}

void CursorView::SetCursorImages(
    const std::vector<gfx::ImageSkia>& cursor_images,
    const gfx::Size& cursor_size,
    const gfx::Point& cursor_hotspot) {
  cursor_images_.clear();
  for (const gfx::ImageSkia& cursor_image : cursor_images) {
    // Scale cursor images to device scale factor.
    cursor_images_.push_back(gfx::ImageSkia::CreateFrom1xBitmap(
        cursor_image.GetRepresentation(device_scale_factor_).GetBitmap()));
  }
  cursor_size_ = cursor_size;
  cursor_hotspot_ = cursor_hotspot;

  UpdateAnimation();

  UpdateCursor();

  stationary_timer_->Reset();
}

void CursorView::SetLocation(const gfx::Point& location) {
  if (location == cursor_location_) {
    return;
  }
  stationary_timer_->Reset();
  cursor_location_ = location;

  UpdateCursor();
}

////////////////////////////////////////////////////////////////////////////////
// CursorView, private:

CursorView::CursorView(const gfx::Point& initial_location)
    : cursor_location_(initial_location),
      ui_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  stationary_timer_.emplace(
      FROM_HERE, base::Milliseconds(kStationaryDelayMs),
      base::BindRepeating(&CursorView::OnStationary,
                          weak_ptr_factory_.GetWeakPtr()));
}

void CursorView::Init() {
  buffer_to_screen_transform_ =
      host()->window_to_buffer_transform().GetCheckedInverse();
  device_scale_factor_ =
      GetWidget()->GetNativeView()->GetHost()->device_scale_factor();
}

void CursorView::UpdateAnimation() {
  cursor_image_index_ = 0;
  if (cursor_images_.size() <= 1) {
    animated_cursor_timer_.Stop();
  } else if (cursor_images_.size() > 1) {
    animated_cursor_timer_.Start(
        FROM_HERE, base::Milliseconds(16),
        base::BindRepeating(&CursorView::AdvanceFrame,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void CursorView::AdvanceFrame() {
  cursor_image_index_ = (cursor_image_index_ + 1) % cursor_images_.size();
  Draw();
}

void CursorView::Draw() {
  {
    std::unique_ptr<FastInkHost::ScopedPaint> paint =
        GetScopedPaint(damage_rect_);
    cc::PaintCanvas* sk_canvas = paint->canvas().sk_canvas();
    sk_canvas->translate(cursor_rect_.x(), cursor_rect_.y());

    // Undo device scale factor on the canvas.
    sk_canvas->scale(SkFloatToScalar(1 / device_scale_factor_));
    if (!cursor_images_.empty()) {
      paint->canvas().DrawImageInt(cursor_images_[cursor_image_index_], 0, 0);
    }
  }

  // TODO(b/360768376): Disable overlay until we know why
  // fast ink cursor glitches.
  UpdateSurface(cursor_rect_, damage_rect_,
                /*auto_refresh=*/false);
}

void CursorView::OnStationary() {
  stationary_timer_->Stop();
  Draw();
}

void CursorView::UpdateCursor() {
  damage_rect_ = cursor_rect_;
  cursor_rect_ = gfx::Rect(cursor_size_);
  cursor_rect_.set_x(SkIntToScalar(cursor_location_.x() - cursor_hotspot_.x()));
  cursor_rect_.set_y(SkIntToScalar(cursor_location_.y() - cursor_hotspot_.y()));
  damage_rect_.Union(cursor_rect_);

  // Grow `damage_rect_` on all sides by 1 pixel for the possible rounding
  // errors after scaling cursor images to device scale factor.
  damage_rect_.Outset(1);

  Draw();
}

}  // namespace ash
