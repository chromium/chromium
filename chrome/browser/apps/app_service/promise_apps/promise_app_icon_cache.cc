// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_icon_cache.h"

#include <map>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "ui/gfx/image/image.h"

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
    icon_cache_.insert(std::make_pair(package_id, std::move(icons)));
  }
  icon_cache_[package_id].emplace(icon->width_in_pixels, std::move(icon));
}

bool PromiseAppIconCache::DoesPackageIdHaveIcons(const PackageId& package_id) {
  return icon_cache_.contains(package_id);
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
