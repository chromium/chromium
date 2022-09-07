// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_power_controller.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"

namespace ash {

// The delay between bluez started and bluez power initialized. This number is
// determined empirically: most of the time bluez takes less than 200 ms to
// initialize power, so taking 1000 ms has enough time buffer for worst cases.
const int kBluetoothInitializationDelay = 1000;

BluetoothPowerController::BluetoothPowerController(PrefService* local_state)
    : local_state_(local_state) {
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&BluetoothPowerController::InitializeOnAdapterReady,
                     weak_ptr_factory_.GetWeakPtr()));
  Shell::Get()->session_controller()->AddObserver(this);

  // AppLaunchTest.TestQuickLaunch fails under target=linux due to
  // |local_state_| being nullptr.
  if (local_state_) {
    StartWatchingLocalStatePrefsChanges();

    DCHECK(!Shell::Get()->session_controller()->IsActiveUserSessionStarted());
    // Apply the local state pref since no user has logged in (still in login
    // screen).
    ApplyBluetoothLocalStatePref();
  }
}

BluetoothPowerController::~BluetoothPowerController() {
  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void BluetoothPowerController::SetBluetoothEnabled(bool enabled) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetBoolean(prefs::kUserBluetoothAdapterEnabled,
                                          enabled);
  } else if (local_state_) {
    local_state_->SetBoolean(prefs::kSystemBluetoothAdapterEnabled, enabled);
  } else {
    DLOG(ERROR)
        << "active user and local state pref service cannot both be null";
  }
}

// static
void BluetoothPowerController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kSystemBluetoothAdapterEnabled, false);
}

// static
void BluetoothPowerController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kUserBluetoothAdapterEnabled, false);
}

void BluetoothPowerController::StartWatchingActiveUserPrefsChanges() {
  DCHECK(active_user_pref_service_);
  DCHECK(Shell::Get()->session_controller()->IsUserPrimary());

  active_user_pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  active_user_pref_change_registrar_->Init(active_user_pref_service_);
  active_user_pref_change_registrar_->Add(
      prefs::kUserBluetoothAdapterEnabled,
      base::BindRepeating(
          &BluetoothPowerController::OnBluetoothPowerActiveUserPrefChanged,
          base::Unretained(this)));
}

void BluetoothPowerController::StartWatchingLocalStatePrefsChanges() {
  DCHECK(local_state_);

  local_state_pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  local_state_pref_change_registrar_->Init(local_state_);
  local_state_pref_change_registrar_->Add(
      prefs::kSystemBluetoothAdapterEnabled,
      base::BindRepeating(
          &BluetoothPowerController::OnBluetoothPowerLocalStatePrefChanged,
          base::Unretained(this)));
}

void BluetoothPowerController::StopWatchingActiveUserPrefsChanges() {
  active_user_pref_change_registrar_.reset();
}

void BluetoothPowerController::OnBluetoothPowerActiveUserPrefChanged() {
  DCHECK(active_user_pref_service_);
  BLUETOOTH_LOG(EVENT) << "Active user bluetooth power pref changed";
  SetBluetoothPower(active_user_pref_service_->GetBoolean(
      prefs::kUserBluetoothAdapterEnabled));
}

void BluetoothPowerController::OnBluetoothPowerLocalStatePrefChanged() {
  DCHECK(local_state_);
  BLUETOOTH_LOG(EVENT) << "Local state bluetooth power pref changed";
  SetBluetoothPower(
      local_state_->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
}

void BluetoothPowerController::SetPrimaryUserBluetoothPowerSetting(
    bool enabled) {
  // This method should only be called when the primary user is the active user.
  CHECK(Shell::Get()->session_controller()->IsUserPrimary());

  active_user_pref_service_->SetBoolean(prefs::kUserBluetoothAdapterEnabled,
                                        enabled);
}

void BluetoothPowerController::InitializeOnAdapterReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = std::move(adapter);
  bluetooth_adapter_->AddObserver(this);
  bool adapter_present = bluetooth_adapter_->IsPresent();
  BLUETOOTH_LOG(EVENT) << "Bluetooth adapter ready, IsPresent = "
                       << adapter_present;
  if (adapter_present)
    TriggerRunPendingBluetoothTasks();
}

void BluetoothPowerController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  active_user_pref_service_ = pref_service;

  // Only listen to primary user's pref changes since non-primary users
  // are not able to change bluetooth pref.
  if (!Shell::Get()->session_controller()->IsUserPrimary()) {
    StopWatchingActiveUserPrefsChanges();
    return;
  }
  StartWatchingActiveUserPrefsChanges();

  // Apply the bluetooth pref only for regular users (i.e. users representing
  // a human individual). We don't want to apply bluetooth pref for other users
  // e.g. kiosk, guest etc. For non-human users, bluetooth power should be left
  // to the current power state.
  if (!is_primary_user_bluetooth_applied_) {
    ApplyBluetoothPrimaryUserPref();
    is_primary_user_bluetooth_applied_ = true;
  }
}

void BluetoothPowerController::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  BLUETOOTH_LOG(EVENT) << "Bluetooth adapter present changed = " << present;
  if (present) {
    // If adapter->IsPresent() has just changed from false to true, this means
    // that bluez has just started but not yet finished power initialization,
    // so we should not start any bluetooth tasks until bluez power
    // initialization is done. Since there is no signal from bluez when that
    // happens, this adds a bit delay before triggering pending bluetooth tasks.
    //
    // TODO(sonnysasaka): Replace this delay hack with a signal from bluez when
    // it has "initialized" signal in the future (http://crbug.com/765390).
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &BluetoothPowerController::TriggerRunPendingBluetoothTasks,
            weak_ptr_factory_.GetWeakPtr()),
        base::Milliseconds(kBluetoothInitializationDelay));
  }
}

