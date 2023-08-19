// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_icon_cache.h"

#include <map>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_icon/dip_px_util.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep_default.h"

namespace apps {

PromiseAppIconCache::PromiseAppIconCache() = default;
PromiseAppIconCache::~PromiseAppIconCache() = default;

void PromiseAppIconCache::SaveIcon(const PackageId& package_id,
                                   std::unique_ptr<PromiseAppIcon> icon) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (icon == nullptr) {
    LOG(ERROR) << "PromiseAppIconCache cannot save nullptr icon.";
    return;
  }
  if (!icon_cache_.contains(package_id)) {
    std::map<int, PromiseAppIconPtr> icons;
    icon_cache_.emplace(package_id, std::move(icons));
  }
  icon_cache_[package_id].emplace(icon->width_in_pixels, std::move(icon));
}

bool PromiseAppIconCache::DoesPackageIdHaveIcons(const PackageId& package_id) {
  return icon_cache_.contains(package_id);
}

gfx::ImageSkia PromiseAppIconCache::GetIcon(const PackageId& package_id,
                                            int32_t size_in_dip) {
  auto iter = icon_cache_.find(package_id);
  if (iter == icon_cache_.end()) {
    VLOG(1) << "No icon found for promise app – Package ID not in "
               "PromiseAppIconCache: "
            << package_id.ToString();
    return gfx::ImageSkia();
  }
  std::map<int, PromiseAppIconPtr>* icons = &iter->second;

  if (icons->size() == 0) {
    VLOG(1) << "No icon found for promise app - No icons registered for "
               "package ID: "
            << package_id.ToString();
    return gfx::ImageSkia();
  }

  gfx::ImageSkia image_skia;

  // Get a representation for every scale factor.
  for (const auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
    const float icon_scale = ui::GetScaleForResourceScaleFactor(scale_factor);
    const int icon_size_in_px =
        apps_util::ConvertDipToPxForScale(size_in_dip, icon_scale);

    auto size_iter = icons->lower_bound(icon_size_in_px);

    // If there isn't an icon that matches our minimum size requirement, use the
    // largest icon available. Do this by decrementing the iterator to get the
    // last icon in the map.
    if (size_iter == icons->end()) {
      --size_iter;
    }

    SkBitmap icon = size_iter->second->icon;

    // The icon shouldn't be empty, as we never allowed saving empty icons.
    CHECK(!icon.empty());

    // Resize |bitmap| to match |icon_scale|.
    if (icon.width() != icon_size_in_px) {
      icon = skia::ImageOperations::Resize(
          icon, skia::ImageOperations::RESIZE_LANCZOS3, icon_size_in_px,
          icon_size_in_px);
    }
    image_skia.AddRepresentation(gfx::ImageSkiaRep(icon, icon_scale));
  }
  return image_skia;
}

void PromiseAppIconCache::RemoveIconsForPackageId(const PackageId& package_id) {
  if (!icon_cache_.contains(package_id)) {
    return;
  }
  icon_cache_.erase(package_id);
}

std::vector<PromiseAppIcon*> PromiseAppIconCache::GetIconsForTesting(
    const PackageId& package_id) {
  std::vector<PromiseAppIcon*> icons;
  if (!icon_cache_.contains(package_id)) {
    return icons;
  }
  for (auto& iter : icon_cache_[package_id]) {
    icons.push_back(iter.second.get());
  }
  return icons;
}

}  // namespace apps
