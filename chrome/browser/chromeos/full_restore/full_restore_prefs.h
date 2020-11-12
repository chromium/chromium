// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_PREFS_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_PREFS_H_

class PrefRegistrySimple;

namespace chromeos {
namespace full_restore {

// Enum that specifies restore options on startup. The values must not be
// changed as they are persisted on disk.
enum class RestoreOption {
  kAlways = 1,
  kAskEveryTime = 2,
  kDoNotRestore = 3,
};

extern const char kRestoreAppsAndPagesPrefName[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace full_restore
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_PREFS_H_
