// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager_impl.h"

#include <array>
#include <string>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/certificates/common.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_storage_impl.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_client.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_profile_info_provider.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_switches.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler_factory.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "components/cross_device/logging/logging.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/prefs/pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/floss/floss_features.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "third_party/nearby/sharing/proto/certificate_rpc.pb.h"
#include "third_party/nearby/sharing/proto/encrypted_metadata.pb.h"

namespace {

const char kDeviceIdPrefix[] = "users/me/devices/";

constexpr base::TimeDelta kListPublicCertificatesTimeout = base::Seconds(30);

constexpr std::array<nearby_share::mojom::Visibility, 3> kVisibilities = {
    nearby_share::mojom::Visibility::kAllContacts,
    nearby_share::mojom::Visibility::kSelectedContacts,
    nearby_share::mojom::Visibility::kYourDevices};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum GetDecryptedPublicCertificateResult {
  kSuccess = 0,
  kNoMatch = 1,
  kStorageFailure = 2,
  kMaxValue = kStorageFailure
};

// Check for a command-line override for number of certificates, otherwise
// return the default |kNearbyShareNumPrivateCertificates|.
size_t NumPrivateCertificates() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kNearbyShareNumPrivateCertificates)) {
    return kNearbyShareNumPrivateCertificates;
  }

  std::string num_certificates_str = command_line->GetSwitchValueASCII(
      switches::kNearbyShareNumPrivateCertificates);
  int num_certificates = 0;
  if (!base::StringToInt(num_certificates_str, &num_certificates) ||
      num_certificates < 1) {
    CD_LOG(ERROR, Feature::NS)
        << __func__
        << ": Invalid value provided with num certificates override.";
    return kNearbyShareNumPrivateCertificates;
  }

  return static_cast<size_t>(num_certificates);
}

size_t NumExpectedPrivateCertificates() {
  return kVisibilities.size() * NumPrivateCertificates();
}

std::optional<std::string> GetBluetoothMacAddress(
    device::BluetoothAdapter* bluetooth_adapter) {
  if (!bluetooth_adapter) {
    CD_LOG(WARNING, Feature::NS)
        << __func__
        << ": Failed to get Bluetooth MAC address; Bluetooth adapter is null.";
    return std::nullopt;
  }

  if (!bluetooth_adapter->IsPresent()) {
    // Note: The sophisticated solution would be to listen for
    // device::BluetoothAdapter::Observer::AdapterPresentChanged() before trying
    // to generate private certificates. We take the simple but unsophisticated
    // approach by failing and retrying.
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Failed to get Bluetooth MAC address; Bluetooth "
        << "adapter is not present.";
    return std::nullopt;
  }

  std::array<uint8_t, 6> bytes;
  if (!device::ParseBluetoothAddress(bluetooth_adapter->GetAddress(), bytes)) {
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Failed to get Bluetooth MAC address; cannot parse "
        << "address: " << bluetooth_adapter->GetAddress();
    return std::nullopt;
  }

  return std::string(bytes.begin(), bytes.end());
}

std::optional<nearby::sharing::proto::EncryptedMetadata> BuildMetadata(
    std::string device_name,
    std::optional<std::string> full_name,
    std::optional<std::string> icon_url,
    std::optional<std::string> account_name,
    device::BluetoothAdapter* bluetooth_adapter) {
  nearby::sharing::proto::EncryptedMetadata metadata;
  if (device_name.empty()) {
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Failed to create private certificate metadata; "
        << "missing device name.";
    return std::nullopt;
  }

  metadata.set_device_name(device_name);
  if (full_name) {
    metadata.set_full_name(*full_name);
  }
  if (icon_url) {
    metadata.set_icon_url(*icon_url);
  }
  if (account_name) {
    metadata.set_account_name(*account_name);
  }

  std::optional<std::string> bluetooth_mac_address =
      GetBluetoothMacAddress(bluetooth_adapter);
  base::UmaHistogramBoolean(
      "Nearby.Share.Certificates.Manager."
      "BluetoothMacAddressPresentForPrivateCertificateCreation",
      bluetooth_mac_address.has_value());
  if (!bluetooth_mac_address) {
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Failed to create private certificate metadata; "
        << "missing adapter address.";
    return std::nullopt;
  }

  metadata.set_bluetooth_mac_address(*bluetooth_mac_address);

  return metadata;
}

