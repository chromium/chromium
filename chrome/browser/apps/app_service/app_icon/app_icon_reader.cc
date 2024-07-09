// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_reader.h"

#include "ash/constants/ash_switches.h"
#include "base/files/file_util.h"
#include "base/not_fatal_until.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_decoder.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_util.h"
#include "chrome/browser/apps/app_service/app_icon/dip_px_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/component_extension_resources.h"
#include "components/app_constants/constants.h"

namespace {

bool UseSmallIcon(int32_t size_in_dip) {
  int size_in_px = apps_util::ConvertDipToPx(
      size_in_dip, /*quantize_to_supported_scale_factor=*/false);
  return size_in_px <= 32;
}

int GetResourceIdForIcon(const std::string& id,
                         int32_t size_in_dip,
                         const apps::IconKey& icon_key) {
  if (id == arc::kPlayStoreAppId) {
    return UseSmallIcon(size_in_dip) ? IDR_ARC_SUPPORT_ICON_32_PNG
                                     : IDR_ARC_SUPPORT_ICON_192_PNG;
  }

  if (ash::switches::IsAshDebugBrowserEnabled() &&
      id == app_constants::kChromeAppId) {
    return UseSmallIcon(size_in_dip) ? IDR_DEBUG_CHROME_APP_ICON_32
                                     : IDR_DEBUG_CHROME_APP_ICON_192;
  }

  return icon_key.resource_id;
}

}  // namespace

namespace apps {

AppIconReader::AppIconReader(Profile* profile) : profile_(profile) {}

AppIconReader::~AppIconReader() = default;

void AppIconReader::ReadIcons(const std::string& id,
                              int32_t size_in_dip,
                              const IconKey& icon_key,
                              IconType icon_type,
                              LoadIconCallback callback) {
  TRACE_EVENT0("ui", "AppIconReader::ReadIcons");
  IconEffects icon_effects = static_cast<IconEffects>(icon_key.icon_effects);
  int resource_id = GetResourceIdForIcon(id, size_in_dip, icon_key);
  if (resource_id != IconKey::kInvalidResourceId) {
    LoadIconFromResource(profile_, id, icon_type, size_in_dip, resource_id,
                         /*is_placeholder_icon=*/false, icon_effects,
                         std::move(callback));
    return;
  }

  const base::FilePath& base_path = profile_->GetPath();

  if (icon_type == IconType::kUnknown) {
    std::move(callback).Run(std::make_unique<apps::IconValue>());
    return;
  }

  decodes_.emplace_back(std::make_unique<AppIconDecoder>(
      base_path, id, size_in_dip,
      base::BindOnce(&AppIconReader::OnUncompressedIconRead,
                     weak_ptr_factory_.GetWeakPtr(), size_in_dip, icon_effects,
                     icon_type, id, std::move(callback))));
  decodes_.back()->Start();
}

void AppIconReader::OnUncompressedIconRead(int32_t size_in_dip,
                                           IconEffects icon_effects,
                                           IconType icon_type,
                                           const std::string& id,
                                           LoadIconCallback callback,
                                           AppIconDecoder* decoder,
                                           IconValuePtr iv) {
  TRACE_EVENT0("ui", "AppIconReader::OnUncompressedIconRead");
  DCHECK_NE(IconType::kUnknown, icon_type);

  auto it = base::ranges::find(decodes_, decoder,
                               &std::unique_ptr<AppIconDecoder>::get);
  CHECK(it != decodes_.end(), base::NotFatalUntil::M130);
  decodes_.erase(it);

  if (!iv || iv->icon_type != IconType::kUncompressed ||
      iv->uncompressed.isNull()) {
    std::move(callback).Run(std::move(iv));
    return;
  }

  iv->icon_type = icon_type;

  if (!icon_effects) {
    // If the caller requests an uncompressed icon, return the uncompressed
    // result; otherwise, encode the icon to a compressed icon, return the
    // compressed result.
    OnCompleteWithIconValue(size_in_dip, icon_type, std::move(callback),
                            std::move(iv));
    return;
  }

  // Apply the icon effects on the uncompressed data.

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
      profile_, id, icon_effects, size_in_dip, std::move(iv),
      base::BindOnce(&AppIconReader::OnCompleteWithIconValue,
                     weak_ptr_factory_.GetWeakPtr(), size_in_dip, icon_type,
                     std::move(callback)));
}

void AppIconReader::OnCompleteWithIconValue(int32_t size_in_dip,
                                            IconType icon_type,
                                            LoadIconCallback callback,
                                            IconValuePtr iv) {
  TRACE_EVENT0("ui", "AppIconReader::OnCompleteWithIconValue");
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
  TRACE_EVENT0("ui", "AppIconReader::OnCompleteWithCompressedData");
  auto iv = std::make_unique<IconValue>();
  iv->icon_type = IconType::kCompressed;
  iv->compressed = std::move(icon_data);

  std::move(callback).Run(std::move(iv));
}

}  // namespace apps
