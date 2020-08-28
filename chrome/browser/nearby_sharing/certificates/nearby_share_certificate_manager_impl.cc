// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager_impl.h"

#include <array>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/certificates/common.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_storage_impl.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_client.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_http_result.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/proto/certificate_rpc.pb.h"
#include "chrome/browser/nearby_sharing/proto/encrypted_metadata.pb.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler_factory.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/prefs/pref_service.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"

namespace {

const char kDeviceIdPrefix[] = "users/me/devices/";

constexpr std::array<NearbyShareVisibility, 2> kVisibilities = {
    NearbyShareVisibility::kAllContacts,
    NearbyShareVisibility::kSelectedContacts};

void TryDecryptPublicCertificates(
    const NearbyShareEncryptedMetadataKey& encrypted_metadata_key,
    NearbyShareCertificateManager::CertDecryptedCallback callback,
    bool success,
    std::unique_ptr<std::vector<nearbyshare::proto::PublicCertificate>>
        public_certificates) {
  if (!success || !public_certificates) {
    NS_LOG(ERROR) << __func__
                  << ": Failed to read public certificates from storage.";
    std::move(callback).Run(base::nullopt);
    return;
  }

  for (const auto& cert : *public_certificates) {
    base::Optional<NearbyShareDecryptedPublicCertificate> decrypted =
        NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
            cert, encrypted_metadata_key);
    if (decrypted) {
      NS_LOG(VERBOSE) << __func__
                      << ": Successfully decrypted public certificate with ID "
                      << base::HexEncode(decrypted->id());
      std::move(callback).Run(std::move(decrypted));
      return;
    }
  }
  NS_LOG(VERBOSE)
      << __func__
      << ": Metadata key could not decrypt any public certificates.";
  std::move(callback).Run(base::nullopt);
}

void RecordResultMetrics(NearbyShareHttpResult result) {
  // TODO(crbug.com/1105579): Record a histogram value for each result.
}

}  // namespace

// static
NearbyShareCertificateManagerImpl::Factory*
    NearbyShareCertificateManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyShareCertificateManager>
NearbyShareCertificateManagerImpl::Factory::Create(
    NearbyShareLocalDeviceDataManager* local_device_data_manager,
    PrefService* pref_service,
    leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
    const base::FilePath& profile_path,
    NearbyShareClientFactory* client_factory,
    base::Clock* clock) {
  DCHECK(clock);

  if (test_factory_) {
    return test_factory_->CreateInstance(local_device_data_manager,
                                         pref_service, proto_database_provider,
                                         profile_path, client_factory, clock);
  }

  return base::WrapUnique(new NearbyShareCertificateManagerImpl(
      local_device_data_manager, pref_service, proto_database_provider,
      profile_path, client_factory, clock));
}

// static
void NearbyShareCertificateManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareCertificateManagerImpl::Factory::~Factory() = default;

NearbyShareCertificateManagerImpl::NearbyShareCertificateManagerImpl(
    NearbyShareLocalDeviceDataManager* local_device_data_manager,
    PrefService* pref_service,
    leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
    const base::FilePath& profile_path,
    NearbyShareClientFactory* client_factory,
    base::Clock* clock)
    : local_device_data_manager_(local_device_data_manager),
      pref_service_(pref_service),
      client_factory_(client_factory),
      clock_(clock),
      private_certificate_expiration_scheduler_(
          NearbyShareSchedulerFactory::CreateExpirationScheduler(
              base::BindRepeating(&NearbyShareCertificateManagerImpl::
                                      NextPrivateCertificateExpirationTime,
                                  base::Unretained(this)),
              /*retry_failures=*/true,
              /*require_connectivity=*/false,
              prefs::
                  kNearbySharingSchedulerPrivateCertificateExpirationPrefName,
              pref_service_,
              base::BindRepeating(&NearbyShareCertificateManagerImpl::
                                      OnPrivateCertificateExpiration,
                                  base::Unretained(this)),
              clock_)),
      upload_local_device_certificates_scheduler_(
          NearbyShareSchedulerFactory::CreateOnDemandScheduler(
              /*retry_failures=*/true,
              /*require_connectivity=*/true,
              prefs::
                  kNearbySharingSchedulerUploadLocalDeviceCertificatesPrefName,
              pref_service_,
              base::BindRepeating(&NearbyShareCertificateManagerImpl::
                                      OnLocalDeviceCertificateUploadRequest,
                                  base::Unretained(this)),
              clock_)),
      download_public_certificates_scheduler_(
          NearbyShareSchedulerFactory::CreatePeriodicScheduler(
              kNearbySharePublicCertificateDownloadPeriod,
              /*retry_failures=*/true,
              /*require_connectivity=*/true,
              prefs::kNearbySharingSchedulerDownloadPublicCertificatesPrefName,
              pref_service_,
              base::BindRepeating(&NearbyShareCertificateManagerImpl::
                                      OnDownloadPublicCertificatesRequest,
                                  base::Unretained(this),
                                  /*page_token=*/base::nullopt),
              clock_)),
      cert_store_(NearbyShareCertificateStorageImpl::Factory::Create(
          pref_service_,
          proto_database_provider,
          profile_path)) {}

