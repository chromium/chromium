// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/image_editor/screenshot_flow.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cursor/cursor.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event_target.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/render_text.h"
#include "ui/snapshot/snapshot.h"
#include "ui/views/background.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/image_editor/event_capture_mac.h"
#include "components/lens/lens_features.h"
#include "content/public/browser/render_view_host.h"
#include "ui/views/widget/widget.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"
#endif

namespace image_editor {

// Colors for semitransparent overlay.
static constexpr SkColor kColorSemitransparentOverlayMask =
    SkColorSetARGB(0x9F, 0x00, 0x00, 0x00);
static constexpr SkColor kColorSemitransparentOverlayVisible =
    SkColorSetARGB(0x00, 0x00, 0x00, 0x00);
static constexpr SkColor kColorSelectionRect = SkColorSetRGB(0xEE, 0xEE, 0xEE);

// Minimum selection rect edge size to treat as a valid capture region.
static constexpr int kMinimumValidSelectionEdgePixels = 30;

ScreenshotCapturedData::ScreenshotCapturedData() = default;
ScreenshotCapturedData::~ScreenshotCapturedData() = default;

ScreenshotFlow::ScreenshotFlow(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents->GetWeakPtr()) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

ScreenshotFlow::~ScreenshotFlow() {
  RemoveUIOverlay();
}

void ScreenshotFlow::CreateAndAddUIOverlay() {
  if (screen_capture_layer_)
    return;
  web_contents_observer_ = std::make_unique<UnderlyingWebContentsObserver>(
      web_contents_.get(), this);
  screen_capture_layer_ =
      std::make_unique<ui::Layer>(ui::LayerType::LAYER_TEXTURED);
  screen_capture_layer_->SetName("ScreenshotRegionSelectionLayer");
  screen_capture_layer_->SetFillsBoundsOpaquely(false);
  screen_capture_layer_->set_delegate(this);
#if BUILDFLAG(IS_MAC)
  gfx::Rect bounds = web_contents_->GetViewBounds();
  const gfx::NativeView web_contents_view =
      web_contents_->GetContentNativeView();
  views::Widget* widget =
      views::Widget::GetWidgetForNativeView(web_contents_view);
  ui::Layer* content_layer = widget->GetLayer();
  const gfx::Rect offset_bounds = widget->GetWindowBoundsInScreen();
  bounds.Offset(-offset_bounds.x(), -offset_bounds.y());

  event_capture_mac_ = std::make_unique<EventCaptureMac>(
      this,
      base::BindOnce(&ScreenshotFlow::CancelCapture, base::Unretained(this)),
      web_contents_view, web_contents_->GetTopLevelNativeWindow());
#else
  const gfx::NativeWindow& native_window = web_contents_->GetNativeView();
  ui::Layer* content_layer = native_window->layer();
  const gfx::Rect bounds = native_window->bounds();
  // Capture mouse down and drag events on our window.
  event_capture_.Observe(native_window);
#endif
  content_layer->Add(screen_capture_layer_.get());
  content_layer->StackAtTop(screen_capture_layer_.get());
  screen_capture_layer_->SetBounds(bounds);
  screen_capture_layer_->SetVisible(true);

  SetCursor(ui::mojom::CursorType::kCross);

  // After setup is done, we should set the capture mode to active.
  capture_mode_ = CaptureMode::SELECTION_RECTANGLE;
}

void ScreenshotFlow::RemoveUIOverlay() {
  if (capture_mode_ == CaptureMode::NOT_CAPTURING)
    return;
  is_dragging_ = false;
  capture_mode_ = CaptureMode::NOT_CAPTURING;
  if (!web_contents_ || !screen_capture_layer_)
    return;

#if BUILDFLAG(IS_MAC)
  views::Widget* widget = views::Widget::GetWidgetForNativeView(
      web_contents_->GetContentNativeView());
  if (!widget)
    return;
  ui::Layer* content_layer = widget->GetLayer();
  event_capture_mac_.reset();
#else
  const gfx::NativeWindow& native_window = web_contents_->GetNativeView();
#if BUILDFLAG(IS_WIN)
  // This handles cases where the overlay is removed while a drag is still in
  // progress, including if ScreenshotFlow is destroyed. It is safe to call
  // ReleaseCapture() even if the capture has not been set or has already been
  // released.
  native_window->ReleaseCapture();
#endif  // BUILDFLAG(IS_WIN)
  event_capture_.Reset();
  ui::Layer* content_layer = native_window->layer();
#endif  // else

  content_layer->Remove(screen_capture_layer_.get());

  screen_capture_layer_->set_delegate(nullptr);
  screen_capture_layer_.reset();

  // Restore the cursor to pointer; there's no corresponding GetCursor()
  // to store the pre-capture-mode cursor, and the pointer will have moved
  // in the meantime.
  SetCursor(ui::mojom::CursorType::kPointer);
}

void ScreenshotFlow::Start(ScreenshotCaptureCallback flow_callback) {
  flow_callback_ = std::move(flow_callback);
  CreateAndAddUIOverlay();
  RequestRepaint(gfx::Rect());
}

void ScreenshotFlow::StartFullscreenCapture(
    ScreenshotCaptureCallback flow_callback) {
  // Start and finish the capture process by screenshotting the full window.
  // There is no region selection step in this mode.
  flow_callback_ = std::move(flow_callback);
  CaptureAndRunScreenshotCompleteCallback(ScreenshotCaptureResultCode::SUCCESS,
                                          gfx::Rect(web_contents_->GetSize()));
}

void ScreenshotFlow::CaptureAndRunScreenshotCompleteCallback(
    ScreenshotCaptureResultCode result_code,
    gfx::Rect region) {
  if (region.IsEmpty()) {
    RunScreenshotCompleteCallback(result_code, gfx::Rect(), gfx::Image());
    return;
  }

  gfx::Rect bounds = web_contents_->GetViewBounds();
  ui::GrabSnapshotImageCallback screenshot_callback =
      base::BindOnce(&ScreenshotFlow::RunScreenshotCompleteCallback, weak_this_,
                     result_code, bounds);
  ui::GrabViewSnapshot(web_contents_->GetNativeView(), region,
                       std::move(screenshot_callback));
}

void ScreenshotFlow::CancelCapture() {
  RemoveUIOverlay();
  CaptureAndRunScreenshotCompleteCallback(
      ScreenshotCaptureResultCode::USER_NAVIGATED_EXIT, gfx::Rect());
}

void ScreenshotFlow::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() == ui::EventType::kKeyPressed &&
      event->key_code() == ui::VKEY_ESCAPE) {
    event->StopPropagation();
    CompleteCapture(ScreenshotCaptureResultCode::USER_ESCAPE_EXIT, gfx::Rect());
  }
}

