// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_CONTROLLER_H_

#include <memory>
#include <ostream>
#include <string_view>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/common/actor/task_id.h"
#include "components/page_content_annotations/content/browser/page_settled_monitor.h"
#include "components/page_content_annotations/content/mojom/page_stability.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace actor {

class ObservationDelayMetrics;
class PageSettledMonitorDelegate;

// Observes a page during tool-use and determines when the page has settled
// after an action and is ready for an observation.
//
// This class uses PageSettledMonitor to wait for general page stability,
// loading, and visual updates, while injecting Actor-specific waits, e.g.
// federated login and autofill predictions, at the appropriate milestones.
class ObservationDelayController : public content::WebContentsObserver {
 public:
  enum class Result {
    kOk,
    // This is returned if the primary main frame starts a new navigation
    // while we are waiting. (ie. during a Wait call).
    kPageNavigated,
  };

  using ReadyCallback = base::OnceCallback<void(Result)>;

  using PageStabilityConfig =
      page_content_annotations::PageSettledMonitor::PageStabilityConfig;

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

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // The navigation count is the number of subsequent navigations that have
  // happend in a series and kPageNavigated being returned. Once the number
  // exceeds too many, kPageNavigated will be returned.
  size_t NavigationCount() const;
  void SetNavigationCount(size_t);

  // Internal states of the controller, including both generic page settling
  // states and Actor-specific wait states.
  enum class State {
    kInitial,
    kWaitForPageStability,
    kPageStabilityMonitorDisconnected,
    kWaitForFederatedLogin,
    kWaitForLoadCompletion,
    kWaitForVisualStateUpdate,
    kMaybeDelayForLcp,
    kDelayForLcp,
    kWaitForAutofillPredictions,
    kDidTimeout,
    kPageNavigated,
    kDone
  };
  static std::string_view StateToString(State state);

 protected:
  // Protected so tests can hook into state changes and some internal state.
  virtual void SetState(State state);
  State state_ = State::kInitial;
  std::unique_ptr<page_content_annotations::PageSettledMonitor>
      page_settled_monitor_;

 private:
  friend std::ostream& operator<<(
      std::ostream& o,
      const ObservationDelayController::State& state);

  friend class PageSettledMonitorDelegate;

  void OnFederatedLoginRequestComplete(base::OnceClosure resume_callback);
  void OnAutofillPredictionsFinished(base::OnceClosure resume_callback);
  void DCheckStateTransition(State old_state, State new_state);
  void MoveToState(State state);
  base::OnceClosure MoveToStateClosure(State new_state);
  base::OnceClosure PostMoveToStateClosure(
      State new_state,
      base::TimeDelta delay = base::TimeDelta());

  void OnPageSettled();
  void WillMoveToState(
      page_content_annotations::PageSettledMonitor::State state);
  void OnMilestoneReached(
      page_content_annotations::PageSettledMonitor::Milestone milestone,
      base::OnceClosure resume_callback);
  void OnEvent(page_content_annotations::PageSettledMonitor::Event event);

  ReadyCallback ready_callback_;
  Result result_ = Result::kOk;
  base::raw_ref<AggregatedJournal> journal_;
  TaskId task_id_;
  size_t navigation_count_ = 0;

  // Async entry for entire duration after Wait is called.
  std::unique_ptr<AggregatedJournal::PendingAsyncEntry> wait_journal_entry_;

  // Async entry for nested inner states. Note that this is only created for
  // states after PageStability to avoid nesting issues - PageStabilityMonitor
  // provides its own async entries.
  std::unique_ptr<AggregatedJournal::PendingAsyncEntry> inner_journal_entry_;

  base::CallbackListSubscription federated_login_subscription_;

  // A callback provided by PageSettledMonitor when a milestone is reached. It
  // must be run to allow the monitor to proceed to the next state.
  base::OnceClosure resume_callback_;

  std::unique_ptr<ObservationDelayMetrics> metrics_;

  base::WeakPtrFactory<ObservationDelayController> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_CONTROLLER_H_
