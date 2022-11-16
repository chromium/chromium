// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_reader.h"

#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_decoder.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/dip_px_util.h"
#include "chrome/browser/profiles/profile.h"

namespace apps {

AppIconReader::AppIconReader(Profile* profile) : profile_(profile) {}

AppIconReader::~AppIconReader() = default;

void AppIconReader::ReadIcons(const std::string& app_id,
                              int32_t size_in_dip,
                              IconEffects icon_effects,
                              IconType icon_type,
                              LoadIconCallback callback) {
  const base::FilePath& base_path = profile_->GetPath();

  switch (icon_type) {
    case IconType::kUnknown: {
      std::move(callback).Run(std::make_unique<apps::IconValue>());
      return;
    }
    case IconType::kCompressed:
      if (icon_effects == apps::IconEffects::kNone) {
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
            base::BindOnce(&ReadOnBackgroundThread, base_path, app_id,
                           apps_util::ConvertDipToPx(
                               size_in_dip,
                               /*quantize_to_supported_scale_factor=*/true)),
            base::BindOnce(&AppIconReader::OnCompleteWithCompressedData,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
        return;
      }
      [[fallthrough]];
    case IconType::kUncompressed:
      [[fallthrough]];
    case IconType::kStandard: {
      decodes_.emplace_back(std::make_unique<AppIconDecoder>(
          base_path, app_id, size_in_dip,
          base::BindOnce(&AppIconReader::OnUncompressedIconRead,
                         weak_ptr_factory_.GetWeakPtr(), size_in_dip,
                         icon_effects, icon_type, std::move(callback))));
      decodes_.back()->Start();
    }
  }
}

void AppIconReader::OnUncompressedIconRead(int32_t size_in_dip,
                                           IconEffects icon_effects,
                                           IconType icon_type,
                                           LoadIconCallback callback,
                                           AppIconDecoder* decoder,
                                           gfx::ImageSkia image) {
  DCHECK_NE(IconType::kUnknown, icon_type);

  auto iv = std::make_unique<apps::IconValue>();
  iv->icon_type = icon_type;
  iv->uncompressed = image;

  auto it = base::ranges::find(decodes_, decoder,
                               &std::unique_ptr<AppIconDecoder>::get);
  DCHECK(it != decodes_.end());
  decodes_.erase(it);

  if (image.isNull()) {
    std::move(callback).Run(std::move(iv));
    return;
  }

  // Apply the icon effects on the uncompressed data. If the caller requests
  // an uncompressed icon, return the uncompressed result; otherwise, encode
  // the icon to a compressed icon, return the compressed result.
  if (icon_effects) {
    apps::ApplyIconEffects(
        icon_effects, size_in_dip, std::move(iv),
        base::BindOnce(&AppIconReader::OnCompleteWithIconValue,
                       weak_ptr_factory_.GetWeakPtr(), size_in_dip, icon_type,
                       std::move(callback)));
    return;
  }

  // If icon effects are none, ReadIcons can return the compressed icon
  // directly.
  DCHECK_NE(IconType::kCompressed, icon_type);

  std::move(callback).Run(std::move(iv));
}

void AppIconReader::OnCompleteWithIconValue(int32_t size_in_dip,
                                            IconType icon_type,
                                            LoadIconCallback callback,
                                            IconValuePtr iv) {
  if (icon_type != IconType::kCompressed) {
    std::move(callback).Run(std::move(iv));
    return;
  }

  float icon_scale = static_cast<float>(apps_util::ConvertDipToPx(
                         size_in_dip,
                         /*quantize_to_supported_scale_factor=*/true)) /
                     size_in_dip;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&apps::EncodeImageToPngBytes, iv->uncompressed,
                     icon_scale),
      base::BindOnce(&AppIconReader::OnCompleteWithCompressedData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AppIconReader::OnCompleteWithCompressedData(
    LoadIconCallback callback,
    std::vector<uint8_t> icon_data) {
  auto iv = std::make_unique<IconValue>();
  iv->icon_type = IconType::kCompressed;
  iv->compressed = std::move(icon_data);

  std::move(callback).Run(std::move(iv));
}

}  // namespace apps
