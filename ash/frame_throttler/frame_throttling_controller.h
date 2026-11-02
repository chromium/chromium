// Copyright 2020 The Chromium Authors
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
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host_observer.h"

namespace aura {
class WindowTreeHost;
class Window;
}

namespace viz {
class HostFrameSinkManager;
}  // namespace viz

namespace ash {

class ASH_EXPORT ThottleControllerWindowDelegate {
 public:
  virtual ~ThottleControllerWindowDelegate() = default;
  virtual viz::FrameSinkId GetFrameSinkIdForWindow(
      const aura::Window* window) const = 0;
};

ASH_EXPORT void SetThottleControllerWindowDelegate(
    std::unique_ptr<ThottleControllerWindowDelegate> delegate);

constexpr uint8_t kDefaultThrottleFps = 20;

struct ThrottleCandidates {
  ThrottleCandidates();
  ~ThrottleCandidates();
  ThrottleCandidates(const ThrottleCandidates&);
  ThrottleCandidates& operator=(const ThrottleCandidates&);

  // Returns true if there are no candidates to throttle.
  bool IsEmpty() const;

  // The frame sink ids of the browser windows to be throttled this frame.
  base::flat_set<viz::FrameSinkId> browser_frame_sink_ids;
};

class ASH_EXPORT FrameThrottlingController final
    : public aura::WindowTreeHostObserver,
      public aura::WindowObserver {
 public:
  explicit FrameThrottlingController(
      viz::HostFrameSinkManager* host_frame_sink_manager);
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

  // Starts to throttle the frame rate of |windows| at kDefaultThrottleFps.
  void StartThrottling(
      const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows);

  // Ends throttling of all windows specified via StartThrottling().
  void EndThrottling();

  std::vector<viz::FrameSinkId> GetFrameSinkIdsToThrottle() const;

  void AddArcObserver(FrameThrottlingObserver* observer);
  void RemoveArcObserver(FrameThrottlingObserver* observer);
  bool HasArcObserver(FrameThrottlingObserver* observer);

  // The frame interval used for all windows that the controller throttles.
  // The viz service does not allow for multiple simultaneous frame rates.
  base::TimeDelta current_throttled_frame_interval() const {
    return current_throttled_frame_interval_;
  }

  // Returns 1 / current_throttled_frame_interval() rounded to the nearest
  // integer.
  uint8_t GetCurrentThrottledFrameRate() const;

 private:
  void StartThrottlingArc(const std::vector<aura::Window*>& windows);
  void EndThrottlingArc();

  void UpdateThrottlingOnFrameSinks();

  void ResetThrottleCandidates(ThrottleCandidates* candidates);

  const raw_ptr<viz::HostFrameSinkManager> host_frame_sink_manager_;
  base::ObserverList<FrameThrottlingObserver> arc_observers_;

  // Maps aura::WindowTreeHost* to a set of FrameSinkIds to be throttled.
  using WindowTreeHostMap =
      base::flat_map<const aura::WindowTreeHost*, ThrottleCandidates>;
  // Compositing-based throttling updates the set of FrameSinkIds per tree and
  // this map keeps each aura::WindowTreeHost* to the most recently updated
  // candidates, including browser and lacros windows.
  WindowTreeHostMap host_to_candidates_map_;

  // Window candidates (browser and lacros windows inclusive) to be throttled in
  // special UI modes, such as overview and window cycling. This will be empty
  // when UI is not in such modes.
  ThrottleCandidates manually_throttled_candidates_;

  const base::TimeDelta current_throttled_frame_interval_;

  bool windows_manually_throttled_ = false;
};

}  // namespace ash

#endif  // ASH_FRAME_THROTTLER_FRAME_THROTTLING_CONTROLLER_H_
