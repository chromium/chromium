// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/session_controller.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/state_transitions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/logging.h"
#include "chrome/browser/dictation/metrics.h"
#include "chrome/browser/dictation/session_controller_delegate.h"
#include "chrome/browser/dictation/session_state.h"
#include "chrome/browser/dictation/session_ui.h"
#include "chrome/browser/dictation/stream_provider.h"
#include "chrome/browser/dictation/target.h"
#include "content/public/browser/global_dom_node_id.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace dictation {

SessionController::StreamEntry::StreamEntry(
    std::unique_ptr<Target> target,
    std::unique_ptr<StreamProvider> provider)
    : target(std::move(target)), provider(std::move(provider)) {}

SessionController::StreamEntry::~StreamEntry() = default;

SessionController::SessionController(SessionControllerDelegate& delegate)
    : delegate_(delegate) {
  VT_LOG(GetBrowserContext()) << "=== Session Started";
}

SessionController::~SessionController() {
  CHECK(state_ != SessionState::kInactive ||
        (!attached_stream_ && finalizing_streams_.empty()));
  ClearCurrentTarget();
  if (attached_stream_) {
    EndDictationStream(DictationStreamEndTrigger::kDestructor);
  }
  for (auto& entry : finalizing_streams_) {
    entry->provider->DetachTarget();
  }
  if (is_wake_lock_active_) {
    GetWakeLock()->CancelWakeLock();
    is_wake_lock_active_ = false;
  }
  VT_LOG(GetBrowserContext()) << "=== Session Ended";
}

void SessionController::ResetUi() {
  ui_ = delegate_->CreateUi(*this);
}

void SessionController::StartDictationStream(
    std::unique_ptr<Target> target,
    DictationStreamStartTrigger trigger,
    base::TimeTicks trigger_time) {
  CHECK(target);
  // TODO(b/525856380): Add support for "swapping in" a new stream. That is,
  // end the current stream and start a new one without entering the
  // finalization state which could flash states the UI.
  CHECK(state_ == SessionState::kInactive ||
        state_ == SessionState::kFinalizing);
  CHECK(!attached_stream_);

  if (is_shutting_down_) {
    VT_LOG(GetBrowserContext()) << "\tAborting session shutdown";
    is_shutting_down_ = false;
    auto_session_end_timer_.Stop();
  }

  ClearCurrentTarget();
  idle_target_.reset();

  last_used_target_id_ = target->GetGlobalDOMNodeId();
  if (last_used_target_id_) {
    VT_LOG(GetBrowserContext())
        << "Starting DictationStream on target " << *last_used_target_id_;
  } else {
    VT_LOG(GetBrowserContext()) << "Starting DictationStream on custom target";
  }

  RecordDictationStreamStartTrigger(trigger);
  stream_trigger_time_ = trigger_time;

  std::unique_ptr<StreamProvider> stream_provider =
      delegate_->CreateStreamProvider(*this);
  Target* const raw_target = target.get();
  attached_stream_ = std::make_unique<StreamEntry>(std::move(target),
                                                   std::move(stream_provider));

  current_target_ = raw_target;
  current_target_->SetDelegate(this);
  current_target_->SetStreamActive(true);

  MoveToState(SessionState::kStreamInitializing);

  if (ui_) {
    ui_->OnStartedStream(
        last_used_target_id_.value_or(content::GlobalDOMNodeId()));
  }

  attached_stream_->provider->BindToTargetAndConnect(*raw_target);
}

void SessionController::OnTargetRequestEndStream(
    DictationStreamEndTrigger trigger) {
  if (attached_stream_) {
    EndDictationStream(trigger);
  }
}

void SessionController::OnTargetRequestEndSession(
    DictationStreamEndTrigger trigger) {
  FinalizeAndShutdown(trigger);
}

void SessionController::OnTargetInvalidated() {
  if (ui_) {
    ui_->OnStopped();
  }
  EndSessionAsynchronously();
}

void SessionController::EndDictationStream(DictationStreamEndTrigger trigger) {
  VT_LOG(GetBrowserContext()) << __func__;
  CHECK(attached_stream_);
  CHECK(state_ == SessionState::kStreamInitializing ||
        state_ == SessionState::kTranscribing);
  is_shutting_down_ = true;
  attached_stream_->target->SetStreamActive(false);
  attached_stream_->provider->Stop(trigger);
  // TODO(b/525943882): Consider whether an initializing stream should be
  // immediately moved to deletion, rather than finalizing.
  finalizing_streams_.insert(std::move(attached_stream_));
  MoveToState(SessionState::kFinalizing);
}

