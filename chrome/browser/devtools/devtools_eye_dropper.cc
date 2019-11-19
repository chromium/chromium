// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_eye_dropper.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/shared_memory_mapping.h"
#include "build/build_config.h"
#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/common/features.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/cursor_info.h"
#include "content/public/common/screen_info.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/gfx/geometry/size_conversions.h"

DevToolsEyeDropper::DevToolsEyeDropper(content::WebContents* web_contents,
                                       EyeDropperCallback callback)
    : content::WebContentsObserver(web_contents),
      callback_(callback),
      last_cursor_x_(-1),
      last_cursor_y_(-1),
      host_(nullptr) {
  mouse_event_callback_ =
      base::Bind(&DevToolsEyeDropper::HandleMouseEvent, base::Unretained(this));
  content::RenderViewHost* rvh = web_contents->GetRenderViewHost();
  if (rvh)
    AttachToHost(rvh->GetWidget());
}

DevToolsEyeDropper::~DevToolsEyeDropper() {
  DetachFromHost();
}

void DevToolsEyeDropper::AttachToHost(content::RenderWidgetHost* host) {
  host_ = host;
  host_->AddMouseEventCallback(mouse_event_callback_);

  // The view can be null if the renderer process has crashed.
  // (https://crbug.com/847363)
  if (!host_->GetView())
    return;

  // Capturing a full-page screenshot can be costly so we shouldn't do it too
  // often. We can capture at a lower frame rate without hurting the user
  // experience.
  constexpr static int kMaxFrameRate = 15;

  // Create and configure the video capturer.
  video_capturer_ = host_->GetView()->CreateVideoCapturer();
  video_capturer_->SetResolutionConstraints(
      host_->GetView()->GetViewBounds().size(),
      host_->GetView()->GetViewBounds().size(), true);
  video_capturer_->SetAutoThrottlingEnabled(false);
  video_capturer_->SetMinSizeChangePeriod(base::TimeDelta());
  video_capturer_->SetFormat(media::PIXEL_FORMAT_ARGB,
                             gfx::ColorSpace::CreateREC709());
  video_capturer_->SetMinCapturePeriod(base::TimeDelta::FromSeconds(1) /
                                       kMaxFrameRate);
  video_capturer_->Start(this);
}

void DevToolsEyeDropper::DetachFromHost() {
  if (!host_)
    return;
  host_->RemoveMouseEventCallback(mouse_event_callback_);
  content::CursorInfo cursor_info;
  cursor_info.type = ui::CursorType::kPointer;
  host_->SetCursor(cursor_info);
  video_capturer_.reset();
  host_ = nullptr;
}

void DevToolsEyeDropper::RenderViewCreated(content::RenderViewHost* host) {
  if (!host_)
    AttachToHost(host->GetWidget());
}

void DevToolsEyeDropper::RenderViewDeleted(content::RenderViewHost* host) {
  if (host->GetWidget() == host_) {
    DetachFromHost();
    ResetFrame();
  }
}

void DevToolsEyeDropper::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  if ((old_host && old_host->GetWidget() == host_) || (!old_host && !host_)) {
    DetachFromHost();
    AttachToHost(new_host->GetWidget());
  }
}

void DevToolsEyeDropper::ResetFrame() {
  frame_.reset();
  last_cursor_x_ = -1;
  last_cursor_y_ = -1;
}

