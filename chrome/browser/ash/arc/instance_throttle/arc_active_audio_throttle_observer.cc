// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_active_audio_throttle_observer.h"

#include "ash/public/cpp/app_types_util.h"

namespace arc {

ArcActiveAudioThrottleObserver::ArcActiveAudioThrottleObserver()
    : ThrottleObserver(kArcActiveAudioThrottleObserverName) {}

void ArcActiveAudioThrottleObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  ThrottleObserver::StartObserving(context, callback);
  // CrasAudioHandler can be null in unit tests
  if (ash::CrasAudioHandler::Get()) {
    ash::CrasAudioHandler::Get()->AddAudioObserver(this);
    UpdateActive(ash::CrasAudioHandler::Get()->NumberOfArcStreams());
  }
}

void ArcActiveAudioThrottleObserver::StopObserving() {
  // CrasAudioHandler can be null in unit tests
  if (ash::CrasAudioHandler::Get()) {
    ash::CrasAudioHandler::Get()->RemoveAudioObserver(this);
  }
  ThrottleObserver::StopObserving();
}

void ArcActiveAudioThrottleObserver::OnNumberOfArcStreamsChanged(int32_t num) {
  UpdateActive(num);
}

void ArcActiveAudioThrottleObserver::UpdateActive(int32_t num_arc_streams) {
  SetActive(num_arc_streams > 0);
}

}  // namespace arc
