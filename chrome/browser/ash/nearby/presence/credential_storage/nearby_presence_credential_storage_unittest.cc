// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/presence/credential_storage/nearby_presence_credential_storage.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/internal/proto/credential.pb.h"
#include "third_party/nearby/internal/proto/local_credential.pb.h"

namespace {

constexpr int64_t kId_1 = 111;
const std::vector<uint8_t> kMetadataEncryptionKeyV0_1 = {
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e};
constexpr int64_t kStartTimeMillis_1 = 255486129307;
constexpr int64_t kEndTimeMillis_1 = 265486239507;
const std::vector<uint8_t> kKeySeed_1 = {
    0x21, 0x22, 0x23, 0x24, 0x2A, 0x21, 0x27, 0x28, 0x29, 0x2A, 0x2B,
    0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x37, 0x32, 0x33, 0x34, 0x35, 0x36,
    0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40};
const std::vector<uint8_t> kEncryptedMetadataBytesV0_1 = {0x33, 0x33, 0x33,
                                                          0x33, 0x33, 0x33};
const std::vector<uint8_t> kMetadataEncryptionTag_1 = {0x44, 0x44, 0x44,
                                                       0x44, 0x44, 0x44};
const std::vector<uint8_t> kConnectionSignatureVerificationKey_1 = {
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55};
const std::vector<uint8_t> kAdvertisementSignatureVerificationKey_1 = {
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66};
const std::vector<uint8_t> kVersion_1 = {0x77, 0x77, 0x77, 0x77, 0x77, 0x77};
const std::vector<uint8_t> kEncryptedMetadataBytesV1_1 = {0x81, 0x81, 0x81,
                                                          0x81, 0x81, 0x81};
const std::vector<uint8_t> kIdentityTokenShortSaltAdvHmacKeyV1_1 = {
    0xA1, 0xA1, 0xA1, 0xA1, 0xA1, 0xA1};
const std::vector<uint8_t> kIdentityTokenExtendedSaltAdvHmacKeyV1_1 = {
    0xB2, 0xB2, 0xB2, 0xB2, 0xB2, 0xB2};
const std::vector<uint8_t> kIdentityTokenSignedAdvHmacKeyV1_1 = {
    0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3};
const char kSignatureVersion_1[] = "2981212593";
const char AdvertisementSigningKeyCertificateAlias_1[] =
    "NearbySharingABCDEF123456";
const char kDusi_1[] = "11";
const std::vector<uint8_t> kAdvertisementPrivateKey_1 = {0x41, 0x42, 0x43,
                                                         0x44, 0x45, 0x46};
const char ConnectionSigningKeyCertificateAlias_1[] = "NearbySharingXYZ789";
const std::vector<uint8_t> kConnectionPrivateKey_1 = {0x51, 0x52, 0x53,
                                                      0x54, 0x55, 0x56};
const std::vector<uint8_t> kIdentityTokenV1_1 = {
    0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70};
const base::flat_map<uint32_t, bool> kConsumedSalts_1 = {{0xb412, true},
                                                         {0x34b2, false},
                                                         {0x5171, false}};

constexpr int64_t kId_2 = 222;
const std::vector<uint8_t> kMetadataEncryptionKeyV0_2 = {
    0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA};
constexpr int64_t kStartTimeMillis_2 = 255486129307;
constexpr int64_t kEndTimeMillis_2 = 265486239725;
const std::vector<uint8_t> kKeySeed_2 = {
    0x21, 0x22, 0x23, 0x24, 0x2A, 0x24, 0x27, 0x28, 0x29, 0x2A, 0x2B,
    0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x32, 0x31, 0x23, 0x14, 0x12, 0x21,
    0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3A, 0x3E, 0x3F, 0x31};
const std::vector<uint8_t> kEncryptedMetadataBytesV0_2 = {0x44, 0x44, 0x44,
                                                          0x44, 0x44, 0x44};
const std::vector<uint8_t> kMetadataEncryptionTag_2 = {0x55, 0x55, 0x55,
                                                       0x55, 0x55, 0x55};
const std::vector<uint8_t> kConnectionSignatureVerificationKey_2 = {
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66};
const std::vector<uint8_t> kAdvertisementSignatureVerificationKey_2 = {
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66};
const std::vector<uint8_t> kVersion_2 = {0x88, 0x88, 0x88, 0x88, 0x88, 0x88};
const std::vector<uint8_t> kEncryptedMetadataBytesV1_2 = {0x82, 0x82, 0x82,
                                                          0x82, 0x82, 0x82};
const std::vector<uint8_t> kIdentityTokenShortSaltAdvHmacKeyV1_2 = {
    0xA2, 0xA2, 0xA2, 0xA2, 0xA2, 0xA2};
const std::vector<uint8_t> kIdentityTokenExtendedSaltAdvHmacKeyV1_2 = {
    0xB3, 0xB3, 0xB3, 0xB3, 0xB3, 0xB3};
const std::vector<uint8_t> kIdentityTokenSignedAdvHmacKeyV1_2 = {
    0xC4, 0xC4, 0xC4, 0xC4, 0xC4, 0xC4};
const char kSignatureVersion_2[] = "2998055602";
const char kDusi_2[] = "22";
const char AdvertisementSigningKeyCertificateAlias_2[] =
    "NearbySharingFEDCBA987654";
const std::vector<uint8_t> kAdvertisementPrivateKey_2 = {0xBB, 0xBC, 0xBD,
                                                         0xBE, 0xBF, 0xC0};
const char ConnectionSigningKeyCertificateAlias_2[] = "NearbySharingZYX543";
const std::vector<uint8_t> kConnectionPrivateKey_2 = {0xC1, 0xC2, 0xC3,
                                                      0xC4, 0xC5, 0xC6};
const std::vector<uint8_t> kIdentityTokenV1_2 = {
    0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE,
    0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6};
const base::flat_map<uint32_t, bool> kConsumedSalts_2 = {{0xb412, false},
                                                         {0x34b2, true},
                                                         {0x5171, false}};

constexpr int64_t kId_3 = 333;
const std::vector<uint8_t> kMetadataEncryptionKeyV0_3 = {
    0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13,
    0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A};
constexpr int64_t kStartTimeMillis_3 = 255486129307;
constexpr int64_t kEndTimeMillis_3 = 263485225725;
const std::vector<uint8_t> kKeySeed_3 = {
    0x21, 0x22, 0x23, 0x24, 0x2A, 0x22, 0x27, 0x21, 0x29, 0x2A, 0x2B,
    0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x31, 0x22, 0x14, 0x12, 0x21,
    0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3B, 0x3E, 0x3F, 0x31};
const std::vector<uint8_t> kEncryptedMetadataBytesV0_3 = {0x55, 0x55, 0x55,
                                                          0x55, 0x55, 0x55};
const std::vector<uint8_t> kMetadataEncryptionTag_3 = {0x66, 0x66, 0x66,
                                                       0x66, 0x66, 0x66};
const std::vector<uint8_t> kConnectionSignatureVerificationKey_3 = {
    0x77, 0x77, 0x77, 0x77, 0x77, 0x77};
const std::vector<uint8_t> kAdvertisementSignatureVerificationKey_3 = {
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88};
const std::vector<uint8_t> kVersion_3 = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16};
const std::vector<uint8_t> kEncryptedMetadataBytesV1_3 = {0x83, 0x83, 0x83,
                                                          0x83, 0x83, 0x83};
