// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager_impl.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_profile_info_provider.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_switches.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_device_data_updater.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_device_data_updater_impl.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler_factory.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

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

constexpr base::TimeDelta kUpdateDeviceDataTimeout = base::Seconds(30);
constexpr base::TimeDelta kDeviceDataDownloadPeriod = base::Hours(12);

// Returns a truncated version of |name| that is |overflow_length| characters
// too long. For example, name="Reallylongname" with overflow_length=5 will
// return "Really...".
std::string GetTruncatedName(std::string name, size_t overflow_length) {
  std::string ellipsis("...");
  size_t max_name_length = name.length() - overflow_length - ellipsis.length();
  DCHECK_GT(max_name_length, 0u);
  std::string truncated;
  base::TruncateUTF8ToByteSize(name, max_name_length, &truncated);
  truncated.append(ellipsis);
  return truncated;
}

}  // namespace

// static
NearbyShareLocalDeviceDataManagerImpl::Factory*
    NearbyShareLocalDeviceDataManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyShareLocalDeviceDataManager>
NearbyShareLocalDeviceDataManagerImpl::Factory::Create(
    PrefService* pref_service,
    NearbyShareClientFactory* http_client_factory,
    NearbyShareProfileInfoProvider* profile_info_provider) {
  if (test_factory_) {
    return test_factory_->CreateInstance(pref_service, http_client_factory,
                                         profile_info_provider);
  }

  return base::WrapUnique(new NearbyShareLocalDeviceDataManagerImpl(
      pref_service, http_client_factory, profile_info_provider));
}

// static
void NearbyShareLocalDeviceDataManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareLocalDeviceDataManagerImpl::Factory::~Factory() = default;

NearbyShareLocalDeviceDataManagerImpl::NearbyShareLocalDeviceDataManagerImpl(
    PrefService* pref_service,
    NearbyShareClientFactory* http_client_factory,
    NearbyShareProfileInfoProvider* profile_info_provider)
    : pref_service_(pref_service),
      profile_info_provider_(profile_info_provider),
      device_data_updater_(NearbyShareDeviceDataUpdaterImpl::Factory::Create(
          GetId(),
          kUpdateDeviceDataTimeout,
          http_client_factory)),
      download_device_data_scheduler_(
          ash::nearby::NearbySchedulerFactory::CreatePeriodicScheduler(
              kDeviceDataDownloadPeriod,
              /*retry_failures=*/true,
              /*require_connectivity=*/true,
              prefs::kNearbySharingSchedulerDownloadDeviceDataPrefName,
              pref_service_,
              base::BindRepeating(&NearbyShareLocalDeviceDataManagerImpl::
                                      OnDownloadDeviceDataRequested,
                                  base::Unretained(this)),
              Feature::NS)) {}

NearbyShareLocalDeviceDataManagerImpl::
    ~NearbyShareLocalDeviceDataManagerImpl() = default;

std::string NearbyShareLocalDeviceDataManagerImpl::GetId() {
  std::string id =
      pref_service_->GetString(prefs::kNearbySharingDeviceIdPrefName);
  if (!id.empty())
    return id;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kNearbyShareDeviceID)) {
    id = command_line->GetSwitchValueASCII(switches::kNearbyShareDeviceID);
  } else {
    for (size_t i = 0; i < kDeviceIdLength; ++i)
      id += kAlphaNumericChars[base::RandGenerator(kAlphaNumericChars.size())];
  }

  pref_service_->SetString(prefs::kNearbySharingDeviceIdPrefName, id);

  return id;
}

std::string NearbyShareLocalDeviceDataManagerImpl::GetDeviceName() const {
  std::string device_name =
      pref_service_->GetString(prefs::kNearbySharingDeviceNamePrefName);
  return device_name.empty() ? GetDefaultDeviceName() : device_name;
}

