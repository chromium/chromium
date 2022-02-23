// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_styles.h"

namespace web_app {

void CreateIconInfoForSystemWebApp(
    const GURL& app_url,
    const std::initializer_list<IconResourceInfo>& manifest_icons,
    WebAppInstallInfo& web_app) {
  for (const auto& info : manifest_icons) {
    web_app.manifest_icons.emplace_back(app_url.Resolve(info.icon_name),
                                        info.size);
    auto image =
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(info.resource_id);
    web_app.icon_bitmaps.any[info.size] = image.AsBitmap();
  }
}

SkColor GetDefaultBackgroundColor(const bool use_dark_mode) {
  return cros_styles::ResolveColor(
      cros_styles::ColorName::kBgColor, use_dark_mode,
      base::FeatureList::IsEnabled(
          ash::features::kSemanticColorsDebugOverride));
}

}  // namespace web_app
