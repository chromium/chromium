// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_THROTTLER_FRAME_THROTTLING_CONTROLLER_H_
#define ASH_FRAME_THROTTLER_FRAME_THROTTLING_CONTROLLER_H_

#include <stdint.h>
#include <vector>

#include "ash/ash_export.h"
#include "ash/frame_throttler/frame_throttling_observer.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host_observer.h"

namespace aura {
class WindowTreeHost;
class Window;
}

namespace ui {
class ContextFactory;
}

namespace ash {

constexpr uint8_t kDefaultThrottleFps = 20;

class ASH_EXPORT FrameThrottlingController final
    : public aura::WindowTreeHostObserver,
      public aura::WindowObserver {
 public:
  explicit FrameThrottlingController(ui::ContextFactory* context_factory);
  FrameThrottlingController(const FrameThrottlingController&) = delete;
  FrameThrottlingController& operator=(const FrameThrottlingController&) =
      delete;
  ~FrameThrottlingController() override;

  // ui::WindowTreeHostObserver overrides
  void OnCompositingFrameSinksToThrottleUpdated(
      const aura::WindowTreeHost* host,
      const base::flat_set<viz::FrameSinkId>& ids) override;

  // ui::WindowObserver overrides
  void OnWindowDestroying(aura::Window* window) override;

  void OnWindowTreeHostCreated(aura::WindowTreeHost* host);

  // Starts to throttle the framerate of |windows|.
  void StartThrottling(const std::vector<aura::Window*>& windows);
  // Ends throttling of all throttled windows.
  void EndThrottling();

  std::vector<viz::FrameSinkId> GetFrameSinkIdsToThrottle() const;

  void AddArcObserver(FrameThrottlingObserver* observer);
  void RemoveArcObserver(FrameThrottlingObserver* observer);
  bool HasArcObserver(FrameThrottlingObserver* observer);

  uint8_t throttled_fps() const { return throttled_fps_; }

 private:
  void StartThrottlingArc(const std::vector<aura::Window*>& windows);
  void EndThrottlingArc();

  void UpdateThrottlingOnBrowserWindows();

  ui::ContextFactory* context_factory_ = nullptr;
  base::ObserverList<FrameThrottlingObserver> observers_;
  base::ObserverList<FrameThrottlingObserver> arc_observers_;

  // Maps aura::WindowTreeHost* to a set of FrameSinkIds to be throttled.
  using WindowTreeHostMap = base::flat_map<const aura::WindowTreeHost*,
                                           base::flat_set<viz::FrameSinkId>>;
  // Compositing-based throttling updates the set of FrameSinkIds per tree and
  // this map keeps each aura::WindowTreeHost* to the most recent updated
  // FrameSinkIds.
  WindowTreeHostMap host_to_ids_map_;

  // Frame sink ids to be throttled in special UI modes, such as overview and
  // window cycling. This set will be empty when UI is not in such modes.
  base::flat_set<viz::FrameSinkId> manually_throttled_ids_;

  // The fps used for throttling.
  uint8_t throttled_fps_ = kDefaultThrottleFps;
  bool windows_manually_throttled_ = false;
};

}  // namespace ash

#endif  // ASH_FRAME_THROTTLER_FRAME_THROTTLING_CONTROLLER_H_
