// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_WINDOW_RESTORE_UTIL_H_
#define ASH_WM_WINDOW_RESTORE_WINDOW_RESTORE_UTIL_H_

#include "components/app_restore/window_info.h"

namespace aura {
class Window;
}

namespace ash {

// Builds the WindowInfo for `window`. Optionally passes `activation_index`,
// which is used to set `WindowInfo.activation_index` if it has value.
// Otherwise, `WindowInfo.activation_index` will be calculated from
// `mru_windows`. If `for_saved_desks` this was called from a feature which
// saves desks, and we need to add extra information such as the app title.
std::unique_ptr<app_restore::WindowInfo> BuildWindowInfo(
    aura::Window* window,
    std::optional<int> activation_index,
    bool for_saved_desks,
    const std::vector<aura::Window*>& mru_windows);

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_WINDOW_RESTORE_UTIL_H_
