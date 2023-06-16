// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"

#include "ash/shell.h"
#include "ash/style/color_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider_source.h"

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
  // TODO(b/255842593): If windows can ever have different ColorProviders, this
  // should be deleted as we'll need to move this logic into
  // web_app_browser_controller instead.
  ui::ColorProviderSource* color_provider_source =
      ash::ColorUtil::GetColorProviderSourceForWindow(
          ash::Shell::GetPrimaryRootWindow());
  DCHECK(color_provider_source);
  const ui::ColorProvider* color_provider =
      color_provider_source->GetColorProvider();
  DCHECK(color_provider);

  ui::ColorId color_id =
      use_dark_mode ? cros_tokens::kBgColorDark : cros_tokens::kBgColorLight;

  return color_provider->GetColor(color_id);
}

}  // namespace web_app
