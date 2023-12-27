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

  // The lacros windows that are to be throttled this frame.
  base::flat_map<aura::Window*, viz::FrameSinkId> lacros_candidates;
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

  // Starts to throttle the frame rate of |windows| at the custom
  // |requested_frame_interval|. The |requested_frame_interval| is used unless
  // the controller is actively throttling other windows not specified via
  // StartThrottling() at a frame rate higher than the one requested here.
  //
  // Examples:
  // Case 1: Controller is throttling a window specified via
  //         OnCompositingFrameSinksToThrottleUpdated() at 20 fps, and
  //         StartThrottling(<new window>, 30fps) is called. Both windows
  //         will be throttled at 30 fps.
  // Case 2: Controller is throttling a window specified via
  //         OnCompositingFrameSinksToThrottleUpdated() at 20 fps, and
  //         StartThrottling(<new window>, 15fps) is called. Both windows
  //         will be throttled at 20 fps.
  // Case 3: Controller is not throttling any windows, and
  //         StartThrottling(<new window>, 15fps) is called. The new window
  //         will be throttled at 15 fps.
  //
  // The higher frame rate is always picked to ensure all UIs are smooth enough,
  // even if it comes at the cost of more power consumption.
  //
  // If the |requested_frame_interval| is zero, the default throttled frame rate
  // is used internally.
  void StartThrottling(
      const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows,
      base::TimeDelta requested_frame_interval = base::TimeDelta());

  // Ends throttling of all windows specified via StartThrottling(). The
  // throttled frame rate for any remaining windows returns to the default.
  void EndThrottling();

  std::vector<viz::FrameSinkId> GetFrameSinkIdsToThrottle() const;

  void AddArcObserver(FrameThrottlingObserver* observer);
  void RemoveArcObserver(FrameThrottlingObserver* observer);
  bool HasArcObserver(FrameThrottlingObserver* observer);

  // The current frame interval being used. Note this applies to all windows
  // that the controller is currently throttling. The viz service does not allow
  // for multiple simultaneous frame rates.
  base::TimeDelta current_throttled_frame_interval() const {
    return current_throttled_frame_interval_;
  }

  // Returns 1 / current_throttled_frame_interval() rounded to the nearest
  // integer. Note if the frame interval is very large, this may legitimately
  // return 0 fps.
  uint8_t GetCurrentThrottledFrameRate() const;

 private:
  void StartThrottlingArc(const std::vector<aura::Window*>& windows,
                          uint8_t throttled_fps);
  void EndThrottlingArc();

  // Collect the lacros window in the given |window|. This function recursively
  // walks through |window|'s descendents and finds the lacros window if any.
  // |inside_lacros| is a flag to indicate if the functions is called inside a
  // lacros window. |ids| are the ids of the frame sinks that are qualified for
  // throttling. |candidates|, as output, will be filled with throttle
  // candidates info. |lacros_window|, as output, will be set to the lacros
  // window found.
  void CollectLacrosWindowsInWindow(
      aura::Window* window,
      bool inside_lacros,
      const base::flat_set<viz::FrameSinkId>& ids,
      base::flat_map<aura::Window*, viz::FrameSinkId>* candidates,
      aura::Window* lacros_window = nullptr);

  // Collect the lacros candidate in the given |window|. This function
  // recursively walks through |window|'s descendents and finds the lacros
  // candidate if any.
  void CollectLacrosCandidates(
      aura::Window* window,
      base::flat_map<aura::Window*, viz::FrameSinkId>* candidates,
      aura::Window* lacros_window);

  void UpdateThrottlingOnFrameSinks();
  void SetWindowsManuallyThrottled(bool windows_manually_throttled);
  void SetCurrentThrottledFrameInterval();
  // Whether there are any windows to throttle besides the ones specified via
  // StartThrottling().
  bool HasCompositingBasedThrottling() const;

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

  // The default frame interval that should be used when a custom interval is
  // not requested via StartThrottling(). This value is effectively immutable.
  base::TimeDelta default_throttled_frame_interval_;
  // The current frame interval used for throttling. Changes according to which
  // windows are throttled and what frame rates were requested by the caller.
  base::TimeDelta current_throttled_frame_interval_;
  // The latest |requested_frame_interval| provided in StartThrottling().
  // May be zero if one was not requested.
  base::TimeDelta latest_custom_throttled_frame_interval_;

  bool windows_manually_throttled_ = false;
};

}  // namespace ash

#endif  // ASH_FRAME_THROTTLER_FRAME_THROTTLING_CONTROLLER_H_
