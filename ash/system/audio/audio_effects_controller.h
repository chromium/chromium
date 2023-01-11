// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_AUDIO_EFFECTS_CONTROLLER_H_
#define ASH_SYSTEM_AUDIO_AUDIO_EFFECTS_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_delegate.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class ASH_EXPORT AudioEffectsController : public VcEffectsDelegate,
                                          public SessionObserver {
 public:
  enum AudioEffectId {
    kNone = 0,
    kNoiseCancellation = 1,
    kLiveCaption = 2,
  };

  AudioEffectsController();

  AudioEffectsController(const AudioEffectsController&) = delete;
  AudioEffectsController& operator=(const AudioEffectsController&) = delete;

  ~AudioEffectsController() override;

  // Returns whether `effect_id` is supported. If passed an `effect_id` of
  // `AudioEffectId::kNone`, the function returns whether *any* effects are
  // supported.
  bool IsEffectSupported(AudioEffectId effect_id = AudioEffectId::kNone);

  // VcEffectsDelegate:
  absl::optional<int> GetEffectState(int effect_id) override;
  void OnEffectControlActivated(absl::optional<int> effect_id,
                                absl::optional<int> state) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

 private:
  // Construct effect for noise cancellation.
  void AddNoiseCancellationEffect();

  // Construct effect for live caption.
  void AddLiveCaptionEffect();

  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};

  base::WeakPtrFactory<AudioEffectsController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_AUDIO_EFFECTS_CONTROLLER_H_