void RecordGetDecryptedPublicCertificateResultMetric(
    GetDecryptedPublicCertificateResult result) {
  base::UmaHistogramEnumeration(
      "Nearby.Share.Certificates.Manager.GetDecryptedPublicCertificateResult",
      result);
}

void RecordDownloadPublicCertificatesResultMetrics(
    bool success,
    ash::nearby::NearbyHttpResult result,
    size_t page_number,
    size_t certificate_count) {
  base::UmaHistogramBoolean(
      "Nearby.Share.Certificates.Manager.DownloadPublicCertificatesSuccessRate",
      success);
  base::UmaHistogramEnumeration(
      "Nearby.Share.Certificates.Manager.DownloadPublicCertificatesHttpResult",
      result);
  if (success) {
    base::UmaHistogramExactLinear(
        "Nearby.Share.Certificates.Manager."
        "DownloadPublicCertificatesSuccessPageCount",
        page_number, 20);
    base::UmaHistogramCounts10000(
        "Nearby.Share.Certificates.Manager."
        "DownloadPublicCertificatesCount",
        certificate_count);
  } else {
    base::UmaHistogramExactLinear(
        "Nearby.Share.Certificates.Manager."
        "DownloadPublicCertificatesFailuePageCount",
        page_number, 20);
  }
}

void TryDecryptPublicCertificates(
    const NearbyShareEncryptedMetadataKey& encrypted_metadata_key,
    NearbyShareCertificateManager::CertDecryptedCallback callback,
    bool success,
    std::unique_ptr<std::vector<nearby::sharing::proto::PublicCertificate>>
        public_certificates) {
  if (!success || !public_certificates) {
    CD_LOG(ERROR, Feature::NS)
        << __func__ << ": Failed to read public certificates from storage.";
    RecordGetDecryptedPublicCertificateResultMetric(
        GetDecryptedPublicCertificateResult::kStorageFailure);
    std::move(callback).Run(std::nullopt);
    return;
  }

  for (const auto& cert : *public_certificates) {
    std::optional<NearbyShareDecryptedPublicCertificate> decrypted =
        NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
            cert, encrypted_metadata_key);
    if (decrypted) {
      CD_LOG(VERBOSE, Feature::NS)
          << __func__ << ": Successfully decrypted public certificate with ID "
          << base::HexEncode(decrypted->id());
      RecordGetDecryptedPublicCertificateResultMetric(
          GetDecryptedPublicCertificateResult::kSuccess);
      std::move(callback).Run(std::move(decrypted));
      return;
    }
  }
  CD_LOG(VERBOSE, Feature::NS)
      << __func__
      << ": Metadata key could not decrypt any public certificates.";
  RecordGetDecryptedPublicCertificateResultMetric(
      GetDecryptedPublicCertificateResult::kNoMatch);
  std::move(callback).Run(std::nullopt);
}

// See documentation in declaration of `AttemptPrivateCertificateRefresh()`. The
// `bluetooth_adapter` is considered ready if it can provide its address. On
// BlueZ, it's when the `bluetooth_adapter` is non-null. On Floss, it's when
// the `bluetooth_adapter` is non-null and powered on.
bool IsAdapterReadyToRefreshPrivateCertificates(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  if (!bluetooth_adapter) {
    return false;
  }

  if (floss::features::IsFlossEnabled()) {
    return bluetooth_adapter->IsPowered();
  }

  return true;
}

}  // namespace

// static
NearbyShareCertificateManagerImpl::Factory*
    NearbyShareCertificateManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyShareCertificateManager>
NearbyShareCertificateManagerImpl::Factory::Create(
    NearbyShareLocalDeviceDataManager* local_device_data_manager,
    NearbyShareContactManager* contact_manager,
    NearbyShareProfileInfoProvider* profile_info_provider,
    PrefService* pref_service,
    leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
    const base::FilePath& profile_path,
    NearbyShareClientFactory* client_factory,
    const base::Clock* clock) {
  DCHECK(clock);

  if (test_factory_) {
    return test_factory_->CreateInstance(local_device_data_manager,
                                         contact_manager, profile_info_provider,
                                         pref_service, proto_database_provider,
                                         profile_path, client_factory, clock);
  }

  return base::WrapUnique(new NearbyShareCertificateManagerImpl(
      local_device_data_manager, contact_manager, profile_info_provider,
      pref_service, proto_database_provider, profile_path, client_factory,
      clock));
}

