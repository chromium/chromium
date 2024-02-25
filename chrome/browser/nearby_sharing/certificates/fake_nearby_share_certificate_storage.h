// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_FAKE_NEARBY_SHARE_CERTIFICATE_STORAGE_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_FAKE_NEARBY_SHARE_CERTIFICATE_STORAGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_storage.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_storage_impl.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

class PrefService;

// A fake implementation of NearbyShareCertificateStorage, along with a fake
// factory, to be used in tests.
class FakeNearbyShareCertificateStorage : public NearbyShareCertificateStorage {
 public:
  // Factory that creates FakeNearbyShareCertificateStorage instances. Use
  // in NearbyShareCertificateStorageImpl::Factory::SetFactoryForTesting()
  // in unit tests.
  class Factory : public NearbyShareCertificateStorageImpl::Factory {
   public:
    Factory();
    ~Factory() override;

    // Returns all FakeNearbyShareCertificateStorage instances created by
    // CreateInstance().
    std::vector<raw_ptr<FakeNearbyShareCertificateStorage, VectorExperimental>>&
    instances() {
      return instances_;
    }

    PrefService* latest_pref_service() { return latest_pref_service_; }

    leveldb_proto::ProtoDatabaseProvider* latest_proto_database_provider() {
      return latest_proto_database_provider_;
    }

    const base::FilePath& latest_profile_path() { return latest_profile_path_; }

   private:
    // NearbyShareCertificateStorageImpl::Factory:
    std::unique_ptr<NearbyShareCertificateStorage> CreateInstance(
        PrefService* pref_service,
        leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
        const base::FilePath& profile_path) override;

    std::vector<raw_ptr<FakeNearbyShareCertificateStorage, VectorExperimental>>
        instances_;
    raw_ptr<PrefService, DanglingUntriaged> latest_pref_service_ = nullptr;
    raw_ptr<leveldb_proto::ProtoDatabaseProvider>
        latest_proto_database_provider_ = nullptr;
    base::FilePath latest_profile_path_;
  };

  struct ReplacePublicCertificatesCall {
    ReplacePublicCertificatesCall(
        const std::vector<nearby::sharing::proto::PublicCertificate>&
            public_certificates,
        ResultCallback callback);
    ReplacePublicCertificatesCall(ReplacePublicCertificatesCall&& other);
    ~ReplacePublicCertificatesCall();

    std::vector<nearby::sharing::proto::PublicCertificate> public_certificates;
    ResultCallback callback;
  };

  struct AddPublicCertificatesCall {
    AddPublicCertificatesCall(
        const std::vector<nearby::sharing::proto::PublicCertificate>&
            public_certificates,
        ResultCallback callback);
    AddPublicCertificatesCall(AddPublicCertificatesCall&& other);
    ~AddPublicCertificatesCall();

    std::vector<nearby::sharing::proto::PublicCertificate> public_certificates;
    ResultCallback callback;
  };

  struct RemoveExpiredPublicCertificatesCall {
    RemoveExpiredPublicCertificatesCall(base::Time now,
                                        ResultCallback callback);
    RemoveExpiredPublicCertificatesCall(
        RemoveExpiredPublicCertificatesCall&& other);
    ~RemoveExpiredPublicCertificatesCall();

    base::Time now;
    ResultCallback callback;
  };

  FakeNearbyShareCertificateStorage();
  ~FakeNearbyShareCertificateStorage() override;

  // NearbyShareCertificateStorage:
  void GetPublicCertificates(PublicCertificateCallback callback) override;
  std::optional<std::vector<NearbySharePrivateCertificate>>
  GetPrivateCertificates() const override;
  std::optional<base::Time> NextPublicCertificateExpirationTime()
      const override;
  void ReplacePrivateCertificates(
      const std::vector<NearbySharePrivateCertificate>& private_certificates)
      override;
  void AddPublicCertificates(
      const std::vector<nearby::sharing::proto::PublicCertificate>&
          public_certificates,
      ResultCallback callback) override;
  void RemoveExpiredPublicCertificates(base::Time now,
                                       ResultCallback callback) override;

  void SetPublicCertificateIds(const std::vector<std::string>& ids);
  void SetNextPublicCertificateExpirationTime(std::optional<base::Time> time);

  std::vector<PublicCertificateCallback>& get_public_certificates_callbacks() {
    return get_public_certificates_callbacks_;
  }

  std::vector<ReplacePublicCertificatesCall>&
  replace_public_certificates_calls() {
    return replace_public_certificates_calls_;
  }

  std::vector<AddPublicCertificatesCall>& add_public_certificates_calls() {
    return add_public_certificates_calls_;
  }

  std::vector<RemoveExpiredPublicCertificatesCall>&
  remove_expired_public_certificates_calls() {
    return remove_expired_public_certificates_calls_;
  }

  std::vector<ResultCallback>& clear_public_certificates_callbacks() {
    return clear_public_certificates_callbacks_;
  }

 private:
  std::optional<base::Time> next_public_certificate_expiration_time_;
  std::vector<std::string> public_certificate_ids_;
  std::optional<std::vector<NearbySharePrivateCertificate>>
      private_certificates_;
  std::vector<PublicCertificateCallback> get_public_certificates_callbacks_;
  std::vector<ReplacePublicCertificatesCall> replace_public_certificates_calls_;
  std::vector<AddPublicCertificatesCall> add_public_certificates_calls_;
  std::vector<RemoveExpiredPublicCertificatesCall>
      remove_expired_public_certificates_calls_;
  std::vector<ResultCallback> clear_public_certificates_callbacks_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_FAKE_NEARBY_SHARE_CERTIFICATE_STORAGE_H_
