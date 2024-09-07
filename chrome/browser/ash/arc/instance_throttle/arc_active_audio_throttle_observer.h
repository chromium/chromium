// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_ACTIVE_AUDIO_THROTTLE_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_ACTIVE_AUDIO_THROTTLE_OBSERVER_H_

#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/throttle/throttle_observer.h"

namespace arc {

constexpr char kArcActiveAudioThrottleObserverName[] = "ArcActiveAudio";

// This class observes active audio streams from ARC and sets the state to
// active if there is an active audio stream.
class ArcActiveAudioThrottleObserver
    : public ash::ThrottleObserver,
      public ash::CrasAudioHandler::AudioObserver {
 public:
  ArcActiveAudioThrottleObserver();

  ArcActiveAudioThrottleObserver(const ArcActiveAudioThrottleObserver&) =
      delete;
  ArcActiveAudioThrottleObserver& operator=(
      const ArcActiveAudioThrottleObserver&) = delete;

  ~ArcActiveAudioThrottleObserver() override = default;

  // ash::ThrottleObserver:
  void StartObserving(content::BrowserContext* context,
                      const ObserverStateChangedCallback& callback) override;
  void StopObserving() override;

  // CrasAudioHandler::AudioObserver:
  void OnNumberOfArcStreamsChanged(int32_t num) override;

 private:
  void UpdateActive(int32_t num_arc_streams);
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_ACTIVE_AUDIO_THROTTLE_OBSERVER_H_