// static
void NearbyShareCertificateManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareCertificateManagerImpl::Factory::~Factory() = default;

NearbyShareCertificateManagerImpl::NearbyShareCertificateManagerImpl(
    NearbyShareLocalDeviceDataManager* local_device_data_manager,
    NearbyShareContactManager* contact_manager,
    NearbyShareProfileInfoProvider* profile_info_provider,
    PrefService* pref_service,
    leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
    const base::FilePath& profile_path,
    NearbyShareClientFactory* client_factory,
    const base::Clock* clock)
    : local_device_data_manager_(local_device_data_manager),
      contact_manager_(contact_manager),
      profile_info_provider_(profile_info_provider),
      pref_service_(pref_service),
      client_factory_(client_factory),
      clock_(clock),
      certificate_storage_(NearbyShareCertificateStorageImpl::Factory::Create(
          pref_service_,
          proto_database_provider,
          profile_path)),
      private_certificate_expiration_scheduler_(
          ash::nearby::NearbySchedulerFactory::CreateExpirationScheduler(
              base::BindRepeating(&NearbyShareCertificateManagerImpl::
                                      NextPrivateCertificateExpirationTime,
                                  base::Unretained(this)),
              /*retry_failures=*/true,
              /*require_connectivity=*/false,
              prefs::
                  kNearbySharingSchedulerPrivateCertificateExpirationPrefName,
              pref_service_,
              base::BindRepeating(&NearbyShareCertificateManagerImpl::
                                      AttemptPrivateCertificateRefresh,
                                  base::Unretained(this)),
              Feature::NS,
              clock_)),
      public_certificate_expiration_scheduler_(
          ash::nearby::NearbySchedulerFactory::CreateExpirationScheduler(
              base::BindRepeating(&NearbyShareCertificateManagerImpl::
                                      NextPublicCertificateExpirationTime,
                                  base::Unretained(this)),
              /*retry_failures=*/true,
              /*require_connectivity=*/false,
              prefs::kNearbySharingSchedulerPublicCertificateExpirationPrefName,
              pref_service_,
              base::BindRepeating(&NearbyShareCertificateManagerImpl::
                                      OnPublicCertificateExpiration,
                                  base::Unretained(this)),
              Feature::NS,
              clock_)),
      upload_local_device_certificates_scheduler_(
          ash::nearby::NearbySchedulerFactory::CreateOnDemandScheduler(
              /*retry_failures=*/true,
              /*require_connectivity=*/true,
              prefs::
                  kNearbySharingSchedulerUploadLocalDeviceCertificatesPrefName,
              pref_service_,
              base::BindRepeating(&NearbyShareCertificateManagerImpl::
                                      OnLocalDeviceCertificateUploadRequest,
                                  base::Unretained(this)),
              Feature::NS,
              clock_)),
      download_public_certificates_scheduler_(
          ash::nearby::NearbySchedulerFactory::CreatePeriodicScheduler(
              kNearbySharePublicCertificateDownloadPeriod,
              /*retry_failures=*/true,
              /*require_connectivity=*/true,
              prefs::kNearbySharingSchedulerDownloadPublicCertificatesPrefName,
              pref_service_,
              base::BindRepeating(&NearbyShareCertificateManagerImpl::
                                      OnDownloadPublicCertificatesRequest,
                                  base::Unretained(this),
                                  /*page_token=*/std::nullopt,
                                  /*page_number=*/1,
                                  /*certificate_count=*/0),
              Feature::NS,
              clock_)) {
  local_device_data_manager_->AddObserver(this);
  contact_manager_->AddObserver(this);

  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&NearbyShareCertificateManagerImpl::OnGetAdapter,
                     weak_ptr_factory_.GetWeakPtr()));
}

NearbyShareCertificateManagerImpl::~NearbyShareCertificateManagerImpl() {
  local_device_data_manager_->RemoveObserver(this);
  contact_manager_->RemoveObserver(this);
}

