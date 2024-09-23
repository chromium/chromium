// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/owner_pending_setting_controller.h"

#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/ownership/owner_settings_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

OwnerPendingSettingController::OwnerPendingSettingController(
    const std::string& pref_name,
    const std::string& pending_pref_name,
    PrefService* local_state)
    : local_state_(local_state),
      pref_name_(pref_name),
      pending_pref_name_(pending_pref_name) {
  value_notified_to_observers_ = GetValue();
}

void OwnerPendingSettingController::Set(Profile* profile,
                                        const base::Value& value) {
  DCHECK(profile);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetOwnershipStatus() ==
      DeviceSettingsService::OwnershipStatus::kOwnershipTaken) {
    // The device has an owner. If the current profile is that owner, we will
    // write the value on their behalf, otherwise no action is taken.
    VLOG(1) << "Already has owner";
    SetWithServiceAsync(GetOwnerSettingsService(profile), value);
  } else {
    // The device has no owner, or we do not know yet whether the device has an
    // owner. We write a pending value that will be persisted when ownership is
    // taken (if that has not already happened).
    // We store the new value in the local state, so that even if Chrome is
    // restarted before ownership is taken, we will still persist it eventually.
    // See OnOwnershipTaken.
    VLOG(1) << "Pending owner; setting pending pref name: "
            << pending_pref_name_;
    local_state_->Set(pending_pref_name_, value);
    NotifyObservers();
  }
}

std::optional<base::Value> OwnerPendingSettingController::GetValue() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<base::Value> value = GetPendingValue();
  if (ShouldReadFromPendingValue() && value.has_value()) {
    // Return the pending value if it exists.
    return value;
  }
  // Otherwise, always return the value from the signed store.
  return GetSignedStoredValue();
}

base::CallbackListSubscription OwnerPendingSettingController::AddObserver(
    const base::RepeatingClosure& callback) {
  DCHECK(!callback.is_null());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return callback_list_.Add(callback);
}

void OwnerPendingSettingController::OnOwnershipTaken(
    ownership::OwnerSettingsService* service) {
  DCHECK_EQ(GetOwnershipStatus(),
            DeviceSettingsService::OwnershipStatus::kOwnershipTaken);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "OnOwnershipTaken";

  std::optional<base::Value> pending_value = GetPendingValue();
  if (pending_value.has_value()) {
    // At the time ownership is taken, there is a value waiting to be written.
    // Use the OwnerSettingsService of the new owner to write the setting.
    SetWithServiceAsync(service, pending_value.value());
  }
}

