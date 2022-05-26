// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_SYSTEM_INFO_H_
#define BASE_FUCHSIA_SYSTEM_INFO_H_

#include "base/base_export.h"

namespace fuchsia::buildinfo {
class BuildInfo;
}
namespace fuchsia::hwinfo {
class ProductInfo;
}

namespace base {

// Makes a blocking call to fetch the info from the system and caches it
// before returning. Must be called in each process during the initialization
// phase.
BASE_EXPORT void FetchAndCacheSystemInfo();

// Returns the cached build info.
BASE_EXPORT const fuchsia::buildinfo::BuildInfo& GetCachedBuildInfo();

// Returns the cached product info.
BASE_EXPORT const fuchsia::hwinfo::ProductInfo& GetCachedProductInfo();

// Reset the cached system info to empty so that
// FetchAndCacheSystemInfo() can be called again in this process.
BASE_EXPORT void ClearCachedSystemInfoForTesting();

}  // namespace base

#endif  // BASE_FUCHSIA_SYSTEM_INFO_H_
