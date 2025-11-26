// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_CONTROLLER_H_

#include <memory>
#include <ostream>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/task_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace actor {

class ObservationDelayMetrics;

// Observes a page during tool-use and determines when the page has settled
// after an action and is ready for for an observation.
//
// This class will watch for any document loads in the web contents. When the
// tool completes, this class delays until the load also finishes and then a new
// frame is generated and presented.
class ObservationDelayController : public content::WebContentsObserver {
 public:
  using ReadyCallback = base::OnceClosure;

  // Configuration for general page stability if enabled.
  struct PageStabilityConfig {
    // Whether to include paint stability in page stability heuristics.
    bool supports_paint_stability = false;
    // The amount of time to wait when observing tool execution before starting
    // to wait for page stability.
    base::TimeDelta start_delay;
  };

  // This will create a PageStabilityMonitor in the renderer and wait for page
  // stability.
  ObservationDelayController(content::RenderFrameHost& target_frame,
                             TaskId task_id,
                             AggregatedJournal& journal,
                             PageStabilityConfig page_stability_config);
  // Constructor for when we're not watching for page stability and do not have
  // a target RenderFrameHost available.
  ObservationDelayController(TaskId task_id, AggregatedJournal& journal);
  ~ObservationDelayController() override;

  // Note: Callback will always be executed asynchronously. It may be run after
  // this object is deleted so must manage its own lifetime.
  // Note: If a RenderFrame was provided in the constructor, `target_tab` should
  // contain it.
  //
  // `target_tab`: The tab on which to wait. The WebContents of this tab will be
  // observed by this controller, overwriting any previously observed
  // WebContents.
  void Wait(tabs::TabInterface& target_tab, ReadyCallback callback);

  // content::WebContentsObserver
  void DidStopLoading() override;

  // Public for tests
  enum class State {
    kInitial,
    kWaitForPageStability,
    kPageStabilityMonitorDisconnected,
    kWaitForLoadCompletion,
    kWaitForVisualStateUpdate,
    kMaybeDelayForLcp,
    kDelayForLcp,
    kDidTimeout,
    kDone
  };
  static std::string_view StateToString(State state);

 protected:
  // Protected so tests can hook into state changes and some internal state.
  virtual void SetState(State state);
  State state_ = State::kInitial;
  mojo::Remote<mojom::PageStabilityMonitor> page_stability_monitor_remote_;

 private:
  friend std::ostream& operator<<(
      std::ostream& o,
      const ObservationDelayController::State& state);

  void OnPageStable();
  void OnVisualStateUpdated(bool);
  void OnMonitorDisconnected();
  void DCheckStateTransition(State old_state, State new_state);
  void MoveToState(State state);
  base::OnceClosure MoveToStateClosure(State new_state);
  base::OnceClosure PostMoveToStateClosure(
      State new_state,
      base::TimeDelta delay = base::TimeDelta());

  ReadyCallback ready_callback_;
  base::raw_ref<AggregatedJournal> journal_;
  TaskId task_id_;

  // Async entry for entire duration after Wait is called.
  std::unique_ptr<AggregatedJournal::PendingAsyncEntry> wait_journal_entry_;

  // Async entry for nested inner states. Note that this is only created for
  // states after PageStability to avoid nesting issues - PageStabilityMonitor
  // provides its own async entries.
  std::unique_ptr<AggregatedJournal::PendingAsyncEntry> inner_journal_entry_;
  base::TimeDelta page_stability_start_delay_;

  std::unique_ptr<ObservationDelayMetrics> metrics_;

  base::WeakPtrFactory<ObservationDelayController> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_CONTROLLER_H_
