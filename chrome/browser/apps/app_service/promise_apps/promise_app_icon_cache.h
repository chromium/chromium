// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_ICON_CACHE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_ICON_CACHE_H_

#include <map>

#include "chrome/browser/apps/app_service/package_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {

class PromiseAppIcon;

// Stores promise app icons. Each promise app may have several icons of
// different sizes.
class PromiseAppIconCache {
 public:
  PromiseAppIconCache();
  ~PromiseAppIconCache();

  // Save an icon into the cache against a package ID. If there is already an
  // icon for this package ID, append it to the list of existing icons.
  void SaveIcon(const PackageId& package_id,
                std::unique_ptr<PromiseAppIcon> icon);

  // Checks whether there is at least one icon for a package ID.
  bool DoesPackageIdHaveIcons(const PackageId& package_id);

  // For testing only. Retrieves pointers to all the registered icons for a
  // package ID.
  std::vector<PromiseAppIcon*> GetIconsForTesting(const PackageId& package_id);

 private:
  std::map<PackageId, std::vector<std::unique_ptr<PromiseAppIcon>>> icon_cache_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_ICON_CACHE_H_