std::optional<std::string> NearbyShareLocalDeviceDataManagerImpl::GetFullName()
    const {
  if (pref_service_->FindPreference(prefs::kNearbySharingFullNamePrefName)
          ->IsDefaultValue()) {
    return std::nullopt;
  }

  return pref_service_->GetString(prefs::kNearbySharingFullNamePrefName);
}

std::optional<std::string> NearbyShareLocalDeviceDataManagerImpl::GetIconUrl()
    const {
  if (pref_service_->FindPreference(prefs::kNearbySharingIconUrlPrefName)
          ->IsDefaultValue()) {
    return std::nullopt;
  }

  return pref_service_->GetString(prefs::kNearbySharingIconUrlPrefName);
}

std::optional<std::string> NearbyShareLocalDeviceDataManagerImpl::GetIconToken()
    const {
  if (pref_service_->FindPreference(prefs::kNearbySharingIconTokenPrefName)
          ->IsDefaultValue()) {
    return std::nullopt;
  }

  return pref_service_->GetString(prefs::kNearbySharingIconTokenPrefName);
}

nearby_share::mojom::DeviceNameValidationResult
NearbyShareLocalDeviceDataManagerImpl::ValidateDeviceName(
    const std::string& name) {
  if (name.empty())
    return nearby_share::mojom::DeviceNameValidationResult::kErrorEmpty;

  if (!base::IsStringUTF8(name))
    return nearby_share::mojom::DeviceNameValidationResult::kErrorNotValidUtf8;

  if (name.length() > kNearbyShareDeviceNameMaxLength)
    return nearby_share::mojom::DeviceNameValidationResult::kErrorTooLong;

  return nearby_share::mojom::DeviceNameValidationResult::kValid;
}

nearby_share::mojom::DeviceNameValidationResult
NearbyShareLocalDeviceDataManagerImpl::SetDeviceName(const std::string& name) {
  if (name == GetDeviceName())
    return nearby_share::mojom::DeviceNameValidationResult::kValid;

  auto error = ValidateDeviceName(name);
  if (error != nearby_share::mojom::DeviceNameValidationResult::kValid)
    return error;

  pref_service_->SetString(prefs::kNearbySharingDeviceNamePrefName, name);

  NotifyLocalDeviceDataChanged(/*did_device_name_change=*/true,
                               /*did_full_name_change=*/false,
                               /*did_icon_change=*/false);

  return nearby_share::mojom::DeviceNameValidationResult::kValid;
}

void NearbyShareLocalDeviceDataManagerImpl::DownloadDeviceData() {
  download_device_data_scheduler_->MakeImmediateRequest();
}

void NearbyShareLocalDeviceDataManagerImpl::UploadContacts(
    std::vector<nearby::sharing::proto::Contact> contacts,
    UploadCompleteCallback callback) {
  device_data_updater_->UpdateDeviceData(
      std::move(contacts),
      /*certificates=*/std::nullopt,
      base::BindOnce(
          &NearbyShareLocalDeviceDataManagerImpl::OnUploadContactsFinished,
          base::Unretained(this), std::move(callback)));
}

