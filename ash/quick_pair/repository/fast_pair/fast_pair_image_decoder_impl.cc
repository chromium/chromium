// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder_impl.h"

#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/cross_device/logging/logging.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep_default.h"

namespace {

constexpr char kImageFetcherUmaClientName[] = "FastPair";

// Needs to stay in sync with |kLargeImageMaxHeight| declared in
// ui/message_center/views/notification_view_md.cc.
const int kMaxNotificationHeight = 218;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("fast_pair_image_decoder", R"(
        semantics {
          sender: "Get Fast Pair Device Image Data from Google"
          description:
            "Fast Pair can provide device images to be used in notifications "
            "for corresponding Fast Pair devices. For a given image url, "
            "Google's servers will return the image data in bytes to be "
            "futher decoded here."
          trigger: "A notification is being triggered for a Fast Pair device."
          data: "Image pixels and URLs. No user identifier is sent along with "
                "the data."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature by toggling on/off the "
            "Fast Pair toggle in chrome://os-settings under 'Bluetooth'. The "
            "feature is enabled by default. "
          chrome_policy {
            FastPairEnabled {
                FastPairEnabled: false
            }
          }
        })");

int CalculateScaledWidth(int width, int height) {
  return (kMaxNotificationHeight * width) / height;
}

void ToImage(DecodeImageCallback on_image_decoded_callback,
             bool resize_to_notification_size,
             const SkBitmap& bitmap) {
  if (bitmap.empty()) {
    CD_LOG(WARNING, Feature::FP)
        << "Call to DecodeImageIsolated returned null.";
    std::move(on_image_decoded_callback).Run(gfx::Image());
    return;
  }

  // Because the implicit resize when showing device images on display for the
  // notifications by using Skia's `DrawPicture` creates pixelated artifacts for
  // small images, we need to explicitly do the resize to avoid `DrawPicture`
  // doing the scaling. We do this by resizing `bitmap` to 5x to increase the
  // quality on the notification images.
  SkBitmap bitmap5x =
      skia::ImageOperations::Resize(bitmap, skia::ImageOperations::RESIZE_BEST,
                                    5 * bitmap.width(), 5 * bitmap.height());
  gfx::ImageSkia image = gfx::ImageSkia::CreateFromBitmap(bitmap5x, 5.0);

  if (resize_to_notification_size && image.height() > kMaxNotificationHeight) {
    image = gfx::ImageSkiaOperations::CreateResizedImage(
        image, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(CalculateScaledWidth(image.width(), image.height()),
                  kMaxNotificationHeight));
  }

  std::move(on_image_decoded_callback).Run(gfx::Image(image));
}

}  // namespace

namespace ash {
namespace quick_pair {

FastPairImageDecoderImpl::FastPairImageDecoderImpl() = default;

FastPairImageDecoderImpl::~FastPairImageDecoderImpl() = default;

void FastPairImageDecoderImpl::DecodeImageFromUrl(
    const GURL& image_url,
    bool resize_to_notification_size,
    DecodeImageCallback on_image_decoded_callback) {
  if (!fetcher_ && !LoadImageFetcher()) {
    CD_LOG(INFO, Feature::FP) << __func__ << " Could not load image fetcher. ";
    return;
  }

  fetcher_->FetchImageData(
      image_url,
      base::BindOnce(&FastPairImageDecoderImpl::OnImageDataFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_image_decoded_callback),
                     resize_to_notification_size),
      image_fetcher::ImageFetcherParams(kTrafficAnnotation,
                                        kImageFetcherUmaClientName));
}

void FastPairImageDecoderImpl::DecodeImage(
    const std::vector<uint8_t>& encoded_image_bytes,
    bool resize_to_notification_size,
    DecodeImageCallback on_image_decoded_callback) {
  data_decoder::DecodeImageIsolated(
      encoded_image_bytes, data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/false, data_decoder::kDefaultMaxSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(),
      base::BindOnce(&ToImage, std::move(on_image_decoded_callback),
                     resize_to_notification_size));
}

void FastPairImageDecoderImpl::OnImageDataFetched(
    DecodeImageCallback on_image_decoded_callback,
    bool resize_to_notification_size,
    const std::string& image_data,
    const image_fetcher::RequestMetadata& request_metadata) {
  DecodeImage(std::vector<uint8_t>(image_data.begin(), image_data.end()),
              resize_to_notification_size,
              std::move(on_image_decoded_callback));
}

bool FastPairImageDecoderImpl::LoadImageFetcher() {
  fetcher_ = QuickPairBrowserDelegate::Get()->GetImageFetcher();
  return !!fetcher_;
}

}  // namespace quick_pair
}  // namespace ash