const std::vector<uint8_t> kIdentityTokenShortSaltAdvHmacKeyV1_3 = {
    0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3};
const std::vector<uint8_t> kIdentityTokenExtendedSaltAdvHmacKeyV1_3 = {
    0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4};
const std::vector<uint8_t> kIdentityTokenSignedAdvHmacKeyV1_3 = {
    0xC5, 0xC5, 0xC5, 0xC5, 0xC5, 0xC5};
const char kSignatureVersion_3[] = "3014898611";
const char kDusi_3[] = "33";
const char AdvertisementSigningKeyCertificateAlias_3[] =
    "NearbySharingJIHGFED3210";
const std::vector<uint8_t> kAdvertisementPrivateKey_3 = {0x1B, 0x1C, 0x1D,
                                                         0x1E, 0x1F, 0x20};
const char ConnectionSigningKeyCertificateAlias_3[] = "NearbySharingWVU109";
const std::vector<uint8_t> kConnectionPrivateKey_3 = {0x21, 0x22, 0x23,
                                                      0x24, 0x25, 0x26};
const std::vector<uint8_t> kIdentityTokenV1_3 = {
    0x27, 0x28, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE,
    0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6};
const base::flat_map<uint32_t, bool> kConsumedSalts_3 = {{0xb402, false},
                                                         {0x3202, false},
                                                         {0x5b71, true}};

class TestNearbyPresenceCredentialStorage
    : public ash::nearby::presence::NearbyPresenceCredentialStorage {
 public:
  TestNearbyPresenceCredentialStorage(
      mojo::PendingReceiver<
          ash::nearby::presence::mojom::NearbyPresenceCredentialStorage>
          pending_receiver,
      std::unique_ptr<
          leveldb_proto::ProtoDatabase<::nearby::internal::LocalCredential>>
          private_db,
      std::unique_ptr<
          leveldb_proto::ProtoDatabase<::nearby::internal::SharedCredential>>
          local_public_db,
      std::unique_ptr<
          leveldb_proto::ProtoDatabase<::nearby::internal::SharedCredential>>
          remote_public_db)
      : ash::nearby::presence::NearbyPresenceCredentialStorage(
            std::move(pending_receiver),
            std::move(private_db),
            std::move(local_public_db),
            std::move(remote_public_db)) {}
};

ash::nearby::presence::mojom::LocalCredentialPtr CreateLocalCredential(
    const int64_t id,
    const std::vector<uint8_t>& key_seed,
    const int64_t start_time_millis,
    const int64_t end_time_millis,
    const std::vector<uint8_t>& metadata_encryption_key_v0,
    const std::string& advertisement_signing_key_certificate_alias,
    const std::vector<uint8_t>& advertisement_signing_key_data,
    const std::string& connection_signing_key_certificate_alias,
    const std::vector<uint8_t>& connection_signing_key_data,
    const ash::nearby::presence::mojom::IdentityType identity_type,
    const base::flat_map<uint32_t, bool>& consumed_salts,
    const std::vector<uint8_t>& identity_token_v1,
    const std::string& signature_version) {
  return ash::nearby::presence::mojom::LocalCredential::New(
      /*secret_id=*/std::vector<uint8_t>(), key_seed, start_time_millis,
      end_time_millis, metadata_encryption_key_v0,
      ash::nearby::presence::mojom::PrivateKey::New(
          advertisement_signing_key_certificate_alias,
          advertisement_signing_key_data),
      ash::nearby::presence::mojom::PrivateKey::New(
          connection_signing_key_certificate_alias,
          connection_signing_key_data),
      identity_type, consumed_salts, identity_token_v1, id, signature_version);
}

