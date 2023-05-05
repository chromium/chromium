// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_storage.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_private_certificate.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class NearbyShareClient;
class NearbyShareClientFactory;
class NearbyShareLocalDeviceDataManager;
class NearbyShareProfileInfoProvider;
class PrefService;

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace ash::nearby {
class NearbyScheduler;
}  // namespace ash::nearby

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
// This implementation destroys and recreates all private certificates if there
// are any changes to the user's contact list or allowlist, or if there are any
// changes to the local device data, such as the device name.
class NearbyShareCertificateManagerImpl
    : public NearbyShareCertificateManager,
      public NearbyShareContactManager::Observer,
      public NearbyShareLocalDeviceDataManager::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyShareCertificateManager> Create(
        NearbyShareLocalDeviceDataManager* local_device_data_manager,
        NearbyShareContactManager* contact_manager,
        NearbyShareProfileInfoProvider* profile_info_provider,
        PrefService* pref_service,
        leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
        const base::FilePath& profile_path,
        NearbyShareClientFactory* client_factory,
        const base::Clock* clock = base::DefaultClock::GetInstance());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<NearbyShareCertificateManager> CreateInstance(
        NearbyShareLocalDeviceDataManager* local_device_data_manager,
        NearbyShareContactManager* contact_manager,
        NearbyShareProfileInfoProvider* profile_info_provider,
        PrefService* pref_service,
        leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
        const base::FilePath& profile_path,
        NearbyShareClientFactory* client_factory,
        const base::Clock* clock) = 0;

   private:
    static Factory* test_factory_;
  };

  ~NearbyShareCertificateManagerImpl() override;

 private:
  NearbyShareCertificateManagerImpl(
      NearbyShareLocalDeviceDataManager* local_device_data_manager,
      NearbyShareContactManager* contact_manager,
      NearbyShareProfileInfoProvider* profile_info_provider,
      PrefService* pref_service,
      leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
      const base::FilePath& profile_path,
      NearbyShareClientFactory* client_factory,
      const base::Clock* clock);

  // NearbyShareCertificateManager:
  std::vector<nearbyshare::proto::PublicCertificate>
  GetPrivateCertificatesAsPublicCertificates(
      nearby_share::mojom::Visibility visibility) override;
  void GetDecryptedPublicCertificate(
      NearbyShareEncryptedMetadataKey encrypted_metadata_key,
      CertDecryptedCallback callback) override;
  void DownloadPublicCertificates() override;
  void OnStart() override;
  void OnStop() override;
  absl::optional<NearbySharePrivateCertificate> GetValidPrivateCertificate(
      nearby_share::mojom::Visibility visibility) const override;
  void UpdatePrivateCertificateInStorage(
      const NearbySharePrivateCertificate& private_certificate) override;

  // NearbyShareContactManager::Observer:
  void OnContactsDownloaded(
      const std::set<std::string>& allowed_contact_ids,
      const std::vector<nearbyshare::proto::ContactRecord>& contacts,
      uint32_t num_unreachable_contacts_filtered_out) override;
  void OnContactsUploaded(bool did_contacts_change_since_last_upload) override;

  // NearbyShareLocalDeviceDataManager::Observer:
  void OnLocalDeviceDataChanged(bool did_device_name_change,
                                bool did_full_name_change,
                                bool did_icon_change) override;

  // Used by the private certificate expiration scheduler to determine the next
  // private certificate expiration time. Returns base::Time::Min() if
  // certificates are missing. This function never returns absl::nullopt.
  absl::optional<base::Time> NextPrivateCertificateExpirationTime();

  // Used by the public certificate expiration scheduler to determine the next
  // public certificate expiration time. Returns absl::nullopt if no public
  // certificates are present, and no expiration event is scheduled.
  absl::optional<base::Time> NextPublicCertificateExpirationTime();

  // Invoked by the private certificate expiration scheduler when an expired
  // private certificate needs to be removed or if no private certificates exist
  // yet. New certificate(s) will be created, and an upload to the Nearby Share
  // server will be requested.
  void OnPrivateCertificateExpiration();

  void FinishPrivateCertificateRefresh(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);

  // Invoked by the certificate upload scheduler when private certificates need
  // to be converted to public certificates and uploaded to the Nearby Share
  // server.
  void OnLocalDeviceCertificateUploadRequest();

  void OnLocalDeviceCertificateUploadFinished(bool success);

  // Invoked by the public certificate expiration scheduler when an expired
  // public certificate needs to be removed from storage.
  void OnPublicCertificateExpiration();

  void OnExpiredPublicCertificatesRemoved(bool success);

  // Invoked by the certificate download scheduler when the public certificates
  // from trusted contacts need to be downloaded from Nearby Share server via
  // the ListPublicCertificates RPC.
  void OnDownloadPublicCertificatesRequest(
      absl::optional<std::string> page_token,
      size_t page_number,
      size_t certificate_count);

  void OnListPublicCertificatesSuccess(
      size_t page_number,
      size_t certificate_count,
      const nearbyshare::proto::ListPublicCertificatesResponse& response);
  void OnListPublicCertificatesFailure(size_t page_number,
                                       size_t certificate_count,
                                       ash::nearby::NearbyHttpError error);
  void OnListPublicCertificatesTimeout(size_t page_number,
                                       size_t certificate_count);
  void OnPublicCertificatesAddedToStorage(
      absl::optional<std::string> page_token,
      size_t page_number,
      size_t certificate_count,
      bool success);
  void FinishDownloadPublicCertificates(
      bool success,
      ash::nearby::NearbyHttpResult http_result,
      size_t page_number,
      size_t certificate_count);

  base::OneShotTimer timer_;
  raw_ptr<NearbyShareLocalDeviceDataManager, ExperimentalAsh>
      local_device_data_manager_ = nullptr;
  raw_ptr<NearbyShareContactManager, ExperimentalAsh> contact_manager_ =
      nullptr;
  raw_ptr<NearbyShareProfileInfoProvider, ExperimentalAsh>
      profile_info_provider_ = nullptr;
  raw_ptr<PrefService, ExperimentalAsh> pref_service_ = nullptr;
  raw_ptr<NearbyShareClientFactory, ExperimentalAsh> client_factory_ = nullptr;
  raw_ptr<const base::Clock, ExperimentalAsh> clock_;
  std::unique_ptr<NearbyShareCertificateStorage> certificate_storage_;
  std::unique_ptr<ash::nearby::NearbyScheduler>
      private_certificate_expiration_scheduler_;
  std::unique_ptr<ash::nearby::NearbyScheduler>
      public_certificate_expiration_scheduler_;
  std::unique_ptr<ash::nearby::NearbyScheduler>
      upload_local_device_certificates_scheduler_;
  std::unique_ptr<ash::nearby::NearbyScheduler>
      download_public_certificates_scheduler_;
  std::unique_ptr<NearbyShareClient> client_;
  base::WeakPtrFactory<NearbyShareCertificateManagerImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_IMPL_H_
