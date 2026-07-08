// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/session_controller.h"

#include <algorithm>
#include <memory>
#include <ostream>

#include "base/containers/unique_ptr_adapters.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/state_transitions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/dictation/session_controller_delegate.h"
#include "chrome/browser/dictation/session_state.h"
#include "chrome/browser/dictation/session_ui.h"
#include "chrome/browser/dictation/stream_provider.h"
#include "chrome/browser/dictation/target.h"

namespace dictation {

SessionController::SessionController(SessionControllerDelegate& delegate)
    : delegate_(delegate) {}

SessionController::~SessionController() {
  CHECK(state_ != SessionState::kInactive ||
        (!attached_stream_provider_ && finalizing_stream_providers_.empty()));
  if (attached_stream_provider_) {
    EndDictationStream();
  }
}

void SessionController::Initialize() {
  ui_ = delegate_->CreateUi(*this);
}

void SessionController::StartDictationStream(const TargetId& target_id,
                                             const std::string& selected_text) {
  // TODO(b/525856380): Add support for "swapping in" a new stream. That is,
  // end the current stream and start a new one without entering the
  // finalization state which could flash states the UI.
  CHECK(state_ == SessionState::kInactive ||
        state_ == SessionState::kFinalizing);
  CHECK(!attached_stream_provider_);

  std::unique_ptr<StreamProvider> stream_provider =
      delegate_->CreateStreamProvider(*this);
  stream_provider->BindToTargetAndConnect(
      std::make_unique<Target>(target_id, selected_text));
  attached_stream_provider_ = std::move(stream_provider);

  last_used_target_id_ = target_id;

  MoveToState(SessionState::kStreamInitializing);
}

void SessionController::EndDictationStream() {
  CHECK(attached_stream_provider_);
  CHECK(state_ == SessionState::kStreamInitializing ||
        state_ == SessionState::kTranscribing);
  attached_stream_provider_->Stop();
  // TODO(b/525943882): Consider whether an initializing stream should be
  // immediately moved to deletion, rather than finalizing.
  finalizing_stream_providers_.insert(std::move(attached_stream_provider_));
  MoveToState(SessionState::kFinalizing);
}

void SessionController::UiRequestEndSession() {
  // EndSession will destroy `this` which owns other objects that call into here
  // so PostTask to avoid destroying objects in the callstack.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<SessionController> this_ptr) {
                       if (!this_ptr) {
                         return;
                       }
                       this_ptr->delegate_->EndSession();
                       CHECK(!this_ptr);
                     },
                     weak_ptr_factory_.GetWeakPtr()));
}

void SessionController::UiRequestEndActiveStream() {
  EndDictationStream();
}

void SessionController::UiRequestStartStream() {
  CHECK(!attached_stream_provider_);
  CHECK_EQ(state_, SessionState::kInactive);

  // A stream is always started when the session is created using an explicit
  // target. Starting from UI can only happen after that.
  CHECK(last_used_target_id_.has_value());

  // TODO(b/528720407): We have no good way to get the selected_text from here.
  // This will move to be collected with page context.
  StartDictationStream(*last_used_target_id_, /*selected_text=*/"");
}

SessionState SessionController::GetState() const {
  return state_;
}

void SessionController::DidUpdateStreamProviderState(
    StreamProvider& stream_provider,
    StreamProvider::StreamState old_state) {
  if (stream_provider.GetState() == StreamProvider::StreamState::kComplete ||
      stream_provider.GetState() == StreamProvider::StreamState::kFailed) {
    std::unique_ptr<StreamProvider> provider_to_delete;
    if (attached_stream_provider_.get() == &stream_provider) {
      provider_to_delete = std::move(attached_stream_provider_);
    } else {
      auto it = std::ranges::find_if(finalizing_stream_providers_,
                                     base::MatchesUniquePtr(&stream_provider));
      if (it != finalizing_stream_providers_.end()) {
        provider_to_delete =
            std::move(finalizing_stream_providers_.extract(it).value());
      }
    }

    if (provider_to_delete) {
      to_delete_stream_providers_.insert(std::move(provider_to_delete));
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&SessionController::PurgeToDeleteStreamProviders,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  // Update SessionState based on provider states.
  if (attached_stream_provider_) {
    switch (attached_stream_provider_->GetState()) {
      case StreamProvider::StreamState::kInitializing:
        // An initializing stream pust the controller into the initiailzing
        // state at creation time.
        CHECK_EQ(state_, SessionState::kStreamInitializing);
        break;
      case StreamProvider::StreamState::kTranscribing:
        MoveToState(SessionState::kTranscribing);
        break;
      case StreamProvider::StreamState::kFailed:
      case StreamProvider::StreamState::kComplete:
        // Completed streams are detached above.
        NOTREACHED();
    }
  } else {
    if (!finalizing_stream_providers_.empty()) {
      MoveToState(SessionState::kFinalizing);
    } else {
      MoveToState(SessionState::kInactive);
    }
  }
}

base::CallbackListSubscription
SessionController::AddSessionStateChangedCallback(
    SessionStateChangedCallback callback) {
  return session_state_changed_callback_list_.Add(std::move(callback));
}

void SessionController::MoveToState(SessionState new_state) {
  if (new_state == state_) {
    return;
  }

  using enum SessionState;
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<SessionState>>
      allowed_transitions(base::StateTransitions<SessionState>(
          {{kInactive, {kStreamInitializing}},
           {kStreamInitializing, {kInactive, kTranscribing, kFinalizing}},
           {kTranscribing, {kInactive, kFinalizing}},
           {kFinalizing, {kInactive, kStreamInitializing}}}));
  DCHECK_STATE_TRANSITION(allowed_transitions, /*old_state=*/state_,
                          /*new_state=*/new_state);
#endif  // DCHECK_IS_ON()
  state_ = new_state;
  session_state_changed_callback_list_.Notify(new_state);
}

void SessionController::PurgeToDeleteStreamProviders() {
  to_delete_stream_providers_.clear();
}

}  // namespace dictation
