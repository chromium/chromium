// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/save_to_drive_event_dispatcher.h"

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/download/status_text_builder_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/save_to_drive/save_to_drive_recorder.h"
#include "chrome/browser/save_to_drive/save_to_drive_utils.h"
#include "chrome/browser/save_to_drive/time_remaining_calculator.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/constants.h"

namespace save_to_drive {
namespace {

using extensions::api::pdf_viewer_private::SaveToDriveStatus;

}  // namespace

// static
std::unique_ptr<SaveToDriveEventDispatcher> SaveToDriveEventDispatcher::Create(
    content::RenderFrameHost* render_frame_host) {
  auto stream = GetStreamWeakPtr(render_frame_host);
  if (!stream || stream->stream_url().spec().empty()) {
    return nullptr;
  }
  return base::WrapUnique(new SaveToDriveEventDispatcher(
      render_frame_host, stream->stream_url(),
      std::make_unique<TimeRemainingCalculator>(),
      std::make_unique<SaveToDriveRecorder>(Profile::FromBrowserContext(
          render_frame_host->GetBrowserContext()))));
}

// static
std::unique_ptr<SaveToDriveEventDispatcher>
SaveToDriveEventDispatcher::CreateForTesting(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<TimeRemainingCalculator> time_remaining_calculator,
    std::unique_ptr<SaveToDriveRecorder> recorder) {
  auto stream = GetStreamWeakPtr(render_frame_host);
  if (!stream || stream->stream_url().spec().empty()) {
    return nullptr;
  }
  return base::WrapUnique(new SaveToDriveEventDispatcher(
      render_frame_host, stream->stream_url(),
      std::move(time_remaining_calculator), std::move(recorder)));
}

SaveToDriveEventDispatcher::~SaveToDriveEventDispatcher() = default;

std::optional<std::string> SaveToDriveEventDispatcher::GetFileMetadataString(
    const extensions::api::pdf_viewer_private::SaveToDriveProgress& progress)
    const {
  switch (progress.status) {
    case SaveToDriveStatus::kUploadCompleted:
      return base::UTF16ToUTF8(
          StatusTextBuilderUtils::GetCompletedTotalSizeString(
              progress.file_size_bytes.value()));
    case SaveToDriveStatus::kUploadInProgress:
    case SaveToDriveStatus::kUploadStarted: {
      std::u16string file_metadata_string =
          StatusTextBuilderUtils::GetBubbleProgressSizesString(
              progress.uploaded_bytes.value(),
              progress.file_size_bytes.value());
      if (const std::optional<std::u16string> time_remaining_text =
              time_remaining_calculator_->CalculateTimeRemainingText(progress);
          time_remaining_text.has_value()) {
        file_metadata_string =
            StatusTextBuilderUtils::GetBubbleStatusMessageWithBytes(
                file_metadata_string, time_remaining_text.value());
      }
      return base::UTF16ToUTF8(file_metadata_string);
    }
    default:
      return std::nullopt;
  }
}

void SaveToDriveEventDispatcher::Notify(
    extensions::api::pdf_viewer_private::SaveToDriveProgress progress) const {
  CHECK_NE(progress.error_type,
           extensions::api::pdf_viewer_private::SaveToDriveErrorType::kNone);
  CHECK_NE(progress.status, SaveToDriveStatus::kNone);
  recorder_->Record(progress);
  progress.file_metadata = GetFileMetadataString(progress);
  base::Value::List args;
  args.Append(stream_url_.spec());
  args.Append(progress.ToValue());
  auto event = std::make_unique<extensions::Event>(
      extensions::events::PDF_VIEWER_PRIVATE_ON_SAVE_TO_DRIVE_PROGRESS,
      extensions::api::pdf_viewer_private::OnSaveToDriveProgress::kEventName,
      std::move(args), browser_context_);
  auto* event_router = extensions::EventRouter::Get(browser_context_);
  event_router->DispatchEventToExtension(extension_misc::kPdfExtensionId,
                                         std::move(event));
}

SaveToDriveEventDispatcher::SaveToDriveEventDispatcher(
    content::RenderFrameHost* render_frame_host,
    const GURL& stream_url,
    std::unique_ptr<TimeRemainingCalculator> time_remaining_calculator,
    std::unique_ptr<SaveToDriveRecorder> recorder)
    : browser_context_(render_frame_host->GetBrowserContext()),
      stream_url_(stream_url),
      time_remaining_calculator_(std::move(time_remaining_calculator)),
      recorder_(std::move(recorder)) {}

}  // namespace save_to_drive