std::vector<nearby::sharing::proto::PublicCertificate>
NearbyShareCertificateManagerImpl::GetPrivateCertificatesAsPublicCertificates(
    nearby_share::mojom::Visibility visibility) {
  NOTIMPLEMENTED();
  return std::vector<nearby::sharing::proto::PublicCertificate>();
}

void NearbyShareCertificateManagerImpl::GetDecryptedPublicCertificate(
    NearbyShareEncryptedMetadataKey encrypted_metadata_key,
    CertDecryptedCallback callback) {
  certificate_storage_->GetPublicCertificates(
      base::BindOnce(&TryDecryptPublicCertificates,
                     std::move(encrypted_metadata_key), std::move(callback)));
}

void NearbyShareCertificateManagerImpl::DownloadPublicCertificates() {
  download_public_certificates_scheduler_->MakeImmediateRequest();
}

void NearbyShareCertificateManagerImpl::OnStart() {
  private_certificate_expiration_scheduler_->Start();
  public_certificate_expiration_scheduler_->Start();
  upload_local_device_certificates_scheduler_->Start();
  download_public_certificates_scheduler_->Start();
}

void NearbyShareCertificateManagerImpl::OnStop() {
  private_certificate_expiration_scheduler_->Stop();
  public_certificate_expiration_scheduler_->Stop();
  upload_local_device_certificates_scheduler_->Stop();
  download_public_certificates_scheduler_->Stop();
}

std::optional<NearbySharePrivateCertificate>
NearbyShareCertificateManagerImpl::GetValidPrivateCertificate(
    nearby_share::mojom::Visibility visibility) const {
  std::optional<std::vector<NearbySharePrivateCertificate>> certs =
      *certificate_storage_->GetPrivateCertificates();
  for (auto& cert : *certs) {
    if (IsNearbyShareCertificateWithinValidityPeriod(
            clock_->Now(), cert.not_before(), cert.not_after(),
            /*use_public_certificate_tolerance=*/false) &&
        cert.visibility() == visibility) {
      return std::move(cert);
    }
  }

  CD_LOG(WARNING, Feature::NS)
      << __func__ << ": No valid private certificate found with visibility "
      << visibility;
  return std::nullopt;
}

void NearbyShareCertificateManagerImpl::UpdatePrivateCertificateInStorage(
    const NearbySharePrivateCertificate& private_certificate) {
  certificate_storage_->UpdatePrivateCertificate(private_certificate);
}

void NearbyShareCertificateManagerImpl::OnContactsDownloaded(
    const std::set<std::string>& allowed_contact_ids,
    const std::vector<nearby::sharing::proto::ContactRecord>& contacts,
    uint32_t num_unreachable_contacts_filtered_out) {}

void NearbyShareCertificateManagerImpl::OnContactsUploaded(
    bool did_contacts_change_since_last_upload) {
  if (!did_contacts_change_since_last_upload) {
    return;
  }

  // If any of the uploaded contact data--the contact list or the allowlist--has
  // changed since the previous successful upload, recreate certificates. We do
  // not want to continue using the current certificates because they might have
  // been shared with contacts no longer on the contact list or allowlist. NOTE:
  // Ideally, we would only recreate all-contacts visibility certificates when
  // contacts are removed from the contact list, and we would only recreate
  // selected-contacts visibility certificates when contacts are removed from
  // the allowlist, but our information is not that granular.
  certificate_storage_->ClearPrivateCertificates();
  private_certificate_expiration_scheduler_->MakeImmediateRequest();
}

void NearbyShareCertificateManagerImpl::OnLocalDeviceDataChanged(
    bool did_device_name_change,
    bool did_full_name_change,
    bool did_icon_change) {
  if (!did_device_name_change && !did_full_name_change && !did_icon_change) {
    return;
  }

  // Recreate all private certificates to ensure up-to-date metadata.
  certificate_storage_->ClearPrivateCertificates();
  private_certificate_expiration_scheduler_->MakeImmediateRequest();
}

void NearbyShareCertificateManagerImpl::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  // `NearbyShareCertificateManagerImpl` should only be added as an observer
  // of `AdapterPoweredChanged()` if Floss is enabled.
  CHECK(floss::features::IsFlossEnabled());

  if (!powered) {
    return;
  }

  if (is_pending_call_to_refresh_private_certificates_) {
    CD_LOG(VERBOSE, Feature::NS) << __func__
                                 << ": Attempting to execute pending call to "
                                    "refresh private certificates";
    AttemptPrivateCertificateRefresh();
  }
}

