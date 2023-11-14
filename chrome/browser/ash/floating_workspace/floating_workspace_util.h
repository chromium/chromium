// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_UTIL_H_
#define CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_UTIL_H_

#include "ash/ash_export.h"

class PrefRegistrySimple;

namespace ash::floating_workspace_util {

// The restore from error notification button index.
enum class FloatingWorkspaceVersion {
  // Default value, indicates no version was enabled.
  kNoVersionEnabled = 0,

  // Version 1.
  kFloatingWorkspaceV1Enabled = 1,

  // Version 2.
  kFloatingWorkspaceV2Enabled = 2,
};

ASH_EXPORT void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns true if floating workspace is enabled. Note this should
// only be called after primary user profile is loaded and policy
// has initialized.
ASH_EXPORT bool IsFloatingWorkspaceV1Enabled();
ASH_EXPORT bool IsFloatingWorkspaceV2Enabled();

bool IsInternetConnected();

bool IsSafeMode();

bool ShouldHandleRestartRestore();
}  // namespace ash::floating_workspace_util

#endif  // CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_UTIL_H_
