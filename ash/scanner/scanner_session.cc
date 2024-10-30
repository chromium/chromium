// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_session.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/public/cpp/scanner/scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_action_view_model.h"
#include "ash/scanner/scanner_command_delegate.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/scanner.pb.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "url/gurl.h"
namespace ash {

namespace {
// The maximum number of pixels in either width or height of an image to be
// processed by scanner.
// We chose a conservative number based on typical laptop screen sizes in case
// that is required by the backend.
constexpr int64_t kMaxEdgePixels = 2300;

// The quality attribute (out of 100) for the JPEG compression algorithm used.
constexpr int kJpegQuality = 90;

// Converts a proto action - a oneof - into the equivalent variant type in
// Scanner code.
// Returns nullopt if the action is unsupported.
std::optional<ScannerAction> ProtoActionToVariant(
    manta::proto::ScannerAction proto_action) {
  switch (proto_action.action_case()) {
    case manta::proto::ScannerAction::kNewEvent:
      return std::move(*proto_action.mutable_new_event());

    case manta::proto::ScannerAction::kNewContact:
      return std::move(*proto_action.mutable_new_contact());

    case manta::proto::ScannerAction::kNewGoogleDoc: {
      manta::proto::NewGoogleDocAction& new_doc_action =
          *proto_action.mutable_new_google_doc();
      return NewGoogleDocAction(std::move(new_doc_action.title()),
                                std::move(new_doc_action.html_contents()));
    }

    case manta::proto::ScannerAction::kNewGoogleSheet: {
      manta::proto::NewGoogleSheetAction& new_sheet_action =
          *proto_action.mutable_new_google_sheet();
      return NewGoogleSheetAction(std::move(new_sheet_action.title()),
                                  std::move(new_sheet_action.csv_contents()));
    }

    case manta::proto::ScannerAction::kCopyToClipboard: {
      manta::proto::CopyToClipboardAction& copy_action =
          *proto_action.mutable_copy_to_clipboard();
      return CopyToClipboardAction(std::move(copy_action.plain_text()),
                                   std::move(copy_action.html_text()));
    }

    case manta::proto::ScannerAction::ACTION_NOT_SET:
      return std::nullopt;
  }

  // This should never be reached, as `action_case()` should always return a
  // valid enum value. If the oneof field is set to something which is not
  // recognised by this client, that is indistinguishable from an unknown field,
  // and the above case should be `ACTION_NOT_SET`.
  NOTREACHED();
}

// Downscales so that the width and height are both less than kMaxEdgePixels and
// retains the ratio.
scoped_refptr<base::RefCountedMemory> DownscaleImageIfNeeded(
    scoped_refptr<base::RefCountedMemory> original_bytes) {
  if (original_bytes == nullptr) {
    return nullptr;
  }

  SkBitmap img = gfx::JPEGCodec::Decode(*original_bytes);
  int width = img.width();
  int height = img.height();

  if (width <= 0 || height <= 0 ||
      (width <= kMaxEdgePixels && height <= kMaxEdgePixels) ||
      (width * height <= kMaxEdgePixels * kMaxEdgePixels)) {
    return original_bytes;
  }

  if (width > height) {
    height = kMaxEdgePixels * height / width;
    width = kMaxEdgePixels;
  } else {
    width = kMaxEdgePixels * width / height;
    height = kMaxEdgePixels;
  }

  // If the height and width is 0 given that we only try to resize when the
  // total pixels are less than 2300x2300, it will only be possible if it was
  // 5 million pixels in one edge and then 1 pixel in the other.
  //
  // This is clearly an edge case so we will return nullptr to stop processing
  // this image.
  if (height == 0 || width == 0) {
    return nullptr;
  }

  std::optional<std::vector<uint8_t>> shrunk_jpeg_bytes =
      gfx::JPEGCodec::Encode(
          skia::ImageOperations::Resize(img, skia::ImageOperations::RESIZE_BEST,
                                        width, height),
          kJpegQuality);

  if (shrunk_jpeg_bytes.has_value()) {
    return base::MakeRefCounted<base::RefCountedBytes>(
        std::move(*shrunk_jpeg_bytes));
  }

  // Fallback.
  return original_bytes;
}

}  // namespace

ScannerSession::ScannerSession(ScannerProfileScopedDelegate* delegate)
    : delegate_(delegate) {}

ScannerSession::~ScannerSession() = default;

void ScannerSession::FetchActionsForImage(
    scoped_refptr<base::RefCountedMemory> jpeg_bytes,
    FetchActionsCallback callback) {
  delegate_->FetchActionsForImage(
      DownscaleImageIfNeeded(std::move(jpeg_bytes)),
      base::BindOnce(&ScannerSession::OnActionsReturned,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ScannerSession::OnActionsReturned(
    FetchActionsCallback callback,
    std::unique_ptr<manta::proto::ScannerOutput> output,
    manta::MantaStatus status) {
  if (output == nullptr) {
    // TODO(b/363100868): Handle error case
    std::move(callback).Run({});
    return;
  }
  if (output->objects_size() == 0) {
    std::move(callback).Run({});
    return;
  }

  std::vector<ScannerActionViewModel> action_view_models;

  for (manta::proto::ScannerAction& proto_action :
       *output->mutable_objects(0)->mutable_actions()) {
    std::optional<ScannerAction> variant_action =
        ProtoActionToVariant(std::move(proto_action));
    if (variant_action.has_value()) {
      action_view_models.emplace_back(std::move(*variant_action),
                                      weak_ptr_factory_.GetWeakPtr());
    }
  }

  std::move(callback).Run(std::move(action_view_models));
}

void ScannerSession::OpenUrl(const GURL& url) {
  NewWindowDelegate::GetInstance()->OpenUrl(
      url, NewWindowDelegate::OpenUrlFrom::kUnspecified,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

drive::DriveServiceInterface* ScannerSession::GetDriveService() {
  return delegate_->GetDriveService();
}

google_apis::RequestSender* ScannerSession::GetGoogleApisRequestSender() {
  return delegate_->GetGoogleApisRequestSender();
}

void ScannerSession::SetClipboard(std::unique_ptr<ui::ClipboardData> data) {
  CHECK_DEREF(ui::ClipboardNonBacked::GetForCurrentThread())
      .WriteClipboardData(std::move(data));

  // TODO: b/367871707 - Display a toast / notification if necessary.
}

}  // namespace ash
