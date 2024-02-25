// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_util.h"

#include "ash/public/cpp/shelf_types.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_service/app_icon/dip_px_util.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/geometry/size.h"

namespace {

constexpr char kAppService[] = "app_service";
constexpr char kIcon[] = "icons";

// Template for the icon name.
constexpr char kIconNameTemplate[] = "%d.png";
constexpr char kMaskableIconNameTemplate[] = "mask_%d.png";
constexpr char kForegroundIconNameTemplate[] = "foreground_%d.png";
constexpr char kBackgroundIconNameTemplate[] = "background_%d.png";

}  // namespace

namespace apps {

data_decoder::DataDecoder& GetIconDataDecoder() {
  static base::NoDestructor<data_decoder::DataDecoder> data_decoder;
  return *data_decoder;
}

base::FilePath GetIconFolderPath(const base::FilePath& base_path,
                                 const std::string& app_id) {
  return base_path.AppendASCII(kAppService)
      .AppendASCII(kIcon)
      .AppendASCII(app_id);
}

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
  return GetIconFolderPath(base_path, app_id).AppendASCII(icon_file_name);
}

base::FilePath GetForegroundIconPath(const base::FilePath& base_path,
                                     const std::string& app_id,
                                     int32_t icon_size_in_px) {
  auto icon_file_name =
      base::StringPrintf(kForegroundIconNameTemplate, icon_size_in_px);
  return GetIconFolderPath(base_path, app_id).AppendASCII(icon_file_name);
}

base::FilePath GetBackgroundIconPath(const base::FilePath& base_path,
                                     const std::string& app_id,
                                     int32_t icon_size_in_px) {
  auto icon_file_name =
      base::StringPrintf(kBackgroundIconNameTemplate, icon_size_in_px);
  return GetIconFolderPath(base_path, app_id).AppendASCII(icon_file_name);
}

bool IsAdaptiveIcon(const base::FilePath& base_path,
                    const std::string& app_id,
                    int32_t size_in_dip) {
  for (const auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
    int icon_size_in_px = apps_util::ConvertDipToPxForScale(
        size_in_dip, ui::GetScaleForResourceScaleFactor(scale_factor));

    const auto foreground_icon_path =
        apps::GetForegroundIconPath(base_path, app_id, icon_size_in_px);
    const auto background_icon_path =
        apps::GetBackgroundIconPath(base_path, app_id, icon_size_in_px);

    if (!IsAdaptiveIcon(foreground_icon_path, background_icon_path)) {
      return false;
    }
  }
  return true;
}

bool IsAdaptiveIcon(const base::FilePath& foreground_icon_path,
                    const base::FilePath& background_icon_path) {
  return !foreground_icon_path.empty() && !background_icon_path.empty() &&
         base::PathExists(foreground_icon_path) &&
         base::PathExists(background_icon_path);
}

bool HasAdaptiveIconData(const IconValuePtr& iv) {
  return iv && !iv->foreground_icon_png_data.empty() &&
         !iv->background_icon_png_data.empty();
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
      // The icon might have foreground/background icon files for some scale
      // factors, then we can use the foreground icon file for this scale.
      icon_path = GetForegroundIconPath(base_path, app_id, icon_size_in_px);
      if (icon_path.empty() || !base::PathExists(icon_path)) {
        return nullptr;
      }
    }
  }

  std::string unsafe_icon_data;
  if (!base::ReadFileToString(icon_path, &unsafe_icon_data)) {
    return nullptr;
  }

  iv->compressed = {unsafe_icon_data.begin(), unsafe_icon_data.end()};
  return iv;
}

IconValuePtr ReadAdaptiveIconOnBackgroundThread(const base::FilePath& base_path,
                                                const std::string& app_id,
                                                int32_t icon_size_in_px) {
  auto iv = std::make_unique<IconValue>();
  iv->icon_type = IconType::kCompressed;

  const auto foreground_icon_path =
      apps::GetForegroundIconPath(base_path, app_id, icon_size_in_px);
  const auto background_icon_path =
      apps::GetBackgroundIconPath(base_path, app_id, icon_size_in_px);
  if (IsAdaptiveIcon(foreground_icon_path, background_icon_path)) {
    std::string foreground_icon_data;
    std::string background_icon_data;
    if (base::ReadFileToString(foreground_icon_path, &foreground_icon_data) &&
        base::ReadFileToString(background_icon_path, &background_icon_data)) {
      iv->foreground_icon_png_data = {foreground_icon_data.begin(),
                                      foreground_icon_data.end()};
      iv->background_icon_png_data = {background_icon_data.begin(),
                                      background_icon_data.end()};
      return iv;
    }
  }

  return nullptr;
}

std::map<ui::ResourceScaleFactor, IconValuePtr> ReadIconFilesOnBackgroundThread(
    const base::FilePath& base_path,
    const std::string& app_id,
    int32_t size_in_dip) {
  std::map<ui::ResourceScaleFactor, IconValuePtr> result;
  for (const auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
    int icon_size_in_px = apps_util::ConvertDipToPxForScale(
        size_in_dip, ui::GetScaleForResourceScaleFactor(scale_factor));

    // If some scales have non-adaptive icons, we can't generate the adaptive
    // icon for all scales. We still get the foregound/background icon data for
    // the mix scenario, and decode the foreground icon data only, as done in
    // AppIconDecoder.
    auto iv =
        ReadAdaptiveIconOnBackgroundThread(base_path, app_id, icon_size_in_px);
    result[scale_factor] =
        iv ? std::move(iv)
           : ReadOnBackgroundThread(base_path, app_id, icon_size_in_px);
  }
  return result;
}

void ScheduleIconFoldersDeletion(const base::FilePath& base_path,
                                 const std::vector<std::string>& ids,
                                 base::OnceCallback<void()> callback) {
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          [](const base::FilePath& base_path,
             const std::vector<std::string>& ids) {
            for (const auto& id : ids) {
              base::DeletePathRecursively(GetIconFolderPath(base_path, id));
            }
          },
          base_path, ids),
      std::move(callback));
}

}  // namespace apps
