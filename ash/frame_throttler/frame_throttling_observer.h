// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_THROTTLER_FRAME_THROTTLING_OBSERVER_H_
#define ASH_FRAME_THROTTLER_FRAME_THROTTLING_OBSERVER_H_

#include <stdint.h>
#include <vector>

#include "ash/ash_export.h"
#include "base/observer_list_types.h"

namespace aura {
class Window;
}

namespace ash {

// This class observes the start and end of frame throttling.
class ASH_EXPORT FrameThrottlingObserver : public base::CheckedObserver {
 public:
  virtual void OnThrottlingStarted(const std::vector<aura::Window*>& windows,
                                   uint8_t fps) {}
  virtual void OnThrottlingEnded() {}
};

}  // namespace ash

#endif  // ASH_FRAME_THROTTLER_FRAME_THROTTLING_OBSERVER_H_