OwnerPendingSettingController::~OwnerPendingSettingController() {
  owner_settings_service_observation_.Reset();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OwnerPendingSettingController::OnSignedPolicyStored(bool success) {
  if (!success)
    return;
  std::optional<base::Value> pending_value = GetPendingValue();
  std::optional<base::Value> signed_value = GetSignedStoredValue();
  if (pending_value.has_value() && signed_value.has_value() &&
      pending_value == signed_value) {
    is_value_being_set_with_service_ = false;
    owner_settings_service_observation_.Reset();
    ClearPendingValue();
    NotifyObservers();
    if (on_device_settings_stored_callback_)
      std::move(on_device_settings_stored_callback_).Run();
  }
}

void OwnerPendingSettingController::SetOnDeviceSettingsStoredCallBack(
    base::OnceClosure callback) {
  CHECK(!on_device_settings_stored_callback_);
  on_device_settings_stored_callback_ = std::move(callback);
}

void OwnerPendingSettingController::SetWithServiceAsync(
    ownership::OwnerSettingsService* service,  // Can be null for non-owners.
    const base::Value& value) {
  bool not_yet_ready = service && !service->IsReady();
  if (not_yet_ready) {
    VLOG(1) << "Service not yet ready. Adding listener.";
    // Service is not yet ready. Listen for changes in its readiness so we can
    // write the value once it is ready. Uses weak pointers, so if everything
    // is shutdown and deleted in the meantime, this callback isn't run.
    service->IsOwnerAsync(base::BindOnce(
        &OwnerPendingSettingController::SetWithServiceCallback,
        this->as_weak_ptr(), service->as_weak_ptr(), value.Clone()));
  } else {
    // Service is either null, or ready - use it right now.
    SetWithService(service, value);
  }
}

void OwnerPendingSettingController::SetWithServiceCallback(
    const base::WeakPtr<ownership::OwnerSettingsService>& service,
    const base::Value value,
    bool is_owner) {
  if (service)  // Make sure service wasn't deleted in the meantime.
    SetWithService(service.get(), value);
}

void OwnerPendingSettingController::SetWithService(
    ownership::OwnerSettingsService* service,  // Can be null for non-owners.
    const base::Value& value) {
  if (service && service->IsOwner()) {
    if (!owner_settings_service_observation_.IsObserving())
      owner_settings_service_observation_.Observe(service);

    // `is_value_being_set_with_service_` must be set to `true` and
    // `pending_pref_name_` must be set with `value` before setting `pref_name_`
    // with `OwnerSettingService` to guarantee that future calls to `GetValue()`
    // returns the pending value instead of the signed value until
    // `OnSignedPolicyStored(true)` is called.
    // Since calls to `GetValue()` can happen inside `service->Set()`, the order
    // of the following lines is important.
    is_value_being_set_with_service_ = true;
    local_state_->Set(pending_pref_name_, value);

    service->Set(pref_name_, value);
  } else {
    // Do nothing since we are not the owner.
    LOG(WARNING) << "Changing settings from non-owner, setting=" << pref_name_;
  }
}

void OwnerPendingSettingController::NotifyObservers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<base::Value> current_value = GetValue();
  if (current_value != value_notified_to_observers_) {
    VLOG(1) << "Notifying observers";
    value_notified_to_observers_ = std::move(current_value);
    callback_list_.Notify();
  } else {
    VLOG(1) << "Not notifying (already notified)";
  }
}

DeviceSettingsService::OwnershipStatus
OwnerPendingSettingController::GetOwnershipStatus() const {
  return DeviceSettingsService::Get()->GetOwnershipStatus();
}

ownership::OwnerSettingsService*
OwnerPendingSettingController::GetOwnerSettingsService(Profile* profile) {
  return OwnerSettingsServiceAshFactory::GetForBrowserContext(profile);
}

std::optional<base::Value> OwnerPendingSettingController::GetPendingValue()
    const {
  if (local_state_->HasPrefPath(pending_pref_name_)) {
    return local_state_->GetValue(pending_pref_name_).Clone();
  }
  return std::nullopt;
}

void OwnerPendingSettingController::ClearPendingValue() {
  VLOG(1) << "ClearPendingValue";
  local_state_->ClearPref(pending_pref_name_);
}

std::optional<base::Value> OwnerPendingSettingController::GetSignedStoredValue()
    const {
  const base::Value* value = CrosSettings::Get()->GetPref(pref_name_);
  if (value) {
    return value->Clone();
  }
  return std::nullopt;
}

bool OwnerPendingSettingController::ShouldReadFromPendingValue() const {
  // Read from pending value before ownership is taken or ownership is
  // unknown. There's a brief moment when ownership is unknown for every
  // Chrome starts. In that case, we will read from pending value if it exists
  // (which means ownership is not taken), and read from service when pending
  // value pending is cleared (which means ownership is taken).
  if (GetOwnershipStatus() ==
          DeviceSettingsService::OwnershipStatus::kOwnershipNone ||
      GetOwnershipStatus() ==
          DeviceSettingsService::OwnershipStatus::kOwnershipUnknown) {
    return true;
  }
  // Read from pending value if ownership is taken but pending value has not
  // been set successfully with service.
  return is_value_being_set_with_service_;
}

}  // namespace ash
