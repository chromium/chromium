// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/observation_delay_controller.h"

#include <optional>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/state_transitions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor.mojom-data-view.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
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

}  // namespace

ObservationDelayController::ObservationDelayController(
    content::RenderFrameHost& target_frame,
    TaskId task_id,
    AggregatedJournal& journal,
    std::optional<PageStabilityConfig> page_stability_config)
    : content::WebContentsObserver(
          WebContents::FromRenderFrameHost(&target_frame)),
      journal_(journal),
      task_id_(task_id) {
  CHECK(web_contents());
  const bool waits_for_page_stability = page_stability_config.has_value();

  journal.Log(GURL::EmptyGURL(), task_id, mojom::JournalTrack::kActor,
              "ObservationDelay: Created",
              JournalDetailsBuilder()
                  .Add("May Use PageStability", waits_for_page_stability)
                  .Build());

  if (waits_for_page_stability) {
    CHECK_NE(features::kActorGeneralPageStabilityMode.Get(),
             features::ActorGeneralPageStabilityMode::kDisabled);
    journal.EnsureJournalBound(target_frame);

    // Note: It's important that the PageStabilityMonitor be created on the same
    // interface as tool invocation since it relies on being created before a
    // tool is invoked.
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame;
    target_frame.GetRemoteAssociatedInterfaces()->GetInterface(
        &chrome_render_frame);

    chrome_render_frame->CreatePageStabilityMonitor(
        page_stability_monitor_remote_.BindNewPipeAndPassReceiver(), task_id,
        page_stability_config->supports_paint_stability);
    page_stability_monitor_remote_.set_disconnect_handler(
        base::BindOnce(&ObservationDelayController::OnMonitorDisconnected,
                       base::Unretained(this)));
    page_stability_start_delay_ = page_stability_config->start_delay;
  }
}

ObservationDelayController::ObservationDelayController(
    TaskId task_id,
    AggregatedJournal& journal)
    : journal_(journal), task_id_(task_id) {
  journal.Log(
      GURL::EmptyGURL(), task_id, mojom::JournalTrack::kActor,
      "ObservationDelay: Created",
      JournalDetailsBuilder().Add("May Use PageStability", false).Build());
}

ObservationDelayController::~ObservationDelayController() = default;

void ObservationDelayController::Wait(tabs::TabInterface& target_tab,
                                      ReadyCallback callback) {
  ready_callback_ = std::move(callback);

  WebContentsObserver::Observe(target_tab.GetContents());

  wait_journal_entry_ = journal_->CreatePendingAsyncEntry(
      GURL::EmptyGURL(), task_id_, mojom::JournalTrack::kActor,
      "ObservationDelay: Wait", {});

  PostMoveToStateClosure(State::kDidTimeout, GetCompletionTimeout()).Run();

  if (page_stability_monitor_remote_.is_bound()) {
    MoveToState(State::kWaitForPageStability);
  } else {
    MoveToState(State::kWaitForLoadCompletion);
  }
}

void ObservationDelayController::OnMonitorDisconnected() {
  page_stability_monitor_remote_.reset();

  if (state_ == State::kInitial) {
    // If Wait hasn't been called, don't enter the state machine yet. Resetting
    // the remote will skip the page stability state.
    journal_->Log(GURL::EmptyGURL(), task_id_, mojom::JournalTrack::kActor,
                  "ObservationDelay: Monitor Disconnect Before Wait", {});
    return;
  }

  MoveToState(State::kPageStabilityMonitorDisconnected);
}

void ObservationDelayController::MoveToState(State new_state) {
  if (state_ == State::kDone) {
    return;
  }

  DCheckStateTransition(state_, new_state);

  inner_journal_entry_.reset();
  journal_->Log(GURL::EmptyGURL(), task_id_, mojom::JournalTrack::kActor,
                "ObservationDelay: State Change",
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
          MoveToStateClosure(State::kWaitForLoadCompletion));
      break;
    }
    case State::kPageStabilityMonitorDisconnected: {
      MoveToState(State::kWaitForLoadCompletion);
      break;
    }
    case State::kWaitForLoadCompletion: {
      inner_journal_entry_ = journal_->CreatePendingAsyncEntry(
          GURL::EmptyGURL(), task_id_, mojom::JournalTrack::kActor,
          "WaitForLoadCompletion", {});
      page_stability_monitor_remote_.reset();

      if (web_contents()->IsLoading()) {
        // State will advance from DidStopLoading in this case.
        break;
      }

      // Posted so that this state transition is consistently async.
      PostMoveToStateClosure(State::kWaitForVisualStateUpdate).Run();
      break;
    }
    case State::kWaitForVisualStateUpdate: {
      inner_journal_entry_ = journal_->CreatePendingAsyncEntry(
          GURL::EmptyGURL(), task_id_, mojom::JournalTrack::kActor,
          "WaitForVisualStateUpdate", {});
      // Adapt since InsertVisualStateCallback takes a bool-taking callback.
      auto callback =
          base::BindOnce([](base::OnceClosure post_move_to_done,
                            bool) { std::move(post_move_to_done).Run(); },
                         PostMoveToStateClosure(State::kDone));

      // TODO(crbug.com/414662842): This should probably ensure an update from
      // all/selected OOPIFS?
      web_contents()->GetPrimaryMainFrame()->InsertVisualStateCallback(
          std::move(callback));
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
      PostFinishedTask(std::move(ready_callback_));
      break;
    }
  }
}

std::ostream& operator<<(std::ostream& o,
                         const ObservationDelayController::State& state) {
  return o << ObservationDelayController::StateToString(state);
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
               State::kDidTimeout}},
          {State::kPageStabilityMonitorDisconnected,
              {State::kWaitForLoadCompletion}},
          {State::kWaitForLoadCompletion,
              {State::kDidTimeout,
               State::kWaitForVisualStateUpdate}},
          {State::kWaitForVisualStateUpdate,
              {State::kDidTimeout,
               State::kDone}},
          {State::kDidTimeout,
              {State::kDone}}
          // clang-format on
      }));
  DCHECK_STATE_TRANSITION(transitions, old_state, new_state);
#endif  // DCHECK_IS_ON()
}

void ObservationDelayController::DidStopLoading() {
  if (state_ != State::kWaitForLoadCompletion) {
    return;
  }

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
    case State::kDidTimeout:
      return "DidTimeout";
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

}  // namespace actor
