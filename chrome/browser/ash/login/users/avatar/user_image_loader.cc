// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/avatar/user_image_loader.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/login/helper.h"
#include "components/user_manager/user_image/user_image.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skbitmap_operations.h"

namespace ash {
namespace user_image_loader {
namespace {

// Contains attributes we need to know about each image we decode.
struct ImageInfo {
  ImageInfo(const base::FilePath& file_path,
            int pixels_per_side,
            ImageDecoder::ImageCodec image_codec,
            LoadedCallback loaded_cb)
      : file_path(file_path),
        pixels_per_side(pixels_per_side),
        image_codec(image_codec),
        loaded_cb(std::move(loaded_cb)) {}

  ImageInfo(ImageInfo&&) = default;
  ImageInfo& operator=(ImageInfo&&) = default;

  ~ImageInfo() {}

  base::FilePath file_path;
  int pixels_per_side;
  ImageDecoder::ImageCodec image_codec;
  LoadedCallback loaded_cb;
};

// Crops `image` to the square format and downsizes the image to
// `target_size` in pixels. On success, returns the bytes representation and
// stores the cropped image in `bitmap`, and the format of the bytes
// representation in `image_format`. On failure, returns nullptr, and
// the contents of `bitmap` and `image_format` are undefined.
scoped_refptr<base::RefCountedBytes> CropImage(
    const SkBitmap& image,
    int target_size,
    SkBitmap* bitmap,
    user_manager::UserImage::ImageFormat* image_format) {
  DCHECK_GT(target_size, 0);
  DCHECK(image_format);

  SkBitmap final_image;
  // Auto crop the image, taking the largest square in the center.
  int pixels_per_side = std::min(image.width(), image.height());
  int x = (image.width() - pixels_per_side) / 2;
  int y = (image.height() - pixels_per_side) / 2;
  SkBitmap cropped_image = SkBitmapOperations::CreateTiledBitmap(
      image, x, y, pixels_per_side, pixels_per_side);
  if (pixels_per_side > target_size) {
    // Also downsize the image to save space and memory.
    final_image = skia::ImageOperations::Resize(
        cropped_image, skia::ImageOperations::RESIZE_LANCZOS3, target_size,
        target_size);
  } else {
    final_image = cropped_image;
  }

  // Encode the cropped image to web-compatible bytes representation
  *image_format = user_manager::UserImage::ChooseImageFormat(final_image);
  scoped_refptr<base::RefCountedBytes> encoded =
      user_manager::UserImage::Encode(final_image, *image_format);
  if (encoded)
    bitmap->swap(final_image);
  return encoded;
}

// Returns the image format for the bytes representation of the user image
// from the image codec used for loading the image.
user_manager::UserImage::ImageFormat ChooseImageFormatFromCodec(
    ImageDecoder::ImageCodec image_codec) {
  switch (image_codec) {
    case ImageDecoder::PNG_CODEC:
      return user_manager::UserImage::FORMAT_PNG;
    case ImageDecoder::DEFAULT_CODEC:
      // The default codec can accept many kinds of image formats, hence the
      // image format of the bytes representation is unknown.
      return user_manager::UserImage::FORMAT_UNKNOWN;
  }
  NOTREACHED();
  return user_manager::UserImage::FORMAT_UNKNOWN;
}

// Handles the decoded image returned from ImageDecoder through the
// ImageRequest interface.
class UserImageRequest : public ImageDecoder::ImageRequest {
 public:
  UserImageRequest(
      ImageInfo image_info,
      const std::string& image_data,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner)
      : image_info_(std::move(image_info)),
        // TODO(crbug.com/593251): Remove the data copy here.
        image_data_(new base::RefCountedBytes(
            reinterpret_cast<const unsigned char*>(image_data.data()),
            image_data.size())),
        background_task_runner_(background_task_runner) {}
  ~UserImageRequest() override {}

  // ImageDecoder::ImageRequest implementation.
  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

  // Called after the image is cropped (and downsized) as needed.
  void OnImageCropped(SkBitmap* bitmap,
                      user_manager::UserImage::ImageFormat* image_format,
                      scoped_refptr<base::RefCountedBytes> bytes);

  // Called after the image is finalized. `image_bytes_regenerated` is true
  // if `image_bytes` is regenerated from the cropped image.
  void OnImageFinalized(const SkBitmap& image,
                        user_manager::UserImage::ImageFormat image_format,
                        scoped_refptr<base::RefCountedBytes> image_bytes,
                        bool image_bytes_regenerated);

