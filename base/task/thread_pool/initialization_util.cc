// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/initialization_util.h"

#include <algorithm>
#include <cmath>

#include "base/numerics/clamped_math.h"
#include "base/system/sys_info.h"

namespace base {

size_t RecommendedMaxNumberOfThreadsInThreadGroup(size_t min,
                                                  size_t max,
                                                  double cores_multiplier,
                                                  int offset) {
  const auto num_of_cores = static_cast<size_t>(SysInfo::NumberOfProcessors());
  const size_t threads = ClampAdd<size_t>(
      std::ceil<size_t>(num_of_cores * cores_multiplier), offset);
  return std::clamp(threads, min, max);
}

}  // namespace base
