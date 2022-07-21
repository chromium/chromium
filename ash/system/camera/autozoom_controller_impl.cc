// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/autozoom_controller_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

namespace ash {

AutozoomControllerImpl::AutozoomControllerImpl() {
  Shell::Get()->session_controller()->AddObserver(this);
}

AutozoomControllerImpl::~AutozoomControllerImpl() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

cros::mojom::CameraAutoFramingState AutozoomControllerImpl::GetState() {
  return state_;
}

void AutozoomControllerImpl::SetState(
    cros::mojom::CameraAutoFramingState state) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetInteger(prefs::kAutozoomState,
                                          static_cast<int32_t>(state));
  }
}

void AutozoomControllerImpl::Toggle() {
  SetState(state_ == cros::mojom::CameraAutoFramingState::OFF
               ? cros::mojom::CameraAutoFramingState::ON_SINGLE
               : cros::mojom::CameraAutoFramingState::OFF);
}

void AutozoomControllerImpl::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (pref_service == active_user_pref_service_)
    return;

  // Initial login and user switching in multi profiles.
  active_user_pref_service_ = pref_service;
  InitFromUserPrefs();
}

void AutozoomControllerImpl::OnStatePrefChanged() {
  Refresh();
}

void AutozoomControllerImpl::Refresh() {
  if (active_user_pref_service_) {
    state_ = static_cast<cros::mojom::CameraAutoFramingState>(
        active_user_pref_service_->GetInteger(prefs::kAutozoomState));
  } else {
    state_ = cros::mojom::CameraAutoFramingState::OFF;
  }

  auto* camera_hal_dispatcher = media::CameraHalDispatcherImpl::GetInstance();
  if (camera_hal_dispatcher) {
    camera_hal_dispatcher->SetAutoFramingState(GetState());
  }
}

void AutozoomControllerImpl::StartWatchingPrefsChanges() {
  DCHECK(active_user_pref_service_);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(active_user_pref_service_);
  pref_change_registrar_->Add(
      prefs::kAutozoomState,
      base::BindRepeating(&AutozoomControllerImpl::OnStatePrefChanged,
                          base::Unretained(this)));
}

void AutozoomControllerImpl::InitFromUserPrefs() {
  StartWatchingPrefsChanges();
  Refresh();
}

// static
void AutozoomControllerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kAutozoomState,
      static_cast<int32_t>(cros::mojom::CameraAutoFramingState::OFF));
}

}  // namespace ash
