// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/android/session_view_android.h"

#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/android/ttc_keyed_service_android.h"
#include "chrome/browser/ttc/core/session_view_delegate.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"

namespace ttc {

SessionViewAndroid::SessionViewAndroid(SessionViewDelegate& delegate)
    : SessionView(delegate),
      bridge_(TtcKeyedServiceAndroid::Get(
          CHECK_DEREF(TtcKeyedService::Get(delegate.GetProfile())))) {}

SessionViewAndroid::~SessionViewAndroid() = default;

// TODO(danielmendz): We should probably change this and OnMicMutedChanged() to
// not override but instead react to the OnUiStateChanged event and forward
// that UiState to the bridge.
void SessionViewAndroid::OnSessionLifecycleChanged(SessionLifecycle lifecycle) {
  SessionView::OnSessionLifecycleChanged(lifecycle);
  bridge_->OnSessionLifecycleChanged(lifecycle);
}

void SessionViewAndroid::OnError(ErrorCode error) {
  bridge_->OnError(error);
}

void SessionViewAndroid::OnTranscriptionsUpdated(
    const std::string& user_transcript,
    const std::string& model_transcript) {
  bridge_->OnTranscriptionsUpdated(user_transcript, model_transcript);
}

void SessionViewAndroid::OnUserAudioLevelUpdate(float audio_level) {
  bridge_->OnAudioLevelChanged(audio_level);
}

void SessionViewAndroid::OnMicMutedChanged(bool muted) {
  SessionView::OnMicMutedChanged(muted);
  bridge_->OnMicMutedChanged(muted);
}

// Not part of the UiState, so forwarded as is.
void SessionViewAndroid::OnSpeakerChanged(Speaker speaker) {
  bridge_->OnSpeakerChanged(speaker);
}

}  // namespace ttc
