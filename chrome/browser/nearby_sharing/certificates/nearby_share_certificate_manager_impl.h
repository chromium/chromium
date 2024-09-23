// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_IMPL_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_storage.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_private_certificate.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

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

namespace nearby::sharing::proto {
class ListPublicCertificatesResponse;
}  // namespace nearby::sharing::proto

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
      public NearbyShareLocalDeviceDataManager::Observer,
      public device::BluetoothAdapter::Observer {
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
  std::vector<nearby::sharing::proto::PublicCertificate>
  GetPrivateCertificatesAsPublicCertificates(
      nearby_share::mojom::Visibility visibility) override;
  void GetDecryptedPublicCertificate(
      NearbyShareEncryptedMetadataKey encrypted_metadata_key,
      CertDecryptedCallback callback) override;
  void DownloadPublicCertificates() override;
  void OnStart() override;
  void OnStop() override;
  std::optional<NearbySharePrivateCertificate> GetValidPrivateCertificate(
      nearby_share::mojom::Visibility visibility) const override;
  void UpdatePrivateCertificateInStorage(
      const NearbySharePrivateCertificate& private_certificate) override;

  // NearbyShareContactManager::Observer:
  void OnContactsDownloaded(
      const std::set<std::string>& allowed_contact_ids,
      const std::vector<nearby::sharing::proto::ContactRecord>& contacts,
      uint32_t num_unreachable_contacts_filtered_out) override;
  void OnContactsUploaded(bool did_contacts_change_since_last_upload) override;

  // NearbyShareLocalDeviceDataManager::Observer:
  void OnLocalDeviceDataChanged(bool did_device_name_change,
                                bool did_full_name_change,
                                bool did_icon_change) override;

  // device::BluetoothAdapter::Observer:
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);

  // Used by the private certificate expiration scheduler to determine the next
  // private certificate expiration time. Returns base::Time::Min() if
  // certificates are missing. This function never returns std::nullopt.
  std::optional<base::Time> NextPrivateCertificateExpirationTime();

  // Used by the public certificate expiration scheduler to determine the next
  // public certificate expiration time. Returns std::nullopt if no public
  // certificates are present, and no expiration event is scheduled.
  std::optional<base::Time> NextPublicCertificateExpirationTime();

  // Invoked by the private certificate expiration scheduler when an expired
  // private certificate needs to be removed or if no private certificates exist
  // yet. New certificate(s) will be created, and an upload to the Nearby Share
  // server will be requested. If the `adapter_` field is not ready to refresh
  // private certificates NearbyShareCertificateManagerImpl` stores a pending
  // call to refresh the private certificates until the `adapter_` is ready:
  //     - Case 1: [BlueZ] the `adapter_` is ready when the `BluetoothAdapter`
  //       has been retrieved in  `OnGetAdapter()`.
  //     - Case 2: [Floss] the `adapter_` is ready when the `BluetoothAdapter`
  //       has been retrieved in  `OnGetAdapter()` and the `adapter_` is
  //       powered on.
  //     - Case 3: [Floss] if the `BluetoothAdapter` has been retrieved in
  //       `OnGetAdapter()` and the `adapter_` is powered off,
  //       `NearbyShareCertificateManagerImpl` adds itself as a
  //       `AdapterPoweredChanged()` observer, and waits for the `adapter_` to
  //       be powered on. Then, the `adapter_` is ready.
  // Once the `adapter_` is ready as outlined above, a call to
  // `AttemptPrivateCertificateRefresh()` is fired to flush the pending
  // private certificate refresh.
  void AttemptPrivateCertificateRefresh();

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
      std::optional<std::string> page_token,
      size_t page_number,
      size_t certificate_count);

  void OnListPublicCertificatesSuccess(
      size_t page_number,
      size_t certificate_count,
      const nearby::sharing::proto::ListPublicCertificatesResponse& response);
  void OnListPublicCertificatesFailure(size_t page_number,
                                       size_t certificate_count,
                                       ash::nearby::NearbyHttpError error);
  void OnListPublicCertificatesTimeout(size_t page_number,
                                       size_t certificate_count);
  void OnPublicCertificatesAddedToStorage(std::optional<std::string> page_token,
                                          size_t page_number,
                                          size_t certificate_count,
                                          bool success);
  void FinishDownloadPublicCertificates(
      bool success,
      ash::nearby::NearbyHttpResult http_result,
      size_t page_number,
      size_t certificate_count);

  base::OneShotTimer timer_;
  raw_ptr<NearbyShareLocalDeviceDataManager> local_device_data_manager_ =
      nullptr;
  raw_ptr<NearbyShareContactManager> contact_manager_ = nullptr;
  raw_ptr<NearbyShareProfileInfoProvider> profile_info_provider_ = nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;
  raw_ptr<NearbyShareClientFactory> client_factory_ = nullptr;
  raw_ptr<const base::Clock> clock_;
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

  // See documentation in declaration of `AttemptPrivateCertificateRefresh()`.
  // `adapter_` is set asynchronously in a call to `GetBluetoothAdapter()`
  // during the construction of `NearbyShareCertificateManagerImpl`. If
  // private certificates refreshes are requested when `adapter_` is not ready,
  // store a pending call via
  // `is_pending_call_to_refresh_private_certificates_`.
  scoped_refptr<device::BluetoothAdapter> adapter_;
  bool is_pending_call_to_refresh_private_certificates_ = false;
  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};

  base::WeakPtrFactory<NearbyShareCertificateManagerImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_IMPL_H_