NearbyShareCertificateManagerImpl::~NearbyShareCertificateManagerImpl() =
    default;

NearbySharePrivateCertificate
NearbyShareCertificateManagerImpl::GetValidPrivateCertificate(
    NearbyShareVisibility visibility) {
  std::vector<NearbySharePrivateCertificate> certs =
      *cert_store_->GetPrivateCertificates();
  for (auto& cert : certs) {
    if (IsNearbyShareCertificateWithinValidityPeriod(
            clock_->Now(), cert.not_before(), cert.not_after(),
            /*use_public_certificate_tolerance=*/false) &&
        cert.visibility() == visibility) {
      return std::move(cert);
    }
  }
  NOTREACHED();
  NS_LOG(ERROR) << __func__
                << ": No valid private certificate found with visibility "
                << static_cast<int>(visibility);
  return NearbySharePrivateCertificate(NearbyShareVisibility::kNoOne,
                                       /*not_before=*/base::Time(),
                                       nearbyshare::proto::EncryptedMetadata());
}

std::vector<nearbyshare::proto::PublicCertificate>
NearbyShareCertificateManagerImpl::GetPrivateCertificatesAsPublicCertificates(
    NearbyShareVisibility visibility) {
  NOTIMPLEMENTED();
  return std::vector<nearbyshare::proto::PublicCertificate>();
}

void NearbyShareCertificateManagerImpl::GetDecryptedPublicCertificate(
    NearbyShareEncryptedMetadataKey encrypted_metadata_key,
    CertDecryptedCallback callback) {
  cert_store_->GetPublicCertificates(
      base::BindOnce(&TryDecryptPublicCertificates,
                     std::move(encrypted_metadata_key), std::move(callback)));
}

void NearbyShareCertificateManagerImpl::DownloadPublicCertificates() {
  download_public_certificates_scheduler_->MakeImmediateRequest();
}

void NearbyShareCertificateManagerImpl::OnStart() {
  private_certificate_expiration_scheduler_->Start();
  upload_local_device_certificates_scheduler_->Start();
  download_public_certificates_scheduler_->Start();
}

void NearbyShareCertificateManagerImpl::OnStop() {
  private_certificate_expiration_scheduler_->Stop();
  upload_local_device_certificates_scheduler_->Stop();
  download_public_certificates_scheduler_->Stop();
}

base::Time
NearbyShareCertificateManagerImpl::NextPrivateCertificateExpirationTime() {
  // If no private certificates are present, return the minimum time to trigger
  // an immediate refresh.
  return cert_store_->NextPrivateCertificateExpirationTime().value_or(
      base::Time::Min());
}

