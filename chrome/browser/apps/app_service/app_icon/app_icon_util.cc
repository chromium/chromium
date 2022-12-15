// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_util.h"

#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/app_service/app_icon/dip_px_util.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/layout.h"
#include "ui/gfx/geometry/size.h"

namespace {

constexpr char kAppService[] = "app_service";
constexpr char kIcon[] = "icons";

// Template for the icon name.
constexpr char kIconNameTemplate[] = "%d.png";
constexpr char kMaskableIconNameTemplate[] = "mask_%d.png";

}  // namespace

namespace apps {

base::FilePath GetIconPath(const base::FilePath& base_path,
                           const std::string& app_id,
                           int32_t icon_size_in_px,
                           bool is_maskable_icon) {
  // For a maskable icon, the icon file name is mask_%d.png.
  // For a non-maskable icon, the icon file name is %d.png.
  auto icon_file_name =
      is_maskable_icon
          ? base::StringPrintf(kMaskableIconNameTemplate, icon_size_in_px)
          : base::StringPrintf(kIconNameTemplate, icon_size_in_px);
  return base_path.AppendASCII(kAppService)
      .AppendASCII(kIcon)
      .AppendASCII(app_id)
      .AppendASCII(icon_file_name);
}

IconValuePtr ReadOnBackgroundThread(const base::FilePath& base_path,
                                    const std::string& app_id,
                                    int32_t icon_size_in_px) {
  auto iv = std::make_unique<IconValue>();
  iv->icon_type = IconType::kCompressed;
  iv->is_maskable_icon = true;

  // Use the maskable icon if possible.
  auto icon_path = apps::GetIconPath(base_path, app_id, icon_size_in_px,
                                     /*is_maskable_icon=*/true);
  if (icon_path.empty() || !base::PathExists(icon_path)) {
    // If there isn't a maskable icon file, read the non-maskable icon file.
    iv->is_maskable_icon = false;
    icon_path = apps::GetIconPath(base_path, app_id, icon_size_in_px,
                                  /*is_maskable_icon=*/false);
    if (icon_path.empty() || !base::PathExists(icon_path)) {
      return nullptr;
    }
  }

  std::string unsafe_icon_data;
  if (!base::ReadFileToString(icon_path, &unsafe_icon_data)) {
    return nullptr;
  }

  iv->compressed = {unsafe_icon_data.begin(), unsafe_icon_data.end()};
  return iv;
}

std::map<ui::ResourceScaleFactor, IconValuePtr> ReadIconFilesOnBackgroundThread(
    const base::FilePath& base_path,
    const std::string& app_id,
    int32_t size_in_dip) {
  std::map<ui::ResourceScaleFactor, IconValuePtr> result;
  for (auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
    int icon_size_in_px = apps_util::ConvertDipToPxForScale(
        size_in_dip, ui::GetScaleForResourceScaleFactor(scale_factor));
    result[scale_factor] =
        ReadOnBackgroundThread(base_path, app_id, icon_size_in_px);
  }
  return result;
}

}  // namespace apps
