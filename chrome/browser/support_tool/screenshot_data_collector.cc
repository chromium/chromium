// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/support_tool/screenshot_data_collector.h"

#include <vector>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host_view.h"
#include "net/base/data_url.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/snapshot/snapshot.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/aura/window.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

namespace {

constexpr char kOriginURL[] = "chrome://support-tool";
constexpr char16_t kName[] = u"Support Tool";
constexpr int kDefaultQuality = 90;
constexpr char kBase64Header[] = "data:image/jpeg;base64,";
constexpr char kCaptureCanceled[] = "CANCELED";

// Encodes `base64_data` and writes the result to `path`. Returns true if
// succeeded and false otherwise.
bool WriteOutputFile(std::string base64_data, base::FilePath path) {
  std::string mime_type, charset, data;
  if (!net::DataURL::Parse(GURL(std::move(base64_data)), &mime_type, &charset,
                           &data) ||
      !base::WriteFile(path, std::move(data))) {
    return false;
  }
  return true;
}

}  // namespace

ScreenshotDataCollector::ScreenshotDataCollector() = default;

ScreenshotDataCollector::~ScreenshotDataCollector() = default;

std::string ScreenshotDataCollector::GetScreenshotBase64() {
  return std::move(screenshot_base64_);
}

void ScreenshotDataCollector::SetScreenshotBase64(
    std::string screenshot_base64) {
  screenshot_base64_ = std::move(screenshot_base64);
}

std::string ScreenshotDataCollector::GetName() const {
  return "Screenshot";
}

std::string ScreenshotDataCollector::GetDescription() const {
  return "Captures a screenshot.";
}

const PIIMap& ScreenshotDataCollector::GetDetectedPII() {
  return pii_map_;
}

void ScreenshotDataCollector::SetPickerFactoryForTesting(
    DesktopMediaPickerFactory* picker_factory_ptr) {
  picker_factory_for_testing_ = picker_factory_ptr;
}

void ScreenshotDataCollector::ConvertDesktopFrameToBase64JPEG(
    std::unique_ptr<webrtc::DesktopFrame> frame,
    std::string& image_base64) {
  // First converts `frame` to SkBitmap.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(frame->size().width(), frame->size().height(), true);
  // Data in `frame` begin at `data()` but are not necessarily consecutive.
  uint32_t* bitmap_buffer = bitmap.getAddr32(0, 0);
  const uint8_t* frame_buffer = frame->data();
  // There are `frame_bytes` bytes of real data per row in `frame`. This is not
  // necessarily the same as `stride()`, which is where the next row of data
  // begins. Also, `frame_bytes` <= `stride()`.
  const size_t frame_bytes =
      frame->size().width() * webrtc::DesktopFrame::kBytesPerPixel;
  // Next we need to copy the data row by row.
  for (int i = 0; i < frame->size().height(); ++i) {
    // Again, `frame_bytes` is the actual size of the data in a row.
    memcpy(bitmap_buffer, frame_buffer, frame_bytes);
    // Moves to where the next row's data begins.
    bitmap_buffer += bitmap.rowBytesAsPixels();
    frame_buffer += frame->stride();
  }
  bitmap.setImmutable();

  // Then encodes the image with jpeg.
  std::vector<unsigned char> jpeg_encoded_data;
  if (!gfx::JPEGCodec::Encode(std::move(bitmap), kDefaultQuality,
                              &jpeg_encoded_data)) {
    SupportToolError error = {SupportToolErrorCode::kDataCollectorError,
                              "ScreenshotDataCollector had error: Failed to "
                              "encode frame to JPEG image."};
    std::move(data_collector_done_callback_).Run(error);
    return;
  }

  // Finally converts the image in a string with base64 encoding.
  image_base64 = base::StrCat(
      {kBase64Header, base::Base64Encode(std::move(jpeg_encoded_data))});
}

