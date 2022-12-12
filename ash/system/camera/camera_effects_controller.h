// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAMERA_CAMERA_EFFECTS_CONTROLLER_H_
#define ASH_SYSTEM_CAMERA_CAMERA_EFFECTS_CONTROLLER_H_

#include <utility>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "media/capture/video/chromeos/mojom/effects_pipeline.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefRegistrySimple;
class PrefService;
class PrefChangeRegistrar;

namespace ash {

// CameraEffectsController is the interface for any object in ash to
// enable/change camera effects.
class ASH_EXPORT CameraEffectsController : public SessionObserver {
 public:
  // Observer that will be notified on camera effects change.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnCameraEffectsChanged(
        cros::mojom::EffectsConfigPtr new_effects) = 0;
  };

  CameraEffectsController();

  CameraEffectsController(const CameraEffectsController&) = delete;
  CameraEffectsController& operator=(const CameraEffectsController&) = delete;

  ~CameraEffectsController() override;

  // Returns whether a certain / any camera effects is supported.
  // IsCameraEffectsSupported(cros::mojom::CameraEffect::kBackgroundBlur)
  // returns whether background blur is supported. IsCameraEffectsSupported()
  // returns if any camera effects is supported.
  static bool IsCameraEffectsSupported(
      cros::mojom::CameraEffect effect = cros::mojom::CameraEffect::kNone);

  // Returns currently applied camera effects.
  // Should only be called after user logs in.
  cros::mojom::EffectsConfigPtr GetCameraEffects();

  // Adds/Removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Called inside ash/ash_prefs.cc to register related prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  void set_effect_result_for_testing(
      cros::mojom::SetEffectResult effect_result_for_testing) {
    effect_result_for_testing_ = effect_result_for_testing;
  }

 private:
  // This will be automatically called when any pref changes its value.
  void OnCameraEffectsPrefChanged(const std::string& pref_name);

  // SetCameraEffects camera effects with `config`.
  void SetCameraEffects(cros::mojom::EffectsConfigPtr config);

  // SetInitialCameraEffects tells the camera server what `config`
  // to use when it first registers.
  void SetInitialCameraEffects(cros::mojom::EffectsConfigPtr config);

  // Callback after SetCameraEffects. Based on the `result`, this function will
  // update/revert prefs.
  void OnNewCameraEffectsSet(cros::mojom::EffectsConfigPtr new_config,
                             cros::mojom::SetEffectResult result);

  // Constructs EffectsConfigPtr from prefs.
  cros::mojom::EffectsConfigPtr GetEffectsConfigFromPref();

  // Update prefs with the value in `config`.
  void SetEffectsConfigToPref(cros::mojom::EffectsConfigPtr config);

  // Used to bypass the CameraHalDispatcherImpl::SetCameraEffects for testing
  // purpose. The value will be null for non-testing cases; and not null in
  // testing cases.
  absl::optional<cros::mojom::SetEffectResult> effect_result_for_testing_;

  // Used for pref registration.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Records current effects that is applied to camera hal server.
  cros::mojom::EffectsConfigPtr current_effects_;

  base::ObserverList<Observer> observers_;

  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};

  base::WeakPtrFactory<CameraEffectsController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAMERA_CAMERA_EFFECTS_CONTROLLER_H_
