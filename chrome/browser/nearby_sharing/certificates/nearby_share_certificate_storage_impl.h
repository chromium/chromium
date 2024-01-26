// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_STORAGE_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_STORAGE_IMPL_H_

#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_storage.h"
#include "components/leveldb_proto/public/proto_database.h"

class NearbySharePrivateCertificate;
class PrefService;

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace nearby::sharing::proto {
class PublicCertificate;
}  // namespace nearby::sharing::proto

// Implements NearbyShareCertificateStorage using Prefs to store private
// certificates and LevelDB Proto to store public certificates. Must be
// initialized by calling Initialize before retrieving or storing certificates.
// Callbacks are guaranteed to not be invoked after
// NearbyShareCertificateStorageImpl is destroyed.
class NearbyShareCertificateStorageImpl : public NearbyShareCertificateStorage {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyShareCertificateStorage> Create(
        PrefService* pref_service,
        leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
        const base::FilePath& profile_path);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<NearbyShareCertificateStorage> CreateInstance(
        PrefService* pref_service,
        leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
        const base::FilePath& profile_path) = 0;

   private:
    static Factory* test_factory_;
  };

  using ExpirationList = std::vector<std::pair<std::string, base::Time>>;

  NearbyShareCertificateStorageImpl(
      PrefService* pref_service,
      std::unique_ptr<leveldb_proto::ProtoDatabase<
          nearby::sharing::proto::PublicCertificate>> proto_database);
  ~NearbyShareCertificateStorageImpl() override;
  NearbyShareCertificateStorageImpl(NearbyShareCertificateStorageImpl&) =
      delete;
  void operator=(NearbyShareCertificateStorageImpl&) = delete;

  // NearbyShareCertificateStorage
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

 private:
  enum class InitStatus { kUninitialized, kInitialized, kFailed };

  void Initialize();
  void OnDatabaseInitialized(base::TimeTicks initialize_start_time,
                             leveldb_proto::Enums::InitStatus status);
  void FinishInitialization(bool success);

  void OnDatabaseDestroyedReinitialize(bool success);

  void DestroyAndReinitialize();

  void AddPublicCertificatesCallback(
      std::unique_ptr<ExpirationList> new_expirations,
      ResultCallback callback,
      bool proceed);
  void RemoveExpiredPublicCertificatesCallback(
      std::unique_ptr<base::flat_set<std::string>> ids_to_remove,
      ResultCallback callback,
      bool proceed);

  bool FetchPublicCertificateExpirations();
  void SavePublicCertificateExpirations();

  InitStatus init_status_ = InitStatus::kUninitialized;
  size_t num_initialize_attempts_ = 0;
  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<
      leveldb_proto::ProtoDatabase<nearby::sharing::proto::PublicCertificate>>
      db_;
  ExpirationList public_certificate_expirations_;
  base::queue<base::OnceClosure> deferred_callbacks_;
  base::WeakPtrFactory<NearbyShareCertificateStorageImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_STORAGE_IMPL_H_
