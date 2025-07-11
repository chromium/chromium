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
#include "ipc/constants.mojom.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/decode_image.h"
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

IconDecodeRequest::IconDecodeRequest(int dimension_dip)
    : dimension_dip_(dimension_dip) {}

IconDecodeRequest::~IconDecodeRequest() = default;

void IconDecodeRequest::Start(const std::vector<uint8_t>& image_data,
                              SetIconCallback set_icon_callback) {
  TRACE_EVENT0("ui", "IconDecodeRequest::Start");

  if (disable_safe_decoding_for_testing) {
    if (image_data.empty()) {
      OnImageDecoded(std::move(set_icon_callback), SkBitmap());
      return;
    }
    OnImageDecoded(std::move(set_icon_callback),
                   gfx::PNGCodec::Decode(image_data));
    return;
  }

  data_decoder::DecodeImage(
      &GetDataDecoder(), base::as_byte_span(image_data),
      data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/true,
      static_cast<int64_t>(IPC::mojom::kChannelMaximumMessageSize),
      /*desired_image_frame_size=*/gfx::Size(),
      base::BindOnce(&IconDecodeRequest::OnImageDecoded,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(set_icon_callback)));
}

void IconDecodeRequest::OnImageDecoded(SetIconCallback set_icon_callback,
                                       const SkBitmap& bitmap) {
  TRACE_EVENT0("ui", "IconDecodeRequest::OnImageDecoded");
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (bitmap.isNull()) {
    DLOG(ERROR) << "Failed to decode an icon image.";
    // NOTE: Proceed with the null bitmap.
  }

  const gfx::ImageSkia icon(
      std::make_unique<IconSource>(bitmap, dimension_dip_),
      gfx::Size(dimension_dip_, dimension_dip_));
  icon.EnsureRepsForSupportedScales();

  std::move(set_icon_callback).Run(icon);
}

}  // namespace arc
