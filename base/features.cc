// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/features.h"
#include "base/feature_list.h"

namespace base::features {

// Alphabetical:

// Enforce that writeable file handles passed to untrusted processes are not
// backed by executable files.
BASE_FEATURE(kEnforceNoExecutableFileHandles,
             "EnforceNoExecutableFileHandles",
             FEATURE_DISABLED_BY_DEFAULT);

// Optimizes parsing and loading of data: URLs.
BASE_FEATURE(kOptimizeDataUrls, "OptimizeDataUrls", FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSupportsUserDataFlatHashMap,
             "SupportsUserDataFlatHashMap",
             FEATURE_DISABLED_BY_DEFAULT);

}  // namespace base::features