ash::nearby::presence::mojom::SharedCredentialPtr CreateSharedCredential(
    const std::vector<uint8_t>& key_seed,
    const int64_t start_time_millis,
    const int64_t end_time_millis,
    const std::vector<uint8_t>& encrypted_metadata_bytes_v0,
    const std::vector<uint8_t>& metadata_encryption_key_tag_v0,
    const std::vector<uint8_t>& connection_signature_verification_key,
    const std::vector<uint8_t>& advertisement_signature_verification_key,
    const ash::nearby::presence::mojom::IdentityType identity_type,
    const std::vector<uint8_t>& version,
    const ash::nearby::presence::mojom::CredentialType credential_type,
    const std::vector<uint8_t>& encrypted_metadata_bytes_v1,
    const std::vector<uint8_t>& identity_token_short_salt_adv_hmac_key_v1,
    const int64_t id,
    const std::string& dusi,
    const std::string& signature_version,
    const std::vector<uint8_t>& identity_token_extended_salt_adv_hmac_key_v1,
    const std::vector<uint8_t>& identity_token_signed_adv_hmac_key_v1) {
  return ash::nearby::presence::mojom::SharedCredential::New(
      key_seed, start_time_millis, end_time_millis, encrypted_metadata_bytes_v0,
      metadata_encryption_key_tag_v0, connection_signature_verification_key,
      advertisement_signature_verification_key, identity_type, version,
      credential_type, encrypted_metadata_bytes_v1,
      identity_token_short_salt_adv_hmac_key_v1, id, dusi, signature_version,
      identity_token_extended_salt_adv_hmac_key_v1,
      identity_token_signed_adv_hmac_key_v1);
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

    // Since `NearbyPresenceCredentialStorage` binds to a `Receiver`, it
    // must be entangled with a valid `Remote`.
    mojo::PendingReceiver<mojom::NearbyPresenceCredentialStorage>
        pending_receiver = remote_.BindNewPipeAndPassReceiver();

    credential_storage_ = std::make_unique<TestNearbyPresenceCredentialStorage>(
        std::move(pending_receiver), std::move(private_db),
        std::move(local_public_db), std::move(remote_public_db));
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

  void SaveCredentialsWithExpectedResult(
      base::RunLoop& run_loop,
      mojo_base::mojom::AbslStatusCode expected_result,
      std::vector<mojom::LocalCredentialPtr> local_credentials,
      std::vector<mojom::SharedCredentialPtr> shared_credentials,
      ash::nearby::presence::mojom::PublicCredentialType
          public_credential_type) {
    credential_storage_->SaveCredentials(
        std::move(local_credentials), std::move(shared_credentials),
        public_credential_type,
        base::BindLambdaForTesting(
            [&run_loop,
             expected_result](mojo_base::mojom::AbslStatusCode status) {
              EXPECT_EQ(status, expected_result);
              run_loop.Quit();
            }));
  }

  // Pre-populates the shared credential database of the specified type with
  // 3 SharedCredentials. Also pre-populates the private credential database
  // with 3 LocalCredentials if the 'public_credential_type' is
  // kLocalPublicCredential. Credentials are generated and inserted in the order
  // of the anonymous namespace constants.
  void PrepopulateCredentials(base::RunLoop& run_loop,
                              ash::nearby::presence::mojom::PublicCredentialType
                                  public_credential_type) {
    std::vector<mojom::SharedCredentialPtr> shared_credentials;
    shared_credentials.emplace_back(CreateSharedCredential(
        kKeySeed_1, kStartTimeMillis_1, kEndTimeMillis_1,
        kEncryptedMetadataBytesV0_1, kMetadataEncryptionTag_1,
        kConnectionSignatureVerificationKey_1,
        kAdvertisementSignatureVerificationKey_1,
        mojom::IdentityType::kIdentityTypePrivateGroup, kVersion_1,
        mojom::CredentialType::kCredentialTypeDevice,
        kEncryptedMetadataBytesV1_1, kIdentityTokenShortSaltAdvHmacKeyV1_1,
        kId_1, kDusi_1, kSignatureVersion_1,
        kIdentityTokenExtendedSaltAdvHmacKeyV1_1,
        kIdentityTokenSignedAdvHmacKeyV1_1));
    shared_credentials.emplace_back(CreateSharedCredential(
        kKeySeed_2, kStartTimeMillis_2, kEndTimeMillis_2,
        kEncryptedMetadataBytesV0_2, kMetadataEncryptionTag_2,
        kConnectionSignatureVerificationKey_2,
        kAdvertisementSignatureVerificationKey_2,
        mojom::IdentityType::kIdentityTypePrivateGroup, kVersion_2,
        mojom::CredentialType::kCredentialTypeDevice,
        kEncryptedMetadataBytesV1_2, kIdentityTokenShortSaltAdvHmacKeyV1_2,
        kId_2, kDusi_2, kSignatureVersion_2,
        kIdentityTokenExtendedSaltAdvHmacKeyV1_2,
        kIdentityTokenSignedAdvHmacKeyV1_2));
    shared_credentials.emplace_back(CreateSharedCredential(
        kKeySeed_3, kStartTimeMillis_3, kEndTimeMillis_3,
        kEncryptedMetadataBytesV0_3, kMetadataEncryptionTag_3,
        kConnectionSignatureVerificationKey_3,
        kAdvertisementSignatureVerificationKey_3,
        mojom::IdentityType::kIdentityTypePrivateGroup, kVersion_3,
        mojom::CredentialType::kCredentialTypeDevice,
        kEncryptedMetadataBytesV1_3, kIdentityTokenShortSaltAdvHmacKeyV1_3,
        kId_3, kDusi_3, kSignatureVersion_3,
        kIdentityTokenExtendedSaltAdvHmacKeyV1_3,
        kIdentityTokenSignedAdvHmacKeyV1_3));

    std::vector<mojom::LocalCredentialPtr> local_credentials;
    // Prevent passing local credentials to a remote credential save.
    if (public_credential_type ==
        ash::nearby::presence::mojom::PublicCredentialType::
            kLocalPublicCredential) {
      local_credentials.emplace_back(CreateLocalCredential(
          kId_1, kKeySeed_1, kStartTimeMillis_1, kEndTimeMillis_1,
          kMetadataEncryptionKeyV0_1, AdvertisementSigningKeyCertificateAlias_1,
          kAdvertisementPrivateKey_1, ConnectionSigningKeyCertificateAlias_1,
          kConnectionPrivateKey_1,
          mojom::IdentityType::kIdentityTypePrivateGroup, kConsumedSalts_1,
          kIdentityTokenV1_1, kSignatureVersion_1));
      local_credentials.emplace_back(CreateLocalCredential(
          kId_2, kKeySeed_2, kStartTimeMillis_2, kEndTimeMillis_2,
          kMetadataEncryptionKeyV0_2, AdvertisementSigningKeyCertificateAlias_2,
          kAdvertisementPrivateKey_2, ConnectionSigningKeyCertificateAlias_2,
          kConnectionPrivateKey_2,
          mojom::IdentityType::kIdentityTypePrivateGroup, kConsumedSalts_2,
          kIdentityTokenV1_2, kSignatureVersion_2));
      local_credentials.emplace_back(CreateLocalCredential(
          kId_3, kKeySeed_3, kStartTimeMillis_3, kEndTimeMillis_3,
          kMetadataEncryptionKeyV0_3, AdvertisementSigningKeyCertificateAlias_3,
          kAdvertisementPrivateKey_3, ConnectionSigningKeyCertificateAlias_3,
          kConnectionPrivateKey_3,
          mojom::IdentityType::kIdentityTypePrivateGroup, kConsumedSalts_3,
          kIdentityTokenV1_3, kSignatureVersion_3));
    }

    SaveCredentialsWithExpectedResult(
        run_loop, mojo_base::mojom::AbslStatusCode::kOk,
        std::move(local_credentials), std::move(shared_credentials),
        public_credential_type);

    // Only set the callbacks of the databases that are being updated.
    if (public_credential_type ==
        ash::nearby::presence::mojom::PublicCredentialType::
            kRemotePublicCredential) {
      remote_public_db_->UpdateCallback(true);
    } else {
      local_public_db_->UpdateCallback(true);
      private_db_->UpdateCallback(true);
    }
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment;

  std::unique_ptr<NearbyPresenceCredentialStorage> credential_storage_;

  mojo::Remote<mojom::NearbyPresenceCredentialStorage> remote_;

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

  base::HistogramTester histogram_tester_;
};

TEST_F(NearbyPresenceCredentialStorageTest, InitializeDatabases_Successful) {
  base::RunLoop run_loop;

  InitializeCredentialStorage(run_loop, /*expected_success=*/true);

  private_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  local_public_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  remote_public_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  run_loop.Run();

  histogram_tester_.ExpectUniqueSample(
      "Nearby.Presence.Credentials.Storage.PrivateDatabaseInitializationResult",
      /*bucket: success=*/true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Nearby.Presence.Credentials.Storage."
      "LocalPublicDatabaseInitializationResult",
      /*bucket: success=*/true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Nearby.Presence.Credentials.Storage."
      "RemotePublicDatabaseInitializationResult",
      /*bucket: success=*/true, 1);
}

TEST_F(NearbyPresenceCredentialStorageTest, InitializeDatabases_PrivateFails) {
  base::RunLoop run_loop;

  InitializeCredentialStorage(run_loop, /*expected_success=*/false);

  // Only the private status callback is set, as the public callbacks will
  // never be bound.
  private_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kCorrupt);

  run_loop.Run();

  histogram_tester_.ExpectUniqueSample(
      "Nearby.Presence.Credentials.Storage.PrivateDatabaseInitializationResult",
      /*bucket: success=*/false, 1);
}

