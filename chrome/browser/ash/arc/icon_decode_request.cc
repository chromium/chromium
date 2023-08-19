// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/icon_decode_request.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "chrome/grit/component_extension_resources.h"
#include "content/public/browser/browser_thread.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"

using content::BrowserThread;

namespace arc {

namespace {

bool disable_safe_decoding_for_testing = false;

class IconSource : public gfx::ImageSkiaSource {
 public:
  IconSource(const SkBitmap& bitmap, int dimension_dip);

  IconSource(const IconSource&) = delete;
  IconSource& operator=(const IconSource&) = delete;

  ~IconSource() override = default;

 private:
  gfx::ImageSkiaRep GetImageForScale(float scale) override;

  const SkBitmap bitmap_;
  const int dimension_dip_;
};

IconSource::IconSource(const SkBitmap& bitmap, int dimension_dip)
    : bitmap_(bitmap), dimension_dip_(dimension_dip) {}

gfx::ImageSkiaRep IconSource::GetImageForScale(float scale) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const int dimension_px = static_cast<int>(dimension_dip_ * scale + 0.5);
  if (bitmap_.isNull()) {
    const int resource_id = dimension_px <= 32 ? IDR_ARC_SUPPORT_ICON_32_PNG
                                               : IDR_ARC_SUPPORT_ICON_192_PNG;
    const gfx::ImageSkia* resource_image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
    const gfx::ImageSkia resized_image =
        gfx::ImageSkiaOperations::CreateResizedImage(
            *resource_image, skia::ImageOperations::RESIZE_LANCZOS3,
            gfx::Size(dimension_dip_, dimension_dip_));
    return resized_image.GetRepresentation(scale);
  }

  SkBitmap resized_bitmap = skia::ImageOperations::Resize(
      bitmap_, skia::ImageOperations::RESIZE_LANCZOS3, dimension_px,
      dimension_px);
  return gfx::ImageSkiaRep(resized_bitmap, scale);
}

data_decoder::DataDecoder& GetDataDecoder() {
  static base::NoDestructor<data_decoder::DataDecoder> data_decoder;
  return *data_decoder;
}

}  // namespace

// static
void IconDecodeRequest::DisableSafeDecodingForTesting() {
  disable_safe_decoding_for_testing = true;
}

IconDecodeRequest::IconDecodeRequest(SetIconCallback set_icon_callback,
                                     int dimension_dip)
    : ImageRequest(&GetDataDecoder()),
      set_icon_callback_(std::move(set_icon_callback)),
      dimension_dip_(dimension_dip) {}

IconDecodeRequest::~IconDecodeRequest() = default;

void IconDecodeRequest::StartWithOptions(
    const std::vector<uint8_t>& image_data) {
  TRACE_EVENT0("ui", "IconDecodeRequest::StartWithOptions");
  if (disable_safe_decoding_for_testing) {
    if (image_data.empty()) {
      OnDecodeImageFailed();
      return;
    }
    SkBitmap bitmap;
    if (!gfx::PNGCodec::Decode(
            reinterpret_cast<const unsigned char*>(image_data.data()),
            image_data.size(), &bitmap)) {
      OnDecodeImageFailed();
      return;
    }
    OnImageDecoded(bitmap);
    return;
  }
  ImageDecoder::StartWithOptions(this, image_data, ImageDecoder::DEFAULT_CODEC,
                                 true, gfx::Size());
}

void IconDecodeRequest::OnImageDecoded(const SkBitmap& bitmap) {
  TRACE_EVENT0("ui", "IconDecodeRequest::OnImageDecoded");
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const gfx::ImageSkia icon(
      std::make_unique<IconSource>(bitmap, dimension_dip_),
      gfx::Size(dimension_dip_, dimension_dip_));
  icon.EnsureRepsForSupportedScales();
  std::move(set_icon_callback_).Run(icon);
}

void IconDecodeRequest::OnDecodeImageFailed() {
  TRACE_EVENT0("ui", "IconDecodeRequest::OnDecodeImageFailed");
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DLOG(ERROR) << "Failed to decode an icon image.";
  OnImageDecoded(SkBitmap());
}

}  // namespace arc
