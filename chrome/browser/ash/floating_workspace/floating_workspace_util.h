// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_UTIL_H_
#define CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_UTIL_H_

#include "ash/ash_export.h"

class PrefRegistrySimple;
class Profile;

namespace ash::floating_workspace_util {

ASH_EXPORT void RegisterProfilePrefs(PrefRegistrySimple* registry);

// DEPRECATED. Please use `IsFloatingWorkspaceEnabled` which takes the `profile`
// argument.
// Returns true if floating workspace is enabled. Note this should
// only be called after primary user profile is loaded and policy
// has initialized.
// TODO(crbug.com/417724348): migrate call sites to `IsFloatingWorkspaceEnabled`
// and remove this.
ASH_EXPORT bool IsFloatingWorkspaceV2Enabled();

ASH_EXPORT bool IsFloatingWorkspaceEnabled(const Profile* profile);

bool IsFloatingSsoEnabled(Profile* profile);

bool IsInternetConnected();

bool IsSafeMode();

bool ShouldHandleRestartRestore();
}  // namespace ash::floating_workspace_util

#endif  // CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_UTIL_H_
