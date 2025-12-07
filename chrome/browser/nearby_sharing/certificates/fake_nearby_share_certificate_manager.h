// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_FAKE_NEARBY_SHARE_CERTIFICATE_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_FAKE_NEARBY_SHARE_CERTIFICATE_MANAGER_H_

#include <array>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager_impl.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_private_certificate.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

// A fake implementation of NearbyShareCertificateManager, along with a fake
// factory, to be used in tests.
class FakeNearbyShareCertificateManager : public NearbyShareCertificateManager {
 public:
  // Factory that creates FakeNearbyShareCertificateManager instances. Use in
  // NearbyShareCertificateManagerImpl::Factor::SetFactoryForTesting() in unit
  // tests.
  class Factory : public NearbyShareCertificateManagerImpl::Factory {
   public:
    Factory();
    ~Factory() override;

    // Returns all FakeNearbyShareCertificateManager instances created by
    // CreateInstance().
    std::vector<raw_ptr<FakeNearbyShareCertificateManager, VectorExperimental>>&
    instances() {
      return instances_;
    }

   private:
    // NearbyShareCertificateManagerImpl::Factory:
    std::unique_ptr<NearbyShareCertificateManager> CreateInstance(
        std::string user_email,
        const base::FilePath& profile_path,
        PrefService* pref_service,
        NearbyShareLocalDeviceDataManager* local_device_data_manager,
        NearbyShareContactManager* contact_manager,
        leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
        NearbyShareClientFactory* client_factory,
        const base::Clock* clock) override;

    std::vector<raw_ptr<FakeNearbyShareCertificateManager, VectorExperimental>>
        instances_;
  };

  class GetDecryptedPublicCertificateCall {
   public:
    GetDecryptedPublicCertificateCall(
        NearbyShareEncryptedMetadataKey encrypted_metadata_key,
        CertDecryptedCallback callback);
    GetDecryptedPublicCertificateCall(
        GetDecryptedPublicCertificateCall&& other);
    GetDecryptedPublicCertificateCall& operator=(
        GetDecryptedPublicCertificateCall&& other);
    GetDecryptedPublicCertificateCall(
        const GetDecryptedPublicCertificateCall&) = delete;
    GetDecryptedPublicCertificateCall& operator=(
        const GetDecryptedPublicCertificateCall&) = delete;
    ~GetDecryptedPublicCertificateCall();

    NearbyShareEncryptedMetadataKey encrypted_metadata_key;
    CertDecryptedCallback callback;
  };

  FakeNearbyShareCertificateManager();
  ~FakeNearbyShareCertificateManager() override;

  // NearbyShareCertificateManager:
  std::vector<nearby::sharing::proto::PublicCertificate>
  GetPrivateCertificatesAsPublicCertificates(
      nearby_share::mojom::Visibility visibility) override;
  void GetDecryptedPublicCertificate(
      NearbyShareEncryptedMetadataKey encrypted_metadata_key,
      CertDecryptedCallback callback) override;
  void DownloadPublicCertificates() override;

  // Make protected methods from base class public in this fake class.
  using NearbyShareCertificateManager::NotifyPrivateCertificatesChanged;
  using NearbyShareCertificateManager::NotifyPublicCertificatesDownloaded;

  void set_next_salt(
      base::span<const uint8_t, kNearbyShareNumBytesMetadataEncryptionKeySalt>
          salt) {
    base::span(next_salt_).copy_from(salt);
  }

  size_t num_get_private_certificates_as_public_certificates_calls() {
    return num_get_private_certificates_as_public_certificates_calls_;
  }

  size_t num_download_public_certificates_calls() {
    return num_download_public_certificates_calls_;
  }

  std::vector<GetDecryptedPublicCertificateCall>&
  get_decrypted_public_certificate_calls() {
    return get_decrypted_public_certificate_calls_;
  }

 private:
  // NearbyShareCertificateManager:
  void OnStart() override;
  void OnStop() override;
  std::optional<NearbySharePrivateCertificate> GetValidPrivateCertificate(
      nearby_share::mojom::Visibility visibility) const override;
  void UpdatePrivateCertificateInStorage(
      const NearbySharePrivateCertificate& private_certificate) override;

  size_t num_get_private_certificates_as_public_certificates_calls_ = 0;
  size_t num_download_public_certificates_calls_ = 0;
  std::vector<GetDecryptedPublicCertificateCall>
      get_decrypted_public_certificate_calls_;
  std::array<uint8_t, kNearbyShareNumBytesMetadataEncryptionKeySalt> next_salt_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_FAKE_NEARBY_SHARE_CERTIFICATE_MANAGER_H_
