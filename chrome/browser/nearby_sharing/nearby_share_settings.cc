// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_settings.h"

#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_enums.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
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

std::string NearbyShareSettings::GetDeviceName() const {
  return local_device_data_manager_->GetDeviceName();
}

DataUsage NearbyShareSettings::GetDataUsage() const {
  return static_cast<DataUsage>(
      pref_service_->GetInteger(prefs::kNearbySharingDataUsageName));
}

Visibility NearbyShareSettings::GetVisibility() const {
  return static_cast<Visibility>(
      pref_service_->GetInteger(prefs::kNearbySharingBackgroundVisibilityName));
}

const std::vector<std::string> NearbyShareSettings::GetAllowedContacts() const {
  std::vector<std::string> allowed_contacts;
  const base::ListValue* list =
      pref_service_->GetList(prefs::kNearbySharingAllowedContactsPrefName);
  if (list) {
    base::Value::ConstListView view = list->GetList();
    for (const auto& value : view) {
      allowed_contacts.push_back(value.GetString());
    }
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

void NearbyShareSettings::SetEnabled(bool enabled) {
  pref_service_->SetBoolean(prefs::kNearbySharingEnabledPrefName, enabled);
  if (enabled) {
    // We rely on the the UI to enforce that if the feature was enabled for the
    // first time, that onboarding was run.
    pref_service_->SetBoolean(prefs::kNearbySharingOnboardingCompletePrefName,
                              true);

    if (GetVisibility() == Visibility::kUnknown) {
      NS_LOG(ERROR) << "Nearby Share enabled with visibility unset. Setting "
                       "visibility to kNoOne.";
      SetVisibility(Visibility::kNoOne);
    }
  }
}

void NearbyShareSettings::IsOnboardingComplete(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(IsOnboardingComplete());
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
  pref_service_->SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                            static_cast<int>(visibility));
}

void NearbyShareSettings::GetAllowedContacts(
    base::OnceCallback<void(const std::vector<std::string>&)> callback) {
  std::move(callback).Run(GetAllowedContacts());
}

void NearbyShareSettings::SetAllowedContacts(
    const std::vector<std::string>& allowed_contacts) {
  base::ListValue list;
  for (const auto& id : allowed_contacts) {
    list.AppendString(id);
  }
  pref_service_->Set(prefs::kNearbySharingAllowedContactsPrefName, list);
}

void NearbyShareSettings::Bind(
    mojo::PendingReceiver<nearby_share::mojom::NearbyShareSettings> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

void NearbyShareSettings::OnLocalDeviceDataChanged(bool did_device_name_change,
                                                   bool did_full_name_change,
                                                   bool did_icon_url_change) {
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
}

void NearbyShareSettings::OnDataUsagePrefChanged() {
  DataUsage data_usage = GetDataUsage();
  for (auto& remote : observers_set_) {
    remote->OnDataUsageChanged(data_usage);
  }
}

void NearbyShareSettings::OnVisibilityPrefChanged() {
  Visibility visibility = GetVisibility();
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
