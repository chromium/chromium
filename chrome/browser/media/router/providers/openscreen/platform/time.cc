// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/openscreen/src/platform/api/time.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/time/time.h"

using std::chrono::microseconds;
using std::chrono::seconds;

namespace openscreen {
namespace platform {

Clock::time_point Clock::now() noexcept {
  // Open Screen requires at least 10,000
  // ticks per second, according to the docs. If IsHighResolution is false, the
  // supplied resolution is much worse than that (potentially up to ~15.6ms).
  if (UNLIKELY(!base::TimeTicks::IsHighResolution())) {
    static bool need_to_log_once = true;
    LOG_IF(ERROR, need_to_log_once)
        << "Open Screen requires a high resolution clock to work properly.";
    need_to_log_once = false;
  }

  return Clock::time_point(
      microseconds(base::TimeTicks::Now().since_origin().InMicroseconds()));
}

std::chrono::seconds GetWallTimeSinceUnixEpoch() noexcept {
  const auto delta = base::Time::Now() - base::Time::UnixEpoch();
  return seconds(delta.InSeconds());
}

}  // namespace platform
}  // namespace openscreen
