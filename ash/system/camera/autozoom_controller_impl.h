// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAMERA_AUTOZOOM_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_CAMERA_AUTOZOOM_CONTROLLER_IMPL_H_

#include "ash/ash_export.h"

#include "ash/public/cpp/session/session_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"

namespace ash {

// Controls the Autozoom feature that, when enabled, intelligently
// pans/tilts/zooms the camera to frame a set of regions of interest captured
// by the camera.
class ASH_EXPORT AutozoomControllerImpl : public SessionObserver {
 public:
  AutozoomControllerImpl();

  AutozoomControllerImpl(const AutozoomControllerImpl&) = delete;
  AutozoomControllerImpl& operator=(const AutozoomControllerImpl&) = delete;

  ~AutozoomControllerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void SetState(cros::mojom::CameraAutoFramingState state);

  cros::mojom::CameraAutoFramingState GetState();

  void Toggle();

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

 private:
  void InitFromUserPrefs();

  void Refresh();

  void StartWatchingPrefsChanges();

  // Called when the user pref for the enabled status of Autozoom is changed.
  void OnStatePrefChanged();

  // The pref service of the currently active user. Can be null in
  // ash_unittests.
  PrefService* active_user_pref_service_ = nullptr;

  // The registrar used to watch Autozoom prefs changes in the above
  // |active_user_pref_service_| from outside ash.
  // NOTE: Prefs are how Chrome communicates changes to the Autozoom settings
  // controlled by this class.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  cros::mojom::CameraAutoFramingState state_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAMERA_AUTOZOOM_CONTROLLER_IMPL_H_
