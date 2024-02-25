// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_contents_data.h"

namespace ash {

PineContentsData::PineContentsData() = default;

PineContentsData::~PineContentsData() = default;

PineContentsData::AppInfo::AppInfo(const std::string& app_id)
    : app_id(app_id) {}

PineContentsData::AppInfo::AppInfo(const std::string& app_id,
                                   const std::string& tab_title,
                                   const std::vector<std::string>& tab_urls)
    : app_id(app_id), tab_title(tab_title), tab_urls(tab_urls) {}

PineContentsData::AppInfo::AppInfo(const AppInfo&) = default;

PineContentsData::AppInfo::~AppInfo() = default;

}  // namespace ash
