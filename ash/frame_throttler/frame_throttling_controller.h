// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_THROTTLER_FRAME_THROTTLING_CONTROLLER_H_
#define ASH_FRAME_THROTTLER_FRAME_THROTTLING_CONTROLLER_H_

#include <stdint.h>
#include <vector>

#include "ash/ash_export.h"
#include "ash/frame_throttler/frame_throttling_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/viz/common/surfaces/frame_sink_id.h"

namespace aura {
class Window;
}

namespace ui {
class ContextFactory;
}

namespace ash {

constexpr uint8_t kDefaultThrottleFps = 20;

class ASH_EXPORT FrameThrottlingController {
 public:
  explicit FrameThrottlingController(ui::ContextFactory* context_factory);
  FrameThrottlingController(const FrameThrottlingController&) = delete;
  FrameThrottlingController& operator=(const FrameThrottlingController&) =
      delete;
  ~FrameThrottlingController();

  // Starts to throttle the framerate of |windows|.
  void StartThrottling(const std::vector<aura::Window*>& windows);
  // Ends throttling of all throttled windows.
  void EndThrottling();

  void AddObserver(FrameThrottlingObserver* observer);
  void RemoveObserver(FrameThrottlingObserver* observer);

  void AddArcObserver(FrameThrottlingObserver* observer);
  void RemoveArcObserver(FrameThrottlingObserver* observer);

  uint8_t throttled_fps() const { return throttled_fps_; }

 private:
  void StartThrottlingFrameSinks(
      const std::vector<viz::FrameSinkId>& frame_sink_ids);
  void StartThrottlingArc(const std::vector<aura::Window*>& windows);
  void EndThrottlingFrameSinks();
  void EndThrottlingArc();

  ui::ContextFactory* context_factory_ = nullptr;
  base::ObserverList<FrameThrottlingObserver> observers_;
  base::ObserverList<FrameThrottlingObserver> arc_observers_;

  // The fps used for throttling.
  uint8_t throttled_fps_ = kDefaultThrottleFps;
  bool windows_throttled_ = false;
};

}  // namespace ash

#endif  // ASH_FRAME_THROTTLER_FRAME_THROTTLING_CONTROLLER_H_