void NearbyShareCertificateManagerImpl::OnPrivateCertificateExpiration() {
  NS_LOG(VERBOSE)
      << __func__
      << ": Private certificate expiration detected; refreshing certificates.";
  base::Time now = clock_->Now();
  base::flat_map<NearbyShareVisibility, size_t> num_valid_certs;
  base::flat_map<NearbyShareVisibility, base::Time> latest_not_after;
  for (NearbyShareVisibility visibility : kVisibilities) {
    num_valid_certs[visibility] = 0;
    latest_not_after[visibility] = now;
  }

  // Remove all expired certificates.
  std::vector<NearbySharePrivateCertificate> old_certs =
      *cert_store_->GetPrivateCertificates();
  std::vector<NearbySharePrivateCertificate> new_certs;
  for (const NearbySharePrivateCertificate& cert : old_certs) {
    if (IsNearbyShareCertificateExpired(
            now, cert.not_after(),
            /*use_public_certificate_tolerance=*/false)) {
      continue;
    }
    ++num_valid_certs[cert.visibility()];
    latest_not_after[cert.visibility()] =
        std::max(latest_not_after[cert.visibility()], cert.not_after());
    new_certs.push_back(cert);
  }

  if (!old_certs.empty() && new_certs.size() == old_certs.size()) {
    NS_LOG(VERBOSE) << __func__
                    << ": All private certificates are still valid.";
    private_certificate_expiration_scheduler_->HandleResult(/*success=*/true);
    return;
  }

  device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &NearbyShareCertificateManagerImpl::FinishPrivateCertificateRefresh,
      weak_ptr_factory_.GetWeakPtr(), std::move(new_certs),
      std::move(num_valid_certs), std::move(latest_not_after)));
}

void NearbyShareCertificateManagerImpl::FinishPrivateCertificateRefresh(
    std::vector<NearbySharePrivateCertificate> new_certs,
    base::flat_map<NearbyShareVisibility, size_t> num_valid_certs,
    base::flat_map<NearbyShareVisibility, base::Time> latest_not_after,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  nearbyshare::proto::EncryptedMetadata metadata;

  base::Optional<std::string> device_name =
      local_device_data_manager_->GetDeviceName();
  if (!device_name) {
    NS_LOG(WARNING)
        << __func__
        << ": Cannot create private certificates; missing device name.";
    private_certificate_expiration_scheduler_->HandleResult(/*success=*/false);
    return;
  }
  metadata.set_device_name(*device_name);

  base::Optional<std::string> full_name =
      local_device_data_manager_->GetFullName();
  base::Optional<std::string> icon_url =
      local_device_data_manager_->GetIconUrl();
  if (full_name) {
    metadata.set_full_name(*full_name);
  }
  if (icon_url) {
    metadata.set_icon_url(*icon_url);
  }

  std::array<uint8_t, 6> bytes;
  if (bluetooth_adapter &&
      device::ParseBluetoothAddress(bluetooth_adapter->GetAddress(), bytes)) {
    metadata.set_bluetooth_mac_address(std::string(bytes.begin(), bytes.end()));
  } else {
    NS_LOG(WARNING) << __func__
                    << ": No valid Bluetooth MAC available during private "
                    << "certificate creation.";
    // TODO(https://crbug.com/1122641): Decide the best way to handle
    // missing/invalid Bluetooth MAC addresses. Also, log a metric to track how
    // often this happens.
  }

  NS_LOG(VERBOSE)
      << __func__ << ": Creating "
      << kNearbyShareNumPrivateCertificates -
             num_valid_certs[NearbyShareVisibility::kAllContacts]
      << " all-contacts visibility and "
      << kNearbyShareNumPrivateCertificates -
             num_valid_certs[NearbyShareVisibility::kSelectedContacts]
      << " selected-contacts visibility private certificates.";
  // Add new certificates if necessary. Each visibility should have
  // kNearbyShareNumPrivateCertificates.
  for (NearbyShareVisibility visibility : kVisibilities) {
    while (num_valid_certs[visibility] < kNearbyShareNumPrivateCertificates) {
      new_certs.emplace_back(
          visibility, /*not_before=*/latest_not_after[visibility], metadata);
      ++num_valid_certs[visibility];
      latest_not_after[visibility] = new_certs.back().not_after();
    }
  }

  cert_store_->ReplacePrivateCertificates(new_certs);
  NotifyPrivateCertificatesChanged();
  private_certificate_expiration_scheduler_->HandleResult(/*success=*/true);

  upload_local_device_certificates_scheduler_->MakeImmediateRequest();
}

