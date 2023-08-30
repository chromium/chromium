// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/presence/credential_storage/nearby_presence_credential_storage.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/internal/proto/credential.pb.h"
#include "third_party/nearby/internal/proto/local_credential.pb.h"

namespace {

const std::vector<uint8_t> kSecretId_1 = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16};
const std::vector<uint8_t> kKeySeed_1 = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26};
const std::vector<uint8_t> kMetadataEncryptionKeyV0_1 = {
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e};
constexpr int64_t kStartTimeMillis_1 = 255486129307;
const char AdvertisementSigningKeyCertificateAlias_1[] =
    "NearbySharingABCDEF123456";
const std::vector<uint8_t> kAdvertisementPrivateKey_1 = {0x41, 0x42, 0x43,
                                                         0x44, 0x45, 0x46};
const char ConnectionSigningKeyCertificateAlias_1[] = "NearbySharingXYZ789";
const std::vector<uint8_t> kConnectionPrivateKey_1 = {0x51, 0x52, 0x53,
                                                      0x54, 0x55, 0x56};
const std::vector<uint8_t> kMetadataEncryptionKeyV1_1 = {
    0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70};
const base::flat_map<uint32_t, bool> kConsumedSalts_1 = {{0xb412, true},
                                                         {0x34b2, false},
                                                         {0x5171, false}};

const std::vector<uint8_t> kSecretId_2 = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6};
const std::vector<uint8_t> kKeySeed_2 = {0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC};
const std::vector<uint8_t> kMetadataEncryptionKeyV0_2 = {
    0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA};
constexpr int64_t kStartTimeMillis_2 = 255486129307;
const char AdvertisementSigningKeyCertificateAlias_2[] =
    "NearbySharingFEDCBA987654";
const std::vector<uint8_t> kAdvertisementPrivateKey_2 = {0xBB, 0xBC, 0xBD,
                                                         0xBE, 0xBF, 0xC0};
const char ConnectionSigningKeyCertificateAlias_2[] = "NearbySharingZYX543";
const std::vector<uint8_t> kConnectionPrivateKey_2 = {0xC1, 0xC2, 0xC3,
                                                      0xC4, 0xC5, 0xC6};
const std::vector<uint8_t> kMetadataEncryptionKeyV1_2 = {
    0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE,
    0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6};
const base::flat_map<uint32_t, bool> kConsumedSalts_2 = {{0xb412, false},
                                                         {0x34b2, true},
                                                         {0x5171, false}};

const std::vector<uint8_t> kSecretId_3 = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
const std::vector<uint8_t> kKeySeed_3 = {0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C};
const std::vector<uint8_t> kMetadataEncryptionKeyV0_3 = {
    0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13,
    0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A};
constexpr int64_t kStartTimeMillis_3 = 255486129307;
const char AdvertisementSigningKeyCertificateAlias_3[] =
    "NearbySharingJIHGFED3210";
const std::vector<uint8_t> kAdvertisementPrivateKey_3 = {0x1B, 0x1C, 0x1D,
                                                         0x1E, 0x1F, 0x20};
const char ConnectionSigningKeyCertificateAlias_3[] = "NearbySharingWVU109";
const std::vector<uint8_t> kConnectionPrivateKey_3 = {0x21, 0x22, 0x23,
                                                      0x24, 0x25, 0x26};
const std::vector<uint8_t> kMetadataEncryptionKeyV1_3 = {
    0x27, 0x28, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE,
    0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6};
const base::flat_map<uint32_t, bool> kConsumedSalts_3 = {{0xb402, false},
                                                         {0x3202, false},
                                                         {0x5b71, true}};

class TestNearbyPresenceCredentialStorage
    : public ash::nearby::presence::NearbyPresenceCredentialStorage {
 public:
  TestNearbyPresenceCredentialStorage(
      std::unique_ptr<leveldb_proto::ProtoDatabase<
          ::nearby::internal::LocalCredential>> private_db,
      std::unique_ptr<
          leveldb_proto::ProtoDatabase<::nearby::internal::SharedCredential>>
          local_public_db,
      std::unique_ptr<
          leveldb_proto::ProtoDatabase<::nearby::internal::SharedCredential>>
          remote_public_db)
      : ash::nearby::presence::NearbyPresenceCredentialStorage(
            std::move(private_db),
            std::move(local_public_db),
            std::move(remote_public_db)) {}
};

