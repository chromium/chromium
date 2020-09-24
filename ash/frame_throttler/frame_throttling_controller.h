// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_THROTTLER_FRAME_THROTTLING_CONTROLLER_H_
#define ASH_FRAME_THROTTLER_FRAME_THROTTLING_CONTROLLER_H_

#include <stdint.h>
#include <vector>

#include "ash/ash_export.h"
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
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnThrottlingStarted(
        const std::vector<aura::Window*>& windows) {}
    virtual void OnThrottlingEnded() {}
  };

  explicit FrameThrottlingController(ui::ContextFactory* context_factory);
  FrameThrottlingController(const FrameThrottlingController&) = delete;
  FrameThrottlingController& operator=(const FrameThrottlingController&) =
      delete;
  ~FrameThrottlingController();

  // Starts to throttle the framerate of |windows|.
  void StartThrottling(const std::vector<aura::Window*>& windows);
  // Ends throttling of all throttled windows.
  void EndThrottling();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void StartThrottling(const std::vector<viz::FrameSinkId>& frame_sink_ids,
                       uint8_t fps);

  ui::ContextFactory* context_factory_ = nullptr;
  base::ObserverList<Observer> observers_;
  // The fps used for throttling.
  uint8_t fps_ = kDefaultThrottleFps;
  bool windows_throttled_ = false;
};

}  // namespace ash

#endif  // ASH_FRAME_THROTTLER_FRAME_THROTTLING_CONTROLLER_H_
