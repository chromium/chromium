// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/avatar/user_image_loader.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/image_downloader.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/image_downloader/image_downloader_impl.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/browser_process.h"
#include "components/user_manager/user_image/user_image.h"
#include "google_apis/credentials_mode.h"
#include "ipc/ipc_channel.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/encode/SkWebpEncoder.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/skbitmap_operations.h"
#include "url/gurl.h"

namespace ash {
namespace user_image_loader {
namespace {

constexpr const char kURLLoaderDownloadSuccessHistogramName[] =
    "Ash.UserImage.URLLoaderDownloadSuccess";
constexpr int64_t kMaxImageSizeInBytes =
    static_cast<int64_t>(IPC::Channel::kMaximumMessageSize);

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
  if (encoded) {
    bitmap->swap(final_image);
  }
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
  NOTREACHED_IN_MIGRATION();
  return user_manager::UserImage::FORMAT_UNKNOWN;
}

// Handles the decoded image returned from ImageDecoder through the
// ImageRequest interface.
// This class is self-deleting.
class UserImageRequest : public ImageDecoder::ImageRequest {
 public:
  UserImageRequest(
      ImageInfo image_info,
      const std::string& image_data,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner)
      : image_info_(std::move(image_info)),
        image_data_(base::MakeRefCounted<base::RefCountedBytes>(
            base::as_byte_span(image_data))),
        background_task_runner_(background_task_runner) {}

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
  ~UserImageRequest() override = default;

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
    background_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
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
      image_bytes_regenerated) {
    user_image->MarkAsSafe();
  }
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
    std::unique_ptr<std::string> data,
    bool data_is_ready) {
  if (!data_is_ready) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(image_info.loaded_cb),
                       base::WrapUnique(new user_manager::UserImage)));
    return;
  }

  ImageDecoder::ImageCodec codec = image_info.image_codec;
  // UserImageRequest is self-deleting.
  auto* image_request = new UserImageRequest(std::move(image_info), *data,
                                             std::move(background_task_runner));
  ImageDecoder::StartWithOptions(image_request, std::move(*data), codec,
                                 /*shrink_to_fit=*/false);
}

void OnAnimationDecoded(
    LoadedCallback loaded_cb,
    bool require_encode,
    std::unique_ptr<std::string> maybe_safe_encoded_data,
    std::vector<data_decoder::mojom::AnimationFramePtr> mojo_frames) {
  auto frame_size = mojo_frames.size();
  if (!frame_size) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(loaded_cb),
                                  std::make_unique<user_manager::UserImage>()));
    return;
  }

  // If `require_encode` is false, do not encode again, return with
  // `maybe_safe_encoded_data`.
  if (!require_encode) {
    auto image_skia =
        gfx::ImageSkia::CreateFrom1xBitmap(mojo_frames[0]->bitmap);
    image_skia.MakeThreadSafe();

    auto bytes = base::MakeRefCounted<base::RefCountedBytes>(
        base::as_byte_span(*maybe_safe_encoded_data));
    auto user_image = std::make_unique<user_manager::UserImage>(
        image_skia, bytes,
        frame_size == 1 ? user_manager::UserImage::FORMAT_PNG
                        : user_manager::UserImage::FORMAT_WEBP);
    user_image->MarkAsSafe();

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(loaded_cb), std::move(user_image)));
    return;
  }

  // Re-encode static image as PNG and send to requester.
  if (frame_size == 1) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            [](const SkBitmap& bitmap) {
              auto encoded = base::MakeRefCounted<base::RefCountedBytes>();
              if (!gfx::PNGCodec::EncodeBGRASkBitmap(
                      bitmap, /*discard_transparency=*/false,
                      &encoded->as_vector())) {
                return std::make_unique<user_manager::UserImage>();
              }

              auto image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
              image_skia.MakeThreadSafe();

              auto user_image = std::make_unique<user_manager::UserImage>(
                  image_skia, encoded, user_manager::UserImage::FORMAT_PNG);
              user_image->MarkAsSafe();

              return user_image;
            },
            mojo_frames[0]->bitmap),
        std::move(loaded_cb));
    return;
  }

  // The image is animated, re-encode as WebP animated image and send to
  // requester.
  std::vector<gfx::WebpCodec::Frame> frames;
  for (auto& mojo_frame : mojo_frames) {
    gfx::WebpCodec::Frame frame;
    frame.bitmap = mojo_frame->bitmap;
    frame.duration = mojo_frame->duration.InMilliseconds();
    frames.push_back(frame);
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const std::vector<gfx::WebpCodec::Frame>& frames) {
            SkWebpEncoder::Options options;
            options.fCompression = SkWebpEncoder::Compression::kLossless;
            // Lower quality under kLossless compression means compress faster
            // into larger files.
            options.fQuality = 0;

            auto encoded = gfx::WebpCodec::EncodeAnimated(frames, options);
            if (!encoded.has_value()) {
              return std::make_unique<user_manager::UserImage>();
            }

            auto image_skia =
                gfx::ImageSkia::CreateFrom1xBitmap(frames[0].bitmap);
            image_skia.MakeThreadSafe();

            auto bytes =
                base::MakeRefCounted<base::RefCountedBytes>(encoded.value());

            auto user_image = std::make_unique<user_manager::UserImage>(
                image_skia, bytes, user_manager::UserImage::FORMAT_WEBP);
            user_image->MarkAsSafe();

            return user_image;
          },
          std::move(frames)),
      std::move(loaded_cb));
}