TEST_F(NearbyPresenceCredentialStorageTest,
       InitializeDatabases_LocalPublicFails) {
  base::RunLoop run_loop;

  InitializeCredentialStorage(run_loop, /*expected_success=*/false);

  // Failure of the local public database will cause the remote public database
  // callbacks to never be bound.
  private_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  local_public_db_->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kCorrupt);

  run_loop.Run();

  histogram_tester_.ExpectUniqueSample(
      "Nearby.Presence.Credentials.Storage.PrivateDatabaseInitializationResult",
      /*bucket: success=*/true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Nearby.Presence.Credentials.Storage."
      "LocalPublicDatabaseInitializationResult",
      /*bucket: success=*/false, 1);
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

  histogram_tester_.ExpectUniqueSample(
      "Nearby.Presence.Credentials.Storage.PrivateDatabaseInitializationResult",
      /*bucket: success=*/true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Nearby.Presence.Credentials.Storage."
      "LocalPublicDatabaseInitializationResult",
      /*bucket: success=*/true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Nearby.Presence.Credentials.Storage."
      "RemotePublicDatabaseInitializationResult",
      /*bucket: success=*/false, 1);
}

TEST_F(NearbyPresenceCredentialStorageTest, SaveCredentials_Local_Success) {
  {
    base::RunLoop run_loop;
    FullyInitializeDatabases(run_loop);
  }

  std::vector<mojom::LocalCredentialPtr> local_credentials;
  local_credentials.emplace_back(CreateLocalCredential(
      kId_1, kKeySeed_1, kStartTimeMillis_1, kEndTimeMillis_1,
      kMetadataEncryptionKeyV0_1, AdvertisementSigningKeyCertificateAlias_1,
      kAdvertisementPrivateKey_1, ConnectionSigningKeyCertificateAlias_1,
      kConnectionPrivateKey_1, mojom::IdentityType::kIdentityTypePrivateGroup,
      kConsumedSalts_1, kIdentityTokenV1_1, kSignatureVersion_1));
  local_credentials.emplace_back(CreateLocalCredential(
      kId_2, kKeySeed_2, kStartTimeMillis_2, kEndTimeMillis_2,
      kMetadataEncryptionKeyV0_2, AdvertisementSigningKeyCertificateAlias_2,
      kAdvertisementPrivateKey_2, ConnectionSigningKeyCertificateAlias_2,
      kConnectionPrivateKey_2, mojom::IdentityType::kIdentityTypePrivateGroup,
      kConsumedSalts_2, kIdentityTokenV1_2, kSignatureVersion_2));
  local_credentials.emplace_back(CreateLocalCredential(
      kId_3, kKeySeed_3, kStartTimeMillis_3, kEndTimeMillis_3,
      kMetadataEncryptionKeyV0_3, AdvertisementSigningKeyCertificateAlias_3,
      kAdvertisementPrivateKey_3, ConnectionSigningKeyCertificateAlias_3,
      kConnectionPrivateKey_3, mojom::IdentityType::kIdentityTypePrivateGroup,
      kConsumedSalts_3, kIdentityTokenV1_3, kSignatureVersion_3));

  std::vector<mojom::SharedCredentialPtr> shared_credentials;
  shared_credentials.emplace_back(CreateSharedCredential(
      kKeySeed_1, kStartTimeMillis_1, kEndTimeMillis_1,
      kEncryptedMetadataBytesV0_1, kMetadataEncryptionTag_1,
      kConnectionSignatureVerificationKey_1,
      kAdvertisementSignatureVerificationKey_1,
      mojom::IdentityType::kIdentityTypePrivateGroup, kVersion_1,
      mojom::CredentialType::kCredentialTypeDevice, kEncryptedMetadataBytesV1_1,
      kIdentityTokenShortSaltAdvHmacKeyV1_1, kId_1, kDusi_1,
      kSignatureVersion_1, kIdentityTokenExtendedSaltAdvHmacKeyV1_1,
      kIdentityTokenSignedAdvHmacKeyV1_1));
  shared_credentials.emplace_back(CreateSharedCredential(
      kKeySeed_2, kStartTimeMillis_2, kEndTimeMillis_2,
      kEncryptedMetadataBytesV0_2, kMetadataEncryptionTag_2,
      kConnectionSignatureVerificationKey_2,
      kAdvertisementSignatureVerificationKey_2,
      mojom::IdentityType::kIdentityTypePrivateGroup, kVersion_2,
      mojom::CredentialType::kCredentialTypeDevice, kEncryptedMetadataBytesV1_2,
      kIdentityTokenShortSaltAdvHmacKeyV1_2, kId_2, kDusi_2,
      kSignatureVersion_2, kIdentityTokenExtendedSaltAdvHmacKeyV1_2,
      kIdentityTokenSignedAdvHmacKeyV1_2));
  shared_credentials.emplace_back(CreateSharedCredential(
      kKeySeed_3, kStartTimeMillis_3, kEndTimeMillis_3,
      kEncryptedMetadataBytesV0_3, kMetadataEncryptionTag_3,
      kConnectionSignatureVerificationKey_3,
      kAdvertisementSignatureVerificationKey_3,
      mojom::IdentityType::kIdentityTypePrivateGroup, kVersion_3,
      mojom::CredentialType::kCredentialTypeDevice, kEncryptedMetadataBytesV1_3,
      kIdentityTokenShortSaltAdvHmacKeyV1_3, kId_3, kDusi_3,
      kSignatureVersion_3, kIdentityTokenExtendedSaltAdvHmacKeyV1_3,
      kIdentityTokenSignedAdvHmacKeyV1_3));

  {
    base::RunLoop run_loop;

    SaveCredentialsWithExpectedResult(
        run_loop, mojo_base::mojom::AbslStatusCode::kOk,
        std::move(local_credentials), std::move(shared_credentials),
        ash::nearby::presence::mojom::PublicCredentialType::
            kLocalPublicCredential);
    local_public_db_->UpdateCallback(true);
    private_db_->UpdateCallback(true);

    run_loop.Run();
  }

  ASSERT_EQ(3u, local_public_db_entries_.size());
  ASSERT_EQ(3u, private_db_entries_.size());

  histogram_tester_.ExpectUniqueSample(
      "Nearby.Presence.Credentials.Storage.SaveLocalPublicCredentials.Result",
      /*bucket: success=*/true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Nearby.Presence.Credentials.Storage.SavePrivateCredentials.Result",
      /*bucket: success=*/true, 1);
}