void ScreenshotFlow::OnMouseEvent(ui::MouseEvent* event) {
  if (!event->IsLocatedEvent())
    return;
  const ui::LocatedEvent* located_event = ui::LocatedEvent::FromIfValid(event);
  if (!located_event)
    return;

  gfx::Point location = located_event->location();
  gfx::Rect web_contents_bounds = web_contents_->GetViewBounds();
#if BUILDFLAG(IS_MAC)
  // Offset |location| be relative to the WebContents widget, vs the parent
  // window, recomputed rather than cached in case e.g. user disables
  // bookmarks bar from another window.
  const gfx::NativeView web_contents_view =
      web_contents_->GetContentNativeView();
  views::Widget* widget =
      views::Widget::GetWidgetForNativeView(web_contents_view);
  if (!widget)
    return;
  const gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  location.set_x(location.x() + (widget_bounds.x() - web_contents_bounds.x()));
  location.set_y(location.y() + (widget_bounds.y() - web_contents_bounds.y()));
#endif
  switch (event->type()) {
    case ui::EventType::kMouseMoved:
      SetCursor(ui::mojom::CursorType::kCross);
      event->SetHandled();
      break;
    case ui::EventType::kMousePressed:
      if (event->IsOnlyLeftMouseButton()) {
        // Don't capture initial clicks on browser ui outside the webcontents.
        if (location.x() < 0 || location.y() < 0 ||
            location.x() > web_contents_bounds.width() ||
            location.y() > web_contents_bounds.height()) {
          return;
        }
        SetIsDragging(true);
        drag_start_ = location;
        drag_end_ = location;
        event->SetHandled();
      }
      break;
    case ui::EventType::kMouseDragged:
      if (event->IsOnlyLeftMouseButton() && is_dragging_) {
        drag_end_ = location;
        RequestRepaint(gfx::Rect());
        event->SetHandled();
      }
      break;
    case ui::EventType::kMouseReleased:
      if ((capture_mode_ == CaptureMode::SELECTION_RECTANGLE ||
           capture_mode_ == CaptureMode::SELECTION_ELEMENT) &&
          is_dragging_) {
        SetIsDragging(false);
        AttemptRegionCapture(web_contents_bounds);
        event->SetHandled();
      }
      break;
    // This event type is never called on Mac.
    case ui::EventType::kMousewheel:
      if ((capture_mode_ == CaptureMode::SELECTION_RECTANGLE ||
           capture_mode_ == CaptureMode::SELECTION_ELEMENT) &&
          event->AsMouseWheelEvent()->y_offset() > 0 && is_dragging_) {
        SetIsDragging(false);
        AttemptRegionCapture(web_contents_bounds);
        event->SetHandled();
      }
      break;
    default:
      break;
  }
}

