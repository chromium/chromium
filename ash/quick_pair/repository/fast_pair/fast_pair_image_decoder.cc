// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder.h"

#include "ash/quick_pair/common/logging.h"
#include "base/bind.h"
#include "base/callback.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

constexpr char kImageFetcherUmaClientName[] = "FastPair";

// Needs to stay in sync with |kLargeImageMaxHeight| declared in
// ui/message_center/views/notification_view_md.cc.
const int kMaxNotificationHeight = 218;

// TODO(crbug.com/1226117) Update policy from Nearby to Fast Pair.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("fast_pair", R"(
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
            "This feature is only enabled for signed-in users who enable "
            "Nearby Share"
          chrome_policy {
            BrowserSignin {
              policy_options {mode: MANDATORY}
              BrowserSignin: 0
            }
          }
        })");

int CalculateScaledWidth(int width, int height) {
  return (kMaxNotificationHeight * width) / height;
}

void ToImage(DecodeImageCallback on_image_decoded_callback,
             const SkBitmap& bitmap) {
  if (bitmap.empty()) {
    QP_LOG(WARNING) << "Failed to decode image";
    std::move(on_image_decoded_callback).Run(gfx::Image());
    return;
  }
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);

  if (image.height() > kMaxNotificationHeight) {
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

FastPairImageDecoder::FastPairImageDecoder(
    std::unique_ptr<image_fetcher::ImageFetcher> fetcher)
    : fetcher_(std::move(fetcher)) {}

FastPairImageDecoder::~FastPairImageDecoder() = default;

void FastPairImageDecoder::DecodeImage(
    const GURL& image_url,
    DecodeImageCallback on_image_decoded_callback) {
  fetcher_->FetchImageData(
      image_url,
      base::BindOnce(&FastPairImageDecoder::OnImageDataFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_image_decoded_callback)),
      image_fetcher::ImageFetcherParams(kTrafficAnnotation,
                                        kImageFetcherUmaClientName));
}

void FastPairImageDecoder::DecodeImage(
    const std::vector<uint8_t>& encoded_image_bytes,
    DecodeImageCallback on_image_decoded_callback) {
  data_decoder::DecodeImageIsolated(
      encoded_image_bytes, data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/false, data_decoder::kDefaultMaxSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(),
      base::BindOnce(&ToImage, std::move(on_image_decoded_callback)));
}

void FastPairImageDecoder::OnImageDataFetched(
    DecodeImageCallback on_image_decoded_callback,
    const std::string& image_data,
    const image_fetcher::RequestMetadata& request_metadata) {
  DecodeImage(std::vector<uint8_t>(image_data.begin(), image_data.end()),
              std::move(on_image_decoded_callback));
}

}  // namespace quick_pair
}  // namespace ash
