// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/observation_delay_controller.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

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
    content::RenderFrameHost& target_frame)
    : content::WebContentsObserver(
          WebContents::FromRenderFrameHost(&target_frame)) {
  CHECK(web_contents());
}

ObservationDelayController::~ObservationDelayController() = default;

void ObservationDelayController::Wait(
    AggregatedJournal::PendingAsyncEntry& parent_journal_entry,
    ReadyCallback callback) {
  journal_entry_ = parent_journal_entry.GetJournal().CreatePendingAsyncEntry(
      GURL::EmptyGURL(), parent_journal_entry.GetTaskId(), "ObservationDelay",
      StateToString(state_));

  switch (state_) {
    case State::kWaitingForLoadStart:
    case State::kWaitingForLoadStop:
    case State::kWaitingForVisualUpdate: {
      ready_callback_ = std::move(callback);
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
      PostFinishedTask(std::move(callback));
      journal_entry_->EndEntry("Done");
      break;
    }
  }

  CHECK(callback.is_null());
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
    journal_entry_->GetJournal().Log(GURL::EmptyGURL(),
                                     journal_entry_->GetTaskId(),
                                     "ObservationDelay", "Done Loading");
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
  // called. In that case, the callback will be posted when Wait is called.
  if (ready_callback_) {
    PostFinishedTask(std::move(ready_callback_));
    journal_entry_->EndEntry("Visual Update");
  }
}

void ObservationDelayController::Timeout() {
  state_ = State::kDone;
  if (ready_callback_) {
    PostFinishedTask(std::move(ready_callback_));
    journal_entry_->EndEntry("Timeout");
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