TEST_F(NearbyPresenceCredentialStorageTest, SaveCredentials_Local_PublicFails) {
  {
    base::RunLoop run_loop;
    FullyInitializeDatabases(run_loop);
  }

  std::vector<mojom::LocalCredentialPtr> local_credentials;
  local_credentials.emplace_back(CreateLocalCredential(
      kId_1, kKeySeed_1, kStartTimeMillis_1, kEndTimeMillis_1,
      kMetadataEncryptionKeyV0_1, AdvertisementSigningKeyCertificateAlias_1,
      kAdvertisementPrivateKey_1, ConnectionSigningKeyCertificateAlias_1,
      kConnectionPrivateKey_1, mojom::IdentityType::kIdentityTypePrivateGroup,
      kConsumedSalts_1, kIdentityTokenV1_1, kSignatureVersion_1));

  std::vector<mojom::SharedCredentialPtr> shared_credentials;
  shared_credentials.emplace_back(CreateSharedCredential(
      kKeySeed_1, kStartTimeMillis_1, kEndTimeMillis_1,
      kEncryptedMetadataBytesV0_1, kMetadataEncryptionTag_1,
      kConnectionSignatureVerificationKey_1,
      kAdvertisementSignatureVerificationKey_1,
      mojom::IdentityType::kIdentityTypePrivateGroup, kVersion_1,
      mojom::CredentialType::kCredentialTypeDevice, kEncryptedMetadataBytesV1_1,
      kIdentityTokenShortSaltAdvHmacKeyV1_1, kId_1, kDusi_1,
      kSignatureVersion_1, kIdentityTokenExtendedSaltAdvHmacKeyV1_1,
      kIdentityTokenSignedAdvHmacKeyV1_1));

  {
    base::RunLoop run_loop;

    SaveCredentialsWithExpectedResult(
        run_loop, mojo_base::mojom::AbslStatusCode::kAborted,
        std::move(local_credentials), std::move(shared_credentials),
        ash::nearby::presence::mojom::PublicCredentialType::
            kLocalPublicCredential);
    // Only local public database will have its callback bound as we cancel
    // saving to the private database on public save failure.
    local_public_db_->UpdateCallback(false);

    run_loop.Run();
  }

  // Private credentials should not have an entry, as a save attempt did
  // not take place.
  histogram_tester_.ExpectUniqueSample(
      "Nearby.Presence.Credentials.Storage.SaveLocalPublicCredentials.Result",
      /*bucket: success=*/false, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Storage.SavePrivateCredentials.Result",
      /*count=*/0);
}

TEST_F(NearbyPresenceCredentialStorageTest,
       SaveCredentials_Local_PrivateFails) {
  {
    base::RunLoop run_loop;
    FullyInitializeDatabases(run_loop);
  }

  std::vector<mojom::LocalCredentialPtr> local_credentials;
  local_credentials.emplace_back(CreateLocalCredential(
      kId_1, kKeySeed_1, kStartTimeMillis_1, kEndTimeMillis_1,
      kMetadataEncryptionKeyV0_1, AdvertisementSigningKeyCertificateAlias_1,
      kAdvertisementPrivateKey_1, ConnectionSigningKeyCertificateAlias_1,
      kConnectionPrivateKey_1, mojom::IdentityType::kIdentityTypePrivateGroup,
      kConsumedSalts_1, kIdentityTokenV1_1, kSignatureVersion_1));

  std::vector<mojom::SharedCredentialPtr> shared_credentials;
  shared_credentials.emplace_back(CreateSharedCredential(
      kKeySeed_1, kStartTimeMillis_1, kEndTimeMillis_1,
      kEncryptedMetadataBytesV0_1, kMetadataEncryptionTag_1,
      kConnectionSignatureVerificationKey_1,
      kAdvertisementSignatureVerificationKey_1,
      mojom::IdentityType::kIdentityTypePrivateGroup, kVersion_1,
      mojom::CredentialType::kCredentialTypeDevice, kEncryptedMetadataBytesV1_1,
      kIdentityTokenShortSaltAdvHmacKeyV1_1, kId_1, kDusi_1,
      kSignatureVersion_1, kIdentityTokenExtendedSaltAdvHmacKeyV1_1,
      kIdentityTokenSignedAdvHmacKeyV1_1));

  {
    base::RunLoop run_loop;

    SaveCredentialsWithExpectedResult(
        run_loop, mojo_base::mojom::AbslStatusCode::kAborted,
        std::move(local_credentials), std::move(shared_credentials),
        ash::nearby::presence::mojom::PublicCredentialType::
            kLocalPublicCredential);
    local_public_db_->UpdateCallback(true);
    private_db_->UpdateCallback(false);

    run_loop.Run();
  }

  histogram_tester_.ExpectUniqueSample(
      "Nearby.Presence.Credentials.Storage.SaveLocalPublicCredentials.Result",
      /*bucket: success=*/true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Nearby.Presence.Credentials.Storage.SavePrivateCredentials.Result",
      /*bucket: success=*/false, 1);
}

