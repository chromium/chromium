// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AUTOTEST_PRIVATE_API_UTILS_H_
#define ASH_PUBLIC_CPP_AUTOTEST_PRIVATE_API_UTILS_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/callback.h"

namespace aura {
class Window;
}

// Utility functions for autotest private APIs and ShellTestAPI.
namespace ash {

// Get application windows, windows that are shown in overview grid.
ASH_EXPORT std::vector<aura::Window*> GetAppWindowList();

// Runs the callback when the launcher state becomes |state| after
// state transition animation. For clamshell launcher, it invokes closure
// immediately if the state is already at the target state. For home
// launcher, the caller is responsible for making sure the transition
// will happen. (This is because the detecting the home launcher state
// correctly isn't trivial).
// Returns true if the closure has been invoked.
ASH_EXPORT bool WaitForLauncherState(AppListViewState state,
                                     base::OnceClosure closure);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AUTOTEST_PRIVATE_API_UTILS_H_