ash::nearby::presence::mojom::LocalCredentialPtr CreateLocalCredential(
    const std::vector<uint8_t>& secret_id,
    const std::vector<uint8_t>& key_seed,
    const int start_time_millis,
    const std::vector<uint8_t>& metadata_encryption_key_v0,
    const std::string& advertisement_signing_key_certificate_alias,
    const std::vector<uint8_t>& advertisement_signing_key_data,
    const std::string& connection_signing_key_certificate_alias,
    const std::vector<uint8_t>& connection_signing_key_data,
    const ash::nearby::presence::mojom::IdentityType identity_type,
    const base::flat_map<uint32_t, bool>& consumed_salts,
    const std::vector<uint8_t>& metadata_encryption_key_v1) {
  auto local_credential = ash::nearby::presence::mojom::LocalCredential::New();

  local_credential->secret_id = secret_id;
  local_credential->key_seed = key_seed;
  local_credential->start_time_millis = start_time_millis;
  local_credential->metadata_encryption_key_v0 = metadata_encryption_key_v0;
  local_credential->identity_type = identity_type;
  local_credential->consumed_salts = consumed_salts;
  local_credential->metadata_encryption_key_v1 = metadata_encryption_key_v1;

  auto advertisement_key = ash::nearby::presence::mojom::PrivateKey::New();
  advertisement_key->certificate_alias =
      advertisement_signing_key_certificate_alias;
  advertisement_key->key = advertisement_signing_key_data;
  local_credential->advertisement_signing_key = std::move(advertisement_key);

  auto connection_key = ash::nearby::presence::mojom::PrivateKey::New();
  connection_key->certificate_alias = connection_signing_key_certificate_alias;
  connection_key->key = connection_signing_key_data;
  local_credential->connection_signing_key = std::move(connection_key);

  return local_credential;
}

}  // namespace

namespace ash::nearby::presence {

class NearbyPresenceCredentialStorageTest : public testing::Test {
 public:
  NearbyPresenceCredentialStorageTest() = default;

  ~NearbyPresenceCredentialStorageTest() override = default;

  // testing::Test:
  void SetUp() override {
    auto private_db = std::make_unique<
        leveldb_proto::test::FakeDB<::nearby::internal::LocalCredential>>(
        &private_db_entries_);
    auto local_public_db = std::make_unique<
        leveldb_proto::test::FakeDB<::nearby::internal::SharedCredential>>(
        &local_public_db_entries_);
    auto remote_public_db = std::make_unique<
        leveldb_proto::test::FakeDB<::nearby::internal::SharedCredential>>(
        &remote_public_db_entries_);

    private_db_ = private_db.get();
    local_public_db_ = local_public_db.get();
    remote_public_db_ = remote_public_db.get();

    credential_storage_ = std::make_unique<TestNearbyPresenceCredentialStorage>(
        std::move(private_db), std::move(local_public_db),
        std::move(remote_public_db));
  }

  void InitializeCredentialStorage(base::RunLoop& run_loop,
                                   bool expected_success) {
    credential_storage_->Initialize(
        base::BindLambdaForTesting([expected_success, &run_loop](bool success) {
          EXPECT_EQ(expected_success, success);
          run_loop.Quit();
        }));
  }

