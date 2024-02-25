// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_APP_RESTORE_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_APP_RESTORE_TEST_UTIL_H_

#include <string>

#include "ash/components/arc/mojom/app.mojom.h"

namespace views {
class Widget;
}

namespace ash {

// Creates an exo app window, and sets `window_app_id` for its shell application
// id, `app_id` for the window property `app_restore::kAppIdKey`.
views::Widget* CreateExoWindow(const std::string& window_app_id,
                               const std::string& app_id);

// Calls the above function to create an exo app window, and sets
// `window_app_id` for its shell application id, with an empty app id.
views::Widget* CreateExoWindow(const std::string& window_app_id);

std::string GetTestApp1Id(const std::string& package_name);
std::string GetTestApp2Id(const std::string& package_name);

std::vector<arc::mojom::AppInfoPtr> GetTestAppsList(
    const std::string& package_name,
    bool multi_app);

// We create a class so we can friend and access certain private members.
class AppLaunchInfoSaveWaiter {
 public:
  // Instantly saves app restore data, bypassing the normal 2.5s timeout. If
  // `allow_save` is true, allows writing to disk, if it wasn't already.
  static void Wait(bool allow_save = true);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_APP_RESTORE_TEST_UTIL_H_
