// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/avatar/user_image_loader.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/public/cpp/image_downloader.h"
#include "base/check_op.h"
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
#include "ipc/constants.mojom.h"
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
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "url/gurl.h"

namespace ash {
namespace user_image_loader {
namespace {

constexpr const char kURLLoaderDownloadSuccessHistogramName[] =
    "Ash.UserImage.URLLoaderDownloadSuccess";
constexpr int64_t kMaxImageSizeInBytes =
    static_cast<int64_t>(IPC::mojom::kChannelMaximumMessageSize);

// Contains attributes we need to know about each image we decode.
struct ImageInfo {
  ImageInfo(const base::FilePath& file_path,
            user_manager::UserImage::ImageFormat format)
      : file_path(file_path), format(format) {}

  ImageInfo(ImageInfo&&) = default;
  ImageInfo& operator=(ImageInfo&&) = default;

  ~ImageInfo() = default;

  base::FilePath file_path;
  user_manager::UserImage::ImageFormat format;
};

data_decoder::mojom::ImageCodec GetCodecFromImageFormat(
    user_manager::UserImage::ImageFormat format) {
  switch (format) {
    case user_manager::UserImage::ImageFormat::FORMAT_PNG:
      return data_decoder::mojom::ImageCodec::kPng;
    case user_manager::UserImage::ImageFormat::FORMAT_UNKNOWN:
      return data_decoder::mojom::ImageCodec::kDefault;
    default:
      NOTREACHED();
  }
}

struct CropAndShrinkResult {
  CropAndShrinkResult() = default;
  CropAndShrinkResult(CropAndShrinkResult&&) = default;
  CropAndShrinkResult& operator=(CropAndShrinkResult&&) = default;
  ~CropAndShrinkResult() = default;

  bool success() const { return encoded_bytes.get(); }

