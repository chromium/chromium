// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_coordinator_features.h"

#include "build/build_config.h"

namespace base {

BASE_FEATURE(kStatefulMemoryPressure,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
BASE_FEATURE(kLRUCacheMemoryConsumer, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace base
