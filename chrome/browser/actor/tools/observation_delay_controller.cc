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
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
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
// This timeout is long but based on the NavigationToLoadEventFired UMA. This
// should be tuned with real world usage.
constexpr base::TimeDelta kCompletionTimeout = base::Seconds(10);
}  // namespace

ObservationDelayController::ObservationDelayController(
    content::RenderFrameHost& target_frame,
    TaskId task_id,
    std::optional<PageStabilityConfig> page_stability_config)
    : content::WebContentsObserver(
          WebContents::FromRenderFrameHost(&target_frame)) {
  CHECK(web_contents());

  if (page_stability_config.has_value()) {
    CHECK_NE(features::kActorGeneralPageStabilityMode.Get(),
             features::ActorGeneralPageStabilityMode::kDisabled);

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

ObservationDelayController::~ObservationDelayController() = default;

void ObservationDelayController::Wait(
    AggregatedJournal::PendingAsyncEntry& parent_journal_entry,
    ReadyCallback callback) {
  journal_entry_ = parent_journal_entry.GetJournal().CreatePendingAsyncEntry(
      GURL::EmptyGURL(), parent_journal_entry.GetTaskId(),
      mojom::JournalTrack::kActor, "ObservationDelay", {});

  ready_callback_ = std::move(callback);

  if (page_stability_monitor_remote_.is_bound()) {
    MoveToState(State::kWaitForPageStability);
  } else {
    MoveToState(State::kWaitForLoadCompletion);
  }
}

void ObservationDelayController::OnMonitorDisconnected() {
  if (!page_stability_monitor_remote_.is_bound()) {
    return;
  }

  page_stability_monitor_remote_.reset();

  if (!ready_callback_) {
    return;
  }

  journal_entry_->GetJournal().Log(
      GURL::EmptyGURL(), journal_entry_->GetTaskId(),
      mojom::JournalTrack::kActor, "ObservationDelay",
      JournalDetailsBuilder()
          .Add("state", "Page stability monitor disconnected")
          .Build());

  MoveToState(State::kWaitForLoadCompletion);
}

void ObservationDelayController::MoveToState(State new_state) {
  DCheckStateTransition(state_, new_state);

  CHECK(journal_entry_);
  journal_entry_->GetJournal().Log(
      GURL::EmptyGURL(), journal_entry_->GetTaskId(),
      mojom::JournalTrack::kActor, "ObservationDelayState",
      JournalDetailsBuilder()
          .Add("old_state", StateToString(state_))
          .Add("new_state", StateToString(new_state))
          .Build());

  state_ = new_state;
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
    case State::kWaitForLoadCompletion: {
      page_stability_monitor_remote_.reset();

      if (load_state_ == LoadState::kDone) {
        MoveToState(State::kDone);
        break;
      }

      PostFinishedTask(base::BindOnce(&ObservationDelayController::Timeout,
                                      weak_ptr_factory_.GetWeakPtr()),
                       kCompletionTimeout);

      // If no navigating load was started, simply force and wait for a new
      // frame to be presented.
      if (load_state_ == LoadState::kWaitingForLoadStart) {
        WaitForVisualStateUpdate();
      }
      break;
    }
    case State::kDone: {
      CHECK(ready_callback_);
      PostFinishedTask(std::move(ready_callback_));
      break;
    }
  }
}

std::ostream& operator<<(std::ostream& o,
                         const ObservationDelayController::State& state) {
  return o << base::to_underlying(state);
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
              {State::kWaitForLoadCompletion}},
          {State::kWaitForLoadCompletion,
              {State::kDone}}
          // clang-format on
      }));
  DCHECK_STATE_TRANSITION(transitions, old_state, new_state);
#endif  // DCHECK_IS_ON()
}

void ObservationDelayController::DidStartLoading() {
  if (load_state_ != LoadState::kWaitingForLoadStart) {
    return;
  }

  load_state_ = LoadState::kWaitingForLoadStop;
}

void ObservationDelayController::DidStopLoading() {
  if (load_state_ != LoadState::kWaitingForLoadStop) {
    return;
  }

  // If we aren't waiting, then this new state will be logged when
  // we actually wait.
  if (journal_entry_) {
    journal_entry_->GetJournal().Log(
        GURL::EmptyGURL(), journal_entry_->GetTaskId(),
        mojom::JournalTrack::kActor, "ObservationDelay",
        JournalDetailsBuilder().Add("state", "Done loading").Build());
  }
  WaitForVisualStateUpdate();
}

void ObservationDelayController::WaitForVisualStateUpdate() {
  load_state_ = LoadState::kWaitingForVisualUpdate;

  // TODO(crbug.com/414662842): This should probably ensure an update from
  // all/selected OOPIFS?
  web_contents()->GetPrimaryMainFrame()->InsertVisualStateCallback(
      base::BindOnce(&ObservationDelayController::VisualStateUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ObservationDelayController::VisualStateUpdated(bool /*success*/) {
  if (load_state_ != LoadState::kWaitingForVisualUpdate) {
    return;
  }

  load_state_ = LoadState::kDone;

  // It's possible the ready state has been reached before Wait has been
  // called or before page stabilized. In that case, the callback will be posted
  // when Wait is called and the page is stabilized.
  if (ready_callback_ && !page_stability_monitor_remote_.is_bound()) {
    journal_entry_->EndEntry(
        JournalDetailsBuilder().Add("end_state", "Visual Update").Build());
    MoveToState(State::kDone);
  }
}

void ObservationDelayController::Timeout() {
  state_ = State::kDone;
  if (ready_callback_) {
    journal_entry_->EndEntry(
        JournalDetailsBuilder().Add("end_state", "Timeout").Build());
    MoveToState(State::kDone);
  }
}

std::string_view ObservationDelayController::StateToString(State state) {
  switch (state) {
    case State::kInitial:
      return "Initial";
    case State::kWaitForPageStability:
      return "WaitForPageStability";
    case State::kWaitForLoadCompletion:
      return "WaitForLoadCompletion";
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

}  // namespace actor
