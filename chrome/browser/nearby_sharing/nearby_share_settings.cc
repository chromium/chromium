// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_settings.h"

#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/cross_device/logging/logging.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

NearbyShareSettings::NearbyShareSettings(
    PrefService* pref_service,
    NearbyShareLocalDeviceDataManager* local_device_data_manager)
    : pref_service_(pref_service),
      local_device_data_manager_(local_device_data_manager) {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kNearbySharingEnabledPrefName,
      base::BindRepeating(&NearbyShareSettings::OnEnabledPrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kNearbySharingFastInitiationNotificationStatePrefName,
      base::BindRepeating(
          &NearbyShareSettings::OnFastInitiationNotificationStatePrefChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kNearbySharingBackgroundVisibilityName,
      base::BindRepeating(&NearbyShareSettings::OnVisibilityPrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kNearbySharingDataUsageName,
      base::BindRepeating(&NearbyShareSettings::OnDataUsagePrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kNearbySharingAllowedContactsPrefName,
      base::BindRepeating(&NearbyShareSettings::OnAllowedContactsPrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kNearbySharingOnboardingCompletePrefName,
      base::BindRepeating(
          &NearbyShareSettings::OnIsOnboardingCompletePrefChanged,
          base::Unretained(this)));

  local_device_data_manager_->AddObserver(this);

  if (GetEnabled()) {
    base::UmaHistogramEnumeration("Nearby.Share.VisibilityChoice",
                                  GetVisibility());
  }
}

NearbyShareSettings::~NearbyShareSettings() {
  local_device_data_manager_->RemoveObserver(this);
}

bool NearbyShareSettings::GetEnabled() const {
  return pref_service_->GetBoolean(prefs::kNearbySharingEnabledPrefName);
}

nearby_share::mojom::FastInitiationNotificationState
NearbyShareSettings::GetFastInitiationNotificationState() const {
  return static_cast<nearby_share::mojom::FastInitiationNotificationState>(
      pref_service_->GetInteger(
          prefs::kNearbySharingFastInitiationNotificationStatePrefName));
}

void NearbyShareSettings::SetIsFastInitiationHardwareSupported(
    bool is_supported) {
  // If new value is same as old value don't notify observers.
  if (is_fast_initiation_hardware_supported_ == is_supported) {
    return;
  }

  is_fast_initiation_hardware_supported_ = is_supported;
  for (auto& remote : observers_set_) {
    remote->OnIsFastInitiationHardwareSupportedChanged(is_supported);
  }
}

std::string NearbyShareSettings::GetDeviceName() const {
  return local_device_data_manager_->GetDeviceName();
}

NearbyShareSettings::DataUsage NearbyShareSettings::GetDataUsage() const {
  return static_cast<DataUsage>(
      pref_service_->GetInteger(prefs::kNearbySharingDataUsageName));
}

nearby_share::mojom::Visibility NearbyShareSettings::GetVisibility() const {
  int visibility_int =
      pref_service_->GetInteger(prefs::kNearbySharingBackgroundVisibilityName);

  // If Visibility is set to kYourDevices and Self Share is toggled from enabled
  // to disabled, `visibility_int` will have a greater enum value than
  // `Visibility::kMaxValue`, causing UB. In this case, the visibility is set to
  // kNoOne instead.
  if (visibility_int >
      static_cast<int>(nearby_share::mojom::Visibility::kMaxValue)) {
    pref_service_->SetInteger(
        prefs::kNearbySharingBackgroundVisibilityName,
        static_cast<int>(nearby_share::mojom::Visibility::kNoOne));
    return nearby_share::mojom::Visibility::kNoOne;
  } else {
    return static_cast<nearby_share::mojom::Visibility>(visibility_int);
  }
}

const std::vector<std::string> NearbyShareSettings::GetAllowedContacts() const {
  std::vector<std::string> allowed_contacts;
  const base::Value::List& list =
      pref_service_->GetList(prefs::kNearbySharingAllowedContactsPrefName);
  for (const auto& value : list) {
    allowed_contacts.push_back(value.GetString());
  }
  return allowed_contacts;
}

bool NearbyShareSettings::IsOnboardingComplete() const {
  return pref_service_->GetBoolean(
      prefs::kNearbySharingOnboardingCompletePrefName);
}

bool NearbyShareSettings::IsDisabledByPolicy() const {
  return !GetEnabled() && pref_service_->IsManagedPreference(
                              prefs::kNearbySharingEnabledPrefName);
}

void NearbyShareSettings::AddSettingsObserver(
    ::mojo::PendingRemote<nearby_share::mojom::NearbyShareSettingsObserver>
        observer) {
  observers_set_.Add(std::move(observer));
}

void NearbyShareSettings::GetEnabled(base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(GetEnabled());
}

void NearbyShareSettings::GetFastInitiationNotificationState(
    base::OnceCallback<
        void(nearby_share::mojom::FastInitiationNotificationState)> callback) {
  std::move(callback).Run(GetFastInitiationNotificationState());
}

void NearbyShareSettings::GetIsFastInitiationHardwareSupported(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(is_fast_initiation_hardware_supported_);
}

void NearbyShareSettings::SetEnabled(bool enabled) {
  DCHECK(!enabled || IsOnboardingComplete());
  pref_service_->SetBoolean(prefs::kNearbySharingEnabledPrefName, enabled);
  if (enabled && GetVisibility() == nearby_share::mojom::Visibility::kUnknown) {
    CD_LOG(ERROR, Feature::NS)
        << "Nearby Share enabled with visibility unset. Setting "
           "visibility to kNoOne.";
    SetVisibility(nearby_share::mojom::Visibility::kNoOne);
  }
}

void NearbyShareSettings::SetFastInitiationNotificationState(
    nearby_share::mojom::FastInitiationNotificationState state) {
  pref_service_->SetInteger(
      prefs::kNearbySharingFastInitiationNotificationStatePrefName,
      static_cast<int>(state));
}

void NearbyShareSettings::IsOnboardingComplete(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(IsOnboardingComplete());
}

void NearbyShareSettings::SetIsOnboardingComplete(bool completed) {
  pref_service_->SetBoolean(prefs::kNearbySharingOnboardingCompletePrefName,
                            completed);
}

void NearbyShareSettings::GetDeviceName(
    base::OnceCallback<void(const std::string&)> callback) {
  std::move(callback).Run(GetDeviceName());
}

void NearbyShareSettings::ValidateDeviceName(
    const std::string& device_name,
    base::OnceCallback<void(nearby_share::mojom::DeviceNameValidationResult)>
        callback) {
  std::move(callback).Run(
      local_device_data_manager_->ValidateDeviceName(device_name));
}

void NearbyShareSettings::SetDeviceName(
    const std::string& device_name,
    base::OnceCallback<void(nearby_share::mojom::DeviceNameValidationResult)>
        callback) {
  std::move(callback).Run(
      local_device_data_manager_->SetDeviceName(device_name));
}

void NearbyShareSettings::GetDataUsage(
    base::OnceCallback<void(nearby_share::mojom::DataUsage)> callback) {
  std::move(callback).Run(GetDataUsage());
}

void NearbyShareSettings::SetDataUsage(
    nearby_share::mojom::DataUsage data_usage) {
  pref_service_->SetInteger(prefs::kNearbySharingDataUsageName,
                            static_cast<int>(data_usage));
}

void NearbyShareSettings::GetVisibility(
    base::OnceCallback<void(nearby_share::mojom::Visibility)> callback) {
  std::move(callback).Run(GetVisibility());
}

void NearbyShareSettings::SetVisibility(
    nearby_share::mojom::Visibility visibility) {
  DCHECK(pref_service_);

  pref_service_->SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                            static_cast<int>(visibility));
}

void NearbyShareSettings::GetAllowedContacts(
    base::OnceCallback<void(const std::vector<std::string>&)> callback) {
  std::move(callback).Run(GetAllowedContacts());
}

void NearbyShareSettings::SetAllowedContacts(
    const std::vector<std::string>& allowed_contacts) {
  base::Value::List list;
  for (const auto& id : allowed_contacts) {
    list.Append(id);
  }
  pref_service_->SetList(prefs::kNearbySharingAllowedContactsPrefName,
                         std::move(list));
}

void NearbyShareSettings::Bind(
    mojo::PendingReceiver<nearby_share::mojom::NearbyShareSettings> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

void NearbyShareSettings::OnLocalDeviceDataChanged(bool did_device_name_change,
                                                   bool did_full_name_change,
                                                   bool did_icon_change) {
  if (!did_device_name_change)
    return;

  std::string device_name = GetDeviceName();
  for (auto& remote : observers_set_) {
    remote->OnDeviceNameChanged(device_name);
  }
}

void NearbyShareSettings::OnEnabledPrefChanged() {
  bool enabled = GetEnabled();
  for (auto& remote : observers_set_) {
    remote->OnEnabledChanged(enabled);
  }

  ProcessFastInitiationNotificationParentPrefChanged(enabled);
}

void NearbyShareSettings::OnFastInitiationNotificationStatePrefChanged() {
  nearby_share::mojom::FastInitiationNotificationState state =
      GetFastInitiationNotificationState();
  for (auto& remote : observers_set_) {
    remote->OnFastInitiationNotificationStateChanged(state);
  }
}

void NearbyShareSettings::OnDataUsagePrefChanged() {
  DataUsage data_usage = GetDataUsage();
  for (auto& remote : observers_set_) {
    remote->OnDataUsageChanged(data_usage);
  }
}

void NearbyShareSettings::OnVisibilityPrefChanged() {
  nearby_share::mojom::Visibility visibility = GetVisibility();
  for (auto& remote : observers_set_) {
    remote->OnVisibilityChanged(visibility);
  }
}

void NearbyShareSettings::OnAllowedContactsPrefChanged() {
  std::vector<std::string> visible_contacts = GetAllowedContacts();
  for (auto& remote : observers_set_) {
    remote->OnAllowedContactsChanged(visible_contacts);
  }
}

void NearbyShareSettings::OnIsOnboardingCompletePrefChanged() {
  bool is_complete = IsOnboardingComplete();
  for (auto& remote : observers_set_) {
    remote->OnIsOnboardingCompleteChanged(is_complete);
  }
}

void NearbyShareSettings::ProcessFastInitiationNotificationParentPrefChanged(
    bool enabled) {
  // If onboarding is not yet complete the Nearby feature should not be able to
  // affect the enabled state.
  if (!IsOnboardingComplete()) {
    return;
  }

  // If the user explicitly disabled notifications, toggling the Nearby Share
  // feature does not re-enable the notification sub-feature.
  if (GetFastInitiationNotificationState() ==
      nearby_share::mojom::FastInitiationNotificationState::kDisabledByUser) {
    return;
  }
  SetFastInitiationNotificationState(
      enabled ? nearby_share::mojom::FastInitiationNotificationState::kEnabled
              : nearby_share::mojom::FastInitiationNotificationState::
                    kDisabledByFeature);
}
