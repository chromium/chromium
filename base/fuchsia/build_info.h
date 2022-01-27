// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_BUILD_INFO_H_
#define BASE_FUCHSIA_BUILD_INFO_H_

#include "base/base_export.h"
#include "base/strings/string_piece_forward.h"

namespace fuchsia {
namespace buildinfo {
class BuildInfo;
}
}  // namespace fuchsia

namespace base {

// Fetches the build info from the system and caches it before returning.
// Must be called in each process before calling other non-test functions.
BASE_EXPORT void FetchAndCacheSystemBuildInfo();

// Returns the cached build info.
BASE_EXPORT const fuchsia::buildinfo::BuildInfo& GetCachedBuildInfo();

// Returns the cached version string.
BASE_EXPORT StringPiece GetBuildInfoVersion();

// Reset the cached BuildInfo to empty so that FetchAndCacheSystemBuildInfo()
// can be called again in this process.
BASE_EXPORT void ClearCachedBuildInfoForTesting();

}  // namespace base

#endif  // BASE_FUCHSIA_BUILD_INFO_H_
