// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/observation_delay_controller.h"

#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/state_transitions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/observation_delay_metrics.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor.mojom-data-view.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/tabs/public/tab_handle_factory.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace actor {

using ::content::RenderFrameHost;
using ::content::WebContents;
using ::content::WebContentsObserver;

namespace {

// Timeout used when waiting for the tool to complete.
base::TimeDelta GetCompletionTimeout() {
  return features::kActorObservationDelayTimeout.Get();
}

// The additional delay to complete the tool if LCP is not detected yet upon
// loading.
base::TimeDelta GetLcpDelay() {
  return features::kActorObservationDelayLcp.Get();
}

// This should be similar to the number of redirects.
constexpr size_t kMaxNavigations = 20;

}  // namespace

ObservationDelayController::ObservationDelayController(
    content::RenderFrameHost& target_frame,
    TaskId task_id,
    AggregatedJournal& journal,
    PageStabilityConfig page_stability_config)
    : content::WebContentsObserver(
          WebContents::FromRenderFrameHost(&target_frame)),
      journal_(journal),
      task_id_(task_id) {
  CHECK(web_contents());
  journal.Log(
      GURL::EmptyGURL(), task_id, "ObservationDelay: Created",
      JournalDetailsBuilder().Add("May Use PageStability", true).Build());

  journal.EnsureJournalBound(target_frame);

  // Note: It's important that the PageStabilityMonitor be created on the same
  // interface as tool invocation since it relies on being created before a
  // tool is invoked.
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  target_frame.GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);

  chrome_render_frame->CreatePageStabilityMonitor(
      page_stability_monitor_remote_.BindNewPipeAndPassReceiver(), task_id,
      page_stability_config.supports_paint_stability);
  page_stability_monitor_remote_.set_disconnect_handler(
      base::BindOnce(&ObservationDelayController::OnMonitorDisconnected,
                     base::Unretained(this)));
  page_stability_start_delay_ = page_stability_config.start_delay;
}

ObservationDelayController::ObservationDelayController(
    TaskId task_id,
    AggregatedJournal& journal)
    : journal_(journal), task_id_(task_id) {
  journal.Log(
      GURL::EmptyGURL(), task_id, "ObservationDelay: Created",
      JournalDetailsBuilder().Add("May Use PageStability", false).Build());
}

ObservationDelayController::~ObservationDelayController() = default;

void ObservationDelayController::Wait(tabs::TabInterface& target_tab,
                                      ReadyCallback callback) {
  ready_callback_ = std::move(callback);

  metrics_ = std::make_unique<ObservationDelayMetrics>();
  metrics_->Start();

  WebContentsObserver::Observe(target_tab.GetContents());

  wait_journal_entry_ = journal_->CreatePendingAsyncEntry(
      GURL::EmptyGURL(), task_id_, MakeBrowserTrackUUID(task_id_),
      "ObservationDelay: Wait", {});

  PostMoveToStateClosure(State::kDidTimeout, GetCompletionTimeout()).Run();

  if (page_stability_monitor_remote_.is_bound()) {
    MoveToState(State::kWaitForPageStability);
  } else {
    MoveToState(State::kWaitForLoadCompletion);
  }
}

void ObservationDelayController::OnPageStable() {
  if (state_ != State::kWaitForPageStability) {
    return;
  }

  CHECK(metrics_);
  metrics_->OnPageStable();

  MoveToState(State::kWaitForLoadCompletion);
}

void ObservationDelayController::OnMonitorDisconnected() {
  page_stability_monitor_remote_.reset();

  if (state_ == State::kInitial) {
    // If Wait hasn't been called, don't enter the state machine yet. Resetting
    // the remote will skip the page stability state.
    journal_->Log(GURL::EmptyGURL(), task_id_,
                  "ObservationDelay: Monitor Disconnect Before Wait", {});
    return;
  }

  MoveToState(State::kPageStabilityMonitorDisconnected);
}

