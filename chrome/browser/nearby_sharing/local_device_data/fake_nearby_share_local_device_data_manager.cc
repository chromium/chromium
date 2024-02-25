// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"

#include <utility>

namespace {
const char kDefaultId[] = "123456789A";
const char kDefaultDeviceName[] = "Barack's Chromebook";
}  // namespace

FakeNearbyShareLocalDeviceDataManager::Factory::Factory() = default;

FakeNearbyShareLocalDeviceDataManager::Factory::~Factory() = default;

std::unique_ptr<NearbyShareLocalDeviceDataManager>
FakeNearbyShareLocalDeviceDataManager::Factory::CreateInstance(
    PrefService* pref_service,
    NearbyShareClientFactory* http_client_factory,
    NearbyShareProfileInfoProvider* profile_info_provider) {
  latest_pref_service_ = pref_service;
  latest_http_client_factory_ = http_client_factory;
  latest_profile_info_provider_ = profile_info_provider;

  auto instance = std::make_unique<FakeNearbyShareLocalDeviceDataManager>(
      kDefaultDeviceName);
  instances_.push_back(instance.get());

  return instance;
}

FakeNearbyShareLocalDeviceDataManager::UploadContactsCall::UploadContactsCall(
    std::vector<nearby::sharing::proto::Contact> contacts,
    UploadCompleteCallback callback)
    : contacts(std::move(contacts)), callback(std::move(callback)) {}

FakeNearbyShareLocalDeviceDataManager::UploadContactsCall::UploadContactsCall(
    UploadContactsCall&&) = default;

FakeNearbyShareLocalDeviceDataManager::UploadContactsCall::
    ~UploadContactsCall() = default;

FakeNearbyShareLocalDeviceDataManager::UploadCertificatesCall::
    UploadCertificatesCall(
        std::vector<nearby::sharing::proto::PublicCertificate> certificates,
        UploadCompleteCallback callback)
    : certificates(std::move(certificates)), callback(std::move(callback)) {}

FakeNearbyShareLocalDeviceDataManager::UploadCertificatesCall::
    UploadCertificatesCall(UploadCertificatesCall&&) = default;

FakeNearbyShareLocalDeviceDataManager::UploadCertificatesCall::
    ~UploadCertificatesCall() = default;

FakeNearbyShareLocalDeviceDataManager::FakeNearbyShareLocalDeviceDataManager(
    const std::string& default_device_name)
    : id_(kDefaultId), device_name_(default_device_name) {}

FakeNearbyShareLocalDeviceDataManager::
    ~FakeNearbyShareLocalDeviceDataManager() = default;

std::string FakeNearbyShareLocalDeviceDataManager::GetId() {
  return id_;
}

std::string FakeNearbyShareLocalDeviceDataManager::GetDeviceName() const {
  return device_name_;
}

std::optional<std::string> FakeNearbyShareLocalDeviceDataManager::GetFullName()
    const {
  return full_name_;
}

std::optional<std::string> FakeNearbyShareLocalDeviceDataManager::GetIconUrl()
    const {
  return icon_url_;
}

nearby_share::mojom::DeviceNameValidationResult
FakeNearbyShareLocalDeviceDataManager::ValidateDeviceName(
    const std::string& name) {
  return next_validation_result_;
}

nearby_share::mojom::DeviceNameValidationResult
FakeNearbyShareLocalDeviceDataManager::SetDeviceName(const std::string& name) {
  if (next_validation_result_ !=
      nearby_share::mojom::DeviceNameValidationResult::kValid)
    return next_validation_result_;

  if (device_name_ != name) {
    device_name_ = name;
    NotifyLocalDeviceDataChanged(
        /*did_device_name_change=*/true,
        /*did_full_name_change=*/false,
        /*did_icon_change=*/false);
  }

  return nearby_share::mojom::DeviceNameValidationResult::kValid;
}

void FakeNearbyShareLocalDeviceDataManager::DownloadDeviceData() {
  ++num_download_device_data_calls_;
}

void FakeNearbyShareLocalDeviceDataManager::UploadContacts(
    std::vector<nearby::sharing::proto::Contact> contacts,
    UploadCompleteCallback callback) {
  upload_contacts_calls_.emplace_back(std::move(contacts), std::move(callback));
}

void FakeNearbyShareLocalDeviceDataManager::UploadCertificates(
    std::vector<nearby::sharing::proto::PublicCertificate> certificates,
    UploadCompleteCallback callback) {
  upload_certificates_calls_.emplace_back(std::move(certificates),
                                          std::move(callback));
}
void FakeNearbyShareLocalDeviceDataManager::SetFullName(
    const std::optional<std::string>& full_name) {
  if (full_name_ == full_name)
    return;

  full_name_ = full_name;
  NotifyLocalDeviceDataChanged(
      /*did_device_name_change=*/false,
      /*did_full_name_change=*/true,
      /*did_icon_change=*/false);
}

void FakeNearbyShareLocalDeviceDataManager::SetIconUrl(
    const std::optional<std::string>& icon_url) {
  if (icon_url_ == icon_url)
    return;

  icon_url_ = icon_url;
  NotifyLocalDeviceDataChanged(
      /*did_device_name_change=*/false,
      /*did_full_name_change=*/false,
      /*did_icon_change=*/true);
}

void FakeNearbyShareLocalDeviceDataManager::OnStart() {}

void FakeNearbyShareLocalDeviceDataManager::OnStop() {}
