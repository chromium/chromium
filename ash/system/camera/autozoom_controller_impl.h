// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAMERA_AUTOZOOM_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_CAMERA_AUTOZOOM_CONTROLLER_IMPL_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"

#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/camera/autozoom_nudge_controller.h"
#include "ash/system/camera/autozoom_observer.h"
#include "base/observer_list.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"

namespace ash {

// Controls the Autozoom feature that, when enabled, intelligently
// pans/tilts/zooms the camera to frame a set of regions of interest captured
// by the camera.
class ASH_EXPORT AutozoomControllerImpl
    : public SessionObserver,
      public media::CameraActiveClientObserver {
 public:
  AutozoomControllerImpl();

  AutozoomControllerImpl(const AutozoomControllerImpl&) = delete;
  AutozoomControllerImpl& operator=(const AutozoomControllerImpl&) = delete;

  ~AutozoomControllerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void SetState(cros::mojom::CameraAutoFramingState state);

  cros::mojom::CameraAutoFramingState GetState();

  void Toggle();

  void AddObserver(AutozoomObserver* observer);
  void RemoveObserver(AutozoomObserver* observer);

  bool IsAutozoomControlEnabled();

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  void set_autozoom_supported_for_test(bool value) {
    autozoom_supported_for_test_ = value;
  }

 private:
  friend class CameraEffectsControllerTest;

  void InitFromUserPrefs();

  void Refresh();

  void StartWatchingPrefsChanges();

  // Called when the user pref for the enabled status of Autozoom is changed.
  void OnStatePrefChanged();

  void SetAutozoomSupported(bool autozoom_supported);

  // CameraActiveClientObserver
  void OnActiveClientChange(
      cros::mojom::CameraClientType type,
      bool is_new_active_client,
      const base::flat_set<std::string>& active_device_ids) override;

  // The pref service of the currently active user. Can be null in
  // ash_unittests.
  raw_ptr<PrefService> active_user_pref_service_ = nullptr;

  // The registrar used to watch Autozoom prefs changes in the above
  // |active_user_pref_service_| from outside ash.
  // NOTE: Prefs are how Chrome communicates changes to the Autozoom settings
  // controlled by this class.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  cros::mojom::CameraAutoFramingState state_ =
      cros::mojom::CameraAutoFramingState::OFF;

  base::ObserverList<AutozoomObserver> observers_;

  std::unique_ptr<AutozoomNudgeController> nudge_controller_;

  bool autozoom_supported_ = false;

  // The number of current active camera clients. Autozoom control should only
  // be shown when there's at least one active camera client.
  int active_camera_client_count_ = 0;

  // Allows tests to force autozoom support.
  bool autozoom_supported_for_test_ = false;

  // All methods of this class should be run on the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AutozoomControllerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAMERA_AUTOZOOM_CONTROLLER_IMPL_H_