  SkBitmap bitmap;
  scoped_refptr<base::RefCountedBytes> encoded_bytes;
  user_manager::UserImage::ImageFormat format =
      user_manager::UserImage::ImageFormat::FORMAT_UNKNOWN;
};

// Crops `image` to the square format and downsizes the image to
// `target_size` in pixels. On success, returns the bytes representation and
// stores the cropped image in `bitmap`, and the format of the bytes
// representation in `image_format`. On failure, returns nullptr, and
// the contents of `bitmap` and `image_format` are undefined.
CropAndShrinkResult CropAndShrinkImage(const SkBitmap& input_image,
                                       int target_size) {
  CHECK_GT(target_size, 0);

  // Auto crop the image, taking the largest square in the center.
  int pixels_per_side = std::min(input_image.width(), input_image.height());
  int x = (input_image.width() - pixels_per_side) / 2;
  int y = (input_image.height() - pixels_per_side) / 2;
  SkBitmap cropped_image = SkBitmapOperations::CreateTiledBitmap(
      input_image, x, y, pixels_per_side, pixels_per_side);

  CropAndShrinkResult result;

  if (pixels_per_side > target_size) {
    // Also downsize the image to save space and memory.
    result.bitmap = skia::ImageOperations::Resize(
        cropped_image, skia::ImageOperations::RESIZE_LANCZOS3, target_size,
        target_size);
  } else {
    result.bitmap = cropped_image;
  }

  // Encode the cropped image to web-compatible bytes representation
  result.format = user_manager::UserImage::ChooseImageFormat(result.bitmap);
  result.encoded_bytes =
      user_manager::UserImage::Encode(result.bitmap, result.format);

  return result;
}

// Handles asynchronous tasks for the decoding.
class UserImageRequest {
 public:
  // `background_task_runner` is used for `CropAndShrinkImage`.
  UserImageRequest(
      ImageInfo image_info,
      base::span<const uint8_t> original_image_data,
      std::optional<int> target_size,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  void Start(LoadedCallback callback);

 private:
  ~UserImageRequest() = default;

  void OnImageDecoded(LoadedCallback callback, const SkBitmap& decoded_image);

  // Called after the image is cropped (and downsized) as needed.
  void OnImageCropped(LoadedCallback callback, CropAndShrinkResult result);

  // Called after the image is finalized. `encoded_bytes_regenerated` is true
  // if `image_bytes` is regenerated from the cropped image.
  void OnImageFinalized(LoadedCallback callback,
                        const SkBitmap& image,
                        user_manager::UserImage::ImageFormat image_format,
                        scoped_refptr<base::RefCountedBytes> encoded_bytes,
                        bool encoded_bytes_regenerated);

  void OnDecodeImageFailed(LoadedCallback callback);

  void ReplyAndDeleteThis(LoadedCallback callback,
                          std::unique_ptr<user_manager::UserImage> user_image);

  ImageInfo image_info_;
  std::optional<int> target_size_;
  scoped_refptr<base::RefCountedBytes> original_image_data_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
};

UserImageRequest::UserImageRequest(
    ImageInfo image_info,
    base::span<const uint8_t> original_image_data,
    std::optional<int> target_size,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : image_info_(std::move(image_info)),
      target_size_(target_size),
      original_image_data_(
          base::MakeRefCounted<base::RefCountedBytes>(original_image_data)),
      background_task_runner_(background_task_runner) {}

void UserImageRequest::Start(LoadedCallback callback) {
  data_decoder::DecodeImageIsolated(
      *original_image_data_.get(), GetCodecFromImageFormat(image_info_.format),
      /*shrink_to_fit=*/false, kMaxImageSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(),
      base::BindOnce(&UserImageRequest::OnImageDecoded, base::Unretained(this),
                     std::move(callback)));
}

void UserImageRequest::OnImageDecoded(LoadedCallback callback,
                                      const SkBitmap& decoded_image) {
  if (decoded_image.isNull()) {
    OnDecodeImageFailed(std::move(callback));
    return;
  }

  if (target_size_) {
    CHECK_GT(*target_size_, 0);

    // Cropping an image could be expensive, hence posting to the background
    // thread.
    background_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&CropAndShrinkImage, decoded_image, *target_size_),
        base::BindOnce(&UserImageRequest::OnImageCropped,
                       base::Unretained(this), std::move(callback)));
  } else {
    OnImageFinalized(std::move(callback), decoded_image, image_info_.format,
                     original_image_data_,
                     /*encoded_bytes_regenerated=*/false);
  }
}

void UserImageRequest::OnImageCropped(LoadedCallback callback,
                                      CropAndShrinkResult result) {
  if (!result.success()) {
    OnDecodeImageFailed(std::move(callback));
    return;
  }

  OnImageFinalized(std::move(callback), result.bitmap, result.format,
                   result.encoded_bytes,
                   /*encoded_bytes_regenerated=*/true);
}

void UserImageRequest::OnImageFinalized(
    LoadedCallback callback,
    const SkBitmap& image,
    user_manager::UserImage::ImageFormat image_format,
    scoped_refptr<base::RefCountedBytes> encoded_bytes,
    bool encoded_bytes_regenerated) {
  SkBitmap final_image = image;
  // Make the SkBitmap immutable as we won't modify it. This is important
  // because otherwise it gets duplicated during painting, wasting memory.
  final_image.setImmutable();

  gfx::ImageSkia final_image_skia =
      gfx::ImageSkia::CreateFrom1xBitmap(final_image);
  final_image_skia.MakeThreadSafe();

  auto user_image = std::make_unique<user_manager::UserImage>(
      final_image_skia, encoded_bytes, image_format);
  user_image->set_file_path(image_info_.file_path);
  // The user image is safe if it is decoded using one of the robust image
  // decoders, or regenerated by Chrome's image encoder.
  if (image_info_.format == user_manager::UserImage::FORMAT_PNG ||
      encoded_bytes_regenerated) {
    user_image->MarkAsSafe();
  }

  ReplyAndDeleteThis(std::move(callback), std::move(user_image));
}

void UserImageRequest::OnDecodeImageFailed(LoadedCallback callback) {
  ReplyAndDeleteThis(std::move(callback),
                     std::make_unique<user_manager::UserImage>());
}

void UserImageRequest::ReplyAndDeleteThis(
    LoadedCallback callback,
    std::unique_ptr<user_manager::UserImage> user_image) {
  std::move(callback).Run(std::move(user_image));
  delete this;
}

// Starts decoding the image for `data` if `data_is_ready` is true.
void DecodeImage(
    ImageInfo image_info,
    std::optional<int> target_size,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    LoadedCallback callback,
    std::unique_ptr<std::string> data,
    bool data_is_ready) {
  if (!data_is_ready) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::make_unique<user_manager::UserImage>()));
    return;
  }

