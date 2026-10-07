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
    : bridge_(TtcKeyedServiceAndroid::Get(
          CHECK_DEREF(TtcKeyedService::Get(delegate.GetProfile())))) {}

SessionViewAndroid::~SessionViewAndroid() = default;

void SessionViewAndroid::UpdateAudioLevel(float audio_level) {
  bridge_->OnAudioLevelChanged(audio_level);
}

void SessionViewAndroid::OnSessionInitialized() {
  bridge_->OnSessionInitialized();
}

void SessionViewAndroid::OnError(ErrorCode error) {
  bridge_->OnError(error);
}

}  // namespace ttc