void ScreenshotDataCollector::CollectDataAndDetectPII(
    DataCollectorDoneCallback on_data_collected_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_collector_done_callback_ = std::move(on_data_collected_callback);
  Browser* browser = chrome::FindBrowserWithActiveWindow();
  if (!browser) {
    browser = chrome::FindLastActive();
  }
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  gfx::NativeWindow parent_window = web_contents->GetTopLevelNativeWindow();

  // Generates a `DesktopMediaPicker` dialog.
  const GURL origin(kOriginURL);
  const std::u16string name(kName);
  std::vector<DesktopMediaList::Type> media_types = {
      DesktopMediaList::Type::kScreen, DesktopMediaList::Type::kWindow,
      DesktopMediaList::Type::kWebContents};
  AllowedScreenCaptureLevel capture_level =
      capture_policy::GetAllowedCaptureLevel(origin, web_contents);
  capture_policy::FilterMediaList(media_types, capture_level);
  DesktopMediaList::WebContentsFilter includable_web_contents_filter =
      capture_policy::GetIncludableWebContentsFilter(origin, capture_level);

  DesktopMediaPickerController::DoneCallback callback =
      base::BindOnce(&ScreenshotDataCollector::OnSourceSelected,
                     weak_ptr_factory_.GetWeakPtr());
  DesktopMediaPickerController::Params picker_params(
      DesktopMediaPickerController::Params::RequestSource::
          kScreenshotDataCollector);
  picker_params.web_contents = web_contents;
  picker_params.context = parent_window;
  picker_params.parent = parent_window;
  picker_params.app_name = name;
  picker_params.target_name = name;
  picker_params.request_audio = false;
  picker_controller_ = std::make_unique<DesktopMediaPickerController>(
      picker_factory_for_testing_);
  picker_params.restricted_by_policy =
      (capture_level != AllowedScreenCaptureLevel::kUnrestricted);
  picker_controller_->Show(picker_params, std::move(media_types),
                           includable_web_contents_filter, std::move(callback));
}

void ScreenshotDataCollector::OnSourceSelected(const std::string& err,
                                               content::DesktopMediaID id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!err.empty()) {
    SupportToolError error = {
        SupportToolErrorCode::kDataCollectorError,
        base::StringPrintf("ScreenshotDataCollector had error: %s",
                           err.c_str())};
    std::move(data_collector_done_callback_).Run(error);
    return;
  } else if (id.is_null()) {
    // User canceled the dialog.
    screenshot_base64_ = kCaptureCanceled;
    return;
  }
#if BUILDFLAG(IS_CHROMEOS)
  gfx::NativeWindow window = gfx::NativeWindow();
  switch (id.type) {
    case content::DesktopMediaID::Type::TYPE_WEB_CONTENTS: {
      window = content::RenderFrameHost::FromID(
                   id.web_contents_id.render_process_id,
                   id.web_contents_id.main_render_frame_id)
                   ->GetNativeView();
      break;
    }
    case content::DesktopMediaID::Type::TYPE_WINDOW:
    case content::DesktopMediaID::Type::TYPE_SCREEN: {
      window = content::DesktopMediaID::GetNativeWindowById(id);
      break;
    }
    default: {
      NOTREACHED_IN_MIGRATION();
      break;
    }
  }
  const gfx::Rect bounds(window->bounds().width(), window->bounds().height());
  ui::GrabWindowSnapshotAsJPEG(
      std::move(window), std::move(bounds),
      base::BindOnce(&ScreenshotDataCollector::OnScreenshotTaken,
                     weak_ptr_factory_.GetWeakPtr()));
  return;
