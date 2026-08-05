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
#include "chrome/browser/dictation/logging.h"
#include "chrome/browser/dictation/metrics.h"
#include "chrome/browser/dictation/session_controller_delegate.h"
#include "chrome/browser/dictation/session_state.h"
#include "chrome/browser/dictation/session_ui.h"
#include "chrome/browser/dictation/stream_provider.h"
#include "chrome/browser/dictation/target.h"
#include "content/public/browser/editable_level.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/global_dom_node_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom.h"

namespace content {

// TODO(bokan): Move into global_dom_node_id.h
std::ostream& operator<<(std::ostream& os,
                         const content::GlobalDOMNodeId& target_id) {
  const content::RenderFrameHost* rfh =
      target_id.document.AsRenderFrameHostIfValid();

  return os << "GlobalDOMNodeId("
            << (rfh ? rfh->GetGlobalId() : content::GlobalRenderFrameHostId())
            << ", " << target_id.target_element_dom_id << ")";
}

}  // namespace content

namespace dictation {

SessionController::SessionController(SessionControllerDelegate& delegate)
    : delegate_(delegate) {
  VT_LOG() << "=== Session Started";
}

SessionController::~SessionController() {
  CHECK(state_ != SessionState::kInactive ||
        (!attached_stream_provider_ && finalizing_stream_providers_.empty()));
  if (attached_stream_provider_) {
    EndDictationStream();
  }
  VT_LOG() << "=== Session Ended";
}

void SessionController::ResetUi() {
  ui_ = delegate_->CreateUi(*this);
}

void SessionController::StartDictationStream(
    const TargetDetails& target_details,
    DictationStreamStartTrigger trigger) {
  VT_LOG() << "Starting DictationStream on target " << target_details.target_id;
  // TODO(b/525856380): Add support for "swapping in" a new stream. That is,
  // end the current stream and start a new one without entering the
  // finalization state which could flash states the UI.
  CHECK(state_ == SessionState::kInactive ||
        state_ == SessionState::kFinalizing);
  CHECK(!attached_stream_provider_);

  if (is_shutting_down_) {
    VT_LOG() << "\tAborting session shutdown";
    is_shutting_down_ = false;
  }

  Observe(content::WebContents::FromRenderFrameHost(
      target_details.target_id.document.AsRenderFrameHostIfValid()));

  RecordDictationStreamStartTrigger(trigger);

  std::unique_ptr<StreamProvider> stream_provider =
      delegate_->CreateStreamProvider(*this);
  stream_provider->BindToTargetAndConnect(
      std::make_unique<Target>(target_details));
  attached_stream_provider_ = std::move(stream_provider);

  last_used_target_details_ = target_details;

  MoveToState(SessionState::kStreamInitializing);

  if (ui_) {
    ui_->OnStartedStream(target_details.target_id);
  }
}

void SessionController::OnFocusChangedInPage(
    const content::FocusedNodeDetails& details) {
  if (attached_stream_provider_ && attached_stream_provider_->GetTarget()) {
    attached_stream_provider_->GetTarget()->OnFocusChanged(details);
  }
  for (auto& provider : finalizing_stream_providers_) {
    if (provider->GetTarget()) {
      provider->GetTarget()->OnFocusChanged(details);
    }
  }

  if (details.focus_type == blink::mojom::FocusType::kNone ||
      details.focus_type == blink::mojom::FocusType::kScript) {
    return;
  }

  if (attached_stream_provider_) {
    EndDictationStream();
  }

  if (details.editable_level == content::EditableLevel::kNotEditable ||
      !details.global_dom_node_id.document.AsRenderFrameHostIfValid()) {
    return;
  }

  content::GlobalDOMNodeId newly_focused_target_id = details.global_dom_node_id;

  const bool is_finalizing_for_same_element = std::ranges::any_of(
      finalizing_stream_providers_,
      [&newly_focused_target_id](
          const std::unique_ptr<StreamProvider>& provider) {
        CHECK(newly_focused_target_id.document.AsRenderFrameHostIfValid());
        return provider->GetTarget() &&
               provider->GetTarget()
                       ->global_dom_node_id()
                       .document.AsRenderFrameHostIfValid() ==
                   newly_focused_target_id.document
                       .AsRenderFrameHostIfValid() &&
               provider->GetTarget()
                       ->global_dom_node_id()
                       .target_element_dom_id ==
                   newly_focused_target_id.target_element_dom_id;
      });

  if (!is_finalizing_for_same_element && !is_shutting_down_) {
    StartDictationStream(
        TargetDetails(
            newly_focused_target_id,
            details.editable_level == content::EditableLevel::kRichlyEditable),
        DictationStreamStartTrigger::kFocusChange);
  }
}

void SessionController::PrimaryPageChanged(content::Page& page) {
  // Shut down the whole session immediately upon navigating away from the page
  // associated with the session (closes bubble and stops typing).
  if (ui_) {
    ui_->OnStopped();
  }
  EndSessionAsynchronously();
}

void SessionController::EndDictationStream() {
  VT_LOG() << __func__;
  CHECK(attached_stream_provider_);
  CHECK(state_ == SessionState::kStreamInitializing ||
        state_ == SessionState::kTranscribing);
  attached_stream_provider_->Stop();
  // TODO(b/525943882): Consider whether an initializing stream should be
  // immediately moved to deletion, rather than finalizing.
  finalizing_stream_providers_.insert(std::move(attached_stream_provider_));
  MoveToState(SessionState::kFinalizing);
}

void SessionController::UpdateAudioLevel(float audio_level) {
  if (ui_) {
    ui_->UpdateAudioLevel(audio_level);
  }
}

void SessionController::UiRequestEndSession() {
  // EndSession will destroy `this` which owns other objects that call into here
  // so PostTask to avoid destroying objects in the callstack.
  EndSessionAsynchronously();
}

void SessionController::UiRequestEndActiveStream() {
  EndDictationStream();
}

void SessionController::FinalizeAndShutdown() {
  VT_LOG() << __func__;
  is_shutting_down_ = true;
  if (attached_stream_provider_) {
    EndDictationStream();
  } else if (state_ == SessionState::kInactive) {
    // EndSession will destroy `this` which owns other objects that call into
    // here so PostTask to avoid destroying objects in the callstack.
    EndSessionAsynchronously();
  }
}

void SessionController::UiRequestStartStream() {
  CHECK(!attached_stream_provider_);
  CHECK_EQ(state_, SessionState::kInactive);
  CHECK(!is_shutting_down_);

  // A stream is always started when the session is created using an explicit
  // target. Starting from UI can only happen after that.
  CHECK(last_used_target_details_.has_value());

  StartDictationStream(*last_used_target_details_,
                       DictationStreamStartTrigger::kStartButton);
}

SessionState SessionController::GetState() const {
  return state_;
}

void SessionController::HostTabDidClose() {
  // Intentionally end the session synchronously in this path to avoid dangling
  // pointers to deleted UI components.
  delegate_->EndSession();
  // WARNING: `this` is deleted, do not add code below here.
}

void SessionController::DidUpdateStreamProviderState(
    StreamProvider& stream_provider,
    StreamProvider::StreamState old_state) {
  using StreamState = StreamProvider::StreamState;

  const bool is_attached = attached_stream_provider_.get() == &stream_provider;
  const bool is_failure = stream_provider.GetState() == StreamState::kFailed;

  if (stream_provider.GetState() == StreamState::kComplete ||
      stream_provider.GetState() == StreamState::kFailed) {
    std::unique_ptr<StreamProvider> provider_to_delete;
    if (is_attached) {
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
      case StreamState::kInitializing:
        // An initializing stream pust the controller into the initiailzing
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
    if (!finalizing_stream_providers_.empty()) {
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
      ui_->OnError(stream_type);
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

  VT_LOG() << "SessionState: " << state_ << " --> " << new_state;

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

  if (state_ == SessionState::kInactive && is_shutting_down_) {
    // EndSession destroys `this` so do this async so callers to MoveToState
    // don't have to avoid the UAF landmine.
    EndSessionAsynchronously();
  }
}

void SessionController::EndSessionAsynchronously() {
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

void SessionController::PurgeToDeleteStreamProviders() {
  to_delete_stream_providers_.clear();
}

}  // namespace dictation
