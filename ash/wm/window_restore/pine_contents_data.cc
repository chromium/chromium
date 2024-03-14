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
                                   const std::u16string& tab_title,
                                   const std::vector<GURL>& tab_urls,
                                   const size_t tab_count)
    : app_id(app_id),
      tab_title(tab_title),
      tab_urls(tab_urls),
      tab_count(tab_count) {}

PineContentsData::AppInfo::AppInfo(const AppInfo&) = default;

PineContentsData::AppInfo::~AppInfo() = default;

}  // namespace ash
