// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util_internal.h"

#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/file_manager/open_util.h"
#endif

namespace platform_util::internal {
namespace {

bool g_shell_operations_allowed = true;

}  // namespace

void DisableShellOperationsForTesting() {
  g_shell_operations_allowed = false;

#if BUILDFLAG(IS_CHROMEOS)
  // Chrome OS also needs to customize file manager behavior.
  file_manager::util::DisableShellOperationsForTesting();
#endif
}

bool AreShellOperationsAllowed() {
  return g_shell_operations_allowed;
}

}  // namespace platform_util::internal