bool DevToolsEyeDropper::HandleMouseEvent(const blink::WebMouseEvent& event) {
  last_cursor_x_ = event.PositionInWidget().x;
  last_cursor_y_ = event.PositionInWidget().y;
  if (frame_.drawsNothing())
    return true;

  if (event.button == blink::WebMouseEvent::Button::kLeft &&
      (event.GetType() == blink::WebInputEvent::kMouseDown ||
       event.GetType() == blink::WebInputEvent::kMouseMove)) {
    if (last_cursor_x_ < 0 || last_cursor_x_ >= frame_.width() ||
        last_cursor_y_ < 0 || last_cursor_y_ >= frame_.height()) {
      return true;
    }

    SkColor sk_color = frame_.getColor(last_cursor_x_, last_cursor_y_);

    // The picked colors are expected to be sRGB. Convert from |frame_|'s color
    // space to sRGB.
    SkPixmap pm(
        SkImageInfo::Make(1, 1, kBGRA_8888_SkColorType, kUnpremul_SkAlphaType,
                          frame_.refColorSpace()),
        &sk_color, sizeof(sk_color));
    uint8_t rgba_color[4];
    bool ok = pm.readPixels(
        SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType,
                          SkColorSpace::MakeSRGB()),
        rgba_color, sizeof(rgba_color));
    DCHECK(ok);

    callback_.Run(rgba_color[0], rgba_color[1], rgba_color[2], rgba_color[3]);
  }
  UpdateCursor();
  return true;
}

void DevToolsEyeDropper::UpdateCursor() {
  if (!host_ || frame_.drawsNothing())
    return;

  if (last_cursor_x_ < 0 || last_cursor_x_ >= frame_.width() ||
      last_cursor_y_ < 0 || last_cursor_y_ >= frame_.height()) {
    return;
  }

// Due to platform limitations, we are using two different cursors
// depending on the platform. Mac and Win have large cursors with two circles
// for original spot and its magnified projection; Linux gets smaller (64 px)
// magnified projection only with centered hotspot.
// Mac Retina requires cursor to be > 120px in order to render smoothly.

#if defined(OS_LINUX)
  const float kCursorSize = 63;
  const float kDiameter = 63;
  const float kHotspotOffset = 32;
  const float kHotspotRadius = 0;
  const float kPixelSize = 9;
#else
  const float kCursorSize = 150;
  const float kDiameter = 110;
  const float kHotspotOffset = 25;
  const float kHotspotRadius = 5;
  const float kPixelSize = 10;
#endif

  content::ScreenInfo screen_info;
  host_->GetScreenInfo(&screen_info);
  double device_scale_factor = screen_info.device_scale_factor;

  SkBitmap result;
  result.allocN32Pixels(kCursorSize * device_scale_factor,
                        kCursorSize * device_scale_factor);
  result.eraseARGB(0, 0, 0, 0);

  SkCanvas canvas(result);
  canvas.scale(device_scale_factor, device_scale_factor);
  canvas.translate(0.5f, 0.5f);

  SkPaint paint;

  // Paint original spot with cross.
  if (kHotspotRadius > 0) {
    paint.setStrokeWidth(1);
    paint.setAntiAlias(false);
    paint.setColor(SK_ColorDKGRAY);
    paint.setStyle(SkPaint::kStroke_Style);

    canvas.drawLine(kHotspotOffset, kHotspotOffset - 2 * kHotspotRadius,
                    kHotspotOffset, kHotspotOffset - kHotspotRadius, paint);
    canvas.drawLine(kHotspotOffset, kHotspotOffset + kHotspotRadius,
                    kHotspotOffset, kHotspotOffset + 2 * kHotspotRadius, paint);
    canvas.drawLine(kHotspotOffset - 2 * kHotspotRadius, kHotspotOffset,
                    kHotspotOffset - kHotspotRadius, kHotspotOffset, paint);
    canvas.drawLine(kHotspotOffset + kHotspotRadius, kHotspotOffset,
                    kHotspotOffset + 2 * kHotspotRadius, kHotspotOffset, paint);

    paint.setStrokeWidth(2);
    paint.setAntiAlias(true);
    canvas.drawCircle(kHotspotOffset, kHotspotOffset, kHotspotRadius, paint);
  }

  // Clip circle for magnified projection.
  float padding = (kCursorSize - kDiameter) / 2;
  SkPath clip_path;
  clip_path.addOval(SkRect::MakeXYWH(padding, padding, kDiameter, kDiameter));
  clip_path.close();
  canvas.clipPath(clip_path, SkClipOp::kIntersect, true);

  // Project pixels.
  int pixel_count = kDiameter / kPixelSize;
  SkRect src_rect = SkRect::MakeXYWH(last_cursor_x_ - pixel_count / 2,
                                     last_cursor_y_ - pixel_count / 2,
                                     pixel_count, pixel_count);
  SkRect dst_rect = SkRect::MakeXYWH(padding, padding, kDiameter, kDiameter);
  canvas.drawBitmapRect(frame_, src_rect, dst_rect, NULL);

  // Paint grid.
  paint.setStrokeWidth(1);
  paint.setAntiAlias(false);
  paint.setColor(SK_ColorGRAY);
  for (int i = 0; i < pixel_count; ++i) {
    canvas.drawLine(padding + i * kPixelSize, padding, padding + i * kPixelSize,
                    kCursorSize - padding, paint);
    canvas.drawLine(padding, padding + i * kPixelSize, kCursorSize - padding,
                    padding + i * kPixelSize, paint);
  }

  // Paint central pixel in red.
  SkRect pixel =
      SkRect::MakeXYWH((kCursorSize - kPixelSize) / 2,
                       (kCursorSize - kPixelSize) / 2, kPixelSize, kPixelSize);
  paint.setColor(SK_ColorRED);
  paint.setStyle(SkPaint::kStroke_Style);
  canvas.drawRect(pixel, paint);

  // Paint outline.
  paint.setStrokeWidth(2);
  paint.setColor(SK_ColorDKGRAY);
  paint.setAntiAlias(true);
  canvas.drawCircle(kCursorSize / 2, kCursorSize / 2, kDiameter / 2, paint);

  content::CursorInfo cursor_info;
  cursor_info.type = ui::CursorType::kCustom;
  cursor_info.image_scale_factor = device_scale_factor;
  cursor_info.custom_image = result;
  cursor_info.hotspot = gfx::Point(kHotspotOffset * device_scale_factor,
                                   kHotspotOffset * device_scale_factor);
  host_->SetCursor(cursor_info);
}