void ObservationDelayController::MoveToState(State new_state) {
  if (state_ == State::kDone) {
    return;
  }

  CHECK(metrics_);
  metrics_->WillMoveToState(new_state);

  DCheckStateTransition(state_, new_state);

  inner_journal_entry_.reset();
  journal_->Log(GURL::EmptyGURL(), task_id_, "ObservationDelay: State Change",
                JournalDetailsBuilder()
                    .Add("old_state", StateToString(state_))
                    .Add("new_state", StateToString(new_state))
                    .Build());

  SetState(new_state);

  switch (state_) {
    case State::kInitial: {
      NOTREACHED();
    }
    case State::kWaitForPageStability: {
      // Unretained since `this` owns the pipe.
      page_stability_monitor_remote_->NotifyWhenStable(
          page_stability_start_delay_,
          base::BindOnce(&ObservationDelayController::OnPageStable,
                         base::Unretained(this)));
      break;
    }
    case State::kPageStabilityMonitorDisconnected: {
      MoveToState(State::kWaitForLoadCompletion);
      break;
    }
    case State::kWaitForLoadCompletion: {
      inner_journal_entry_ = journal_->CreatePendingAsyncEntry(
          GURL::EmptyGURL(), task_id_, MakeBrowserTrackUUID(task_id_),
          "WaitForLoadCompletion", {});
      page_stability_monitor_remote_.reset();

      bool is_web_contents_loading =
          base::FeatureList::IsEnabled(
              features::kGlicActorObservationDelayExcludeAdFrameLoading)
              ? web_contents()->IsLoadingExcludingAdSubframes()
              : web_contents()->IsLoading();
      if (is_web_contents_loading) {
        // State will advance from DidStopLoading in this case.
        break;
      }

      // Posted so that this state transition is consistently async.
      PostMoveToStateClosure(State::kWaitForVisualStateUpdate).Run();
      break;
    }
    case State::kWaitForVisualStateUpdate: {
      inner_journal_entry_ = journal_->CreatePendingAsyncEntry(
          GURL::EmptyGURL(), task_id_, MakeBrowserTrackUUID(task_id_),
          "WaitForVisualStateUpdate", {});
      if (base::FeatureList::IsEnabled(
              actor::kGlicSkipAwaitVisualStateForNewTabs) &&
          web_contents()->GetVisibility() != content::Visibility::VISIBLE &&
          !web_contents()->IsBeingCaptured()) {
        // If this is a new tab that is not yet being captured, we won't get
        // visual updates, so we proceed to the next step.
        // TODO(mcnee): Consider a more general approach of skipping this when
        // the creator of this delay controller is not watching for page
        // stability.
        journal_->Log(
            web_contents()->GetLastCommittedURL(), task_id_,
            "ObservationDelay: Skip visual state update of non-captured tab",
            {});

        // Posted so that this state transition is consistently async.
        PostMoveToStateClosure(State::kMaybeDelayForLcp).Run();
      } else {
        // TODO(crbug.com/414662842): This should probably ensure an update from
        // all/selected OOPIFS?
        web_contents()->GetPrimaryMainFrame()->InsertVisualStateCallback(
            base::BindOnce(&ObservationDelayController::OnVisualStateUpdated,
                           weak_ptr_factory_.GetWeakPtr()));
      }
      break;
    }
    case State::kMaybeDelayForLcp: {
      inner_journal_entry_ = journal_->CreatePendingAsyncEntry(
          GURL::EmptyGURL(), task_id_, MakeBrowserTrackUUID(task_id_),
          "MaybeDelayForLcp", {});
      State next_state = State::kDone;
      if (GetLcpDelay().is_positive()) {
        // Conservatively, only apply delay if we get a clear signal that LCP
        // has not yet occurred on a trackable webpage. This avoids adding
        // unnecessary delays on pages where LCP is not applicable or
        // `PageLoadMetricsObserver is not in a valid state to be queried.
        if (auto* metrics_observer =
                page_load_metrics::MetricsWebContentsObserver::FromWebContents(
                    web_contents())) {
          if (const page_load_metrics::PageLoadMetricsObserverDelegate*
                  delegate =
                      metrics_observer->GetDelegateForCommittedLoadOrNull()) {
            const page_load_metrics::ContentfulPaintTimingInfo& lcp =
                delegate->GetLargestContentfulPaintHandler()
                    .MergeMainFrameAndSubframes();
            if (!lcp.ContainsValidTime()) {
              next_state = State::kDelayForLcp;
            }
          }
        }
      }
      // Posted so that this state transition is consistently async.
      PostMoveToStateClosure(next_state).Run();
      break;
    }
    case State::kDelayForLcp: {
      PostMoveToStateClosure(State::kDone, GetLcpDelay()).Run();
      break;
    }
    case State::kPageNavigated: {
      result_ = Result::kPageNavigated;
      MoveToState(State::kDone);
      break;
    }
    case State::kDidTimeout: {
      MoveToState(State::kDone);
      break;
    }
    case State::kDone: {
      // The state machine is never entered until Wait is called so a callback
      // must be provided.
      CHECK(ready_callback_);
      wait_journal_entry_.reset();
      page_stability_monitor_remote_.reset();
      PostFinishedTask(
          base::BindOnce([](ReadyCallback callback,
                            Result result) { std::move(callback).Run(result); },
                         std::move(ready_callback_), result_));
      break;
    }
  }
}

