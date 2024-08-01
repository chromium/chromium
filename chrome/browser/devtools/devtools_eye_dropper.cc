// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_eye_dropper.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/shared_memory_mapping.h"
#include "build/build_config.h"
#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/common/features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/size_conversions.h"

DevToolsEyeDropper::DevToolsEyeDropper(content::WebContents* web_contents,
                                       EyeDropperCallback callback)
    : content::WebContentsObserver(web_contents), callback_(callback) {
  mouse_event_callback_ = base::BindRepeating(
      &DevToolsEyeDropper::HandleMouseEvent, base::Unretained(this));
  if (web_contents->GetPrimaryMainFrame()->IsRenderFrameLive())
    AttachToHost(web_contents->GetPrimaryMainFrame());
}

DevToolsEyeDropper::~DevToolsEyeDropper() {
  if (host_) {
    // If the renderer frame was destroyed already, we're already detached.
    DetachFromHost();
  }
}

void DevToolsEyeDropper::AttachToHost(content::RenderFrameHost* frame_host) {
  DCHECK(frame_host->IsRenderFrameLive());
  // Historically, (see https://crbug.com/847363) this code handled the
  // RenderWidgetHostView being null, but now it is listening to creation of the
  // frame which includes creation of the widget so it is implied that
  // RenderWidgetHostView exists.
  DCHECK(frame_host->GetView());

  host_ = frame_host->GetView()->GetRenderWidgetHost();
  host_->AddMouseEventCallback(mouse_event_callback_);

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
  video_capturer_->SetFormat(media::PIXEL_FORMAT_ARGB);
  video_capturer_->SetMinCapturePeriod(base::Seconds(1) / kMaxFrameRate);
  video_capturer_->Start(this, viz::mojom::BufferFormatPreference::kDefault);
}

void DevToolsEyeDropper::DetachFromHost() {
  host_->RemoveMouseEventCallback(mouse_event_callback_);
  host_->SetCursor(ui::mojom::CursorType::kPointer);
  video_capturer_.reset();
  host_ = nullptr;
}

void DevToolsEyeDropper::RenderFrameCreated(
    content::RenderFrameHost* frame_host) {
  // Only handle the initial main frame, not speculative ones.
  if (frame_host != web_contents()->GetPrimaryMainFrame())
    return;
  DCHECK(!host_);

  AttachToHost(frame_host);
}

void DevToolsEyeDropper::RenderFrameDeleted(
    content::RenderFrameHost* frame_host) {
  // Only handle the active main frame, not speculative ones.
  if (frame_host != web_contents()->GetPrimaryMainFrame())
    return;
  DCHECK(host_);
  DCHECK_EQ(host_, frame_host->GetRenderWidgetHost());

  DetachFromHost();
  ResetFrame();
}

void DevToolsEyeDropper::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  // Since we skipped speculative main frames in RenderFrameCreated, we must
  // watch for them being swapped in by watching for RenderFrameHostChanged().
  if (new_host != web_contents()->GetPrimaryMainFrame())
    return;
  // Don't watch for the initial main frame RenderFrameHost, which does not come
  // with a renderer frame. We'll hear about that from RenderFrameCreated.
  if (!old_host) {
    // If this fails, then we need to AttachToHost() here when the `new_host`
    // has its renderer frame. Since `old_host` is null only when this observer
    // method is called at startup, it should be before the renderer frame is
    // created.
    DCHECK(!new_host->IsRenderFrameLive());
    return;
  }
  DCHECK(host_);
  DCHECK_EQ(host_, old_host->GetRenderWidgetHost());

  DetachFromHost();
  AttachToHost(new_host);
}

void DevToolsEyeDropper::ResetFrame() {
  frame_.reset();
  last_cursor_x_ = -1;
  last_cursor_y_ = -1;
}