void NearbyShareCertificateManagerImpl::
    OnLocalDeviceCertificateUploadRequest() {
  std::vector<nearbyshare::proto::PublicCertificate> public_certs;
  std::vector<NearbySharePrivateCertificate> private_certs =
      *cert_store_->GetPrivateCertificates();
  for (const NearbySharePrivateCertificate& private_cert : private_certs) {
    public_certs.push_back(*private_cert.ToPublicCertificate());
  }

  NS_LOG(VERBOSE) << __func__ << ": Uploading local device certificates.";
  local_device_data_manager_->UploadCertificates(
      std::move(public_certs),
      base::BindOnce(&NearbyShareCertificateManagerImpl::
                         OnLocalDeviceCertificateUploadFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyShareCertificateManagerImpl::OnLocalDeviceCertificateUploadFinished(
    bool success) {
  NS_LOG(VERBOSE) << __func__ << ": Upload of local device certificates "
                  << (success ? "succeeded" : "failed.");
  upload_local_device_certificates_scheduler_->HandleResult(success);
}

void NearbyShareCertificateManagerImpl::OnDownloadPublicCertificatesRequest(
    base::Optional<std::string> page_token) {
  DCHECK(!client_);

  nearbyshare::proto::ListPublicCertificatesRequest request;
  request.set_parent(kDeviceIdPrefix + local_device_data_manager_->GetId());
  if (page_token)
    request.set_page_token(*page_token);

  for (const std::string& id : cert_store_->GetPublicCertificateIds()) {
    request.add_secret_ids(id);
  }

  NS_LOG(VERBOSE) << __func__ << ": Downloading public certificates.";
  // TODO(https://crbug.com/1116910): Enforce a timeout for each
  // ListPublicCertificates call.
  client_ = client_factory_->CreateInstance();
  client_->ListPublicCertificates(
      request,
      base::BindOnce(&NearbyShareCertificateManagerImpl::OnRpcSuccess,
                     base::Unretained(this)),
      base::BindOnce(&NearbyShareCertificateManagerImpl::OnRpcFailure,
                     base::Unretained(this)));
}

void NearbyShareCertificateManagerImpl::OnRpcSuccess(
    const nearbyshare::proto::ListPublicCertificatesResponse& response) {
  std::vector<nearbyshare::proto::PublicCertificate> certs(
      response.public_certificates().begin(),
      response.public_certificates().end());

  base::Optional<std::string> page_token =
      response.next_page_token().empty()
          ? base::nullopt
          : base::make_optional(response.next_page_token());

  client_.reset();

  NS_LOG(VERBOSE) << __func__ << ": " << certs.size()
                  << " public certificates downloaded.";
  cert_store_->AddPublicCertificates(
      certs, base::BindOnce(
                 &NearbyShareCertificateManagerImpl::OnPublicCertificatesAdded,
                 base::Unretained(this), page_token));
}

void NearbyShareCertificateManagerImpl::OnRpcFailure(
    NearbyShareHttpError error) {
  client_.reset();

  FinishDownloadPublicCertificates(/*success=*/false,
                                   NearbyShareHttpErrorToResult(error));
}

void NearbyShareCertificateManagerImpl::OnPublicCertificatesAdded(
    base::Optional<std::string> page_token,
    bool success) {
  if (success && page_token) {
    OnDownloadPublicCertificatesRequest(page_token);
  } else {
    FinishDownloadPublicCertificates(success, NearbyShareHttpResult::kSuccess);
  }
}

void NearbyShareCertificateManagerImpl::FinishDownloadPublicCertificates(
    bool success,
    NearbyShareHttpResult http_result) {
  if (success) {
    NS_LOG(VERBOSE)
        << __func__
        << ": Public certificates successfully downloaded and stored.";
    NotifyPublicCertificatesDownloaded();
  } else if (http_result == NearbyShareHttpResult::kSuccess) {
    NS_LOG(ERROR) << __func__ << ": Public certificates not stored.";
  } else {
    NS_LOG(ERROR) << __func__
                  << ": Public certificates download failed with HTTP error: "
                  << http_result;
  }
  RecordResultMetrics(http_result);
  download_public_certificates_scheduler_->HandleResult(success);
}