void NearbyShareLocalDeviceDataManagerImpl::UploadCertificates(
    std::vector<nearby::sharing::proto::PublicCertificate> certificates,
    UploadCompleteCallback callback) {
  device_data_updater_->UpdateDeviceData(
      /*contacts=*/std::nullopt, std::move(certificates),
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

std::string NearbyShareLocalDeviceDataManagerImpl::GetDefaultDeviceName()
    const {
  std::u16string device_type = ui::GetChromeOSDeviceName();
  std::optional<std::u16string> given_name =
      profile_info_provider_->GetGivenName();
  if (!given_name)
    return base::UTF16ToUTF8(device_type);

  std::string device_name = l10n_util::GetStringFUTF8(
      IDS_NEARBY_DEFAULT_DEVICE_NAME, *given_name, device_type);
  if (device_name.length() <= kNearbyShareDeviceNameMaxLength)
    return device_name;

  std::string truncated_name =
      GetTruncatedName(base::UTF16ToUTF8(*given_name),
                       device_name.length() - kNearbyShareDeviceNameMaxLength);
  return l10n_util::GetStringFUTF8(IDS_NEARBY_DEFAULT_DEVICE_NAME,
                                   base::UTF8ToUTF16(truncated_name),
                                   device_type);
}

void NearbyShareLocalDeviceDataManagerImpl::OnDownloadDeviceDataRequested() {
  device_data_updater_->UpdateDeviceData(
      /*contacts=*/std::nullopt,
      /*certificates=*/std::nullopt,
      base::BindOnce(
          &NearbyShareLocalDeviceDataManagerImpl::OnDownloadDeviceDataFinished,
          base::Unretained(this)));
}

void NearbyShareLocalDeviceDataManagerImpl::OnDownloadDeviceDataFinished(
    const std::optional<nearby::sharing::proto::UpdateDeviceResponse>&
        response) {
  if (response)
    HandleUpdateDeviceResponse(response);

  download_device_data_scheduler_->HandleResult(
      /*success=*/response.has_value());
}

void NearbyShareLocalDeviceDataManagerImpl::OnUploadContactsFinished(
    UploadCompleteCallback callback,
    const std::optional<nearby::sharing::proto::UpdateDeviceResponse>&
        response) {
  // NOTE(http://crbug.com/1211189): Only process the UpdateDevice response for
  // DownloadDeviceData() calls. We want avoid infinite loops if the full name
  // or icon URL unexpectedly change.

  std::move(callback).Run(/*success=*/response.has_value());
}

void NearbyShareLocalDeviceDataManagerImpl::OnUploadCertificatesFinished(
    UploadCompleteCallback callback,
    const std::optional<nearby::sharing::proto::UpdateDeviceResponse>&
        response) {
  // NOTE(http://crbug.com/1211189): Only process the UpdateDevice response for
  // DownloadDeviceData() calls. We want avoid infinite loops if the full name
  // or icon URL unexpectedly change.

  std::move(callback).Run(/*success=*/response.has_value());
}

void NearbyShareLocalDeviceDataManagerImpl::HandleUpdateDeviceResponse(
    const std::optional<nearby::sharing::proto::UpdateDeviceResponse>&
        response) {
  if (!response)
    return;

  bool did_full_name_change = response->person_name() != GetFullName();
  if (did_full_name_change) {
    pref_service_->SetString(prefs::kNearbySharingFullNamePrefName,
                             response->person_name());
  }

  // NOTE(http://crbug.com/1211189): An icon URL can change without the
  // underlying image changing. For example, icon URLs for some child accounts
  // can rotate on every UpdateDevice RPC call; a timestamp is included in the
  // URL. The icon token is used to detect changes in the underlying image. If a
  // new URL is sent and the token doesn't change, the old URL may still be
  // valid for a couple weeks, for example. So, private certificates do not
  // necessarily need to update the icon URL whenever it changes. Also, we don't
  // expect the token to change without the URL changing; regardless, we don't
  // consider the icon changed unless the URL changes. That way, private
  // certificates will not be unnecessarily regenerated.
  bool did_icon_url_change = response->image_url() != GetIconUrl();
  bool did_icon_token_change = response->image_token() != GetIconToken();
  bool did_icon_change = did_icon_url_change && did_icon_token_change;
  if (did_icon_url_change) {
    pref_service_->SetString(prefs::kNearbySharingIconUrlPrefName,
                             response->image_url());
  }
  if (did_icon_token_change) {
    pref_service_->SetString(prefs::kNearbySharingIconTokenPrefName,
                             response->image_token());
  }

  if (!did_full_name_change && !did_icon_change)
    return;

  NotifyLocalDeviceDataChanged(/*did_device_name_change=*/false,
                               did_full_name_change, did_icon_change);
}
