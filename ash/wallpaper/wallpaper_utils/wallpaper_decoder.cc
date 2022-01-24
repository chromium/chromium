// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_decoder.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "ipc/ipc_channel.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {

const int64_t kMaxImageSizeInBytes =
    static_cast<int64_t>(IPC::Channel::kMaximumMessageSize);

std::string ReadFileToString(const base::FilePath& path) {
  std::string result;
  if (!base::ReadFileToString(path, &result)) {
    LOG(WARNING) << "Failed reading file";
    result.clear();
  }

  return result;
}

void ToImageSkia(DecodeImageCallback callback, const SkBitmap& bitmap) {
  if (bitmap.empty()) {
    LOG(WARNING) << "Failed to decode image";
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  std::move(callback).Run(image);
}

}  // namespace

void DecodeImageFile(DecodeImageCallback callback,
                     const base::FilePath& file_path) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ReadFileToString, file_path),
      base::BindOnce(&DecodeImageData, std::move(callback)));
}

void DecodeImageData(DecodeImageCallback callback, const std::string& data) {
  if (data.empty()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }
  data_decoder::DecodeImageIsolated(
      base::as_bytes(base::make_span(data)),
      data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/true, kMaxImageSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(),
      base::BindOnce(&ToImageSkia, std::move(callback)));
}

}  // namespace ash
