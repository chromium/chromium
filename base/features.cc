// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/features.h"
#include "base/feature_list.h"

namespace base::features {

// Alphabetical:

// Optimizes parsing and loading of data: URLs.
BASE_FEATURE(kOptimizeDataUrls,
             "OptimizeDataUrls",
             FEATURE_DISABLED_BY_DEFAULT);

}  // namespace base::features
