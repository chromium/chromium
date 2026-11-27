// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_UTIL_H_
#define CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_UTIL_H_

#include "ash/ash_export.h"

class PrefRegistrySimple;
class PrefService;
class Profile;

namespace ash::floating_workspace_util {

ASH_EXPORT void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns true if floating workspace is enabled for the user `prefs` belong
// to. Note this should only be called after the user's profile is loaded and
// policy has initialized.
ASH_EXPORT bool IsFloatingWorkspaceEnabled(const PrefService& prefs);

bool IsFloatingSsoEnabled(Profile* profile);

bool IsInternetConnected();

bool IsSafeMode();

bool ShouldHandleRestartRestore(const PrefService& prefs);
}  // namespace ash::floating_workspace_util

#endif  // CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_UTIL_H_