 private:
  ImageInfo image_info_;
  scoped_refptr<base::RefCountedBytes> image_data_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // This should be the last member.
  base::WeakPtrFactory<UserImageRequest> weak_ptr_factory_{this};
};

void UserImageRequest::OnImageDecoded(const SkBitmap& decoded_image) {
  int target_size = image_info_.pixels_per_side;
  if (target_size > 0) {
    // Cropping an image could be expensive, hence posting to the background
    // thread.
    SkBitmap* bitmap = new SkBitmap;
    auto* image_format = new user_manager::UserImage::ImageFormat(
        user_manager::UserImage::FORMAT_UNKNOWN);
    base::PostTaskAndReplyWithResult(
        background_task_runner_.get(), FROM_HERE,
        base::BindOnce(&CropImage, decoded_image, target_size, bitmap,
                       image_format),
        base::BindOnce(&UserImageRequest::OnImageCropped,
                       weak_ptr_factory_.GetWeakPtr(), base::Owned(bitmap),
                       base::Owned(image_format)));
  } else {
    const user_manager::UserImage::ImageFormat image_format =
        ChooseImageFormatFromCodec(image_info_.image_codec);
    OnImageFinalized(decoded_image, image_format, image_data_,
                     false /* image_bytes_regenerated */);
  }
}

void UserImageRequest::OnImageCropped(
    SkBitmap* bitmap,
    user_manager::UserImage::ImageFormat* image_format,
    scoped_refptr<base::RefCountedBytes> bytes) {
  DCHECK_GT(image_info_.pixels_per_side, 0);

  if (!bytes) {
    OnDecodeImageFailed();
    return;
  }
  OnImageFinalized(*bitmap, *image_format, bytes,
                   true /* image_bytes_regenerated */);
}

void UserImageRequest::OnImageFinalized(
    const SkBitmap& image,
    user_manager::UserImage::ImageFormat image_format,
    scoped_refptr<base::RefCountedBytes> image_bytes,
    bool image_bytes_regenerated) {
  SkBitmap final_image = image;
  // Make the SkBitmap immutable as we won't modify it. This is important
  // because otherwise it gets duplicated during painting, wasting memory.
  final_image.setImmutable();
  gfx::ImageSkia final_image_skia =
      gfx::ImageSkia::CreateFrom1xBitmap(final_image);
  final_image_skia.MakeThreadSafe();
  std::unique_ptr<user_manager::UserImage> user_image(
      new user_manager::UserImage(final_image_skia, image_bytes, image_format));
  user_image->set_file_path(image_info_.file_path);
  // The user image is safe if it is decoded using one of the robust image
  // decoders, or regenerated by Chrome's image encoder.
  if (image_info_.image_codec == ImageDecoder::PNG_CODEC ||
      image_bytes_regenerated)
    user_image->MarkAsSafe();
  std::move(image_info_.loaded_cb).Run(std::move(user_image));
  delete this;
}

void UserImageRequest::OnDecodeImageFailed() {
  std::move(image_info_.loaded_cb)
      .Run(base::WrapUnique(new user_manager::UserImage));
  delete this;
}

// Starts decoding the image with ImageDecoder for the image `data` if
// `data_is_ready` is true.
void DecodeImage(
    ImageInfo image_info,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const std::string* data,
    bool data_is_ready) {
  if (!data_is_ready) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(image_info.loaded_cb),
                       base::WrapUnique(new user_manager::UserImage)));
    return;
  }

  ImageDecoder::ImageCodec codec = image_info.image_codec;
  UserImageRequest* image_request = new UserImageRequest(
      std::move(image_info), *data, background_task_runner);
  ImageDecoder::StartWithOptions(image_request, *data, codec, false);
}

}  // namespace

void StartWithFilePath(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& file_path,
    ImageDecoder::ImageCodec image_codec,
    int pixels_per_side,
    LoadedCallback loaded_cb) {
  std::string* data = new std::string;
  base::PostTaskAndReplyWithResult(
      background_task_runner.get(), FROM_HERE,
      base::BindOnce(&base::ReadFileToString, file_path, data),
      base::BindOnce(&DecodeImage,
                     ImageInfo(file_path, pixels_per_side, image_codec,
                               std::move(loaded_cb)),
                     background_task_runner, base::Owned(data)));
}

void StartWithData(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    std::unique_ptr<std::string> data,
    ImageDecoder::ImageCodec image_codec,
    int pixels_per_side,
    LoadedCallback loaded_cb) {
  DecodeImage(ImageInfo(base::FilePath(), pixels_per_side, image_codec,
                        std::move(loaded_cb)),
              background_task_runner, data.get(), true /* data_is_ready */);
}

}  // namespace user_image_loader
}  // namespace ash
