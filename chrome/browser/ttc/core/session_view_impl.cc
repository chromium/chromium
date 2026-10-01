// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_view_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "chrome/browser/ttc/core/session_view_delegate.h"
#include "chrome/browser/ttc/core/voice_plate_controller.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/views/dictation/ui_state.h"

namespace ttc {

ToastId GetToastIdForError(ErrorCode error) {
  switch (error) {
    case ErrorCode::kUnknown:
    case ErrorCode::kRateLimited:
    case ErrorCode::kSafetyBlocked:
    case ErrorCode::kInternalBackendError:
    case ErrorCode::kSessionExpired:
    case ErrorCode::kOptimizationGuideUnavailable:
    case ErrorCode::kExecutionSessionCreationFailed:
    case ErrorCode::kAudioUnknownError:
      return ToastId::kTtcGenericError;

    case ErrorCode::kAudioNoMicrophoneDetected:
    case ErrorCode::kAudioMicrophoneInUse:
      return ToastId::kTtcNoMicrophoneError;
  }

  NOTREACHED();
}

SessionViewImpl::SessionViewImpl(SessionViewDelegate& delegate)
    : delegate_(delegate),
      voice_plate_controller_(std::make_unique<VoicePlateController>(
          *delegate.GetProfile(),
          base::BindRepeating(&SessionViewImpl::OnVoicePlateCloseClicked,
                              base::Unretained(this)))) {}

SessionViewImpl::~SessionViewImpl() = default;

void SessionViewImpl::UpdateAudioLevel(float audio_level) {
  if (voice_plate_controller_) {
    voice_plate_controller_->UpdateAudioLevel(audio_level);
  }
}

void SessionViewImpl::OnSessionInitialized() {
  if (voice_plate_controller_) {
    voice_plate_controller_->SetState(dictation::UiState::kTranscribing);
  }
}

void SessionViewImpl::OnError(ErrorCode error) {
  if (ToastController* toast_controller =
          ToastController::From(delegate_->GetBrowserWindowInterface())) {
    toast_controller->MaybeShowToast(ToastParams(GetToastIdForError(error)));
  }
}

void SessionViewImpl::OnVoicePlateCloseClicked() {
  delegate_->EndSessionAsync();
}

}  // namespace ttc