void DevToolsEyeDropper::OnFrameCaptured(
    base::ReadOnlySharedMemoryRegion data,
    ::media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  gfx::Size view_size = host_->GetView()->GetViewBounds().size();
  if (view_size != content_rect.size()) {
    video_capturer_->SetResolutionConstraints(view_size, view_size, true);
    video_capturer_->RequestRefreshFrame();
    return;
  }

  mojo::Remote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
      callbacks_remote(std::move(callbacks));

  if (!data.IsValid()) {
    callbacks_remote->Done();
    return;
  }
  base::ReadOnlySharedMemoryMapping mapping = data.Map();
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "Shared memory mapping failed.";
    return;
  }
  if (mapping.size() <
      media::VideoFrame::AllocationSize(info->pixel_format, info->coded_size)) {
    DLOG(ERROR) << "Shared memory size was less than expected.";
    return;
  }
  if (!info->color_space) {
    DLOG(ERROR) << "Missing mandatory color space info.";
    return;
  }

  // The SkBitmap's pixels will be marked as immutable, but the installPixels()
  // API requires a non-const pointer. So, cast away the const.
  void* const pixels = const_cast<void*>(mapping.memory());

  // Call installPixels() with a |releaseProc| that: 1) notifies the capturer
  // that this consumer has finished with the frame, and 2) releases the shared
  // memory mapping.
  struct FramePinner {
    // Keeps the shared memory that backs |frame_| mapped.
    base::ReadOnlySharedMemoryMapping mapping;
    // Prevents FrameSinkVideoCapturer from recycling the shared memory that
    // backs |frame_|.
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        releaser;
  };
  frame_.installPixels(
      SkImageInfo::MakeN32(content_rect.width(), content_rect.height(),
                           kPremul_SkAlphaType,
                           info->color_space->ToSkColorSpace()),
      pixels,
      media::VideoFrame::RowBytes(media::VideoFrame::kARGBPlane,
                                  info->pixel_format, info->coded_size.width()),
      [](void* addr, void* context) {
        delete static_cast<FramePinner*>(context);
      },
      new FramePinner{std::move(mapping), callbacks_remote.Unbind()});
  frame_.setImmutable();

  UpdateCursor();
}

void DevToolsEyeDropper::OnStopped() {}