TEST_F(NearbyPresenceCredentialStorageTest, SaveCredentials_Remote_Success) {
  {
    base::RunLoop run_loop;
    FullyInitializeDatabases(run_loop);
  }

  {
    base::RunLoop run_loop;
    // Nearby Presence provides an empty vector of private credentials when
    // remote public credentials are saved.
    std::vector<mojom::LocalCredentialPtr> local_credentials;
    std::vector<mojom::SharedCredentialPtr> shared_credentials;
    shared_credentials.emplace_back(CreateSharedCredential(
        kKeySeed_1, kStartTimeMillis_1, kEndTimeMillis_1,
        kEncryptedMetadataBytesV0_1, kMetadataEncryptionTag_1,
        kConnectionSignatureVerificationKey_1,
        kAdvertisementSignatureVerificationKey_1,
        mojom::IdentityType::kIdentityTypePrivateGroup, kVersion_1,
        mojom::CredentialType::kCredentialTypeDevice,
        kEncryptedMetadataBytesV1_1, kIdentityTokenShortSaltAdvHmacKeyV1_1,
        kId_1, kDusi_1, kSignatureVersion_1,
        kIdentityTokenExtendedSaltAdvHmacKeyV1_1,
        kIdentityTokenSignedAdvHmacKeyV1_1));

    SaveCredentialsWithExpectedResult(
        run_loop, mojo_base::mojom::AbslStatusCode::kOk,
        std::move(local_credentials), std::move(shared_credentials),
        ash::nearby::presence::mojom::PublicCredentialType::
            kRemotePublicCredential);
    // The local credential database callback is never set as it is never
    // updated.
    remote_public_db_->UpdateCallback(true);

    run_loop.Run();
  }

  ASSERT_EQ(1u, remote_public_db_entries_.size());

  histogram_tester_.ExpectUniqueSample(
      "Nearby.Presence.Credentials.Storage.SaveRemotePublicCredentials.Result",
      /*bucket: success=*/true, 1);
}

TEST_F(NearbyPresenceCredentialStorageTest,
       SaveCredentials_Remote_PublicFails) {
  {
    base::RunLoop run_loop;
    FullyInitializeDatabases(run_loop);
  }

  {
    base::RunLoop run_loop;
    std::vector<mojom::LocalCredentialPtr> local_credentials;
    std::vector<mojom::SharedCredentialPtr> shared_credentials;
    shared_credentials.emplace_back(CreateSharedCredential(
        kKeySeed_1, kStartTimeMillis_1, kEndTimeMillis_1,
        kEncryptedMetadataBytesV0_1, kMetadataEncryptionTag_1,
        kConnectionSignatureVerificationKey_1,
        kAdvertisementSignatureVerificationKey_1,
        mojom::IdentityType::kIdentityTypePrivateGroup, kVersion_1,
        mojom::CredentialType::kCredentialTypeDevice,
        kEncryptedMetadataBytesV1_1, kIdentityTokenShortSaltAdvHmacKeyV1_1,
        kId_1, kDusi_1, kSignatureVersion_1,
        kIdentityTokenExtendedSaltAdvHmacKeyV1_1,
        kIdentityTokenSignedAdvHmacKeyV1_1));

    SaveCredentialsWithExpectedResult(
        run_loop, mojo_base::mojom::AbslStatusCode::kAborted,
        std::move(local_credentials), std::move(shared_credentials),
        ash::nearby::presence::mojom::PublicCredentialType::
            kRemotePublicCredential);
    remote_public_db_->UpdateCallback(false);

    run_loop.Run();
  }

  histogram_tester_.ExpectUniqueSample(
      "Nearby.Presence.Credentials.Storage.SaveRemotePublicCredentials.Result",
      /*bucket: success=*/false, 1);
}

TEST_F(NearbyPresenceCredentialStorageTest,
       SaveCredentials_RemotePublicSaveDoesNotClearPrivateEntries) {
  {
    base::RunLoop run_loop;
    FullyInitializeDatabases(run_loop);
  }

  {
    base::RunLoop run_loop;
    std::vector<mojom::LocalCredentialPtr> local_credentials;
    std::vector<mojom::SharedCredentialPtr> shared_credentials;
    local_credentials.emplace_back(CreateLocalCredential(
        kId_1, kKeySeed_1, kStartTimeMillis_1, kEndTimeMillis_1,
        kMetadataEncryptionKeyV0_1, AdvertisementSigningKeyCertificateAlias_1,
        kAdvertisementPrivateKey_1, ConnectionSigningKeyCertificateAlias_1,
        kConnectionPrivateKey_1, mojom::IdentityType::kIdentityTypePrivateGroup,
        kConsumedSalts_1, kIdentityTokenV1_1, kSignatureVersion_1));
    shared_credentials.emplace_back(CreateSharedCredential(
        kKeySeed_1, kStartTimeMillis_1, kEndTimeMillis_1,
        kEncryptedMetadataBytesV0_1, kMetadataEncryptionTag_1,
        kConnectionSignatureVerificationKey_1,
        kAdvertisementSignatureVerificationKey_1,
        mojom::IdentityType::kIdentityTypePrivateGroup, kVersion_1,
        mojom::CredentialType::kCredentialTypeDevice,
        kEncryptedMetadataBytesV1_1, kIdentityTokenShortSaltAdvHmacKeyV1_1,
        kId_1, kDusi_1, kSignatureVersion_1,
        kIdentityTokenExtendedSaltAdvHmacKeyV1_1,
        kIdentityTokenSignedAdvHmacKeyV1_1));

    SaveCredentialsWithExpectedResult(
        run_loop, mojo_base::mojom::AbslStatusCode::kOk,
        std::move(local_credentials), std::move(shared_credentials),
        ash::nearby::presence::mojom::PublicCredentialType::
            kLocalPublicCredential);
    local_public_db_->UpdateCallback(true);
    private_db_->UpdateCallback(true);

    run_loop.Run();
  }

  EXPECT_EQ(1u, private_db_entries_.size());

  {
    base::RunLoop run_loop;
    // When the library saves remote public credentials, it provides an
    // empty vector of local credentials.
    std::vector<mojom::LocalCredentialPtr> local_credentials;
    std::vector<mojom::SharedCredentialPtr> shared_credentials;
    shared_credentials.emplace_back(CreateSharedCredential(
        kKeySeed_1, kStartTimeMillis_1, kEndTimeMillis_1,
        kEncryptedMetadataBytesV0_1, kMetadataEncryptionTag_1,
        kConnectionSignatureVerificationKey_1,
        kAdvertisementSignatureVerificationKey_1,
        mojom::IdentityType::kIdentityTypePrivateGroup, kVersion_1,
        mojom::CredentialType::kCredentialTypeDevice,
        kEncryptedMetadataBytesV1_1, kIdentityTokenShortSaltAdvHmacKeyV1_1,
        kId_1, kDusi_1, kSignatureVersion_1,
        kIdentityTokenExtendedSaltAdvHmacKeyV1_1,
        kIdentityTokenSignedAdvHmacKeyV1_1));

    SaveCredentialsWithExpectedResult(
        run_loop, mojo_base::mojom::AbslStatusCode::kOk,
        std::move(local_credentials), std::move(shared_credentials),
        ash::nearby::presence::mojom::PublicCredentialType::
            kRemotePublicCredential);
    remote_public_db_->UpdateCallback(true);

    run_loop.Run();
  }

  // The private credentials should be preserved despite an empty vector of
  // private credentials being provided in the remote public credential save.
  EXPECT_EQ(1u, private_db_entries_.size());
}

