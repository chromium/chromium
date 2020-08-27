// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_device_data_updater.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_device_data_updater_impl.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler_factory.h"
#include "components/prefs/pref_service.h"

namespace {

// Using the alphanumeric characters below, this provides 36^10 unique device
// IDs. Note that the uniqueness requirement is not global; the IDs are only
// used to differentiate between devices associated with a single GAIA account.
// This ID length agrees with the GmsCore implementation.
const size_t kDeviceIdLength = 10;

// Possible characters used in a randomly generated device ID. This agrees with
// the GmsCore implementation.
constexpr std::array<char, 36> kAlphaNumericChars = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
    'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

constexpr base::TimeDelta kUpdateDeviceDataTimeout =
    base::TimeDelta::FromSeconds(30);
constexpr base::TimeDelta kDeviceDataDownloadPeriod =
    base::TimeDelta::FromHours(1);

}  // namespace

// static
NearbyShareLocalDeviceDataManagerImpl::Factory*
    NearbyShareLocalDeviceDataManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyShareLocalDeviceDataManager>
NearbyShareLocalDeviceDataManagerImpl::Factory::Create(
    PrefService* pref_service,
    NearbyShareClientFactory* http_client_factory) {
  if (test_factory_) {
    return test_factory_->CreateInstance(pref_service, http_client_factory);
  }

  return base::WrapUnique(new NearbyShareLocalDeviceDataManagerImpl(
      pref_service, http_client_factory));
}

// static
void NearbyShareLocalDeviceDataManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareLocalDeviceDataManagerImpl::Factory::~Factory() = default;

NearbyShareLocalDeviceDataManagerImpl::NearbyShareLocalDeviceDataManagerImpl(
    PrefService* pref_service,
    NearbyShareClientFactory* http_client_factory)
    : pref_service_(pref_service),
      device_data_updater_(NearbyShareDeviceDataUpdaterImpl::Factory::Create(
          GetId(),
          kUpdateDeviceDataTimeout,
          http_client_factory)),
      download_device_data_scheduler_(
          NearbyShareSchedulerFactory::CreatePeriodicScheduler(
              kDeviceDataDownloadPeriod,
              /*retry_failures=*/true,
              /*require_connectivity=*/true,
              prefs::kNearbySharingSchedulerDownloadDeviceDataPrefName,
              pref_service_,
              base::BindRepeating(&NearbyShareLocalDeviceDataManagerImpl::
                                      OnDownloadDeviceDataRequested,
                                  base::Unretained(this)))) {}

NearbyShareLocalDeviceDataManagerImpl::
    ~NearbyShareLocalDeviceDataManagerImpl() = default;

std::string NearbyShareLocalDeviceDataManagerImpl::GetId() {
  base::Optional<std::string> id =
      GetStringPref(prefs::kNearbySharingDeviceIdPrefName);
  if (id && !id->empty())
    return *id;

  id = std::string();
  for (size_t i = 0; i < kDeviceIdLength; ++i)
    *id += kAlphaNumericChars[base::RandGenerator(kAlphaNumericChars.size())];

  SetStringPref(prefs::kNearbySharingDeviceIdPrefName, id);

  return *id;
}

base::Optional<std::string>
NearbyShareLocalDeviceDataManagerImpl::GetDeviceName() const {
  return GetStringPref(prefs::kNearbySharingDeviceNamePrefName);
}

base::Optional<std::string> NearbyShareLocalDeviceDataManagerImpl::GetFullName()
    const {
  return GetStringPref(prefs::kNearbySharingFullNamePrefName);
}

base::Optional<std::string> NearbyShareLocalDeviceDataManagerImpl::GetIconUrl()
    const {
  return GetStringPref(prefs::kNearbySharingIconUrlPrefName);
}

void NearbyShareLocalDeviceDataManagerImpl::SetDeviceName(
    const std::string& name) {
  if (name == GetDeviceName())
    return;

  // TODO(b/161297140): Perform input validation.
  SetStringPref(prefs::kNearbySharingDeviceNamePrefName, name);

  NotifyLocalDeviceDataChanged(/*did_device_name_change=*/true,
                               /*did_full_name_change=*/false,
                               /*did_icon_url_change=*/false);
}

