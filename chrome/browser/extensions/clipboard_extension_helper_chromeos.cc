// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/clipboard_extension_helper_chromeos.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/atomic_flag.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

using content::BrowserThread;

namespace extensions {

namespace clipboard = api::clipboard;

class ClipboardExtensionHelper::ClipboardImageDataDecoder
    : public ImageDecoder::ImageRequest {
 public:
  explicit ClipboardImageDataDecoder(ClipboardExtensionHelper* owner)
      : owner_(owner) {}

  ClipboardImageDataDecoder(const ClipboardImageDataDecoder&) = delete;
  ClipboardImageDataDecoder& operator=(const ClipboardImageDataDecoder&) =
      delete;

  ~ClipboardImageDataDecoder() override { ImageDecoder::Cancel(this); }

  bool has_request_pending() const { return has_request_pending_; }

  void Start(std::vector<uint8_t> image_data, clipboard::ImageType type) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    ImageDecoder::ImageCodec codec = ImageDecoder::DEFAULT_CODEC;
    switch (type) {
      case clipboard::ImageType::kPng:
        codec = ImageDecoder::PNG_CODEC;
        break;
      case clipboard::ImageType::kJpeg:
        codec = ImageDecoder::DEFAULT_CODEC;
        break;
      case clipboard::ImageType::kNone:
        NOTREACHED_IN_MIGRATION();
        break;
    }

    has_request_pending_ = true;
    ImageDecoder::StartWithOptions(this, std::move(image_data), codec, true);
  }

  void Cancel() {
    has_request_pending_ = false;
    ImageDecoder::Cancel(this);
    owner_->OnImageDecodeCancel();
  }

  void OnImageDecoded(const SkBitmap& decoded_image) override {
    has_request_pending_ = false;
    owner_->OnImageDecoded(decoded_image);
  }

  void OnDecodeImageFailed() override {
    has_request_pending_ = false;
    owner_->OnImageDecodeFailure();
  }

 private:
  raw_ptr<ClipboardExtensionHelper> owner_;  // Not owned.
  bool has_request_pending_ = false;
};

ClipboardExtensionHelper::ClipboardExtensionHelper() {
  clipboard_image_data_decoder_ =
      std::make_unique<ClipboardImageDataDecoder>(this);
}

ClipboardExtensionHelper::~ClipboardExtensionHelper() {}

void ClipboardExtensionHelper::DecodeAndSaveImageData(
    std::vector<uint8_t> data,
    clipboard::ImageType type,
    AdditionalDataItemList additional_items,
    base::OnceClosure success_callback,
    base::OnceCallback<void(const std::string&)> error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If there is a previous image decoding request still running, cancel it
  // first. We only need the most recent image save request be completed, since
  // the clipboard will only store data set by the most recent request, which
  // is consistent with the clipboard "paste" behavior.
  if (clipboard_image_data_decoder_->has_request_pending())
    clipboard_image_data_decoder_->Cancel();

  // Cache additonal items.
  additonal_items_ = std::move(additional_items);

  image_save_success_callback_ = std::move(success_callback);
  image_save_error_callback_ = std::move(error_callback);
  clipboard_image_data_decoder_->Start(std::move(data), type);
}

void ClipboardExtensionHelper::OnImageDecodeFailure() {
  std::move(image_save_error_callback_).Run("Image data decoding failed.");
}

void ClipboardExtensionHelper::OnImageDecoded(const SkBitmap& bitmap) {
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    // Write the decoded image data to clipboard.
    if (!bitmap.empty() && !bitmap.isNull())
      scw.WriteImage(bitmap);

    for (const clipboard::AdditionalDataItem& item : additonal_items_) {
      if (item.type == clipboard::DataItemType::kTextPlain) {
        scw.WriteText(base::UTF8ToUTF16(item.data));
      } else if (item.type == clipboard::DataItemType::kTextHtml) {
        scw.WriteHTML(base::UTF8ToUTF16(item.data), std::string());
      }
    }
  }
  std::move(image_save_success_callback_).Run();
}

void ClipboardExtensionHelper::OnImageDecodeCancel() {
  std::move(image_save_error_callback_).Run("Request canceled.");
}

}  // namespace extensions
