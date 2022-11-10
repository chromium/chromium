// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_util.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"

namespace {

constexpr char kAppService[] = "app_service";
constexpr char kIcon[] = "icons";

// Template for the icon name.
constexpr char kIconNameTemplate[] = "%d.png";

}  // namespace

namespace apps {

base::FilePath GetIconPath(Profile* profile,
                           const std::string& app_id,
                           int32_t icon_size_in_px) {
  DCHECK(profile);
  auto icon_file_name = base::StringPrintf(kIconNameTemplate, icon_size_in_px);
  return profile->GetPath()
      .AppendASCII(kAppService)
      .AppendASCII(kIcon)
      .AppendASCII(app_id)
      .AppendASCII(icon_file_name);
}

}  // namespace apps
