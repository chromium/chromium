// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_CONTENTS_DATA_H_
#define ASH_WM_WINDOW_RESTORE_PINE_CONTENTS_DATA_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// Various data needed to populate the pine dialog.
struct ASH_EXPORT PineContentsData {
 public:
  PineContentsData();
  PineContentsData(const PineContentsData&) = delete;
  PineContentsData& operator=(const PineContentsData&) = delete;
  ~PineContentsData();

  struct AppInfo {
    explicit AppInfo(const std::string& id);
    AppInfo(const std::string& app_id,
            const std::string& tab_title,
            const std::vector<std::string>& tab_urls);
    AppInfo(const AppInfo&);
    ~AppInfo();
    // App id. Used to retrieve the app name and app icon from the app registry
    // cache.
    std::string app_id;
    // Used for browser and PWAs. Shows a more descriptive title than "Chrome".
    std::string tab_title;
    // Used by browser only. Urls of up to 5 tabs including the active tab. Used
    // to retrieve favicons.
    std::vector<std::string> tab_urls;
  };

  using AppsInfos = std::vector<AppInfo>;

  // Image read from the pine image file. Will be null if pine image file was
  // missing or decoding failed.
  gfx::ImageSkia image;

  // List of `AppInfo`'s. Each one representing an app window. There may be
  // multiple entries with the same app id.
  AppsInfos apps_infos;

  // True if the previous session crashed. The pine dialog will have slightly
  // different strings in this case.
  bool last_session_crashed = false;

  // Callbacks for the restore and cancel buttons.
  base::OnceClosure restore_callback;
  base::OnceClosure cancel_callback;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_PINE_CONTENTS_DATA_H_
