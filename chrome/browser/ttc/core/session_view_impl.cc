// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_view_impl.h"

#include <memory>
#include <utility>

#include "base/notreached.h"
#include "chrome/browser/ttc/core/overlay_controller.h"
#include "chrome/browser/ttc/core/session_view_delegate.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"

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
    case ErrorCode::kSetupTimeout:
      return ToastId::kTtcGenericError;

    case ErrorCode::kAudioNoMicrophoneDetected:
    case ErrorCode::kAudioMicrophoneInUse:
      return ToastId::kTtcNoMicrophoneError;

    case ErrorCode::kInactivityTimeout:
      return ToastId::kTtcInactivityTimeout;
  }

  NOTREACHED();
}

SessionViewImpl::SessionViewImpl(SessionViewDelegate& delegate)
    : SessionView(delegate) {
  overlay_controller_ =
      std::make_unique<OverlayController>(*delegate.GetProfile(), *this);
}

SessionViewImpl::~SessionViewImpl() = default;

void SessionViewImpl::OnError(ErrorCode error) {
  if (ToastController* toast_controller =
          ToastController::From(delegate().GetBrowserWindowInterface())) {
    toast_controller->MaybeShowToast(ToastParams(GetToastIdForError(error)));
  }
}

void SessionViewImpl::OnUserAudioLevelUpdate(float audio_level) {
  audio_level_callbacks_.Notify(audio_level);
}

void SessionViewImpl::EndSessionAsync() {
  delegate().EndSessionAsync();
}

base::CallbackListSubscription SessionViewImpl::RegisterAudioLevelCallback(
    base::RepeatingCallback<void(float)> callback) {
  return audio_level_callbacks_.Add(std::move(callback));
}

void SessionViewImpl::OnUiStateChanged(UiState state) {
  overlay_controller_->ApplyUiState(state);
}

}  // namespace ttc
