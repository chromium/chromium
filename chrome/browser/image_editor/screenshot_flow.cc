// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/image_editor/screenshot_flow.h"

#include <memory>

#include "base/logging.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/snapshot/snapshot.h"
#include "ui/views/background.h"

#if defined(OS_MAC)
#include "content/public/browser/render_view_host.h"
#include "ui/views/widget/widget.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"
#endif

namespace image_editor {

// Color for the selection rectangle.
static constexpr SkColor kColorSelectionRect = SkColorSetRGB(0xEE, 0xEE, 0xEE);

ScreenshotFlow::ScreenshotFlow(content::WebContents* web_contents)
    : web_contents_(web_contents->GetWeakPtr()) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

ScreenshotFlow::~ScreenshotFlow() {
  RemoveUIOverlay();
}

// Creates a desaturated, darkened bitmap for the background layer.
static SkBitmap CreateBackgroundBitmap(const SkBitmap* foreground) {
  // No change to hue (signified by -1). Desaturate and darken.
  color_utils::HSL hsl_shift = {-1, 0.25, 0.25};
  return SkBitmapOperations::CreateHSLShiftedBitmap(*foreground, hsl_shift);
}

void ScreenshotFlow::OnSnapshotComplete(gfx::Image snapshot) {
  image_foreground_ = snapshot.AsImageSkia();

  const SkBitmap* bitmap_foreground = snapshot.ToSkBitmap();
  image_background_ = gfx::ImageSkia::CreateFrom1xBitmap(
      CreateBackgroundBitmap(bitmap_foreground));

  CreateAndAddUIOverlay();
  RequestRepaint(gfx::Rect());
}

void ScreenshotFlow::CreateAndAddUIOverlay() {
  if (screen_capture_layer_)
    return;

  screen_capture_layer_ =
      std::make_unique<ui::Layer>(ui::LayerType::LAYER_TEXTURED);
  screen_capture_layer_->SetName("ScreenshotRegionSelectionLayer");
  screen_capture_layer_->SetFillsBoundsOpaquely(true);
  screen_capture_layer_->set_delegate(this);
#if defined(OS_MAC)
  gfx::Rect bounds = web_contents_->GetViewBounds();
  const gfx::NativeView web_contents_view =
      web_contents_->GetContentNativeView();
  views::Widget* widget =
      views::Widget::GetWidgetForNativeView(web_contents_view);
  ui::Layer* content_layer = widget->GetLayer();
  const gfx::Rect offset_bounds = widget->GetWindowBoundsInScreen();
  bounds.Offset(-offset_bounds.x(), -offset_bounds.y());

  views::Widget* top_widget =
      views::Widget::GetTopLevelWidgetForNativeView(web_contents_view);
  views::View* root_view = top_widget->GetRootView();
  root_view->AddPreTargetHandler(this);
#else
  const gfx::NativeWindow& native_window = web_contents_->GetNativeView();
  ui::Layer* content_layer = native_window->layer();
  const gfx::Rect bounds = native_window->bounds();
  // Capture mouse down and drag events on our window.
  // TODO(skare): We should exit from this mode when moving between tabs,
  // clicking on browser chrome, etc.
  native_window->AddPreTargetHandler(this);
#endif
  content_layer->Add(screen_capture_layer_.get());
  content_layer->StackAtTop(screen_capture_layer_.get());
  screen_capture_layer_->SetBounds(bounds);
  screen_capture_layer_->SetVisible(true);
}

void ScreenshotFlow::RemoveUIOverlay() {
  if (!web_contents_ || !screen_capture_layer_)
    return;

#if defined(OS_MAC)
  views::Widget* widget = views::Widget::GetWidgetForNativeView(
      web_contents_->GetContentNativeView());
  ui::Layer* content_layer = widget->GetLayer();
  views::View* root_view = widget->GetRootView();
  root_view->RemovePreTargetHandler(this);
#else
  const gfx::NativeWindow& native_window = web_contents_->GetNativeView();
  native_window->RemovePreTargetHandler(this);
  ui::Layer* content_layer = native_window->layer();
#endif

  content_layer->Remove(screen_capture_layer_.get());

  screen_capture_layer_->set_delegate(nullptr);
  screen_capture_layer_.reset();
}

void ScreenshotFlow::Start(ScreenshotCaptureCallback flow_callback) {
  flow_callback_ = std::move(flow_callback);
#if defined(OS_MAC)
  const gfx::NativeView& native_view = web_contents_->GetContentNativeView();
  gfx::Image img;
  bool rval = ui::GrabViewSnapshot(native_view,
                                   gfx::Rect(web_contents_->GetSize()), &img);
  // If |img| is empty, clients should treat it as a canceled action, but
  // we have a DCHECK for development as we expected this call to succeed.
  DCHECK(rval);
  OnSnapshotComplete(img);
#else
  // Start the capture process by capturing the entire window, then allow
  // the user to drag out a selection mask.
  ui::GrabWindowSnapshotAsyncCallback screenshot_callback =
      base::BindOnce(&ScreenshotFlow::OnSnapshotComplete, weak_this_);
  const gfx::NativeWindow& native_window = web_contents_->GetNativeView();
  // TODO(skare): Evaluate against other screenshot capture methods.
  // The synchronous variant mentions support is different between platforms
  // and another library might be better if there is a browser process.
  ui::GrabWindowSnapshotAsync(native_window,
                              gfx::Rect(web_contents_->GetSize()),
                              std::move(screenshot_callback));
#endif
}

void ScreenshotFlow::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() == ui::ET_KEY_PRESSED &&
      event->key_code() == ui::VKEY_ESCAPE) {
    CompleteCapture(gfx::Rect());
    event->StopPropagation();
  }
}

