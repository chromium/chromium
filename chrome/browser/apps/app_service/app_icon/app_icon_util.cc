// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_util.h"

#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"

namespace {

constexpr char kAppService[] = "app_service";
constexpr char kIcon[] = "icons";

// Template for the icon name.
constexpr char kIconNameTemplate[] = "%d.png";

}  // namespace

namespace apps {

base::FilePath GetIconPath(const base::FilePath& base_path,
                           const std::string& app_id,
                           int32_t icon_size_in_px) {
  auto icon_file_name = base::StringPrintf(kIconNameTemplate, icon_size_in_px);
  return base_path.AppendASCII(kAppService)
      .AppendASCII(kIcon)
      .AppendASCII(app_id)
      .AppendASCII(icon_file_name);
}

std::vector<uint8_t> ReadOnBackgroundThread(const base::FilePath& base_path,
                                            const std::string& app_id,
                                            int32_t icon_size_in_px) {
  const auto icon_path = apps::GetIconPath(base_path, app_id, icon_size_in_px);
  if (icon_path.empty() || !base::PathExists(icon_path)) {
    return std::vector<uint8_t>{};
  }

  std::string unsafe_icon_data;
  if (!base::ReadFileToString(icon_path, &unsafe_icon_data)) {
    return std::vector<uint8_t>{};
  }

  return {unsafe_icon_data.begin(), unsafe_icon_data.end()};
}

}  // namespace apps
