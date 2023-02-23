// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_decoder.h"

#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace {

data_decoder::DataDecoder& GetDataDecoder() {
  static base::NoDestructor<data_decoder::DataDecoder> data_decoder;
  return *data_decoder;
}

}  // namespace

namespace apps {

bool g_decode_request_for_testing = false;

AppIconDecoder::ImageSource::ImageSource(int32_t size_in_dip)
    : size_in_dip_(size_in_dip) {}

AppIconDecoder::ImageSource::~ImageSource() = default;

gfx::ImageSkiaRep AppIconDecoder::ImageSource::GetImageForScale(float scale) {
  // Host loads icon asynchronously, so use default icon so far.

  // Get the ImageSkia for the resource IDR_APP_DEFAULT_ICON and the size
  // `size_in_dip_`.
  return CreateResizedResourceImage(IDR_APP_DEFAULT_ICON, size_in_dip_)
      .GetRepresentation(scale);
}

AppIconDecoder::DecodeRequest::DecodeRequest(
    ui::ResourceScaleFactor scale_factor,
    AppIconDecoder& host,
    gfx::ImageSkia& image_skia,
    std::set<ui::ResourceScaleFactor>& incomplete_scale_factors)
    : ImageRequest(&GetDataDecoder()),
      scale_factor_(scale_factor),
      host_(host),
      image_skia_(image_skia),
      incomplete_scale_factors_(incomplete_scale_factors) {}

AppIconDecoder::DecodeRequest::~DecodeRequest() {
  ImageDecoder::Cancel(this);
}

void AppIconDecoder::DecodeRequest::OnImageDecoded(const SkBitmap& bitmap) {
  DCHECK(!bitmap.isNull() && !bitmap.empty());
  host_.UpdateImageSkia(scale_factor_, bitmap, image_skia_,
                        incomplete_scale_factors_);
}

void AppIconDecoder::DecodeRequest::OnDecodeImageFailed() {
  host_.DiscardDecodeRequest();
}

AppIconDecoder::FakeDecodeRequestForTesting::FakeDecodeRequestForTesting(
    ui::ResourceScaleFactor scale_factor,
    AppIconDecoder& host,
    gfx::ImageSkia& image_skia,
    std::set<ui::ResourceScaleFactor>& incomplete_scale_factors)
    : scale_factor_(scale_factor),
      host_(host),
      image_skia_(image_skia),
      incomplete_scale_factors_(incomplete_scale_factors) {}

void AppIconDecoder::FakeDecodeRequestForTesting::Start(
    std::vector<uint8_t> icon_data) {
  CompressedDataToSkBitmap(
      std::move(icon_data),
      base::BindOnce(
          &AppIconDecoder::FakeDecodeRequestForTesting::DecodeRequestReply,
          weak_ptr_factory_.GetWeakPtr()));
}

AppIconDecoder::FakeDecodeRequestForTesting::~FakeDecodeRequestForTesting() =
    default;

void AppIconDecoder::FakeDecodeRequestForTesting::DecodeRequestReply(
    SkBitmap bitmap) {
  if (!bitmap.isNull()) {
    host_.UpdateImageSkia(scale_factor_, bitmap, image_skia_,
                          incomplete_scale_factors_);
  } else {
    host_.DiscardDecodeRequest();
  }
}

AppIconDecoder::AppIconDecoder(
    const base::FilePath& base_path,
    const std::string& app_id,
    int32_t size_in_dip,
    base::OnceCallback<void(AppIconDecoder* decoder, IconValuePtr iv)> callback)
    : base_path_(base_path),
      app_id_(app_id),
      size_in_dip_(size_in_dip),
      callback_(std::move(callback)) {}

AppIconDecoder::~AppIconDecoder() = default;

void AppIconDecoder::Start() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadIconFilesOnBackgroundThread, base_path_, app_id_,
                     size_in_dip_),
      base::BindOnce(&AppIconDecoder::OnIconRead,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool AppIconDecoder::SetScaleFactors(
    const std::map<ui::ResourceScaleFactor, IconValuePtr>& icon_datas) {
  for (const auto& [scale_factor, iv] : icon_datas) {
    if (!iv || iv->icon_type != IconType::kCompressed) {
      return false;
    }

    if (HasAdaptiveIconData(iv)) {
      is_adaptive_icon_ = true;
      foreground_incomplete_scale_factors_.insert(scale_factor);
      background_incomplete_scale_factors_.insert(scale_factor);
    } else if (iv->compressed.empty()) {
      return false;
    } else {
      incomplete_scale_factors_.insert(scale_factor);
    }
  }

  if (is_adaptive_icon_ && !incomplete_scale_factors_.empty()) {
    // Some scales have non-adaptive icons. Then we can't generate the adaptive
    // icon for all scales. Set `is_adaptive_icon_` as false, and decode the
    // foreground images only for scales with adaptive icon data.
    is_adaptive_icon_ = false;
    for (const auto& [scale_factor, iv] : icon_datas) {
      incomplete_scale_factors_.insert(scale_factor);
    }
  }

  // Initialize the ImageSkia with placeholder bitmaps, and the correct icon
  // size to generate the adaptive icon using CompositeImagesAndApplyMask, which
  // checks the ImageSkia's size to chop for paddings and resize the image_reps.
  gfx::Size image_size(size_in_dip_, size_in_dip_);
  if (is_adaptive_icon_) {
    foreground_image_skia_ =
        gfx::ImageSkia(std::make_unique<ImageSource>(size_in_dip_), image_size);
    background_image_skia_ =
        gfx::ImageSkia(std::make_unique<ImageSource>(size_in_dip_), image_size);
  } else {
    image_skia_ =
        gfx::ImageSkia(std::make_unique<ImageSource>(size_in_dip_), image_size);
  }
  return true;
}

void AppIconDecoder::OnIconRead(
    std::map<ui::ResourceScaleFactor, IconValuePtr> icon_datas) {
  // Check `icon_datas` to set scale factors.
  if (!SetScaleFactors(icon_datas)) {
    DiscardDecodeRequest();
    return;
  }

  // Create DecodeRequest to decode images safely in a sandboxed service per
  // security requests.
  for (auto& [scale_factor, iv] : icon_datas) {
    if (HasAdaptiveIconData(iv)) {
      if (!is_adaptive_icon_) {
        // If we can't generate the adaptive icon for all scales, decode the
        // foreground images only to fill in `image_skia_`.
        DecodeImage(scale_factor, std::move(iv->foreground_icon_png_data),
                    image_skia_, incomplete_scale_factors_);
        continue;
      }

      // Decode for the foreground and background image.
      DecodeImage(scale_factor, std::move(iv->foreground_icon_png_data),
                  foreground_image_skia_, foreground_incomplete_scale_factors_);
      DecodeImage(scale_factor, std::move(iv->background_icon_png_data),
                  background_image_skia_, background_incomplete_scale_factors_);
      continue;
    }

    is_maskable_icon_ = iv->is_maskable_icon;
    DecodeImage(scale_factor, std::move(iv->compressed), image_skia_,
                incomplete_scale_factors_);
  }
}

void AppIconDecoder::DecodeImage(
    ui::ResourceScaleFactor scale_factor,
    std::vector<uint8_t> icon_data,
    gfx::ImageSkia& image_skia,
    std::set<ui::ResourceScaleFactor>& incomplete_scale_factors) {
  if (g_decode_request_for_testing) {
    fake_decode_requests_for_testing_.emplace_back(
        std::make_unique<FakeDecodeRequestForTesting>(
            scale_factor, *this, image_skia, incomplete_scale_factors));
    fake_decode_requests_for_testing_.back().get()->Start(std::move(icon_data));
    return;
  }

  decode_requests_.emplace_back(std::make_unique<DecodeRequest>(
      scale_factor, *this, image_skia, incomplete_scale_factors));
  ImageDecoder::Start(decode_requests_.back().get(), std::move(icon_data));
}

void AppIconDecoder::UpdateImageSkia(
    ui::ResourceScaleFactor scale_factor,
    const SkBitmap& bitmap,
    gfx::ImageSkia& image_skia,
    std::set<ui::ResourceScaleFactor>& incomplete_scale_factors) {
  gfx::ImageSkiaRep image_rep(bitmap,
                              ui::GetScaleForResourceScaleFactor(scale_factor));
  DCHECK(ui::IsSupportedScale(image_rep.scale()));

  image_skia.RemoveRepresentation(image_rep.scale());
  image_skia.AddRepresentation(image_rep);
  image_skia.RemoveUnsupportedRepresentationsForScale(image_rep.scale());

  incomplete_scale_factors.erase(scale_factor);

  // For the adaptive icon, generate the adaptive icon with the foreground and
  // background icon images.
  if (is_adaptive_icon_) {
    if (foreground_incomplete_scale_factors_.empty() &&
        background_incomplete_scale_factors_.empty()) {
      auto image = apps::CompositeImagesAndApplyMask(foreground_image_skia_,
                                                     background_image_skia_);
      image.MakeThreadSafe();
      CompleteWithImageSkia(image);
    }
    return;
  }

  if (incomplete_scale_factors_.empty()) {
    CompleteWithImageSkia(image_skia_);
  }
}

void AppIconDecoder::DiscardDecodeRequest() {
  // 'callback_' is responsible to remove this AppIconDecoder object, then
  // all decode requests saved in `decode_requests_` can be destroyed, so we
  // don't need to free  DecodeRequest's objects in `decode_requests_`.
  //
  // Return an empty icon value, because the callers assume the icon value
  // should never be nullptr.
  std::move(callback_).Run(this, std::make_unique<apps::IconValue>());
}

void AppIconDecoder::CompleteWithImageSkia(const gfx::ImageSkia& image_skia) {
  // 'callback_' is responsible to remove this AppIconDecoder object, then
  // all decode requests saved in `decode_requests_` can be destroyed, so we
  // don't need to free  DecodeRequest's objects in `decode_requests_`.
  auto iv = std::make_unique<apps::IconValue>();
  iv->icon_type = IconType::kUncompressed;
  iv->uncompressed = image_skia;
  iv->is_maskable_icon = is_maskable_icon_;
  std::move(callback_).Run(this, std::move(iv));
}

ScopedDecodeRequestForTesting::ScopedDecodeRequestForTesting() {
  g_decode_request_for_testing = true;
}

ScopedDecodeRequestForTesting::~ScopedDecodeRequestForTesting() {
  g_decode_request_for_testing = false;
}

}  // namespace apps
