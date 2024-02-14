// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_ICON_CACHE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_ICON_CACHE_H_

#include <map>
#include <optional>

#include "base/sequence_checker.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {

class PromiseAppIcon;
using PromiseAppIconPtr = std::unique_ptr<PromiseAppIcon>;

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

  // Get the icon for the specified package ID.
  void GetIconAndApplyEffects(const PackageId& package_id,
                              int32_t size_hint_in_dip,
                              IconEffects icon_effects,
                              LoadIconCallback callback);

  // Removes the icons cached for a specified package ID.
  void RemoveIconsForPackageId(const PackageId& package_id);

  // For testing only. Retrieves pointers to all the registered icons for a
  // package ID.
  std::vector<PromiseAppIcon*> GetIconsForTesting(const PackageId& package_id);

 private:
  // Map of all icons for each promise app registration. The inner map
  // (std::map<int, PromiseAppIconPtr>) contains all the icons for a promise
  // app, keyed (and ordered) by the icon width to make it easier to
  // retrieve a requested icon size.
  std::map<PackageId, std::map<int, PromiseAppIconPtr>> icon_cache_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_ICON_CACHE_H_
