// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_WINDOW_RESTORE_UTIL_H_
#define ASH_WM_WINDOW_RESTORE_WINDOW_RESTORE_UTIL_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "components/app_restore/window_info.h"

namespace aura {
class Window;
}

namespace ash {

namespace full_restore {

// Enum that specifies restore options on startup. The values must not be
// changed as they are persisted on disk.
//
// This is used to record histograms, so do not remove or reorder existing
// entries.
enum class RestoreOption {
  kAlways = 1,
  kAskEveryTime = 2,
  kDoNotRestore = 3,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kDoNotRestore,
};

}  // namespace full_restore

// Builds the WindowInfo for `window`. Optionally passes `activation_index`,
// which is used to set `WindowInfo.activation_index` if it has value.
// Otherwise, `WindowInfo.activation_index` will be calculated from
// `mru_windows`.
std::unique_ptr<app_restore::WindowInfo> BuildWindowInfo(
    aura::Window* window,
    std::optional<int> activation_index,
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>& mru_windows);

bool IsBrowserAppId(const std::string& id);

// Gets the path of the informed restore image being taken on the session state
// changes. It will be written to
// /home/chronos/u-<hash>/informed_restore_image.png.
ASH_EXPORT base::FilePath GetInformedRestoreImagePath();

// Sets the informed restore image path for tests.
ASH_EXPORT void SetInformedRestoreImagePathForTest(const base::FilePath& path);

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_WINDOW_RESTORE_UTIL_H_
