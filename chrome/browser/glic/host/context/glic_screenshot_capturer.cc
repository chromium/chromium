// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace glic {

namespace {

// Helper used to convert the captured DesktopFrame to a JPEG.
std::vector<uint8_t> ConvertFrameToJpeg(
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  CHECK(frame);
  SkImageInfo image_info =
      SkImageInfo::Make(frame->size().width(), frame->size().height(),
                        kBGRA_8888_SkColorType, kPremul_SkAlphaType);

  SkBitmap sk_bitmap;
  if (!sk_bitmap.tryAllocPixels(image_info)) {
    return {};
  }

  SkPixmap pixmap(image_info, frame->data(), frame->stride());
  if (!sk_bitmap.writePixels(pixmap)) {
    return {};
  }

  auto start_time = base::Time::Now();
  auto jpeg_data = gfx::JPEGCodec::Encode(
      sk_bitmap, features::kGlicScreenshotEncodeQuality.Get());
  if (!jpeg_data.has_value()) {
    return {};
  }
  auto encode_time = base::Time::Now() - start_time;
  VLOG(1) << "JPEGCodec::Encode() frame size: " << frame->size().width() << "x"
          << frame->size().height();
  VLOG(1) << "JPEGCodec::Encode() time: " << encode_time;
  VLOG(1) << "JPEGCodec::Encode() result file size: " << jpeg_data->size();
  return std::move(*jpeg_data);
}

}  // namespace

GlicScreenshotCapturer::GlicScreenshotCapturer() = default;

GlicScreenshotCapturer::~GlicScreenshotCapturer() = default;

void GlicScreenshotCapturer::CaptureScreenshot(
    gfx::NativeWindow parent_window,
    glic::mojom::WebClientHandler::CaptureScreenshotCallback callback) {
  if (capture_callback_) {
    std::move(callback).Run(mojom::CaptureScreenshotResult::NewErrorReason(
        glic::mojom::CaptureScreenshotErrorReason::
            kScreenCaptureRequestThrottled));
    return;
  }
  capture_callback_ = std::move(callback);
  if (!parent_window) {
    SignalError(glic::mojom::CaptureScreenshotErrorReason::kUnknown);
    return;
  }
  // Construct picker.
  picker_controller_ = std::make_unique<DesktopMediaPickerController>();
  const std::u16string name(
      l10n_util::GetStringUTF16(IDS_GLIC_SCREEN_PICKER_REQUESTER));
  DesktopMediaPickerController::Params picker_params(
      DesktopMediaPickerController::Params::RequestSource::kGlic);
  picker_params.context = parent_window;
  picker_params.parent = parent_window;
  picker_params.app_name = name;
  picker_params.target_name = name;
  picker_params.request_audio = false;
  picker_params.restricted_by_policy = false;
  DesktopMediaList::WebContentsFilter includable_web_contents_filter =
      base::BindRepeating([](content::WebContents* wc) { return false; });
  DesktopMediaPickerController::DoneCallback source_selected_callback =
      base::BindOnce(&GlicScreenshotCapturer::OnSourceSelected,
                     weak_ptr_factory_.GetWeakPtr());
  picker_controller_->Show(picker_params, {DesktopMediaList::Type::kScreen},
                           includable_web_contents_filter,
                           std::move(source_selected_callback));
}

void GlicScreenshotCapturer::CloseScreenPicker() {
  picker_controller_.reset();
  if (capture_callback_) {
    SignalError(glic::mojom::CaptureScreenshotErrorReason::
                    kUserCancelledScreenPickerDialog);
  }
}

void GlicScreenshotCapturer::OnSourceSelected(const std::string& err,
                                              content::DesktopMediaID id) {
  picker_controller_ = nullptr;
  if (!err.empty()) {
    DVLOG(1) << "Unknown error while selecting source: " << err;
    SignalError(glic::mojom::CaptureScreenshotErrorReason::kUnknown);
    return;
  }
  if (id.is_null()) {
    SignalError(glic::mojom::CaptureScreenshotErrorReason::
                    kUserCancelledScreenPickerDialog);
    return;
  }
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/405177421): Remove this delay once we land the code to skip
  // the screen picker entirely on Windows.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GlicScreenshotCapturer::OnCaptureStarted,
                     weak_ptr_factory_.GetWeakPtr(), id),
      base::Milliseconds(500));
#else
  OnCaptureStarted(id);
#endif
}

void GlicScreenshotCapturer::OnCaptureStarted(content::DesktopMediaID id) {
  desktop_capturer_ = content::desktop_capture::CreateScreenCapturer();
  desktop_capturer_->Start(this);
  if (!desktop_capturer_->SelectSource(id.id)) {
    SignalError(glic::mojom::CaptureScreenshotErrorReason::kUnknown);
    return;
  }
  desktop_capturer_->CaptureFrame();
}

void GlicScreenshotCapturer::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  if (!frame) {
    SignalError(glic::mojom::CaptureScreenshotErrorReason::kUnknown);
    return;
  }
  frame_size_ = frame->size();
  // Encode frame to JPEG off thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ConvertFrameToJpeg, std::move(frame)),
      base::BindOnce(&GlicScreenshotCapturer::SignalScreenshotResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GlicScreenshotCapturer::SignalScreenshotResult(
    std::vector<uint8_t> jpeg_data) {
  if (jpeg_data.empty()) {
    DVLOG(1) << "Could not convert frame to JPEG";
    SignalError(glic::mojom::CaptureScreenshotErrorReason::kUnknown);
    return;
  }
  mojom::ScreenshotPtr screenshot = mojom::Screenshot::New();
  screenshot->width_pixels = frame_size_.width();
  screenshot->height_pixels = frame_size_.height();
  screenshot->mime_type = "image/jpeg";
  screenshot->data = std::move(jpeg_data);
  screenshot->origin_annotations = mojom::ImageOriginAnnotations::New();
  std::move(capture_callback_)
      .Run(
          mojom::CaptureScreenshotResult::NewScreenshot(std::move(screenshot)));
}

void GlicScreenshotCapturer::SignalError(
    glic::mojom::CaptureScreenshotErrorReason error_reason) {
  std::move(capture_callback_)
      .Run(mojom::CaptureScreenshotResult::NewErrorReason(error_reason));
}

// static
std::vector<uint8_t> GlicScreenshotCapturer::ConvertFrameToJpegForTesting(
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  return ConvertFrameToJpeg(std::move(frame));
}

}  // namespace glic
