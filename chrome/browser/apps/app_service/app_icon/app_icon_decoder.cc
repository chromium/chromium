// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_decoder.h"

#include "base/task/thread_pool.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/profiles/profile.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "ui/base/layout.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace {

data_decoder::DataDecoder& GetDataDecoder() {
  static base::NoDestructor<data_decoder::DataDecoder> data_decoder;
  return *data_decoder;
}

}  // namespace

namespace apps {

bool g_decode_request_for_testing = false;

AppIconDecoder::DecodeRequest::DecodeRequest(
    ui::ResourceScaleFactor scale_factor,
    AppIconDecoder& host)
    : ImageRequest(&GetDataDecoder()),
      scale_factor_(scale_factor),
      host_(host) {}

AppIconDecoder::DecodeRequest::~DecodeRequest() {
  ImageDecoder::Cancel(this);
}

void AppIconDecoder::DecodeRequest::OnImageDecoded(const SkBitmap& bitmap) {
  DCHECK(!bitmap.isNull() && !bitmap.empty());
  host_.UpdateImageSkia(scale_factor_, bitmap);
}

void AppIconDecoder::DecodeRequest::OnDecodeImageFailed() {
  host_.DiscardDecodeRequest();
}

AppIconDecoder::AppIconDecoder(
    const base::FilePath& base_path,
    const std::string& app_id,
    int32_t size_in_dip,
    base::OnceCallback<void(AppIconDecoder* decoder, IconValuePtr iv)> callback)
    : base_path_(base_path),
      app_id_(app_id),
      size_in_dip_(size_in_dip),
      callback_(std::move(callback)) {
  for (const auto& scale_factor : ui::GetSupportedResourceScaleFactors()) {
    incomplete_scale_factors_.insert(scale_factor);
  }
}

AppIconDecoder::~AppIconDecoder() = default;

void AppIconDecoder::Start() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadIconFilesOnBackgroundThread, base_path_, app_id_,
                     size_in_dip_),
      base::BindOnce(&AppIconDecoder::OnIconRead,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AppIconDecoder::OnIconRead(
    std::map<ui::ResourceScaleFactor, IconValuePtr> icon_datas) {
  for (auto& [scale_factor, iv] : icon_datas) {
    if (!iv || iv->icon_type != IconType::kCompressed ||
        iv->compressed.empty()) {
      DiscardDecodeRequest();
      return;
    }

    is_maskable_icon_ = iv->is_maskable_icon;

    if (g_decode_request_for_testing) {
      SkBitmap bitmap;
      if (!iv->compressed.empty() &&
          gfx::PNGCodec::Decode(
              reinterpret_cast<const unsigned char*>(&iv->compressed.front()),
              iv->compressed.size(), &bitmap)) {
        UpdateImageSkia(scale_factor, bitmap);
      } else {
        DiscardDecodeRequest();
      }
      continue;
    }

    // Create DecodeRequest to decode images safely in a sandboxed service per
    // ARC app icons' security requests.
    decode_requests_.emplace_back(
        std::make_unique<DecodeRequest>(scale_factor, *this));
    ImageDecoder::Start(decode_requests_.back().get(),
                        std::move(iv->compressed));
  }
}

void AppIconDecoder::UpdateImageSkia(ui::ResourceScaleFactor scale_factor,
                                     const SkBitmap& bitmap) {
  gfx::ImageSkiaRep image_rep(bitmap,
                              ui::GetScaleForResourceScaleFactor(scale_factor));
  DCHECK(ui::IsSupportedScale(image_rep.scale()));

  image_skia_.RemoveRepresentation(image_rep.scale());
  image_skia_.AddRepresentation(image_rep);
  image_skia_.RemoveUnsupportedRepresentationsForScale(image_rep.scale());

  incomplete_scale_factors_.erase(scale_factor);
  if (incomplete_scale_factors_.empty()) {
    // 'callback_' is responsible to remove this AppIconDecoder object, then
    // all decode requests saved in `decode_requests_` can be destroyed, so we
    // don't need to free  DecodeRequest's objects in `decode_requests_`.
    auto iv = std::make_unique<apps::IconValue>();
    iv->icon_type = IconType::kUncompressed;
    iv->uncompressed = image_skia_;
    iv->is_maskable_icon = is_maskable_icon_;
    std::move(callback_).Run(this, std::move(iv));
  }
}

void AppIconDecoder::DiscardDecodeRequest() {
  // 'callback_' is responsible to remove this AppIconDecoder object, then
  // all decode requests saved in `decode_requests_` can be destroyed, so we
  // don't need to free  DecodeRequest's objects in `decode_requests_`.
  std::move(callback_).Run(this, nullptr);
}

ScopedDecodeRequestForTesting::ScopedDecodeRequestForTesting() {
  g_decode_request_for_testing = true;
}

ScopedDecodeRequestForTesting::~ScopedDecodeRequestForTesting() {
  g_decode_request_for_testing = false;
}

}  // namespace apps
