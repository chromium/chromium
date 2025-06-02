// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/observation_delay_controller.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_coordinator.h"
#include "chrome/browser/actor/tools/observation_delay_type.h"
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

ObservationDelayController::LoadWatcher::LoadWatcher() = default;
ObservationDelayController::LoadWatcher::~LoadWatcher() = default;

ObservationDelayController::ObservationDelayController(
    content::RenderFrameHost& target_frame,
    ObservationDelayType type)
    : content::WebContentsObserver(
          WebContents::FromRenderFrameHost(&target_frame)),
      observation_type_(type) {
  CHECK(web_contents());

  if (observation_type_ == ObservationDelayType::kWatchForLoad) {
    load_watcher_.emplace();
  }
}

ObservationDelayController::~ObservationDelayController() = default;

void ObservationDelayController::Wait(ReadyCallback callback) {
  switch (observation_type_) {
    case ObservationDelayType::kNone: {
      PostFinishedTask(std::move(callback));
      break;
    }
    case ObservationDelayType::kUseCompletionDelay: {
      // TODO(crbug.com/409564704): Delay the callback to give the page a chance
      // to react to the tool's effects. Temporary until we can do this more
      // reliably in the renderer. Once that's done, the renderer will delay
      // replying until the page is ready and PageTool should then start using
      // kWatchForLoad so that PageTools that cause a navigation (e.g. click on
      // a link) also make use of the browser-side load delay implemented here.
      PostFinishedTask(std::move(callback),
                       ActorCoordinator::GetActionObservationDelay());
      break;
    }
    case ObservationDelayType::kWatchForLoad: {
      CHECK(load_watcher_);
      switch (load_watcher_->state) {
        case LoadWatcher::State::kWaitingForLoadStart:
        case LoadWatcher::State::kWaitingForLoadStop:
        case LoadWatcher::State::kWaitingForVisualUpdate: {
          load_watcher_->ready_callback_ = std::move(callback);
          PostFinishedTask(base::BindOnce(&ObservationDelayController::Timeout,
                                          weak_ptr_factory_.GetWeakPtr()),
                           kCompletionTimeout);

          // If no navigating load was started, simply force and wait for a new
          // frame to be presented.
          if (load_watcher_->state ==
              LoadWatcher::State::kWaitingForLoadStart) {
            WaitForVisualStateUpdate();
          }
          break;
        }
        case LoadWatcher::State::kDone: {
          PostFinishedTask(std::move(callback));
          break;
        }
      }

      break;
    }
  }

  CHECK(callback.is_null());
}

void ObservationDelayController::DidStartLoading() {
  if (!load_watcher_ ||
      load_watcher_->state != LoadWatcher::State::kWaitingForLoadStart) {
    return;
  }

  load_watcher_->state = LoadWatcher::State::kWaitingForLoadStop;
}

void ObservationDelayController::DidStopLoading() {
  if (!load_watcher_ ||
      load_watcher_->state != LoadWatcher::State::kWaitingForLoadStop) {
    return;
  }

  WaitForVisualStateUpdate();
}

void ObservationDelayController::WaitForVisualStateUpdate() {
  CHECK(load_watcher_);
  load_watcher_->state = LoadWatcher::State::kWaitingForVisualUpdate;

  // TODO(crbug.com/414662842): This should probably ensure an update from
  // all/selected OOPIFS?
  web_contents()->GetPrimaryMainFrame()->InsertVisualStateCallback(
      base::BindOnce(&ObservationDelayController::VisualStateUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ObservationDelayController::VisualStateUpdated(bool /*success*/) {
  CHECK(load_watcher_);
  if (load_watcher_->state != LoadWatcher::State::kWaitingForVisualUpdate) {
    return;
  }

  load_watcher_->state = LoadWatcher::State::kDone;

  // It's possible the ready state has been reached before Wait has been
  // called. In that case, the callback will be posted when Wait is called.
  if (load_watcher_->ready_callback_) {
    PostFinishedTask(std::move(load_watcher_->ready_callback_));
  }
}

void ObservationDelayController::Timeout() {
  CHECK(load_watcher_);
  load_watcher_->state = LoadWatcher::State::kDone;
  if (load_watcher_->ready_callback_) {
    PostFinishedTask(std::move(load_watcher_->ready_callback_));
  }
}

}  // namespace actor
