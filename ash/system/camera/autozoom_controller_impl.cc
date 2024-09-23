// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/autozoom_controller_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

namespace ash {

AutozoomControllerImpl::AutozoomControllerImpl()
    : nudge_controller_(std::make_unique<AutozoomNudgeController>(this)) {
  auto* camera_hal_dispatcher = media::CameraHalDispatcherImpl::GetInstance();
  if (camera_hal_dispatcher) {
    camera_hal_dispatcher->GetAutoFramingSupported(
        base::BindOnce(&AutozoomControllerImpl::SetAutozoomSupported,
                       weak_ptr_factory_.GetWeakPtr()));
    camera_hal_dispatcher->AddActiveClientObserver(this);
  }

  Shell::Get()->session_controller()->AddObserver(this);
}

AutozoomControllerImpl::~AutozoomControllerImpl() {
  Shell::Get()->session_controller()->RemoveObserver(this);

  auto* camera_hal_dispatcher = media::CameraHalDispatcherImpl::GetInstance();
  if (camera_hal_dispatcher) {
    camera_hal_dispatcher->RemoveActiveClientObserver(this);
  }
}

bool AutozoomControllerImpl::IsAutozoomControlEnabled() {
  if (autozoom_supported_for_test_) {
    return true;
  }
  // TODO(b/264472916): Add simon vs. non-simon logic here.
  return autozoom_supported_ && active_camera_client_count_ > 0;
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

void AutozoomControllerImpl::SetAutozoomSupported(bool autozoom_supported) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool orig_control_enabled = IsAutozoomControlEnabled();
  autozoom_supported_ = autozoom_supported;

  bool control_enabled = IsAutozoomControlEnabled();
  if (control_enabled != orig_control_enabled) {
    for (auto& observer : observers_)
      observer.OnAutozoomControlEnabledChanged(control_enabled);
  }
}

void AutozoomControllerImpl::Toggle() {
  SetState(state_ == cros::mojom::CameraAutoFramingState::OFF
               ? cros::mojom::CameraAutoFramingState::ON_SINGLE
               : cros::mojom::CameraAutoFramingState::OFF);
  AnchoredNudgeManager::Get()->MaybeRecordNudgeAction(
      NudgeCatalogName::kAutozoom);
}

void AutozoomControllerImpl::AddObserver(AutozoomObserver* observer) {
  observers_.AddObserver(observer);
}

void AutozoomControllerImpl::RemoveObserver(AutozoomObserver* observer) {
  observers_.RemoveObserver(observer);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (active_user_pref_service_) {
    state_ = static_cast<cros::mojom::CameraAutoFramingState>(
        active_user_pref_service_->GetInteger(prefs::kAutozoomState));
  } else {
    state_ = cros::mojom::CameraAutoFramingState::OFF;
  }

  auto* camera_hal_dispatcher = media::CameraHalDispatcherImpl::GetInstance();
  if (camera_hal_dispatcher) {
    camera_hal_dispatcher->SetAutoFramingState(state_);
  }

  for (auto& observer : observers_)
    observer.OnAutozoomStateChanged(state_);
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

void AutozoomControllerImpl::OnActiveClientChange(
    cros::mojom::CameraClientType type,
    bool is_new_active_client,
    const base::flat_set<std::string>& active_device_ids) {
  bool orig_control_enabled = IsAutozoomControlEnabled();
  if (is_new_active_client) {
    active_camera_client_count_++;
  } else if (active_device_ids.empty()) {
    DCHECK(active_camera_client_count_ > 0);
    active_camera_client_count_--;
  }

  bool control_enabled = IsAutozoomControlEnabled();
  if (control_enabled != orig_control_enabled) {
    for (auto& observer : observers_)
      observer.OnAutozoomControlEnabledChanged(control_enabled);
  }
}

// static
void AutozoomControllerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kAutozoomState,
      static_cast<int32_t>(cros::mojom::CameraAutoFramingState::OFF));
}

}  // namespace ash
