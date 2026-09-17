// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_view_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ttc/core/session_view_delegate.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"
#include "chrome/browser/ui/views/dictation/ui_state.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "ui/views/view.h"

namespace ttc {

SessionViewImpl::SessionViewImpl(SessionViewDelegate& delegate)
    : delegate_(delegate) {
  CreateVoicePlateUi();
}

SessionViewImpl::~SessionViewImpl() = default;

void SessionViewImpl::UpdateAudioLevel(float audio_level) {
  if (voice_plate_) {
    voice_plate_->UpdateAudioLevel(audio_level);
  }
}

void SessionViewImpl::OnSessionInitialized() {
  if (voice_plate_) {
    voice_plate_->SetState(dictation::UiState::kTranscribing);
  }
}

void SessionViewImpl::OnSessionEnded() {
  voice_plate_.reset();
}

void SessionViewImpl::CreateVoicePlateUi() {
  BrowserWindowInterface* window = delegate_->GetBrowserWindowInterface();
  if (!window) {
    return;
  }

  // TODO(b/555790343): Move when active window changes.
  auto* browser_elements = BrowserElementsViews::From(window);
  views::View* anchor_view =
      browser_elements ? browser_elements->GetView(kTopContainerElementId)
                       : nullptr;
  if (!anchor_view) {
    return;
  }

  voice_plate_ = std::make_unique<dictation::DictationBubbleUi>(
      anchor_view,
      base::BindRepeating(&SessionViewImpl::OnVoicePlateCloseClicked,
                          base::Unretained(this)),
      /*toggle_active_stream_callback=*/base::DoNothing());
  voice_plate_->SetState(dictation::UiState::kInitializing);
  voice_plate_->Show();
}

void SessionViewImpl::OnVoicePlateCloseClicked() {
  delegate_->EndSessionAsync();
}

}  // namespace ttc
