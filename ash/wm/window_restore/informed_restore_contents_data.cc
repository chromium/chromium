// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/informed_restore_contents_data.h"

namespace ash {

InformedRestoreContentsData::InformedRestoreContentsData() = default;

InformedRestoreContentsData::~InformedRestoreContentsData() = default;

InformedRestoreContentsData::AppInfo::AppInfo(const std::string& app_id,
                                   const std::string& title)
    : app_id(app_id), title(title) {}

InformedRestoreContentsData::AppInfo::AppInfo(const std::string& app_id,
                                   const std::string& title,
                                   const std::vector<GURL>& tab_urls,
                                   const size_t tab_count,
                                   uint64_t lacros_profile_id)
    : app_id(app_id),
      title(title),
      tab_urls(tab_urls),
      tab_count(tab_count),
      lacros_profile_id(lacros_profile_id) {}

InformedRestoreContentsData::AppInfo::AppInfo(const AppInfo&) = default;

InformedRestoreContentsData::AppInfo::~AppInfo() = default;

}  // namespace ash
