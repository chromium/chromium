// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_UTIL_H_

#include <map>
#include <vector>

#include "ash/public/cpp/shelf_types.h"
#include "base/files/file_path.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace apps {

// Returns a shared Data Decoder instance to be used for decoding app icons.
data_decoder::DataDecoder& GetIconDataDecoder();

// Constructs the path to an app icon folder for the given `app_id`.
base::FilePath GetIconFolderPath(const base::FilePath& base_path,
                                 const std::string& app_id);

// Constructs the path to an app icon file for the given `app_id` and
// `icon_size_in_px`.
base::FilePath GetIconPath(const base::FilePath& base_path,
                           const std::string& app_id,
                           int32_t icon_size_in_px,
                           bool is_maskable_icon);

// Constructs the path to an app foreground icon file for the given `app_id` and
// `icon_size_in_px`.
base::FilePath GetForegroundIconPath(const base::FilePath& base_path,
                                     const std::string& app_id,
                                     int32_t icon_size_in_px);

// Constructs the path to an app background icon file for the given `app_id` and
// `icon_size_in_px`.
base::FilePath GetBackgroundIconPath(const base::FilePath& base_path,
                                     const std::string& app_id,
                                     int32_t icon_size_in_px);

// Returns true if the icon files for the given `app_id` and `size_in_dip`
// include the foreground and background icon files for all scale factors.
// Otherwise, returns false.
bool IsAdaptiveIcon(const base::FilePath& base_path,
                    const std::string& app_id,
                    int32_t size_in_dip);

// Returns true if both `foreground_icon_path` and `background_icon_path` are
// valid. Otherwise, returns false.
bool IsAdaptiveIcon(const base::FilePath& foreground_icon_path,
                    const base::FilePath& background_icon_path);

// Returns true if both `iv` has both `foreground_icon_png_data` and
// `background_icon_png_data`. Otherwise, returns false.
bool HasAdaptiveIconData(const IconValuePtr& iv);

// Reads icon file for the given `app_id` and `icon_size_in_px`, and
// returns the compressed icon.
//
// * If there is a maskable icon file, reads the maskable icon file.
// * Otherwise, reads other icon file.
// * If there is no appropriate icon file, or failed reading the icon file,
// returns nullptr.
IconValuePtr ReadOnBackgroundThread(const base::FilePath& base_path,
                                    const std::string& app_id,
                                    int32_t icon_size_in_px);

// Reads the foreground and background icon files for the given `app_id` and
// `icon_size_in_px`, and returns the compressed icon. If there is no
// appropriate icon file, or failed reading the icon file, returns nullptr.
IconValuePtr ReadAdaptiveIconOnBackgroundThread(const base::FilePath& base_path,
                                                const std::string& app_id,
                                                int32_t icon_size_in_px);

// Calls ReadOnBackgroundThread to read icon files for all scale factors for
// the given `app_id` and `size_in_dip`, and returns the compressed icons for
// all scale factors. Reads the foreground/background icon data as higher
// priority, then maskable icon files, and if there is no appropriate icon file,
// or failed reading the icon file, return nullptr for the scale factor.
std::map<ui::ResourceScaleFactor, IconValuePtr> ReadIconFilesOnBackgroundThread(
    const base::FilePath& base_path,
    const std::string& app_id,
    int32_t size_in_dip);

// Schedules deletion of the icon folders for `ids`, then call `callback`.
void ScheduleIconFoldersDeletion(const base::FilePath& base_path,
                                 const std::vector<std::string>& ids,
                                 base::OnceCallback<void()> callback);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_UTIL_H_