bool DevToolsEyeDropper::HandleMouseEvent(const blink::WebMouseEvent& event) {
  last_cursor_x_ = event.PositionInWidget().x();
  last_cursor_y_ = event.PositionInWidget().y();
  if (frame_.drawsNothing())
    return true;

  if (event.button == blink::WebMouseEvent::Button::kLeft &&
      (event.GetType() == blink::WebInputEvent::Type::kMouseDown ||
       event.GetType() == blink::WebInputEvent::Type::kMouseMove)) {
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

  // Due to platform limitations, we are using two different cursors depending
  // on the platform. Linux, Mac and Win have large cursors with two circles for
  // original spot and its magnified projection; Ash gets smaller (64 px)
  // magnified projection only with centered hotspot.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const float kCursorSize = 63;
  const float kDiameter = 63;
  const float kHotspotOffset = 32;
  const float kHotspotRadius = 0;
  const float kPixelSize = 9;
#else
  // Mac Retina requires cursor to be > 120px in order to render smoothly.
  const float kCursorSize = 150;
  const float kDiameter = 110;
  const float kHotspotOffset = 25;
  const float kHotspotRadius = 5;
  const float kPixelSize = 10;
#endif

  float device_scale_factor = host_->GetDeviceScaleFactor();

  SkBitmap result;
  result.allocN32Pixels(kCursorSize * device_scale_factor,
                        kCursorSize * device_scale_factor);
  result.eraseARGB(0, 0, 0, 0);

  SkCanvas canvas(result, skia::LegacyDisplayGlobals::GetSkSurfaceProps());
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
  canvas.drawImageRect(frame_.asImage(), src_rect, dst_rect,
                       SkSamplingOptions(), nullptr,
                       SkCanvas::kStrict_SrcRectConstraint);

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

  ui::Cursor cursor =
      ui::Cursor::NewCustom(std::move(result),
                            gfx::Point(kHotspotOffset * device_scale_factor,
                                       kHotspotOffset * device_scale_factor),
                            device_scale_factor);
  host_->SetCursor(std::move(cursor));
}

void DevToolsEyeDropper::OnFrameCaptured(
    ::media::mojom::VideoBufferHandlePtr data,
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

  CHECK(data->is_read_only_shmem_region());
  base::ReadOnlySharedMemoryRegion& shmem_region =
      data->get_read_only_shmem_region();

  // The |data| parameter is not nullable and mojo type mapping for
  // `base::ReadOnlySharedMemoryRegion` defines that nullable version of it is
  // the same type, with null check being equivalent to IsValid() check. Given
  // the above, we should never be able to receive a read only shmem region that
  // is not valid - mojo will enforce it for us.
  DCHECK(shmem_region.IsValid());

  base::ReadOnlySharedMemoryMapping mapping = shmem_region.Map();
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "Shared memory mapping failed.";
    return;
  }

  base::span<const uint8_t> pixels(mapping);

  if (pixels.size() <
      media::VideoFrame::AllocationSize(info->pixel_format, info->coded_size)) {
    DLOG(ERROR) << "Shared memory size was less than expected.";
    return;
  }

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

  auto image_info = SkImageInfo::MakeN32(
      content_rect.width(), content_rect.height(), kPremul_SkAlphaType,
      info->color_space.ToSkColorSpace());
  const size_t row_bytes =
      media::VideoFrame::RowBytes(media::VideoFrame::Plane::kARGB,
                                  info->pixel_format, info->coded_size.width());
  // installPixels() takes an unsafe unbounded pointer, ensure it's pointing to
  // enough memory.
  CHECK_GE(pixels.size(), image_info.computeByteSize(row_bytes));

  frame_.installPixels(
      image_info,
      // The SkBitmap's pixels will be marked as immutable, but the
      // installPixels() API requires a non-const pointer. So, cast away the
      // const.
      const_cast<uint8_t*>(pixels.data()), row_bytes,
      [](void* addr, void* context) {
        delete static_cast<FramePinner*>(context);
      },
      new FramePinner{std::move(mapping), callbacks_remote.Unbind()});
  frame_.setImmutable();

  UpdateCursor();
}

void DevToolsEyeDropper::OnNewSubCaptureTargetVersion(
    uint32_t sub_capture_target_version) {}

void DevToolsEyeDropper::OnFrameWithEmptyRegionCapture() {}

void DevToolsEyeDropper::OnStopped() {}