void SessionController::UpdateAudioLevel(float audio_level) {
  if (ui_) {
    ui_->UpdateAudioLevel(audio_level);
  }
}

void SessionController::UiRequestEndSession() {
  if (attached_stream_) {
    EndDictationStream(DictationStreamEndTrigger::kCancelButton);
  }
  // EndSession will destroy `this` which owns other objects that call into here
  // so PostTask to avoid destroying objects in the callstack.
  EndSessionAsynchronously();
}

void SessionController::FinalizeAndShutdown(DictationStreamEndTrigger trigger) {
  VT_LOG(GetBrowserContext()) << __func__;
  is_shutting_down_ = true;
  if (attached_stream_) {
    EndDictationStream(trigger);
  } else if (state_ == SessionState::kInactive) {
    // EndSession will destroy `this` which owns other objects that call into
    // here so PostTask to avoid destroying objects in the callstack.
    EndSessionAsynchronously();
  }
}

SessionState SessionController::GetState() const {
  return state_;
}

void SessionController::HostTabDidClose() {
  // Intentionally end the session synchronously in this path to avoid dangling
  // pointers to deleted UI components.
  EndSessionSynchronously();
  // WARNING: `this` is deleted, do not add code below here.
}

void SessionController::DidUpdateStreamProviderState(
    StreamProvider& stream_provider,
    StreamProvider::StreamState old_state,
    StreamErrorReason reason) {
  using StreamState = StreamProvider::StreamState;

  const bool is_attached =
      attached_stream_ && attached_stream_->provider.get() == &stream_provider;
  const bool is_failure = stream_provider.GetState() == StreamState::kFailed;

  if (stream_provider.GetState() == StreamState::kComplete ||
      stream_provider.GetState() == StreamState::kFailed) {
    std::unique_ptr<StreamEntry> completed_entry;
    if (is_attached) {
      is_shutting_down_ = true;
      completed_entry = std::move(attached_stream_);
      completed_entry->target->SetStreamActive(false);
    } else {
      auto it = std::ranges::find_if(
          finalizing_streams_,
          [&stream_provider](const std::unique_ptr<StreamEntry>& entry) {
            return entry->provider.get() == &stream_provider;
          });
      if (it != finalizing_streams_.end()) {
        completed_entry = std::move(finalizing_streams_.extract(it).value());
      }
    }

    if (completed_entry) {
      completed_entry->provider->DetachTarget();
      to_delete_stream_providers_.insert(std::move(completed_entry->provider));
      if (current_target_ == completed_entry->target.get()) {
        // Keep the current target alive while `SessionController` is idle so
        // it can continue reporting target events (e.g. a second Escape press
        // or page navigation).
        idle_target_ = std::move(completed_entry->target);
      }
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&SessionController::PurgeToDeleteStreamProviders,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  // Update SessionState based on provider states.
  if (attached_stream_) {
    switch (attached_stream_->provider->GetState()) {
      case StreamState::kInitializing:
        // An initializing stream puts the controller into the initializing
        // state at creation time.
        CHECK_EQ(state_, SessionState::kStreamInitializing);
        break;
      case StreamState::kTranscribing:
        MoveToState(SessionState::kTranscribing);
        break;
      case StreamState::kFailed:
      case StreamState::kComplete:
        // Completed streams are detached above.
        NOTREACHED();
    }
  } else {
    if (!finalizing_streams_.empty()) {
      MoveToState(SessionState::kFinalizing);
    } else {
      MoveToState(SessionState::kInactive);
    }
  }

  if (is_failure && old_state != StreamState::kComplete) {
    const SessionUi::StreamType stream_type =
        is_attached ? SessionUi::StreamType::kAttached
                    : SessionUi::StreamType::kFinalizing;
    if (ui_) {
      ui_->OnError(stream_type, reason);
    }
  }
}

base::CallbackListSubscription
SessionController::AddSessionStateChangedCallback(
    SessionStateChangedCallback callback) {
  return session_state_changed_callback_list_.Add(std::move(callback));
}

void SessionController::FlushWakeLockForTesting() {
  if (wake_lock_) {
    wake_lock_.FlushForTesting();  // IN-TEST
  }
}

void SessionController::ClearCurrentTarget() {
  if (current_target_) {
    current_target_->SetStreamActive(false);
    current_target_->SetDelegate(nullptr);
    current_target_ = nullptr;
  }
}

void SessionController::MoveToState(SessionState new_state) {
  if (new_state == state_) {
    return;
  }

  VT_LOG(GetBrowserContext())
      << "SessionState: " << state_ << " --> " << new_state;

  using enum SessionState;
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<SessionState>>
      allowed_transitions(base::StateTransitions<SessionState>(
          {{kInactive, {kStreamInitializing, kEnded}},
           {kStreamInitializing, {kInactive, kTranscribing, kFinalizing}},
           {kTranscribing, {kInactive, kFinalizing}},
           {kFinalizing, {kInactive, kStreamInitializing, kEnded}}}));
  DCHECK_STATE_TRANSITION(allowed_transitions, /*old_state=*/state_,
                          /*new_state=*/new_state);
#endif  // DCHECK_IS_ON()
  const SessionState old_state = state_;
  state_ = new_state;

  UpdateWakeLock();

  if (old_state == kStreamInitializing) {
    const base::TimeDelta elapsed =
        base::TimeTicks::Now() - stream_trigger_time_;
    if (new_state == kTranscribing) {
      VT_LOG(GetBrowserContext())
          << "Stream start latency: " << elapsed.InMilliseconds() << "ms";
      RecordDictationStreamStartLatency(elapsed);
    } else {
      VT_LOG(GetBrowserContext()) << "Stream abandoned before listening after "
                                  << elapsed.InMilliseconds() << "ms";
      RecordDictationStreamAbandonedBeforeListening(elapsed);
    }
  }

  session_state_changed_callback_list_.Notify(new_state);

  if (state_ == SessionState::kInactive && is_shutting_down_) {
    auto_session_end_timer_.Start(
        FROM_HERE, kAutoSessionEndDelay.Get(),
        base::BindOnce(&SessionController::EndSessionSynchronously,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void SessionController::UpdateWakeLock() {
  bool should_hold_wake_lock = false;
  switch (state_) {
    case SessionState::kInactive:
    case SessionState::kEnded:
      should_hold_wake_lock = false;
      break;
    case SessionState::kStreamInitializing:
    case SessionState::kTranscribing:
    case SessionState::kFinalizing:
      should_hold_wake_lock = true;
      break;
  }
  if (should_hold_wake_lock && !is_wake_lock_active_) {
    GetWakeLock()->RequestWakeLock();
    is_wake_lock_active_ = true;
  } else if (!should_hold_wake_lock && is_wake_lock_active_) {
    GetWakeLock()->CancelWakeLock();
    is_wake_lock_active_ = false;
  }
}

device::mojom::WakeLock* SessionController::GetWakeLock() {
  // Lazy binding; does not reconnect after connection error.
  if (!wake_lock_) {
    mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
    delegate_->BindWakeLockProvider(
        wake_lock_provider.BindNewPipeAndPassReceiver());
    // Although we could attribute the lock to the associated WebContents, it
    // does not make a difference on any platform where this feature is enabled,
    // and so is omitted for simplicity.
    wake_lock_provider->GetWakeLockWithoutContext(
        device::mojom::WakeLockType::kPreventDisplaySleep,
        device::mojom::WakeLockReason::kOther, "Dictation",
        wake_lock_.BindNewPipeAndPassReceiver());
  }
  return wake_lock_.get();
}

void SessionController::EndSessionAsynchronously() {
  auto_session_end_timer_.Stop();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<SessionController> this_ptr) {
                       if (!this_ptr) {
                         return;
                       }
                       this_ptr->EndSessionSynchronously();
                       CHECK(!this_ptr);
                     },
                     weak_ptr_factory_.GetWeakPtr()));
}

void SessionController::EndSessionSynchronously() {
  auto_session_end_timer_.Stop();
  // Stop the stream now, as `kEnded` can't be left once entered.
  if (attached_stream_) {
    EndDictationStream(DictationStreamEndTrigger::kDestructor);
  }
  ClearCurrentTarget();
  MoveToState(SessionState::kEnded);
  delegate_->EndSession();
  // WARNING: `this` is deleted, do not add code below here.
}

void SessionController::PurgeToDeleteStreamProviders() {
  to_delete_stream_providers_.clear();
}

content::BrowserContext* SessionController::GetBrowserContext() const {
  return delegate_->GetBrowserContext();
}

}  // namespace dictation
