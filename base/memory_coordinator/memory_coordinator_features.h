// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_MEMORY_COORDINATOR_FEATURES_H_
#define BASE_MEMORY_COORDINATOR_MEMORY_COORDINATOR_FEATURES_H_

#include "base/base_export.h"
#include "base/feature_list.h"

namespace base {

// When enabled, MemoryPressureListeners/MemoryConsumers that support this
// feature will change their cache's max size for the duration of memory
// pressure, instead of simply evicting entries.
BASE_EXPORT BASE_DECLARE_FEATURE(kStatefulMemoryPressure);

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_MEMORY_COORDINATOR_FEATURES_H_
