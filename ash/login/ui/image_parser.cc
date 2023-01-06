// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/image_parser.h"

#include <utility>

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/functional/bind.h"
#include "ipc/ipc_channel.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/decode_image.h"

namespace ash {
namespace {

const int64_t kMaxImageSizeInBytes =
    static_cast<int64_t>(IPC::Channel::kMaximumMessageSize);

void ConvertToAnimationFrame(
    OnDecoded callback,
    std::vector<data_decoder::mojom::AnimationFramePtr> mojo_frames) {
  AnimationFrames animation;
  for (auto& mojo_frame : mojo_frames) {
    AnimationFrame frame;
    frame.image = gfx::ImageSkia::CreateFrom1xBitmap(mojo_frame->bitmap);
    frame.duration = mojo_frame->duration;
    animation.push_back(frame);
  }
  std::move(callback).Run(std::move(animation));
}

}  // namespace

void DecodeAnimation(const std::vector<uint8_t>& image_data,
                     OnDecoded on_decoded) {
  // There are two reasons to decode the image in a sandboxed process:
  // - image_data may have come from the user, so it cannot be trusted.
  // - PNGCodec::Decode uses libpng which does not support APNG. blink::WebImage
  // also goes through libpng, but APNG support is handled specifically by
  // blink's PNGImageReader.cpp.
  data_decoder::DecodeAnimationIsolated(
      image_data, true /*shrink_to_fit*/, kMaxImageSizeInBytes,
      base::BindOnce(&ConvertToAnimationFrame, std::move(on_decoded)));
}

}  // namespace ash
