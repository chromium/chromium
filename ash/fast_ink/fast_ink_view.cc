// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/fast_ink_view.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/views/widget/widget.h"

namespace fast_ink {

FastInkView::FastInkView() = default;

FastInkView::~FastInkView() = default;

// static
views::UniqueWidgetPtr FastInkView::CreateWidgetWithContents(
    std::unique_ptr<FastInkView> fast_ink_view,
    aura::Window* container) {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.name = "FastInkOverlay";
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = container;
  params.layer_type = ui::LAYER_SOLID_COLOR;

  aura::Window* root_window = container->GetRootWindow();
  gfx::Rect screen_bounds = root_window->GetBoundsInScreen();

  views::UniqueWidgetPtr widget(
      std::make_unique<views::Widget>(std::move(params)));
  FastInkView* fast_ink_view_ptr =
      widget->SetContentsView(std::move(fast_ink_view));
  widget->SetBounds(screen_bounds);
  fast_ink_view_ptr->SetFastInkHost(std::make_unique<FastInkHost>(
      widget->GetNativeWindow(), fast_ink_view_ptr->GetPresentationCallback()));
  widget->Show();
  return widget;
}

FastInkView::ScopedPaint::ScopedPaint(FastInkView* view,
                                      const gfx::Rect& damage_rect_in_window)
    : gpu_memory_buffer_(view->host()->gpu_memory_buffer()),
      damage_rect_(FastInkHost::BufferRectFromWindowRect(
          view->host()->window_to_buffer_transform(),
          gpu_memory_buffer_->GetSize(),
          damage_rect_in_window)),
      canvas_(damage_rect_.size(), 1.0f, false) {
  canvas_.Translate(-damage_rect_.OffsetFromOrigin());
  canvas_.Transform(view->host()->window_to_buffer_transform());
}

FastInkView::ScopedPaint::~ScopedPaint() {
  if (damage_rect_.IsEmpty())
    return;

  {
    TRACE_EVENT0("ui", "FastInkView::ScopedPaint::Map");

    if (!gpu_memory_buffer_->Map()) {
      LOG(ERROR) << "Failed to map GPU memory buffer";
      return;
    }
  }

  // Copy result to GPU memory buffer. This is effectively a memcpy and unlike
  // drawing to the buffer directly this ensures that the buffer is never in a
  // state that would result in flicker.
  {
    TRACE_EVENT1("ui", "FastInkView::ScopedPaint::Copy", "damage_rect",
                 damage_rect_.ToString());

    uint8_t* data = static_cast<uint8_t*>(gpu_memory_buffer_->memory(0));
    int stride = gpu_memory_buffer_->stride(0);
    canvas_.GetBitmap().readPixels(
        SkImageInfo::MakeN32Premul(damage_rect_.width(), damage_rect_.height()),
        data + damage_rect_.y() * stride + damage_rect_.x() * 4, stride, 0, 0);
  }

  {
    TRACE_EVENT0("ui", "FastInkView::UpdateBuffer::Unmap");

    // Unmap to flush writes to buffer.
    gpu_memory_buffer_->Unmap();
  }
}

void FastInkView::UpdateSurface(const gfx::Rect& content_rect,
                                const gfx::Rect& damage_rect,
                                bool auto_refresh) {
  host_->UpdateSurface(content_rect, damage_rect, auto_refresh);
}

FastInkHost::PresentationCallback FastInkView::GetPresentationCallback() {
  return FastInkHost::PresentationCallback();
}

void FastInkView::SetFastInkHost(std::unique_ptr<FastInkHost> host) {
  host_ = std::move(host);
}

}  // namespace fast_ink