void NearbyShareLocalDeviceDataManagerImpl::DownloadDeviceData() {
  download_device_data_scheduler_->MakeImmediateRequest();
}

void NearbyShareLocalDeviceDataManagerImpl::UploadContacts(
    std::vector<nearbyshare::proto::Contact> contacts,
    UploadCompleteCallback callback) {
  device_data_updater_->UpdateDeviceData(
      std::move(contacts),
      /*certificates=*/base::nullopt,
      base::BindOnce(
          &NearbyShareLocalDeviceDataManagerImpl::OnUploadContactsFinished,
          base::Unretained(this), std::move(callback)));
}

void NearbyShareLocalDeviceDataManagerImpl::UploadCertificates(
    std::vector<nearbyshare::proto::PublicCertificate> certificates,
    UploadCompleteCallback callback) {
  device_data_updater_->UpdateDeviceData(
      /*contacts=*/base::nullopt, std::move(certificates),
      base::BindOnce(
          &NearbyShareLocalDeviceDataManagerImpl::OnUploadCertificatesFinished,
          base::Unretained(this), std::move(callback)));
}

void NearbyShareLocalDeviceDataManagerImpl::OnStart() {
  // This schedules an immediate download of the full name and icon URL from the
  // server if that has never happened before.
  download_device_data_scheduler_->Start();
}

void NearbyShareLocalDeviceDataManagerImpl::OnStop() {
  download_device_data_scheduler_->Stop();
}

base::Optional<std::string>
NearbyShareLocalDeviceDataManagerImpl::GetStringPref(
    const std::string& pref_name) const {
  std::string value = pref_service_->GetString(pref_name);
  if (value.empty())
    return base::nullopt;

  return value;
}

void NearbyShareLocalDeviceDataManagerImpl::SetStringPref(
    const std::string& pref_name,
    const base::Optional<std::string>& value) {
  if (value)
    pref_service_->SetString(pref_name, *value);
  else
    pref_service_->ClearPref(pref_name);
}

void NearbyShareLocalDeviceDataManagerImpl::OnDownloadDeviceDataRequested() {
  device_data_updater_->UpdateDeviceData(
      /*contacts=*/base::nullopt,
      /*certificates=*/base::nullopt,
      base::BindOnce(
          &NearbyShareLocalDeviceDataManagerImpl::OnDownloadDeviceDataFinished,
          base::Unretained(this)));
}

void NearbyShareLocalDeviceDataManagerImpl::OnDownloadDeviceDataFinished(
    const base::Optional<nearbyshare::proto::UpdateDeviceResponse>& response) {
  if (response)
    HandleUpdateDeviceResponse(response);

  download_device_data_scheduler_->HandleResult(
      /*success=*/response.has_value());
}

void NearbyShareLocalDeviceDataManagerImpl::OnUploadContactsFinished(
    UploadCompleteCallback callback,
    const base::Optional<nearbyshare::proto::UpdateDeviceResponse>& response) {
  if (response)
    HandleUpdateDeviceResponse(response);

  std::move(callback).Run(/*success=*/response.has_value());
}

void NearbyShareLocalDeviceDataManagerImpl::OnUploadCertificatesFinished(
    UploadCompleteCallback callback,
    const base::Optional<nearbyshare::proto::UpdateDeviceResponse>& response) {
  if (response)
    HandleUpdateDeviceResponse(response);

  std::move(callback).Run(/*success=*/response.has_value());
}

void NearbyShareLocalDeviceDataManagerImpl::HandleUpdateDeviceResponse(
    const base::Optional<nearbyshare::proto::UpdateDeviceResponse>& response) {
  if (!response)
    return;

  bool did_full_name_change = response->person_name() != GetFullName();
  bool did_icon_url_change = response->image_url() != GetIconUrl();
  if (!did_full_name_change && !did_icon_url_change)
    return;

  if (did_full_name_change) {
    SetStringPref(prefs::kNearbySharingFullNamePrefName,
                  response->person_name());
  }
  if (did_icon_url_change) {
    SetStringPref(prefs::kNearbySharingIconUrlPrefName, response->image_url());
  }

  NotifyLocalDeviceDataChanged(/*did_device_name_change=*/false,
                               did_full_name_change, did_icon_url_change);
}
