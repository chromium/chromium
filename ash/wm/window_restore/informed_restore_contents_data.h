// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_CONTENTS_DATA_H_
#define ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_CONTENTS_DATA_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

// Various data needed to populate the informed restore dialog.
struct ASH_EXPORT InformedRestoreContentsData {
 public:
  InformedRestoreContentsData();
  InformedRestoreContentsData(const InformedRestoreContentsData&) = delete;
  InformedRestoreContentsData& operator=(const InformedRestoreContentsData&) = delete;
  ~InformedRestoreContentsData();

  // The dialog will display a different description string based on the type.
  enum class DialogType {
    kNormal,
    kCrash,
    kUpdate,
  };

  // TODO(http://b/365839465): Introduce more data for coral usage. We need more
  // than just 5 tabs and the tab title of all tabs, not just the active one.
  struct AppInfo {
    AppInfo(const std::string& id, const std::string& title, int window_id);
    AppInfo(const std::string& app_id,
            const std::string& title,
            int window_id,
            const std::vector<GURL>& tab_urls,
            const size_t tab_count,
            uint64_t lacros_profile_id);
    AppInfo(const AppInfo&);
    ~AppInfo();

    // App id. Used to retrieve the app name and app icon from the app registry
    // cache.
    std::string app_id;

    // This title has two uses. If it is a browser, then it shows the active tab
    // title, so that it is more descriptive than "Chrome". Otherwise, it shows
    // a temporary title (last session's window title)  that will be overridden
    // once we can fetch titles from the app service using `app_id`.
    std::string title;

    // Window id. Used to identify the restore content item when the
    // corresponding content data gets updated.
    int window_id;

    // Used by browser only. Urls of up to 5 tabs including the active tab. Used
    // to retrieve favicons.
    std::vector<GURL> tab_urls;

    // Used by browser only. The total number of tabs, including ones not listed
    // in `tab_urls`.
    size_t tab_count = 0u;

    // Used by lacros-browser only. Used to fetch the favicon from the favicon
    // service associated with this id.
    uint64_t lacros_profile_id = 0;
  };

  using AppsInfos = std::vector<AppInfo>;

  // Image read from the image file. Will be null if image file was missing or
  // decoding failed.
  gfx::ImageSkia image;

  // List of `AppInfo`'s. Each one representing an app window. There may be
  // multiple entries with the same app id.
  AppsInfos apps_infos;

  // True if the previous session crashed. The dialog will have slightly
  // different strings in this case.
  bool last_session_crashed = false;

  // The dialog will have slightly different strings depending on its type.
  DialogType dialog_type = DialogType::kNormal;

  // Callbacks for the restore and cancel buttons.
  base::OnceClosure restore_callback;
  base::OnceClosure cancel_callback;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_CONTENTS_DATA_H_
