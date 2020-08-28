// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_storage.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_private_certificate.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_visibility.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_http_result.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"

class NearbyShareClient;
class NearbyShareClientFactory;
class NearbyShareLocalDeviceDataManager;
class NearbyShareScheduler;
class PrefService;

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace nearbyshare {
namespace proto {
class ListPublicCertificatesResponse;
}  // namespace proto
}  // namespace nearbyshare

// An implementation of the NearbyShareCertificateManager that handles
//   1) creating, storing, and uploading local device certificates, as well as
//      removing expired/revoked local device certificates;
//   2) downloading, storing, and decrypting public certificates from trusted
//      contacts, as well as removing expired public certificates.
//
// TODO(https://crbug.com/1121443): Add the following if we remove
// GetValidPrivateCertificate() and perform all private certificate crypto
// operations internally: "This implementation also provides the high-level
// interface for performing cryptographic operations related to certificates."
class NearbyShareCertificateManagerImpl : public NearbyShareCertificateManager {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyShareCertificateManager> Create(
        NearbyShareLocalDeviceDataManager* local_device_data_manager,
        PrefService* pref_service,
        leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
        const base::FilePath& profile_path,
        NearbyShareClientFactory* client_factory,
        base::Clock* clock = base::DefaultClock::GetInstance());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<NearbyShareCertificateManager> CreateInstance(
        NearbyShareLocalDeviceDataManager* local_device_data_manager,
        PrefService* pref_service,
        leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
        const base::FilePath& profile_path,
        NearbyShareClientFactory* client_factory,
        base::Clock* clock) = 0;

   private:
    static Factory* test_factory_;
  };

  ~NearbyShareCertificateManagerImpl() override;

 private:
  NearbyShareCertificateManagerImpl(
      NearbyShareLocalDeviceDataManager* local_device_data_manager,
      PrefService* pref_service,
      leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
      const base::FilePath& profile_path,
      NearbyShareClientFactory* client_factory,
      base::Clock* clock);

  // NearbyShareCertificateManager:
  NearbySharePrivateCertificate GetValidPrivateCertificate(
      NearbyShareVisibility visibility) override;
  std::vector<nearbyshare::proto::PublicCertificate>
  GetPrivateCertificatesAsPublicCertificates(
      NearbyShareVisibility visibility) override;
  void GetDecryptedPublicCertificate(
      NearbyShareEncryptedMetadataKey encrypted_metadata_key,
      CertDecryptedCallback callback) override;
  void DownloadPublicCertificates() override;
  void OnStart() override;
  void OnStop() override;

  // Removes expired privates certificates, ensures that at least
  // kNearbyShareNumPrivateCertificates are present for each visibility with
  // contiguous validity periods, and uploads any changes to the Nearby Share
  // server.
  base::Time NextPrivateCertificateExpirationTime();
  void OnPrivateCertificateExpiration();
  void FinishPrivateCertificateRefresh(
      std::vector<NearbySharePrivateCertificate> new_certs,
      base::flat_map<NearbyShareVisibility, size_t> num_valid_certs,
      base::flat_map<NearbyShareVisibility, base::Time> latest_not_after,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
  void OnLocalDeviceCertificateUploadRequest();
  void OnLocalDeviceCertificateUploadFinished(bool success);

  void OnDownloadPublicCertificatesRequest(
      base::Optional<std::string> page_token);
  void OnRpcSuccess(
      const nearbyshare::proto::ListPublicCertificatesResponse& response);
  void OnRpcFailure(NearbyShareHttpError error);
  void OnPublicCertificatesAdded(base::Optional<std::string> page_token,
                                 bool success);
  void FinishDownloadPublicCertificates(bool success,
                                        NearbyShareHttpResult http_result);

  NearbyShareLocalDeviceDataManager* local_device_data_manager_ = nullptr;
  PrefService* pref_service_ = nullptr;
  NearbyShareClientFactory* client_factory_ = nullptr;
  base::Clock* clock_ = nullptr;
  std::unique_ptr<NearbyShareScheduler>
      private_certificate_expiration_scheduler_;
  std::unique_ptr<NearbyShareScheduler>
      upload_local_device_certificates_scheduler_;
  std::unique_ptr<NearbyShareScheduler> download_public_certificates_scheduler_;
  std::unique_ptr<NearbyShareCertificateStorage> cert_store_;
  std::unique_ptr<NearbyShareClient> client_;
  base::WeakPtrFactory<NearbyShareCertificateManagerImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_IMPL_H_