void BluetoothPowerController::ApplyBluetoothPrimaryUserPref() {
  absl::optional<user_manager::UserType> user_type =
      Shell::Get()->session_controller()->GetUserType();
  if (!user_type || !ShouldApplyUserBluetoothSetting(*user_type)) {
    // Do not apply bluetooth setting if user is not of the allowed types.
    return;
  }

  DCHECK(Shell::Get()->session_controller()->IsUserPrimary());

  PrefService* prefs = active_user_pref_service_;

  if (!prefs->FindPreference(prefs::kUserBluetoothAdapterEnabled)
           ->IsDefaultValue()) {
    bool enabled = prefs->GetBoolean(prefs::kUserBluetoothAdapterEnabled);
    BLUETOOTH_LOG(EVENT) << "Applying primary user pref bluetooth power: "
                         << enabled;
    SetBluetoothPower(enabled);
    return;
  }

  // If the user has not had the bluetooth pref yet, set the user pref
  // according to whatever the current bluetooth power is, except for
  // new users (first login on the device) always set the new pref to true.
  if (Shell::Get()->session_controller()->IsUserFirstLogin()) {
    prefs->SetBoolean(prefs::kUserBluetoothAdapterEnabled, true);
  } else {
    SavePrefValue(prefs, prefs::kUserBluetoothAdapterEnabled);
  }
}

void BluetoothPowerController::ApplyBluetoothLocalStatePref() {
  if (local_state_->FindPreference(prefs::kSystemBluetoothAdapterEnabled)
          ->IsDefaultValue()) {
    // If the device has not had the local state bluetooth pref, set the pref
    // according to whatever the current bluetooth power is.
    SavePrefValue(local_state_, prefs::kSystemBluetoothAdapterEnabled);
  } else {
    bool enabled =
        local_state_->GetBoolean(prefs::kSystemBluetoothAdapterEnabled);
    BLUETOOTH_LOG(EVENT) << "Applying local state pref bluetooth power: "
                         << enabled;
    SetBluetoothPower(enabled);
  }
}

void BluetoothPowerController::SetBluetoothPower(bool enabled) {
  if (pending_bluetooth_power_target_.has_value()) {
    // There is already a pending bluetooth power change request, so don't
    // enqueue a new SetPowered operation but rather change the target power.
    pending_bluetooth_power_target_ = enabled;
    return;
  }
  pending_bluetooth_power_target_ = enabled;
  RunBluetoothTaskWhenAdapterReady(
      base::BindOnce(&BluetoothPowerController::SetBluetoothPowerOnAdapterReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothPowerController::SetBluetoothPowerOnAdapterReady() {
  DCHECK(pending_bluetooth_power_target_.has_value());
  bool enabled = pending_bluetooth_power_target_.value();
  pending_bluetooth_power_target_.reset();

  device::PoweredStateOperation power_operation =
      enabled ? device::PoweredStateOperation::kEnable
              : device::PoweredStateOperation::kDisable;

  bluetooth_adapter_->SetPowered(
      enabled,
      base::BindOnce(&BluetoothPowerController::OnSetBluetoothPower,
                     weak_ptr_factory_.GetWeakPtr(), power_operation,
                     /*success=*/true),
      base::BindOnce(&BluetoothPowerController::OnSetBluetoothPower,
                     weak_ptr_factory_.GetWeakPtr(), power_operation,
                     /*success=*/false));

  device::RecordPoweredState(enabled);
}

void BluetoothPowerController::RunBluetoothTaskWhenAdapterReady(
    BluetoothTask task) {
  pending_bluetooth_tasks_.push(std::move(task));
  TriggerRunPendingBluetoothTasks();
}

void BluetoothPowerController::TriggerRunPendingBluetoothTasks() {
  if (pending_tasks_busy_)
    return;
  pending_tasks_busy_ = true;
  RunNextPendingBluetoothTask();
}

void BluetoothPowerController::RunNextPendingBluetoothTask() {
  if (!bluetooth_adapter_ || !bluetooth_adapter_->IsPresent() ||
      pending_bluetooth_tasks_.empty()) {
    // Stop running pending tasks if either adapter becomes not present or
    // all the pending tasks have been run.
    pending_tasks_busy_ = false;
    return;
  }
  BluetoothTask task = std::move(pending_bluetooth_tasks_.front());
  pending_bluetooth_tasks_.pop();
  std::move(task).Run();
}

void BluetoothPowerController::SavePrefValue(PrefService* prefs,
                                             const char* pref_name) {
  RunBluetoothTaskWhenAdapterReady(
      base::BindOnce(&BluetoothPowerController::SavePrefValueOnAdapterReady,
                     weak_ptr_factory_.GetWeakPtr(), prefs, pref_name));
}

void BluetoothPowerController::SavePrefValueOnAdapterReady(
    PrefService* prefs,
    const char* pref_name) {
  prefs->SetBoolean(pref_name, bluetooth_adapter_->IsPowered());
  RunNextPendingBluetoothTask();
}

bool BluetoothPowerController::ShouldApplyUserBluetoothSetting(
    user_manager::UserType user_type) const {
  return user_type == user_manager::USER_TYPE_REGULAR ||
         user_type == user_manager::USER_TYPE_CHILD ||
         user_type == user_manager::USER_TYPE_ACTIVE_DIRECTORY;
}

void BluetoothPowerController::OnSetBluetoothPower(
    device::PoweredStateOperation power_operation,
    bool success) {
  device::RecordPoweredStateOperationResult(power_operation, success);
  // Always run the next pending task after SetPowered completes regardless
  // of whether there was an error.
  RunNextPendingBluetoothTask();
}
}  // namespace ash