  void FullyInitializeDatabases(base::RunLoop& run_loop) {
    InitializeCredentialStorage(run_loop, /*expected_success=*/true);

    private_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
    local_public_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
    remote_public_db_->InitStatusCallback(
        leveldb_proto::Enums::InitStatus::kOK);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment;

  std::unique_ptr<NearbyPresenceCredentialStorage> credential_storage_;

  std::map<std::string, ::nearby::internal::LocalCredential>
      private_db_entries_;
  std::map<std::string, ::nearby::internal::SharedCredential>
      local_public_db_entries_;
  std::map<std::string, ::nearby::internal::SharedCredential>
      remote_public_db_entries_;

  raw_ptr<leveldb_proto::test::FakeDB<::nearby::internal::LocalCredential>>
      private_db_;
  raw_ptr<leveldb_proto::test::FakeDB<::nearby::internal::SharedCredential>>
      local_public_db_;
  raw_ptr<leveldb_proto::test::FakeDB<::nearby::internal::SharedCredential>>
      remote_public_db_;
};

TEST_F(NearbyPresenceCredentialStorageTest, InitializeDatabases_Successful) {
  base::RunLoop run_loop;

  InitializeCredentialStorage(run_loop, /*expected_success=*/true);

  private_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  local_public_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  remote_public_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  run_loop.Run();
}

TEST_F(NearbyPresenceCredentialStorageTest, InitializeDatabases_PrivateFails) {
  base::RunLoop run_loop;

  InitializeCredentialStorage(run_loop, /*expected_success=*/false);

  // Only the private status callback is set, as the public callbacks will
  // never be bound.
  private_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kCorrupt);

  run_loop.Run();
}

TEST_F(NearbyPresenceCredentialStorageTest,
       InitializeDatabases_LocalPublicFails) {
  base::RunLoop run_loop;

  InitializeCredentialStorage(run_loop, /*expected_success=*/false);

  // Failure of the local public database will cause the remote public database
  // callback to never be bound.
  private_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  local_public_db_->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kCorrupt);

  run_loop.Run();
}

TEST_F(NearbyPresenceCredentialStorageTest,
       InitializeDatabases_RemotePublicFails) {
  base::RunLoop run_loop;

  InitializeCredentialStorage(run_loop, /*expected_success=*/false);

  private_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  local_public_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  remote_public_db_->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kCorrupt);

  run_loop.Run();
}

TEST_F(NearbyPresenceCredentialStorageTest, SaveCredentials) {
  {
    base::RunLoop run_loop;
    FullyInitializeDatabases(run_loop);
  }

  std::vector<mojom::LocalCredentialPtr> credentials;
  credentials.emplace_back(CreateLocalCredential(
      kSecretId_1, kKeySeed_1, kStartTimeMillis_1, kMetadataEncryptionKeyV0_1,
      AdvertisementSigningKeyCertificateAlias_1, kAdvertisementPrivateKey_1,
      ConnectionSigningKeyCertificateAlias_1, kConnectionPrivateKey_1,
      mojom::IdentityType::kIdentityTypePrivate, kConsumedSalts_1,
      kMetadataEncryptionKeyV1_1));
  credentials.emplace_back(CreateLocalCredential(
      kSecretId_2, kKeySeed_2, kStartTimeMillis_2, kMetadataEncryptionKeyV0_2,
      AdvertisementSigningKeyCertificateAlias_2, kAdvertisementPrivateKey_2,
      ConnectionSigningKeyCertificateAlias_2, kConnectionPrivateKey_2,
      mojom::IdentityType::kIdentityTypePrivate, kConsumedSalts_2,
      kMetadataEncryptionKeyV1_2));
  credentials.emplace_back(CreateLocalCredential(
      kSecretId_3, kKeySeed_3, kStartTimeMillis_3, kMetadataEncryptionKeyV0_3,
      AdvertisementSigningKeyCertificateAlias_3, kAdvertisementPrivateKey_3,
      ConnectionSigningKeyCertificateAlias_3, kConnectionPrivateKey_3,
      mojom::IdentityType::kIdentityTypePrivate, kConsumedSalts_3,
      kMetadataEncryptionKeyV1_3));

  {
    base::RunLoop run_loop;
    credential_storage_->SaveCredentials(
        std::move(credentials),
        base::BindLambdaForTesting(
            [&run_loop](mojo_base::mojom::AbslStatusCode status) {
              EXPECT_EQ(status, mojo_base::mojom::AbslStatusCode::kOk);
              run_loop.Quit();
            }));

    private_db_->UpdateCallback(true);
    run_loop.Run();
  }

  ASSERT_EQ(3u, private_db_entries_.size());
}

}  // namespace ash::nearby::presence
