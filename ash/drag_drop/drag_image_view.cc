// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_image_view.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "base/memory/ptr_util.h"
#include "skia/ext/image_operations.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/widget/widget.h"

namespace ash {

DragImageView::DragImageView(ui::mojom::DragEventSource event_source)
    : drag_event_source_(event_source) {}

DragImageView::~DragImageView() = default;

// static
std::unique_ptr<views::Widget> DragImageView::Create(
    aura::Window* root_window,
    ui::mojom::DragEventSource event_source) {
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_TOOLTIP);
  params.name = "DragWidget";
  params.accept_events = false;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent =
      root_window->GetChildById(kShellWindowId_DragImageAndTooltipContainer);
  if (!params.parent)
    params.context = root_window;  // Happens in tests.
  auto drag_widget = std::make_unique<views::Widget>(std::move(params));
  drag_widget->SetOpacity(1.f);
  drag_widget->SetContentsView(
      base::WrapUnique(new DragImageView(event_source)));
  return drag_widget;
}

void DragImageView::SetBoundsInScreen(const gfx::Rect& bounds) {
  drag_image_size_ = bounds.size();
  GetWidget()->SetBounds(bounds);
}

void DragImageView::SetScreenPosition(const gfx::Point& position) {
  GetWidget()->SetBounds(
      gfx::Rect(position, GetWidget()->GetWindowBoundsInScreen().size()));
}

gfx::Rect DragImageView::GetBoundsInScreen() const {
  return GetWidget()->GetWindowBoundsInScreen();
}

void DragImageView::SetWidgetVisible(bool visible) {
  auto* widget = GetWidget();
  if (visible != widget->IsVisible()) {
    if (visible)
      widget->Show();
    else
      widget->Hide();
  }
}

void DragImageView::SetTouchDragOperationHintOff() {
  // Simply set the drag type to non-touch so that no hint is drawn.
  drag_event_source_ = ui::mojom::DragEventSource::kMouse;

  // This disables the drag hint image. This should reduce the widget size if
  // the drag image is smaller than the drag hint image, so we set new bounds.
  gfx::Rect new_bounds = GetBoundsInScreen();
  new_bounds.set_size(drag_image_size_);
  SetBoundsInScreen(new_bounds);
  SchedulePaint();
}

void DragImageView::SetTouchDragOperation(int operation) {
  if (touch_drag_operation_ == operation)
    return;
  touch_drag_operation_ = operation;
  SchedulePaint();
}

void DragImageView::SetTouchDragOperationHintPosition(
    const gfx::Point& position) {
  if (touch_drag_operation_indicator_position_ == position)
    return;
  touch_drag_operation_indicator_position_ = position;
  SchedulePaint();
}

void DragImageView::SetOpacity(float visibility) {
  DCHECK_GE(visibility, 0.0f);
  DCHECK_LE(visibility, 1.0f);
  GetWidget()->SetOpacity(visibility);
}

void DragImageView::OnPaint(gfx::Canvas* canvas) {
  if (GetImage().isNull())
    return;

  // |drag_image_size_| is in DIP.
  // ImageSkia::size() also returns the size in DIP.
  if (GetImage().size() == drag_image_size_) {
    canvas->DrawImageInt(GetImage(), 0, 0);
  } else {
    aura::Window* window = GetWidget()->GetNativeWindow();
    const float device_scale = display::Screen::GetScreen()
                                   ->GetDisplayNearestWindow(window)
                                   .device_scale_factor();
    // The drag image already has device scale factor applied. But
    // |drag_image_size_| is in DIP units.
    gfx::Size drag_image_size_pixels =
        gfx::ScaleToRoundedSize(drag_image_size_, device_scale);
    gfx::ImageSkiaRep image_rep = GetImage().GetRepresentation(device_scale);
    if (image_rep.is_null())
      return;
    SkBitmap scaled = skia::ImageOperations::Resize(
        image_rep.GetBitmap(), skia::ImageOperations::RESIZE_LANCZOS3,
        drag_image_size_pixels.width(), drag_image_size_pixels.height());
    gfx::ImageSkia image_skia =
        gfx::ImageSkia::CreateFromBitmap(scaled, device_scale);
    canvas->DrawImageInt(image_skia, 0, 0);
  }

  gfx::Image* drag_hint = DragHint();
  if (!ShouldDrawDragHint() || drag_hint->IsEmpty())
    return;

  // Make sure drag hint image is positioned within the widget.
  gfx::Size drag_hint_size = drag_hint->Size();
  gfx::Point drag_hint_position = touch_drag_operation_indicator_position_;
  drag_hint_position.Offset(-drag_hint_size.width() / 2, 0);
  gfx::Rect drag_hint_bounds(drag_hint_position, drag_hint_size);

  gfx::Size widget_size = GetWidget()->GetWindowBoundsInScreen().size();
  drag_hint_bounds.AdjustToFit(gfx::Rect(widget_size));

  // Draw image.
  canvas->DrawImageInt(*(drag_hint->ToImageSkia()), drag_hint_bounds.x(),
                       drag_hint_bounds.y());
}

gfx::Image* DragImageView::DragHint() const {
  // Select appropriate drag hint.
  gfx::Image* drag_hint =
      &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_TOUCH_DRAG_TIP_NODROP);
  if (touch_drag_operation_ & ui::DragDropTypes::DRAG_COPY) {
    drag_hint = &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_TOUCH_DRAG_TIP_COPY);
  } else if (touch_drag_operation_ & ui::DragDropTypes::DRAG_MOVE) {
    drag_hint = &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_TOUCH_DRAG_TIP_MOVE);
  } else if (touch_drag_operation_ & ui::DragDropTypes::DRAG_LINK) {
    drag_hint = &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_TOUCH_DRAG_TIP_LINK);
  }
  return drag_hint;
}

bool DragImageView::ShouldDrawDragHint() const {
  return drag_event_source_ == ui::mojom::DragEventSource::kTouch;
}

gfx::Size DragImageView::GetMinimumSize() const {
  gfx::Size minimum_size = drag_image_size_;
  if (ShouldDrawDragHint())
    minimum_size.SetToMax(DragHint()->Size());
  return minimum_size;
}

void DragImageView::Layout(PassKey) {
  LayoutSuperclass<View>(this);

  // Only consider resizing the widget for the drag hint image if we are in a
  // touch initiated drag.
  gfx::Image* drag_hint = DragHint();
  if (!ShouldDrawDragHint() || drag_hint->IsEmpty())
    return;

  gfx::Size drag_hint_size = drag_hint->Size();

  // Enlarge widget if required to fit the drag hint image.
  gfx::Size widget_size = GetWidget()->GetWindowBoundsInScreen().size();
  if (drag_hint_size.width() > widget_size.width() ||
      drag_hint_size.height() > widget_size.height()) {
    widget_size.SetToMax(drag_hint_size);
    GetWidget()->SetSize(widget_size);
  }
}

}  // namespace ash