void NearbyShareCertificateManagerImpl::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  CHECK(bluetooth_adapter);
  adapter_ = bluetooth_adapter;

  // Private certificate refresh depends on the `BluetoothAdapter`'s address,
  // and on Floss, the address is unavailable if the `adapter_` is powered off.
  // When on Floss, add observer for `AdapterPoweredChanged()` events, for
  // when a refresh of private certificates is requested, and the `adapter_`
  // is powered off. `NearbyShareCertificateManagerImpl` will cache the
  // pending refresh request, and wait for the `adapter_` to be powered on again
  // to execute the pending request. See documentation in declaration of
  // `AttemptPrivateCertificateRefresh()`.
  if (floss::features::IsFlossEnabled()) {
    adapter_observation_.Observe(adapter_.get());
  }

  if (is_pending_call_to_refresh_private_certificates_) {
    CD_LOG(VERBOSE, Feature::NS) << __func__
                                 << ": Attempting to execute pending call to "
                                    "refresh private certificates";
    AttemptPrivateCertificateRefresh();
  }
}

std::optional<base::Time>
NearbyShareCertificateManagerImpl::NextPrivateCertificateExpirationTime() {
  // We enforce that a fixed number--kNearbyShareNumPrivateCertificates for each
  // visibility--of private certificates be present at all times. This might not
  // be true the first time the user enables Nearby Share or after certificates
  // are revoked. For simplicity, consider the case of missing certificates an
  // "expired" state. Return the minimum time to immediately trigger the private
  // certificate creation flow.
  if (certificate_storage_->GetPrivateCertificates()->size() <
      NumExpectedPrivateCertificates()) {
    return base::Time::Min();
  }

  std::optional<base::Time> expiration_time =
      certificate_storage_->NextPrivateCertificateExpirationTime();
  DCHECK(expiration_time);

  return *expiration_time;
}

void NearbyShareCertificateManagerImpl::AttemptPrivateCertificateRefresh() {
  // The `adapter_` will not be ready to refresh the private certificates if
  // it cannot provide its address. This can happen in the following scenarios:
  //   - [BlueZ]: The `adapter_` has not been retrieved yet in the asynchronous
  //     call to `OnGetAdapter()`. In this case, wait until the `adapter_`
  //     is retrieved to refresh the private certificates.
  //   - [Floss]: The `adapter_` has either not been retrieved yet (like
  //     the scenario above on BlueZ), or is not powered on. On Floss, the
  //     `adapter_` needs to be powered on to provide its address. In this case,
  //     wait until the `adapter_` is retrieved and/or powered on to refresh
  //     the private certificates.
  if (!IsAdapterReadyToRefreshPrivateCertificates(adapter_)) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Adapter is not ready to generate private certificates, storing "
           "pending call to refresh certificates until Adapter is ready";
    is_pending_call_to_refresh_private_certificates_ = true;
    return;
  }

  CD_LOG(VERBOSE, Feature::NS)
      << __func__
      << ": Private certificate expiration detected; refreshing certificates.";
  is_pending_call_to_refresh_private_certificates_ = false;
  base::Time now = clock_->Now();
  certificate_storage_->RemoveExpiredPrivateCertificates(now);

  std::vector<NearbySharePrivateCertificate> certs =
      *certificate_storage_->GetPrivateCertificates();
  if (certs.size() == NumExpectedPrivateCertificates()) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": All private certificates are still valid.";
    private_certificate_expiration_scheduler_->HandleResult(/*success=*/true);
    return;
  }

  // Determine how many private certificates of each visibility need to be
  // created, and determine the validity period for the new certificates.
  base::flat_map<nearby_share::mojom::Visibility, size_t> num_valid_certs;
  base::flat_map<nearby_share::mojom::Visibility, base::Time> latest_not_after;
  for (nearby_share::mojom::Visibility visibility : kVisibilities) {
    num_valid_certs[visibility] = 0;
    latest_not_after[visibility] = now;
  }
  for (const NearbySharePrivateCertificate& cert : certs) {
    ++num_valid_certs[cert.visibility()];
    latest_not_after[cert.visibility()] =
        std::max(latest_not_after[cert.visibility()], cert.not_after());
  }

  std::optional<nearby::sharing::proto::EncryptedMetadata> metadata =
      BuildMetadata(local_device_data_manager_->GetDeviceName(),
                    local_device_data_manager_->GetFullName(),
                    local_device_data_manager_->GetIconUrl(),
                    profile_info_provider_->GetProfileUserName(),
                    adapter_.get());
  if (!metadata) {
    CD_LOG(WARNING, Feature::NS)
        << __func__
        << "Failed to create private certificates; cannot create metadata";
    private_certificate_expiration_scheduler_->HandleResult(/*success=*/false);
    return;
  }

  // Add new certificates if necessary. Each visibility should have
  // kNearbyShareNumPrivateCertificates (unless overridden by a command-line
  // switch).
  size_t num_certificates = NumPrivateCertificates();
  CD_LOG(INFO, Feature::NS)
      << __func__ << ": Creating "
      << num_certificates -
             num_valid_certs[nearby_share::mojom::Visibility::kAllContacts]
      << " all-contacts visibility, "
      << num_certificates -
             num_valid_certs[nearby_share::mojom::Visibility::kSelectedContacts]
      << " selected-contacts visibility, and "
      << num_certificates -
             num_valid_certs[nearby_share::mojom::Visibility::kYourDevices]
      << " your-devices private certificates.";

  for (nearby_share::mojom::Visibility visibility : kVisibilities) {
    while (num_valid_certs[visibility] < num_certificates) {
      certs.emplace_back(visibility,
                         /*not_before=*/latest_not_after[visibility],
                         *metadata);
      ++num_valid_certs[visibility];
      latest_not_after[visibility] = certs.back().not_after();
    }
  }

  certificate_storage_->ReplacePrivateCertificates(certs);
  NotifyPrivateCertificatesChanged();
  private_certificate_expiration_scheduler_->HandleResult(/*success=*/true);

  upload_local_device_certificates_scheduler_->MakeImmediateRequest();
}

