// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/session_controller.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/dictation/session_controller_delegate.h"
#include "chrome/browser/dictation/session_ui.h"
#include "chrome/browser/dictation/stream_provider.h"

namespace dictation {

SessionController::SessionController(SessionControllerDelegate& delegate)
    : delegate_(delegate) {}

SessionController::~SessionController() {
  CHECK(state_ != State::kInactive || !attached_stream_provider_);
  if (state_ != State::kInactive) {
    EndDictationStream();
  }
}

void SessionController::Initialize() {
  ui_ = delegate_->CreateUi(*this);
}

void SessionController::StartDictationStream(Target& target) {
  CHECK_EQ(state_, State::kInactive);

  std::unique_ptr<StreamProvider> stream_provider =
      delegate_->CreateStreamProvider(*this);
  stream_provider->BindToTarget(target);
  attached_stream_provider_ = std::move(stream_provider);

  MoveToState(State::kStreamInitializing);
}

void SessionController::EndDictationStream() {
  CHECK_NE(state_, State::kInactive);
  attached_stream_provider_->Stop();
  attached_stream_provider_.reset();
  MoveToState(State::kInactive);
}

void SessionController::RequestEndSession() {
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

void SessionController::MoveToState(State new_state) {
  // TODO(bokan): use base::StateTransitions
  state_ = new_state;
}

}  // namespace dictation
