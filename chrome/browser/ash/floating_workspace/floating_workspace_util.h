// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_UTIL_H_
#define CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_UTIL_H_

#include "ash/ash_export.h"

class PrefRegistrySimple;
class Profile;

namespace ash::floating_workspace_util {

enum class FloatingWorkspaceVersion {
  // Default value, indicates no version was enabled.
  kNoVersionEnabled = 0,

  // Version 1.
  // TODO(crbug.com/419505108): clean up related code - V1 is never used in
  // production.
  kFloatingWorkspaceV1Enabled = 1,

  // Version 2.
  kFloatingWorkspaceV2Enabled = 2,

  // FloatingWorkspaceService is to be used for automatic sign-out
  // functionality, but not for app restore.
  kAutoSignoutOnly = 3,
};

ASH_EXPORT void RegisterProfilePrefs(PrefRegistrySimple* registry);

// DEPRECATED. V1 is no longer being used.
// TODO(crbug.com/297795546): Clean up V1 code path
ASH_EXPORT bool IsFloatingWorkspaceV1Enabled();

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
