// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"

#include "ash/shell.h"
#include "ash/style/color_util.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider_source.h"
#include "url/gurl.h"

namespace web_app {

void CreateIconInfoForSystemWebApp(
    const GURL& app_url,
    const std::initializer_list<IconResourceInfo>& manifest_icons,
    WebAppInstallInfo& web_app_info) {
  for (const auto& info : manifest_icons) {
    web_app_info.manifest_icons.emplace_back(app_url.Resolve(info.icon_name),
                                             info.size);
    auto image =
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(info.resource_id);
    web_app_info.icon_bitmaps.any[info.size] = image.AsBitmap();
  }
}

void CreateShortcutsMenuItemForSystemWebApp(
    const std::u16string& name,
    const GURL& shortcut_url,
    const std::initializer_list<IconResourceInfo>& shortcut_menu_item_icons,
    WebAppInstallInfo& web_app_info) {
  WebAppShortcutsMenuItemInfo shortcut_info;
  shortcut_info.name = name;
  shortcut_info.url = shortcut_url;

  IconBitmaps bitmaps;
  for (const auto& icon : shortcut_menu_item_icons) {
    WebAppShortcutsMenuItemInfo::Icon shortcut_icon;
    shortcut_icon.square_size_px = icon.size;
    shortcut_icon.url = shortcut_url.Resolve(icon.icon_name);
    shortcut_info.any.push_back(shortcut_icon);

    bitmaps.any[icon.size] = ui::ResourceBundle::GetSharedInstance()
                                 .GetImageNamed(icon.resource_id)
                                 .AsBitmap();
  }

  web_app_info.shortcuts_menu_item_infos.push_back(shortcut_info);
  web_app_info.shortcuts_menu_icon_bitmaps.push_back(bitmaps);

  CHECK(web_app_info.shortcuts_menu_item_infos.size() ==
        web_app_info.shortcuts_menu_icon_bitmaps.size());
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

std::unique_ptr<WebAppInstallInfo>
CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(const GURL& start_url) {
  auto info = std::make_unique<WebAppInstallInfo>(
      GenerateManifestIdFromStartUrlOnly(start_url), start_url);
  return info;
}

}  // namespace web_app
