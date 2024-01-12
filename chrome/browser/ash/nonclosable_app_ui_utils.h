// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NONCLOSABLE_APP_UI_UTILS_H_
#define CHROME_BROWSER_ASH_NONCLOSABLE_APP_UI_UTILS_H_

#include <string>

namespace ash {

// This function will show a toast indicating to the user
// that a certain app cannot be closed.
void ShowNonclosableAppToast(const std::string& app_id,
                             const std::string& app_name);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NONCLOSABLE_APP_UI_UTILS_H_