std::ostream& operator<<(std::ostream& o,
                         const ObservationDelayController::State& state) {
  return o << ObservationDelayController::StateToString(state);
}

void ObservationDelayController::OnVisualStateUpdated(bool) {
  if (state_ != State::kWaitForVisualStateUpdate) {
    return;
  }

  CHECK(metrics_);
  metrics_->OnVisualStateUpdated();

  // Posted so that this state transition is consistently async.
  PostMoveToStateClosure(State::kMaybeDelayForLcp).Run();
}

void ObservationDelayController::DCheckStateTransition(State old_state,
                                                       State new_state) {
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<State>> transitions(
      base::StateTransitions<State>({
          // clang-format off
          {State::kInitial,
              {State::kWaitForPageStability,
               State::kWaitForLoadCompletion}},
          {State::kWaitForPageStability,
              {State::kWaitForLoadCompletion,
               State::kPageStabilityMonitorDisconnected,
               State::kDidTimeout,
              State::kPageNavigated}},
          {State::kPageStabilityMonitorDisconnected,
              {State::kWaitForLoadCompletion}},
          {State::kWaitForLoadCompletion,
              {State::kDidTimeout,
               State::kPageNavigated,
               State::kWaitForVisualStateUpdate}},
          {State::kWaitForVisualStateUpdate,
              {State::kDidTimeout,
               State::kPageNavigated,
               State::kMaybeDelayForLcp}},
          {State::kMaybeDelayForLcp,
              {State::kDidTimeout,
               State::kPageNavigated,
               State::kDelayForLcp,
               State::kDone}},
          {State::kDelayForLcp,
              {State::kDidTimeout,
               State::kPageNavigated,
               State::kDone}},
          {State::kDidTimeout,
              {State::kDone}},
          {State::kPageNavigated,
              {State::kDone}}
          // clang-format on
      }));
  DCHECK_STATE_TRANSITION(transitions, old_state, new_state);
#endif  // DCHECK_IS_ON()
}

void ObservationDelayController::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument() ||
      !navigation_handle->IsInPrimaryMainFrame() || state_ == State::kInitial) {
    return;
  }
  if (!base::FeatureList::IsEnabled(
          kActorRestartObservationDelayControllerOnNavigate)) {
    return;
  }
  // If we exceed the number of navigations just keep waiting for observations.
  if (navigation_count_ >= kMaxNavigations) {
    return;
  }
  MoveToState(State::kPageNavigated);
}

void ObservationDelayController::DidStopLoading() {
  if (state_ != State::kWaitForLoadCompletion) {
    return;
  }

  CHECK(metrics_);
  metrics_->OnLoadCompleted();

  MoveToState(State::kWaitForVisualStateUpdate);
}

void ObservationDelayController::SetState(State state) {
  state_ = state;
}

std::string_view ObservationDelayController::StateToString(State state) {
  switch (state) {
    case State::kInitial:
      return "Initial";
    case State::kWaitForPageStability:
      return "WaitForPageStability";
    case State::kPageStabilityMonitorDisconnected:
      return "PageStabilityMonitorDisconnected";
    case State::kWaitForLoadCompletion:
      return "WaitForLoadCompletion";
    case State::kWaitForVisualStateUpdate:
      return "WaitForVisualStateUpdate";
    case State::kMaybeDelayForLcp:
      return "MaybeDelayForLcp";
    case State::kDelayForLcp:
      return "DelayForLcp";
    case State::kDidTimeout:
      return "DidTimeout";
    case State::kPageNavigated:
      return "PageNavigated";
    case State::kDone:
      return "Done";
  }
  NOTREACHED();
}

base::OnceClosure ObservationDelayController::MoveToStateClosure(
    State new_state) {
  return base::BindOnce(&ObservationDelayController::MoveToState,
                        weak_ptr_factory_.GetWeakPtr(), new_state);
}

base::OnceClosure ObservationDelayController::PostMoveToStateClosure(
    State new_state,
    base::TimeDelta delay) {
  return base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> task_runner,
         base::OnceClosure task, base::TimeDelta delay) {
        task_runner->PostDelayedTask(FROM_HERE, std::move(task), delay);
      },
      base::SequencedTaskRunner::GetCurrentDefault(),
      MoveToStateClosure(new_state), delay);
}

size_t ObservationDelayController::NavigationCount() const {
  return navigation_count_;
}

void ObservationDelayController::SetNavigationCount(size_t count) {
  navigation_count_ = count;
}

}  // namespace actor