void ScreenshotFlow::OnScrollEvent(ui::ScrollEvent* event) {
  // A single tap can create a scroll event, so ignore scroll starts and
  // cancels but complete capture when scrolls actually occur.
  if (event->type() == ui::EventType::kScrollFlingStart ||
      event->type() == ui::EventType::kScrollFlingCancel) {
    return;
  }

  gfx::Rect web_contents_bounds = web_contents_->GetViewBounds();
  if ((capture_mode_ == CaptureMode::SELECTION_RECTANGLE ||
       capture_mode_ == CaptureMode::SELECTION_ELEMENT) &&
      event->y_offset() > 0 && is_dragging_) {
    SetIsDragging(false);
    AttemptRegionCapture(web_contents_bounds);
    event->SetHandled();
  }
}

void ScreenshotFlow::CompleteCapture(ScreenshotCaptureResultCode result_code,
                                     const gfx::Rect& region) {
  RemoveUIOverlay();
  CaptureAndRunScreenshotCompleteCallback(result_code, region);
}

void ScreenshotFlow::RunScreenshotCompleteCallback(
    ScreenshotCaptureResultCode result_code,
    gfx::Rect bounds,
    gfx::Image image) {
  if (flow_callback_.is_null())
    return;

  ScreenshotCaptureResult result;
  result.result_code = result_code;
  result.image = image;
  result.screen_bounds = bounds;

  std::move(flow_callback_).Run(result);
}

bool ScreenshotFlow::IsUIOverlayShown() {
  return screen_capture_layer_ != nullptr && screen_capture_layer_->visible();
}

void ScreenshotFlow::ResetUIOverlayBounds() {
#if BUILDFLAG(IS_MAC)
  gfx::Rect bounds = web_contents_->GetViewBounds();
  const gfx::NativeView web_contents_view =
      web_contents_->GetContentNativeView();
  views::Widget* widget =
      views::Widget::GetWidgetForNativeView(web_contents_view);
  if (widget != nullptr) {
    const gfx::Rect offset_bounds = widget->GetWindowBoundsInScreen();
    bounds.Offset(-offset_bounds.x(), -offset_bounds.y());
  }
#else
  const gfx::NativeWindow& native_window = web_contents_->GetNativeView();
  const gfx::Rect bounds = native_window->bounds();
#endif
  screen_capture_layer_->SetBounds(bounds);
}

void ScreenshotFlow::OnPaintLayer(const ui::PaintContext& context) {
  if (!screen_capture_layer_)
    return;

  const gfx::Rect& screen_bounds(screen_capture_layer_->bounds());
  ui::PaintRecorder recorder(context, screen_bounds.size());
  gfx::Canvas* canvas = recorder.canvas();

  auto selection_rect = gfx::BoundingRect(drag_start_, drag_end_);
  // Draw border exclusively outside selected region.
  selection_rect.Inset(gfx::Insets::TLBR(-1, -1, 0, 0));
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
  // Adjust for hidpi and lodpi support.
  canvas->UndoDeviceScaleFactor();

  // Clear the canvas with our mask color.
  canvas->DrawColor(kColorSemitransparentOverlayMask);
  // Allow the user's selection to show through, and add a border around it.
  if (!selection.IsEmpty()) {
    float scale_factor = screen_capture_layer_->device_scale_factor();
    gfx::Rect selection_scaled =
        gfx::ScaleToEnclosingRect(selection, scale_factor);
    canvas->FillRect(selection_scaled, kColorSemitransparentOverlayVisible,
                     SkBlendMode::kClear);
    canvas->DrawRect(gfx::RectF(selection_scaled), kColorSelectionRect);
  }
}

