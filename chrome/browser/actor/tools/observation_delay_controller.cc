// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/observation_delay_controller.h"

#include <optional>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
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
      mojom::JournalTrack::kActor, "ObservationDelay",
      JournalDetailsBuilder()
          .Add("begin_state", StateToString(state_))
          .Build());

  ready_callback_ = std::move(callback);

  if (page_stability_monitor_remote_.is_bound()) {
    page_stability_monitor_remote_->NotifyWhenStable(
        page_stability_start_delay_,
        base::BindOnce(&ObservationDelayController::WaitForLoading,
                       base::Unretained(this)));

    journal_entry_->GetJournal().Log(
        GURL::EmptyGURL(), journal_entry_->GetTaskId(),
        mojom::JournalTrack::kActor, "ObservationDelay",
        JournalDetailsBuilder()
            .Add("state", "Waiting on page stability monitor")
            .Build());
  } else {
    WaitForLoading();
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

  WaitForLoading();
}

void ObservationDelayController::WaitForLoading() {
  journal_entry_->GetJournal().Log(
      GURL::EmptyGURL(), journal_entry_->GetTaskId(),
      mojom::JournalTrack::kActor, "ObservationDelay",
      JournalDetailsBuilder()
          .Add("wait_for_loading_state", StateToString(state_))
          .Build());

  page_stability_monitor_remote_.reset();

  switch (state_) {
    case State::kWaitingForLoadStart:
    case State::kWaitingForLoadStop:
    case State::kWaitingForVisualUpdate: {
      PostFinishedTask(base::BindOnce(&ObservationDelayController::Timeout,
                                      weak_ptr_factory_.GetWeakPtr()),
                       kCompletionTimeout);

      // If no navigating load was started, simply force and wait for a new
      // frame to be presented.
      if (state_ == State::kWaitingForLoadStart) {
        WaitForVisualStateUpdate();
      }
      break;
    }
    case State::kDone: {
      CHECK(ready_callback_);
      PostFinishedTask(std::move(ready_callback_));
      journal_entry_->EndEntry(
          JournalDetailsBuilder().Add("end_state", "Done").Build());
      break;
    }
  }
}

void ObservationDelayController::DidStartLoading() {
  if (state_ != State::kWaitingForLoadStart) {
    return;
  }

  state_ = State::kWaitingForLoadStop;
}

void ObservationDelayController::DidStopLoading() {
  if (state_ != State::kWaitingForLoadStop) {
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
  state_ = State::kWaitingForVisualUpdate;

  // TODO(crbug.com/414662842): This should probably ensure an update from
  // all/selected OOPIFS?
  web_contents()->GetPrimaryMainFrame()->InsertVisualStateCallback(
      base::BindOnce(&ObservationDelayController::VisualStateUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ObservationDelayController::VisualStateUpdated(bool /*success*/) {
  if (state_ != State::kWaitingForVisualUpdate) {
    return;
  }

  state_ = State::kDone;

  // It's possible the ready state has been reached before Wait has been
  // called or before page stabilized. In that case, the callback will be posted
  // when Wait is called and the page is stabilized.
  if (ready_callback_ && !page_stability_monitor_remote_.is_bound()) {
    PostFinishedTask(std::move(ready_callback_));
    journal_entry_->EndEntry(
        JournalDetailsBuilder().Add("end_state", "Visual Update").Build());
  }
}

void ObservationDelayController::Timeout() {
  state_ = State::kDone;
  if (ready_callback_) {
    PostFinishedTask(std::move(ready_callback_));
    journal_entry_->EndEntry(
        JournalDetailsBuilder().Add("end_state", "Timeout").Build());
  }
}

std::string_view ObservationDelayController::StateToString(State state) {
  switch (state) {
    case State::kWaitingForLoadStart:
      return "WaitLoadStart";
    case State::kWaitingForLoadStop:
      return "WaitLoadStop";
    case State::kWaitingForVisualUpdate:
      return "WaitVisualUpdate";
    case State::kDone:
      return "Done";
  }
  NOTREACHED();
}

}  // namespace actor