TEST_F(NearbyPresenceCredentialStorageTest,
       GetPublicCredentials_Local_Success) {
  {
    base::RunLoop run_loop;
    FullyInitializeDatabases(run_loop);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    PrepopulateCredentials(run_loop,
                           ash::nearby::presence::mojom::PublicCredentialType::
                               kLocalPublicCredential);
    run_loop.Run();
  }

  ASSERT_EQ(3u, local_public_db_entries_.size());

  {
    base::RunLoop run_loop;
    credential_storage_->GetPublicCredentials(
        ash::nearby::presence::mojom::PublicCredentialType::
            kLocalPublicCredential,
        base::BindLambdaForTesting(
            [&run_loop](mojo_base::mojom::AbslStatusCode status,
                        std::optional<std::vector<mojom::SharedCredentialPtr>>
                            credentials) {
              EXPECT_EQ(status, mojo_base::mojom::AbslStatusCode::kOk);
              EXPECT_TRUE(credentials.has_value());
              EXPECT_EQ(credentials->size(), 3u);
              run_loop.Quit();
            }));

    local_public_db_->LoadCallback(true);

    run_loop.Run();
  }

  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Storage."
      "RetrieveLocalPublicCredentialsDuration",
      1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Storage."
      "RetrieveRemotePublicCredentialsDuration",
      0);
}

TEST_F(NearbyPresenceCredentialStorageTest, GetPublicCredentials_Local_Fail) {
  {
    base::RunLoop run_loop;
    FullyInitializeDatabases(run_loop);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    PrepopulateCredentials(run_loop,
                           ash::nearby::presence::mojom::PublicCredentialType::
                               kLocalPublicCredential);
    run_loop.Run();
  }

  ASSERT_EQ(3u, local_public_db_entries_.size());

  {
    base::RunLoop run_loop;
    credential_storage_->GetPublicCredentials(
        ash::nearby::presence::mojom::PublicCredentialType::
            kLocalPublicCredential,
        base::BindLambdaForTesting(
            [&run_loop](mojo_base::mojom::AbslStatusCode status,
                        std::optional<std::vector<mojom::SharedCredentialPtr>>
                            credentials) {
              EXPECT_EQ(status, mojo_base::mojom::AbslStatusCode::kAborted);
              EXPECT_FALSE(credentials.has_value());
              run_loop.Quit();
            }));

    local_public_db_->LoadCallback(false);

    run_loop.Run();
  }

  // Only record duration for successful loads.
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Storage."
      "RetrieveLocalPublicCredentialsDuration",
      0);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Storage."
      "RetrieveRemotePublicCredentialsDuration",
      0);
}

TEST_F(NearbyPresenceCredentialStorageTest,
       GetPublicCredentials_Remote_Success) {
  {
    base::RunLoop run_loop;
    FullyInitializeDatabases(run_loop);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    PrepopulateCredentials(run_loop,
                           ash::nearby::presence::mojom::PublicCredentialType::
                               kRemotePublicCredential);
    run_loop.Run();
  }

  ASSERT_EQ(3u, remote_public_db_entries_.size());

  {
    base::RunLoop run_loop;
    credential_storage_->GetPublicCredentials(
        ash::nearby::presence::mojom::PublicCredentialType::
            kRemotePublicCredential,
        base::BindLambdaForTesting(
            [&run_loop](mojo_base::mojom::AbslStatusCode status,
                        std::optional<std::vector<mojom::SharedCredentialPtr>>
                            credentials) {
              EXPECT_EQ(status, mojo_base::mojom::AbslStatusCode::kOk);
              EXPECT_TRUE(credentials.has_value());
              EXPECT_EQ(credentials->size(), 3u);
              run_loop.Quit();
            }));

    remote_public_db_->LoadCallback(true);

    run_loop.Run();
  }

  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Storage."
      "RetrieveLocalPublicCredentialsDuration",
      0);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Storage."
      "RetrieveRemotePublicCredentialsDuration",
      1);
}

TEST_F(NearbyPresenceCredentialStorageTest, GetPublicCredentials_Remote_Fail) {
  {
    base::RunLoop run_loop;
    FullyInitializeDatabases(run_loop);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    PrepopulateCredentials(run_loop,
                           ash::nearby::presence::mojom::PublicCredentialType::
                               kRemotePublicCredential);
    run_loop.Run();
  }

  ASSERT_EQ(3u, remote_public_db_entries_.size());

  {
    base::RunLoop run_loop;
    credential_storage_->GetPublicCredentials(
        ash::nearby::presence::mojom::PublicCredentialType::
            kRemotePublicCredential,
        base::BindLambdaForTesting(
            [&run_loop](mojo_base::mojom::AbslStatusCode status,
                        std::optional<std::vector<mojom::SharedCredentialPtr>>
                            credentials) {
              EXPECT_EQ(status, mojo_base::mojom::AbslStatusCode::kAborted);
              EXPECT_FALSE(credentials.has_value());
              run_loop.Quit();
            }));
    remote_public_db_->LoadCallback(false);

    run_loop.Run();
  }

  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Storage."
      "RetrieveLocalPublicCredentialsDuration",
      0);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Storage."
      "RetrieveRemotePublicCredentialsDuration",
      0);
}

