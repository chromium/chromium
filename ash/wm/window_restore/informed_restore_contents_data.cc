// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/informed_restore_contents_data.h"

namespace ash {

InformedRestoreContentsData::InformedRestoreContentsData() = default;

InformedRestoreContentsData::~InformedRestoreContentsData() = default;

InformedRestoreContentsData::TabInfo::TabInfo() = default;

InformedRestoreContentsData::TabInfo::TabInfo(const GURL& url,
                                              const std::string& title)
    : url(url), title(title) {}

InformedRestoreContentsData::AppInfo::AppInfo(const std::string& app_id,
                                              const std::string& title,
                                              int window_id)
    : app_id(app_id), title(title), window_id(window_id) {}

InformedRestoreContentsData::AppInfo::AppInfo(const std::string& app_id,
                                              const std::string& title,
                                              int window_id,
                                              std::vector<TabInfo> tab_infos)
    : app_id(app_id),
      title(title),
      window_id(window_id),
      tab_infos(std::move(tab_infos)) {}

InformedRestoreContentsData::AppInfo::AppInfo(const AppInfo&) = default;

InformedRestoreContentsData::AppInfo::~AppInfo() = default;

}  // namespace ash
