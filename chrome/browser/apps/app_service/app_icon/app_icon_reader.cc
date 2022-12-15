// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_reader.h"

#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_decoder.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/dip_px_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/component_extension_resources.h"

namespace {

int GetResourceIdForIcon(const std::string& app_id,
                         int32_t size_in_dip,
                         const apps::IconKey& icon_key) {
  int resource_id = icon_key.resource_id;

  if (app_id == arc::kPlayStoreAppId) {
    int size_in_px = apps_util::ConvertDipToPx(
        size_in_dip, /*quantize_to_supported_scale_factor=*/true);
    resource_id = (size_in_px <= 32) ? IDR_ARC_SUPPORT_ICON_32_PNG
                                     : IDR_ARC_SUPPORT_ICON_192_PNG;
  }

  return resource_id;
}

}  // namespace

namespace apps {

AppIconReader::AppIconReader(Profile* profile) : profile_(profile) {}

AppIconReader::~AppIconReader() = default;

void AppIconReader::ReadIcons(const std::string& app_id,
                              int32_t size_in_dip,
                              const IconKey& icon_key,
                              IconType icon_type,
                              LoadIconCallback callback) {
  IconEffects icon_effects = static_cast<IconEffects>(icon_key.icon_effects);
  int resource_id = GetResourceIdForIcon(app_id, size_in_dip, icon_key);

  if (icon_key.resource_id != IconKey::kInvalidResourceId) {
    LoadIconFromResource(icon_type, size_in_dip, resource_id,
                         /*is_placeholder_icon=*/false, icon_effects,
                         std::move(callback));
    return;
  }

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
            std::move(callback));
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
                                           IconValuePtr iv) {
  DCHECK_NE(IconType::kUnknown, icon_type);

  auto it = base::ranges::find(decodes_, decoder,
                               &std::unique_ptr<AppIconDecoder>::get);
  DCHECK(it != decodes_.end());
  decodes_.erase(it);

  if (!iv || iv->icon_type != IconType::kUncompressed ||
      iv->uncompressed.isNull()) {
    std::move(callback).Run(std::move(iv));
    return;
  }

  iv->icon_type = icon_type;

  // Apply the icon effects on the uncompressed data. If the caller requests
  // an uncompressed icon, return the uncompressed result; otherwise, encode
  // the icon to a compressed icon, return the compressed result.
  if (icon_effects) {
    // Per https://www.w3.org/TR/appmanifest/#icon-masks, we apply a white
    // background in case the maskable icon contains transparent pixels in its
    // safe zone, and clear the standard icon effect, apply the mask to the icon
    // without shrinking it.
    if (iv->is_maskable_icon) {
      icon_effects &= ~apps::IconEffects::kCrOsStandardIcon;
      icon_effects |= apps::IconEffects::kCrOsStandardBackground;
      icon_effects |= apps::IconEffects::kCrOsStandardMask;
    }

    if (icon_type == apps::IconType::kUncompressed) {
      // For uncompressed icon, apply the resize and pad effect.
      icon_effects |= apps::IconEffects::kMdIconStyle;

      // For uncompressed icon, clear the standard icon effects, kBackground
      // and kMask.
      icon_effects &= ~apps::IconEffects::kCrOsStandardIcon;
      icon_effects &= ~apps::IconEffects::kCrOsStandardBackground;
      icon_effects &= ~apps::IconEffects::kCrOsStandardMask;
    }

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
  iv->uncompressed.MakeThreadSafe();

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