void ScreenshotFlow::SetCursor(ui::mojom::CursorType cursor_type) {
  if (!web_contents_) {
    return;
  }

#if BUILDFLAG(IS_MAC)
  if (cursor_type == ui::mojom::CursorType::kCross) {
    EventCaptureMac::SetCrossCursor();
    return;
  }
#endif

  content::RenderWidgetHost* host =
      web_contents_->GetPrimaryMainFrame()->GetRenderWidgetHost();
  if (host) {
    ui::Cursor cursor(cursor_type);
    host->SetCursor(cursor);
  }
}

void ScreenshotFlow::SetIsDragging(bool value) {
  is_dragging_ = value;
#if BUILDFLAG(IS_WIN)
  const gfx::NativeWindow& native_window = web_contents_->GetNativeView();
  if (value) {
    native_window->SetCapture();
  } else {
    native_window->ReleaseCapture();
  }
#endif
}

bool ScreenshotFlow::IsCaptureModeActive() {
  return capture_mode_ != CaptureMode::NOT_CAPTURING;
}

void ScreenshotFlow::WebContentsDestroyed() {
  if (IsCaptureModeActive()) {
    CancelCapture();
  }
}

void ScreenshotFlow::OnVisibilityChanged(content::Visibility visibility) {
  if (IsCaptureModeActive()) {
    CancelCapture();
  }
}

void ScreenshotFlow::AttemptRegionCapture(gfx::Rect view_bounds) {
  // On Mac, it is possible for drag_end_ to end up outside of the
  // Browser UI. This causes an error when completing the region
  // capture. Update drag_end_ to be within the web content bounds.
  drag_end_.SetToMin(gfx::Point(view_bounds.width(), view_bounds.height()));
  drag_end_.SetToMax(gfx::Point(0, 0));

  gfx::Rect selection = gfx::BoundingRect(drag_start_, drag_end_);
  drag_start_.SetPoint(0, 0);
  drag_end_.SetPoint(0, 0);
  if (selection.width() >= kMinimumValidSelectionEdgePixels &&
      selection.height() >= kMinimumValidSelectionEdgePixels) {
    CompleteCapture(ScreenshotCaptureResultCode::SUCCESS, selection);
  } else {
    RequestRepaint(gfx::Rect());
  }
}

// UnderlyingWebContentsObserver monitors the WebContents and exits screen
// capture mode if a navigation occurs.
class ScreenshotFlow::UnderlyingWebContentsObserver
    : public content::WebContentsObserver {
 public:
  UnderlyingWebContentsObserver(content::WebContents* web_contents,
                                ScreenshotFlow* screenshot_flow)
      : content::WebContentsObserver(web_contents),
        screenshot_flow_(screenshot_flow) {}

  ~UnderlyingWebContentsObserver() override = default;

  UnderlyingWebContentsObserver(const UnderlyingWebContentsObserver&) = delete;
  UnderlyingWebContentsObserver& operator=(
      const UnderlyingWebContentsObserver&) = delete;

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override {
    // We only care to complete/cancel a capture if the capture mode is
    // currently active.
    if (screenshot_flow_->IsCaptureModeActive())
      screenshot_flow_->CompleteCapture(
          ScreenshotCaptureResultCode::USER_NAVIGATED_EXIT, gfx::Rect());
  }

  // content::WebContentsObserver
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override {
    // We only care to resize the UI overlay when it's visible to the user.
    if (screenshot_flow_->IsUIOverlayShown()) {
      screenshot_flow_->ResetUIOverlayBounds();
    }
  }

 private:
  raw_ptr<ScreenshotFlow> screenshot_flow_;
};

}  // namespace image_editor