void DecodeAnimation(LoadedCallback loaded_cb,
                     bool require_encode,
                     std::unique_ptr<std::string> data) {
  if (!data || data->empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(loaded_cb),
                                  std::make_unique<user_manager::UserImage>()));
    return;
  }

  auto bytes = base::as_byte_span(*data);
  data_decoder::DecodeAnimationIsolated(
      bytes, /*shrink_to_fit=*/true, kMaxImageSizeInBytes,
      base::BindOnce(&OnAnimationDecoded, std::move(loaded_cb), require_encode,
                     std::move(data)));
}

void OnImageDownloaded(std::unique_ptr<network::SimpleURLLoader> loader,
                       LoadedCallback loaded_cb,
                       std::unique_ptr<std::string> body) {
  if (loader->NetError() != net::OK || !body) {
    base::UmaHistogramBoolean(kURLLoaderDownloadSuccessHistogramName, false);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(loaded_cb),
                                  std::make_unique<user_manager::UserImage>()));
    return;
  }
  base::UmaHistogramBoolean(kURLLoaderDownloadSuccessHistogramName, true);
  DecodeAnimation(std::move(loaded_cb), /*require_encode=*/true,
                  std::move(body));
}

}  // namespace

void StartWithFilePath(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& file_path,
    ImageDecoder::ImageCodec image_codec,
    int pixels_per_side,
    LoadedCallback loaded_cb) {
  auto data = std::make_unique<std::string>();
  auto* data_ptr = data.get();
  background_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::ReadFileToString, file_path, data_ptr),
      base::BindOnce(&DecodeImage,
                     ImageInfo(file_path, pixels_per_side, image_codec,
                               std::move(loaded_cb)),
                     background_task_runner, std::move(data)));
}

void StartWithData(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    std::unique_ptr<std::string> data,
    ImageDecoder::ImageCodec image_codec,
    int pixels_per_side,
    LoadedCallback loaded_cb) {
  DecodeImage(ImageInfo(base::FilePath(), pixels_per_side, image_codec,
                        std::move(loaded_cb)),
              background_task_runner, std::move(data), /*data_is_ready=*/true);
}

void StartWithFilePathAnimated(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& file_path,
    LoadedCallback loaded_cb) {
  background_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& file_path) {
            std::string data;
            if (!base::ReadFileToString(file_path, &data)) {
              data.clear();
            }
            return std::make_unique<std::string>(std::move(data));
          },
          file_path),
      base::BindOnce(&DecodeAnimation, std::move(loaded_cb),
                     /*require_encode=*/false));
}

// Used to load user images from GURL, specifically in the case of
// retrieving images from the cloud.
void StartWithGURLAnimated(const GURL& default_image_url,
                           LoadedCallback loaded_cb) {
  constexpr net::NetworkTrafficAnnotationTag kNetworkTrafficAnnotationTag =
      net::DefineNetworkTrafficAnnotation("user_image_downloader", R"(
            semantics: {
              sender: "User Image Downloader"
              description:
                "Google default user images are loaded from cloud avatar image "
                "resources. Images are downloaded on an as-needed basis."
              trigger:
                "Triggered when the user opens the avatar image picker or "
                "when the current default user image needs to be cached."
              data: "The URL for which to retrieve a user image."
              destination: GOOGLE_OWNED_SERVICE
            }
            policy: {
              cookies_allowed: NO
              setting:
                "This request cannot be disabled, but it is only triggered "
                "by user action."
              policy_exception_justification: "Not implemented."
            })");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = default_image_url;
  request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();

  auto loader = network::SimpleURLLoader::Create(std::move(request),
                                                 kNetworkTrafficAnnotationTag);
  loader->SetRetryOptions(
      /*max_retries=*/5,
      network::SimpleURLLoader::RetryMode::RETRY_ON_5XX |
          network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
          network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED);

  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      g_browser_process->shared_url_loader_factory().get(),
      base::BindOnce(&OnImageDownloaded, std::move(loader),
                     std::move(loaded_cb)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

}  // namespace user_image_loader
}  // namespace ash