TEST_F(NearbyPresenceCredentialStorageTest, GetPrivateCredentials_Success) {
  {
    base::RunLoop run_loop;
    FullyInitializeDatabases(run_loop);
  }

  {
    base::RunLoop run_loop;
    PrepopulateCredentials(run_loop,
                           ash::nearby::presence::mojom::PublicCredentialType::
                               kLocalPublicCredential);
    run_loop.Run();
  }

  ASSERT_EQ(private_db_entries_.size(), 3u);

  {
    base::RunLoop run_loop;
    credential_storage_->GetPrivateCredentials(base::BindLambdaForTesting(
        [&run_loop](
            mojo_base::mojom::AbslStatusCode status,
            std::optional<std::vector<mojom::LocalCredentialPtr>> credentials) {
          EXPECT_EQ(status, mojo_base::mojom::AbslStatusCode::kOk);
          EXPECT_TRUE(credentials.has_value());
          EXPECT_EQ(credentials->size(), 3u);
          run_loop.Quit();
        }));
    private_db_->LoadCallback(true);

    run_loop.Run();
  }

  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Storage.RetrievePrivateCredentialsDuration",
      1);
}

TEST_F(NearbyPresenceCredentialStorageTest, GetPrivateCredentials_Fail) {
  {
    base::RunLoop run_loop;
    FullyInitializeDatabases(run_loop);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    PrepopulateCredentials(run_loop,
                           ash::nearby::presence::mojom::PublicCredentialType::
                               kLocalPublicCredential);
    run_loop.Run();
  }

  ASSERT_EQ(private_db_entries_.size(), 3u);

  {
    base::RunLoop run_loop;
    credential_storage_->GetPrivateCredentials(base::BindLambdaForTesting(
        [&run_loop](
            mojo_base::mojom::AbslStatusCode status,
            std::optional<std::vector<mojom::LocalCredentialPtr>> credentials) {
          EXPECT_EQ(status, mojo_base::mojom::AbslStatusCode::kAborted);
          EXPECT_FALSE(credentials.has_value());
          run_loop.Quit();
        }));

    private_db_->LoadCallback(false);

    run_loop.Run();
  }

  histogram_tester_.ExpectTotalCount(
      "Nearby.Presence.Credentials.Storage.RetrievePrivateCredentialsDuration",
      0);
}

TEST_F(NearbyPresenceCredentialStorageTest, UpdateLocalCredential_Success) {
  {
    base::RunLoop run_loop;
    FullyInitializeDatabases(run_loop);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    PrepopulateCredentials(run_loop,
                           ash::nearby::presence::mojom::PublicCredentialType::
                               kLocalPublicCredential);
    run_loop.Run();
  }

  // Since the pre-population step populates credentials with each parameter
  // to the matching number (ie, _1 values are assigned to kId_1),
  // update the credential details for _1 to _2.
  auto local_credential_to_be_updated = CreateLocalCredential(
      kId_1, kKeySeed_2, kStartTimeMillis_2, kEndTimeMillis_2,
      kMetadataEncryptionKeyV0_2, AdvertisementSigningKeyCertificateAlias_2,
      kAdvertisementPrivateKey_2, ConnectionSigningKeyCertificateAlias_2,
      kConnectionPrivateKey_2, mojom::IdentityType::kIdentityTypePrivateGroup,
      kConsumedSalts_2, kIdentityTokenV1_2, kSignatureVersion_2);

  {
    base::RunLoop run_loop;
    credential_storage_->UpdateLocalCredential(
        std::move(local_credential_to_be_updated),
        base::BindLambdaForTesting(
            [&run_loop](mojo_base::mojom::AbslStatusCode status) {
              EXPECT_EQ(status, mojo_base::mojom::AbslStatusCode::kOk);
              run_loop.Quit();
            }));

    private_db_->UpdateCallback(true);
    run_loop.Run();
  }

  auto it = private_db_entries_.find(base::NumberToString(kId_1));
  ASSERT_NE(it, private_db_entries_.end());
  auto updated_local_credential = it->second;

  EXPECT_EQ(std::vector<uint8_t>(updated_local_credential.key_seed().begin(),
                                 updated_local_credential.key_seed().end()),
            kKeySeed_2);
  EXPECT_EQ(
      std::vector<uint8_t>(updated_local_credential.identity_token_v1().begin(),
                           updated_local_credential.identity_token_v1().end()),
      kIdentityTokenV1_2);
}

TEST_F(NearbyPresenceCredentialStorageTest, UpdateLocalCredential_Failure) {
  {
    base::RunLoop run_loop;
    FullyInitializeDatabases(run_loop);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    PrepopulateCredentials(run_loop,
                           ash::nearby::presence::mojom::PublicCredentialType::
                               kLocalPublicCredential);
    run_loop.Run();
  }

  auto local_credential_to_be_updated = CreateLocalCredential(
      kId_1, kKeySeed_2, kStartTimeMillis_2, kEndTimeMillis_2,
      kMetadataEncryptionKeyV0_2, AdvertisementSigningKeyCertificateAlias_2,
      kAdvertisementPrivateKey_2, ConnectionSigningKeyCertificateAlias_2,
      kConnectionPrivateKey_2, mojom::IdentityType::kIdentityTypePrivateGroup,
      kConsumedSalts_2, kIdentityTokenV1_2, kSignatureVersion_2);

  {
    base::RunLoop run_loop;
    credential_storage_->UpdateLocalCredential(
        std::move(local_credential_to_be_updated),
        base::BindLambdaForTesting(
            [&run_loop](mojo_base::mojom::AbslStatusCode status) {
              EXPECT_EQ(status, mojo_base::mojom::AbslStatusCode::kAborted);
              run_loop.Quit();
            }));

    private_db_->UpdateCallback(false);
    run_loop.Run();
  }
}

}  // namespace ash::nearby::presence
