// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_view.h"

#include "chrome/browser/ttc/core/session_view_delegate.h"

namespace ttc {

SessionView::SessionView(SessionViewDelegate& delegate) : delegate_(delegate) {}

SessionView::~SessionView() = default;

UiState SessionView::GetUiState() const {
  return ui_state_;
}

void SessionView::OnSessionLifecycleChanged(SessionLifecycle lifecycle) {
  UpdateUiState();
}

void SessionView::OnMicMutedChanged(bool muted) {
  UpdateUiState();
}

UiState SessionView::ComputeUiState() const {
  if (delegate_->GetSessionLifecycle() == SessionLifecycle::kInitializing) {
    return UiState::kInitializing;
  }
  return delegate_->IsMicMuted() ? UiState::kMicMuted : UiState::kLive;
}

void SessionView::UpdateUiState() {
  // The UI is torn down along with the session, so it keeps showing its last
  // state once the session has finished.
  if (delegate_->GetSessionLifecycle() == SessionLifecycle::kFinished) {
    return;
  }
  const UiState ui_state = ComputeUiState();
  if (ui_state == ui_state_) {
    return;
  }
  ui_state_ = ui_state;
  OnUiStateChanged(ui_state_);
}

}  // namespace ttc