void ScreenshotFlow::OnMouseEvent(ui::MouseEvent* event) {
  if (!event->IsLocatedEvent())
    return;
  const ui::LocatedEvent* located_event = ui::LocatedEvent::FromIfValid(event);
  if (!located_event)
    return;

  gfx::Point location = located_event->location();
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      if (event->IsLeftMouseButton()) {
        capture_mode_ = CaptureMode::SELECTION_RECTANGLE;
        drag_start_ = location;
        event->SetHandled();
      }
      break;
    case ui::ET_MOUSE_DRAGGED:
      if (event->IsLeftMouseButton()) {
        drag_end_ = location;
        RequestRepaint(gfx::Rect());
        event->SetHandled();
      }
      break;
    case ui::ET_MOUSE_RELEASED:
      if (capture_mode_ == CaptureMode::SELECTION_RECTANGLE ||
          capture_mode_ == CaptureMode::SELECTION_ELEMENT) {
        capture_mode_ = CaptureMode::NOT_CAPTURING;
        event->SetHandled();
        CompleteCapture(gfx::BoundingRect(drag_start_, drag_end_));
      }
      break;
    default:
      break;
  }
}

void ScreenshotFlow::CompleteCapture(const gfx::Rect& region) {
  ScreenshotCaptureResult result;

  if (!region.IsEmpty()) {
    const int w = region.width();
    const int h = region.height();
    float scale = screen_capture_layer_->device_scale_factor();
    gfx::Canvas canvas(gfx::Size(w, h), scale, /*is_opaque=*/true);
    canvas.DrawImageInt(image_foreground_, region.x() * scale,
                        region.y() * scale, w * scale, h * scale, 0, 0, w, h,
                        false);
    result.image = gfx::Image::CreateFrom1xBitmap(canvas.GetBitmap());
    result.screen_bounds = screen_capture_layer_->bounds();
  }

  RemoveUIOverlay();

  std::move(flow_callback_).Run(result);
}

void ScreenshotFlow::OnPaintLayer(const ui::PaintContext& context) {
  if (!screen_capture_layer_)
    return;

  const gfx::Rect& screen_bounds(screen_capture_layer_->bounds());
  ui::PaintRecorder recorder(context, screen_bounds.size());
  gfx::Canvas* canvas = recorder.canvas();

  auto selection_rect = gfx::BoundingRect(drag_start_, drag_end_);
  PaintSelectionLayer(canvas, selection_rect, gfx::Rect());
  paint_invalidation_ = gfx::Rect();
}

void ScreenshotFlow::RequestRepaint(gfx::Rect region) {
  if (!screen_capture_layer_)
    return;

  if (region.IsEmpty()) {
    const gfx::Size& layer_size = screen_capture_layer_->size();
    region = gfx::Rect(0, 0, layer_size.width(), layer_size.height());
  }

  paint_invalidation_.Union(region);
  screen_capture_layer_->SchedulePaint(region);
}

void ScreenshotFlow::PaintSelectionLayer(gfx::Canvas* canvas,
                                         const gfx::Rect& selection,
                                         const gfx::Rect& invalidation_region) {
  if (image_foreground_.isNull() || image_background_.isNull()) {
    return;
  }

  // Captured image is in native scale.
  canvas->UndoDeviceScaleFactor();

  if (selection.IsEmpty()) {
    canvas->DrawImageInt(image_background_, 0, 0);
    return;
  }

  // At this point we have a nonempty selection region.
  // Draw the background, then copy over the relevant region of the foreground.
  float scale_factor = screen_capture_layer_->device_scale_factor();
  gfx::Rect selection_scaled =
      gfx::ScaleToEnclosingRect(selection, scale_factor);
  const int x = selection_scaled.x();
  const int y = selection_scaled.y();
  const int w = selection_scaled.width();
  const int h = selection_scaled.height();
  canvas->DrawImageInt(image_background_, 0, 0);
  canvas->DrawImageInt(image_foreground_, x, y, w, h, x, y, w, h, false);

  // Add a small border around the selection region.
  canvas->DrawRect(gfx::RectF(selection_scaled), kColorSelectionRect);
}

}  // namespace image_editor