#else
  switch (id.type) {
    case content::DesktopMediaID::Type::TYPE_WEB_CONTENTS: {
      content::RenderFrameHost* const host = content::RenderFrameHost::FromID(
          id.web_contents_id.render_process_id,
          id.web_contents_id.main_render_frame_id);
      content::RenderWidgetHostView* const view =
          host ? host->GetView() : nullptr;
      if (!view) {
        SupportToolError error = {
            SupportToolErrorCode::kDataCollectorError,
            "ScreenshotDataCollector had error: Cannot find selected tab."};
        std::move(data_collector_done_callback_).Run(error);
        return;
      }
      view->CopyFromSurface(
          gfx::Rect(), gfx::Size(),
          base::BindPostTask(
              content::GetUIThreadTaskRunner({}),
              base::BindOnce(&ScreenshotDataCollector::OnTabCaptured,
                             weak_ptr_factory_.GetWeakPtr())));
      return;
    }
    case content::DesktopMediaID::Type::TYPE_WINDOW: {
      desktop_capturer_ = content::desktop_capture::CreateWindowCapturer();
      break;
    }
    case content::DesktopMediaID::Type::TYPE_SCREEN: {
      desktop_capturer_ = content::desktop_capture::CreateScreenCapturer();
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }
  desktop_capturer_->Start(this);
  if (!desktop_capturer_->SelectSource(id.id)) {
    SupportToolError error = {
        SupportToolErrorCode::kDataCollectorError,
        "ScreenshotDataCollector had error: Cannot capture source."};
    std::move(data_collector_done_callback_).Run(error);
    return;
  }
  desktop_capturer_->CaptureFrame();
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS)
void ScreenshotDataCollector::OnScreenshotTaken(
    scoped_refptr<base::RefCountedMemory> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (data && data.get()) {
    screenshot_base64_ =
        base::StrCat({kBase64Header, base::Base64Encode(*data)});
    std::move(data_collector_done_callback_).Run(/*error=*/std::nullopt);
    return;
  }
  SupportToolError error = {
      SupportToolErrorCode::kDataCollectorError,
      "ScreenshotDataCollector had error: Failed to take screenshot."};
  std::move(data_collector_done_callback_).Run(error);
}
#else
void ScreenshotDataCollector::OnTabCaptured(const SkBitmap& bitmap) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<unsigned char> jpeg_encoded_data;
  if (bitmap.drawsNothing() ||
      !gfx::JPEGCodec::Encode(bitmap, kDefaultQuality, &jpeg_encoded_data)) {
    SupportToolError error = {
        SupportToolErrorCode::kDataCollectorError,
        "ScreenshotDataCollector had error: Tab capture failed."};
    std::move(data_collector_done_callback_).Run(error);
    return;
  }
  screenshot_base64_ = base::StrCat(
      {kBase64Header, base::Base64Encode(std::move(jpeg_encoded_data))});
  std::move(data_collector_done_callback_).Run(/*error=*/std::nullopt);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

void ScreenshotDataCollector::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result != webrtc::DesktopCapturer::Result::SUCCESS) {
    SupportToolError error = {
        SupportToolErrorCode::kDataCollectorError,
        "ScreenshotDataCollector had error: Cannot capture source."};
    std::move(data_collector_done_callback_).Run(error);
    return;
  }

  ConvertDesktopFrameToBase64JPEG(std::move(frame), screenshot_base64_);
  std::move(data_collector_done_callback_).Run(/*error=*/std::nullopt);
}

void ScreenshotDataCollector::ExportCollectedDataWithPII(
    std::set<redaction::PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    DataCollectorDoneCallback on_exported_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_collector_done_callback_ = std::move(on_exported_callback);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WriteOutputFile, std::move(screenshot_base64_),
                     std::move(target_directory)
                         .Append(FILE_PATH_LITERAL("screenshot.jpg"))),
      base::BindOnce(&ScreenshotDataCollector::OnScreenshotExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScreenshotDataCollector::OnScreenshotExported(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    SupportToolError error = {
        SupportToolErrorCode::kDataExportError,
        "ScreenshotDataCollector had error: Failed when exporting screenshot."};
    std::move(data_collector_done_callback_).Run(error);
    return;
  }
  std::move(data_collector_done_callback_).Run(/*error=*/std::nullopt);
}