  // UserImageRequest manages the lifetime by itself.
  UserImageRequest* request =
      new UserImageRequest(std::move(image_info), base::as_byte_span(*data),
                           target_size, std::move(background_task_runner));
  request->Start(std::move(callback));
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
              std::optional<std::vector<uint8_t>> encoded =
                  gfx::PNGCodec::EncodeBGRASkBitmap(
                      bitmap, /*discard_transparency=*/false);
              if (!encoded) {
                return std::make_unique<user_manager::UserImage>();
              }

              auto image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
              image_skia.MakeThreadSafe();

              auto user_image = std::make_unique<user_manager::UserImage>(
                  image_skia,
                  base::MakeRefCounted<base::RefCountedBytes>(
                      std::move(encoded).value()),
                  user_manager::UserImage::FORMAT_PNG);
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
                     std::string data) {
  if (data.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(loaded_cb),
                                  std::make_unique<user_manager::UserImage>()));
    return;
  }

  // A pointer to `std::string` may be invalidated after a move operation, so
  // it's necessary for pointer stability to put `data` into `data_container`.
  auto data_container = std::make_unique<std::string>(std::move(data));
  auto bytes = base::as_byte_span(*data_container);
  data_decoder::DecodeAnimationIsolated(
      bytes, /*shrink_to_fit=*/true, kMaxImageSizeInBytes,
      base::BindOnce(&OnAnimationDecoded, std::move(loaded_cb), require_encode,
                     std::move(data_container)));
}

void OnImageDownloaded(std::unique_ptr<network::SimpleURLLoader> loader,
                       LoadedCallback loaded_cb,
                       std::optional<std::string> body) {
  if (loader->NetError() != net::OK || !body) {
    base::UmaHistogramBoolean(kURLLoaderDownloadSuccessHistogramName, false);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(loaded_cb),
                                  std::make_unique<user_manager::UserImage>()));
    return;
  }
  base::UmaHistogramBoolean(kURLLoaderDownloadSuccessHistogramName, true);
  DecodeAnimation(std::move(loaded_cb), /*require_encode=*/true,
                  std::move(body).value());
}

}  // namespace

void StartWithFilePath(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& file_path,
    user_manager::UserImage::ImageFormat image_format,
    int pixels_per_side,
    LoadedCallback loaded_cb) {
  auto target_size = pixels_per_side > 0
                         ? std::make_optional<int>(pixels_per_side)
                         : std::nullopt;

  auto data = std::make_unique<std::string>();
  auto* data_ptr = data.get();
  background_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::ReadFileToString, file_path, data_ptr),
      base::BindOnce(&DecodeImage, ImageInfo(file_path, image_format),
                     target_size, background_task_runner, std::move(loaded_cb),
                     std::move(data)));
}

void StartWithData(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    std::unique_ptr<std::string> data,
    user_manager::UserImage::ImageFormat image_format,
    int pixels_per_side,
    LoadedCallback loaded_cb) {
  auto target_size = pixels_per_side > 0
                         ? std::make_optional<int>(pixels_per_side)
                         : std::nullopt;

  DecodeImage(ImageInfo(base::FilePath(), image_format), target_size,
              background_task_runner, std::move(loaded_cb), std::move(data),
              /*data_is_ready=*/true);
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
            return data;
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