void NearbyShareCertificateManagerImpl::
    OnLocalDeviceCertificateUploadRequest() {
  std::vector<nearby::sharing::proto::PublicCertificate> public_certs;
  std::vector<NearbySharePrivateCertificate> private_certs =
      *certificate_storage_->GetPrivateCertificates();
  for (const NearbySharePrivateCertificate& private_cert : private_certs) {
    public_certs.push_back(*private_cert.ToPublicCertificate());
  }

  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Uploading local device certificates.";
  local_device_data_manager_->UploadCertificates(
      std::move(public_certs),
      base::BindOnce(&NearbyShareCertificateManagerImpl::
                         OnLocalDeviceCertificateUploadFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyShareCertificateManagerImpl::OnLocalDeviceCertificateUploadFinished(
    bool success) {
  CD_LOG(INFO, Feature::NS)
      << __func__ << ": Upload of local device certificates "
      << (success ? "succeeded" : "failed.");
  upload_local_device_certificates_scheduler_->HandleResult(success);
}

std::optional<base::Time>
NearbyShareCertificateManagerImpl::NextPublicCertificateExpirationTime() {
  std::optional<base::Time> next_expiration_time =
      certificate_storage_->NextPublicCertificateExpirationTime();

  // Supposedly there are no store public certificates.
  if (!next_expiration_time) {
    return std::nullopt;
  }

  // To account for clock skew between devices, we accept public certificates
  // that are slightly past their validity period. This conforms with the
  // GmsCore implementation.
  return *next_expiration_time +
         kNearbySharePublicCertificateValidityBoundOffsetTolerance;
}
void NearbyShareCertificateManagerImpl::OnPublicCertificateExpiration() {
  certificate_storage_->RemoveExpiredPublicCertificates(
      clock_->Now(), base::BindOnce(&NearbyShareCertificateManagerImpl::
                                        OnExpiredPublicCertificatesRemoved,
                                    base::Unretained(this)));
}

void NearbyShareCertificateManagerImpl::OnExpiredPublicCertificatesRemoved(
    bool success) {
  public_certificate_expiration_scheduler_->HandleResult(success);
}

void NearbyShareCertificateManagerImpl::OnDownloadPublicCertificatesRequest(
    std::optional<std::string> page_token,
    size_t page_number,
    size_t certificate_count) {
  DCHECK(!client_);

  nearby::sharing::proto::ListPublicCertificatesRequest request;
  request.set_parent(kDeviceIdPrefix + local_device_data_manager_->GetId());
  if (page_token) {
    request.set_page_token(*page_token);
  }

  // TODO(b/168701170): One Platform has a length restriction on request URLs.
  // Adding all secret IDs to the request, and subsequently as query parameters,
  // could result in hitting this limit. Add the secret IDs of all locally
  // stored public certificates when this length restriction is circumvented.

  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Downloading public certificates.";

  timer_.Start(
      FROM_HERE, kListPublicCertificatesTimeout,
      base::BindOnce(
          &NearbyShareCertificateManagerImpl::OnListPublicCertificatesTimeout,
          base::Unretained(this), page_number, certificate_count));

  client_ = client_factory_->CreateInstance();
  client_->ListPublicCertificates(
      request,
      base::BindOnce(
          &NearbyShareCertificateManagerImpl::OnListPublicCertificatesSuccess,
          base::Unretained(this), page_number, certificate_count),
      base::BindOnce(
          &NearbyShareCertificateManagerImpl::OnListPublicCertificatesFailure,
          base::Unretained(this), page_number, certificate_count));
}

void NearbyShareCertificateManagerImpl::OnListPublicCertificatesSuccess(
    size_t page_number,
    size_t certificate_count,
    const nearby::sharing::proto::ListPublicCertificatesResponse& response) {
  timer_.Stop();

  std::vector<nearby::sharing::proto::PublicCertificate> certs(
      response.public_certificates().begin(),
      response.public_certificates().end());

  std::optional<std::string> page_token =
      response.next_page_token().empty()
          ? std::nullopt
          : std::make_optional(response.next_page_token());

  client_.reset();

  CD_LOG(INFO, Feature::NS)
      << __func__ << ": " << certs.size() << " public certificates downloaded.";
  certificate_storage_->AddPublicCertificates(
      certs, base::BindOnce(&NearbyShareCertificateManagerImpl::
                                OnPublicCertificatesAddedToStorage,
                            base::Unretained(this), page_token, page_number,
                            certificate_count + certs.size()));
}

void NearbyShareCertificateManagerImpl::OnListPublicCertificatesFailure(
    size_t page_number,
    size_t certificate_count,
    ash::nearby::NearbyHttpError error) {
  timer_.Stop();
  client_.reset();

  FinishDownloadPublicCertificates(
      /*success=*/false, ash::nearby::NearbyHttpErrorToResult(error),
      page_number, certificate_count);
}

void NearbyShareCertificateManagerImpl::OnListPublicCertificatesTimeout(
    size_t page_number,
    size_t certificate_count) {
  client_.reset();

  FinishDownloadPublicCertificates(
      /*success=*/false, ash::nearby::NearbyHttpResult::kTimeout, page_number,
      certificate_count);
}

void NearbyShareCertificateManagerImpl::OnPublicCertificatesAddedToStorage(
    std::optional<std::string> page_token,
    size_t page_number,
    size_t certificate_count,
    bool success) {
  if (success && page_token) {
    OnDownloadPublicCertificatesRequest(page_token, page_number + 1,
                                        certificate_count);
  } else {
    FinishDownloadPublicCertificates(success,
                                     ash::nearby::NearbyHttpResult::kSuccess,
                                     page_number, certificate_count);
  }
}

void NearbyShareCertificateManagerImpl::FinishDownloadPublicCertificates(
    bool success,
    ash::nearby::NearbyHttpResult http_result,
    size_t page_number,
    size_t certificate_count) {
  if (success) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Public certificates successfully downloaded and stored.";
    NotifyPublicCertificatesDownloaded();

    // Recompute the expiration timer to account for new certificates.
    public_certificate_expiration_scheduler_->Reschedule();
  } else if (http_result == ash::nearby::NearbyHttpResult::kSuccess) {
    CD_LOG(ERROR, Feature::NS)
        << __func__ << ": Public certificates not stored.";
  } else {
    CD_LOG(ERROR, Feature::NS)
        << __func__ << ": Public certificates download failed with HTTP error: "
        << http_result;
  }
  RecordDownloadPublicCertificatesResultMetrics(success, http_result,
                                                page_number, certificate_count);
  download_public_certificates_scheduler_->HandleResult(success);
}
