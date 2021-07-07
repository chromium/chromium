// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/ui/fast_pair/fast_pair_image_decoder.h"

#include "ash/quick_pair/common/logging.h"
#include "base/bind.h"
#include "base/callback.h"
#include "ipc/ipc_channel.h"
#include "services/data_decoder/public/cpp/decode_image.h"

namespace {

const int64_t kMaxImageSizeInBytes =
    static_cast<int64_t>(IPC::Channel::kMaximumMessageSize);

void ToImage(DecodeImageCallback on_image_decoded, const SkBitmap& bitmap) {
  if (bitmap.empty()) {
    QP_LOG(WARNING) << "Failed to decode image";
    std::move(on_image_decoded).Run(gfx::Image());
    return;
  }
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(bitmap);
  std::move(on_image_decoded).Run(image);
}

}  // namespace

namespace ash {
namespace quick_pair {

FastPairImageDecoder::FastPairImageDecoder() = default;

FastPairImageDecoder::~FastPairImageDecoder() = default;

void FastPairImageDecoder::DecodeImage(
    const std::vector<uint8_t>& encoded_image_bytes,
    DecodeImageCallback on_image_decoded) {
  data_decoder::DecodeImageIsolated(
      encoded_image_bytes, data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/false, kMaxImageSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(),
      base::BindOnce(&ToImage, std::move(on_image_decoded)));
}

}  // namespace quick_pair
}  // namespace ash
