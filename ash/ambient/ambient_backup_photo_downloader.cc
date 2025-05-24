// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_backup_photo_downloader.h"

#include <utility>
#include <vector>

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/ambient_photo_cache.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/public/cpp/image_util.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/files/file_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_util.h"

namespace ash {

namespace {

// TODO(b/304527743): Ask the server to resize the photo for us if the backend
// can make the appropriate config changes.
std::vector<unsigned char> ResizeAndEncode(const gfx::ImageSkia& image,
                                           gfx::Size target_size) {
  static constexpr int kJpegEncodingQuality = 95;
  // Only shrink images, and keep the original aspect ratio.
  gfx::Size original_size = image.size();
  if (target_size.width() < original_size.width() &&
      target_size.height() < original_size.height()) {
    float width_scale =
        static_cast<float>(target_size.width()) / original_size.width();
    float height_scale =
        static_cast<float>(target_size.height()) / original_size.height();
    target_size = gfx::ScaleToRoundedSize(original_size,
                                          std::max(width_scale, height_scale));
  } else {
    target_size = original_size;
  }

  gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BETTER, target_size);
  CHECK(!resized_image.isNull());

  std::optional<std::vector<uint8_t>> encoded_image =
      gfx::JPEG1xEncodedDataFromImage(gfx::Image(resized_image),
                                      kJpegEncodingQuality);
  return encoded_image.value_or(std::vector<uint8_t>());
}

}  // namespace

AmbientBackupPhotoDownloader::AmbientBackupPhotoDownloader(
    AmbientAccessTokenController& access_token_controller,
    int cache_idx,
    gfx::Size target_size,
    const std::string& url,
    base::OnceCallback<void(bool success)> completion_cb)
    : cache_idx_(cache_idx),
      target_size_(target_size),
      file_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      completion_cb_(std::move(completion_cb)) {
  // Since the photo is large, downloading to an in-memory string exceeds the
  // response size limits imposed by `SimpleUrLoader`. Downloading to a
  // temporary file is recommended instead.
  ambient_photo_cache::DownloadPhotoToTempFile(
      url, access_token_controller,
      base::BindOnce(&AmbientBackupPhotoDownloader::DecodeImage,
                     weak_factory_.GetWeakPtr()));
}

AmbientBackupPhotoDownloader::~AmbientBackupPhotoDownloader() {
  if (!temp_image_path_.empty()) {
    // The same `file_task_runner_` that reads/decodes the file is used to
    // delete the file. Prevents unpredictable behavior if the call to
    // `image_util::DecodeImageFile()` below is pending when this class is
    // destroyed.
    file_task_runner_->PostTask(FROM_HERE,
                                base::GetDeleteFileCallback(temp_image_path_));
  }
}

void AmbientBackupPhotoDownloader::RunCompletionCallback(bool success) {
  CHECK(completion_cb_);
  std::move(completion_cb_).Run(success);
}

void AmbientBackupPhotoDownloader::DecodeImage(base::FilePath temp_image_path) {
  if (temp_image_path.empty()) {
    RunCompletionCallback(false);
    return;
  }
  temp_image_path_ = std::move(temp_image_path);
  image_util::DecodeImageFile(
      base::BindOnce(&AmbientBackupPhotoDownloader::ScheduleResizeAndEncode,
                     weak_factory_.GetWeakPtr()),
      temp_image_path_, data_decoder::mojom::ImageCodec::kDefault,
      file_task_runner_);
}

void AmbientBackupPhotoDownloader::ScheduleResizeAndEncode(
    const gfx::ImageSkia& decoded_image) {
  if (decoded_image.isNull()) {
    RunCompletionCallback(false);
    return;
  }
  // `ResizeAndEncode()` can be a computationally expensive operation, so run it
  // on a blocking thread in the thread pool to prevent blocking the main
  // thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ResizeAndEncode, decoded_image, target_size_),
      base::BindOnce(&AmbientBackupPhotoDownloader::SaveImage,
                     weak_factory_.GetWeakPtr()));
}

void AmbientBackupPhotoDownloader::SaveImage(
    const std::vector<unsigned char>& encoded_image) {
  if (encoded_image.empty()) {
    RunCompletionCallback(false);
    return;
  }
  ::ambient::PhotoCacheEntry cache_entry;
  cache_entry.mutable_primary_photo()->mutable_image()->assign(
      reinterpret_cast<const char*>(encoded_image.data()),
      encoded_image.size());
  ambient_photo_cache::WritePhotoCache(
      ambient_photo_cache::Store::kBackup, cache_idx_, cache_entry,
      base::BindOnce(&AmbientBackupPhotoDownloader::RunCompletionCallback,
                     weak_factory_.GetWeakPtr(), true));
}

}  // namespace ash
