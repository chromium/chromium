// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_storage_impl.h"
#include "chrome/browser/nearby_sharing/certificates/test_util.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// NOTE: Make sure secret ID alphabetical ordering does not match the 1,2,3,..
// ordering to test sorting expiration times.
const char kSecretId1[] = "b_secretid1";
const char kSecretKey1[] = "secretkey1";
const char kPublicKey1[] = "publickey1";
const int64_t kStartSeconds1 = 0;
const int32_t kStartNanos1 = 10;
const int64_t kEndSeconds1 = 100;
const int32_t kEndNanos1 = 30;
const bool kForSelectedContacts1 = false;
const char kMetadataEncryptionKey1[] = "metadataencryptionkey1";
const char kEncryptedMetadataBytes1[] = "encryptedmetadatabytes1";
const char kMetadataEncryptionKeyTag1[] = "metadataencryptionkeytag1";
const char kSecretId2[] = "c_secretid2";
const char kSecretKey2[] = "secretkey2";
const char kPublicKey2[] = "publickey2";
const int64_t kStartSeconds2 = 0;
const int32_t kStartNanos2 = 20;
const int64_t kEndSeconds2 = 200;
const int32_t kEndNanos2 = 30;
const bool kForSelectedContacts2 = false;
const char kMetadataEncryptionKey2[] = "metadataencryptionkey2";
const char kEncryptedMetadataBytes2[] = "encryptedmetadatabytes2";
const char kMetadataEncryptionKeyTag2[] = "metadataencryptionkeytag2";
const char kSecretId3[] = "a_secretid3";
const char kSecretKey3[] = "secretkey3";
const char kPublicKey3[] = "publickey3";
const int64_t kStartSeconds3 = 0;
const int32_t kStartNanos3 = 30;
const int64_t kEndSeconds3 = 300;
const int32_t kEndNanos3 = 30;
const bool kForSelectedContacts3 = false;
const char kMetadataEncryptionKey3[] = "metadataencryptionkey3";
const char kEncryptedMetadataBytes3[] = "encryptedmetadatabytes3";
const char kMetadataEncryptionKeyTag3[] = "metadataencryptionkeytag3";
const char kSecretId4[] = "d_secretid4";
const char kSecretKey4[] = "secretkey4";
const char kPublicKey4[] = "publickey4";
const int64_t kStartSeconds4 = 0;
const int32_t kStartNanos4 = 10;
const int64_t kEndSeconds4 = 100;
const int32_t kEndNanos4 = 30;
const bool kForSelectedContacts4 = false;
const char kMetadataEncryptionKey4[] = "metadataencryptionkey4";
const char kEncryptedMetadataBytes4[] = "encryptedmetadatabytes4";
const char kMetadataEncryptionKeyTag4[] = "metadataencryptionkeytag4";

std::string EncodeString(const std::string& unencoded_string) {
  std::string encoded_string;
  base::Base64UrlEncode(unencoded_string,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_string);
  return encoded_string;
}

nearby::sharing::proto::PublicCertificate CreatePublicCertificate(
    const std::string& secret_id,
    const std::string& secret_key,
    const std::string& public_key,
    int64_t start_seconds,
    int32_t start_nanos,
    int64_t end_seconds,
    int32_t end_nanos,
    bool for_selected_contacts,
    const std::string& metadata_encryption_key,
    const std::string& encrypted_metadata_bytes,
    const std::string& metadata_encryption_key_tag) {
  nearby::sharing::proto::PublicCertificate cert;
  cert.set_secret_id(secret_id);
  cert.set_secret_key(secret_key);
  cert.set_public_key(public_key);
  cert.mutable_start_time()->set_seconds(start_seconds);
  cert.mutable_start_time()->set_nanos(start_nanos);
  cert.mutable_end_time()->set_seconds(end_seconds);
  cert.mutable_end_time()->set_nanos(end_nanos);
  cert.set_for_selected_contacts(for_selected_contacts);
  cert.set_metadata_encryption_key(metadata_encryption_key);
  cert.set_encrypted_metadata_bytes(encrypted_metadata_bytes);
  cert.set_metadata_encryption_key_tag(metadata_encryption_key_tag);
  return cert;
}

std::vector<NearbySharePrivateCertificate> CreatePrivateCertificates(
    size_t n,
    nearby_share::mojom::Visibility visibility) {
  std::vector<NearbySharePrivateCertificate> certs;
  certs.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    certs.emplace_back(visibility, base::Time::Now(),
                       GetNearbyShareTestMetadata());
  }
  return certs;
}

base::Time TimestampToTime(nearby::sharing::proto::Timestamp timestamp) {
  return base::Time::UnixEpoch() + base::Seconds(timestamp.seconds()) +
         base::Nanoseconds(timestamp.nanos());
}

}  // namespace

class NearbyShareCertificateStorageImplTest : public ::testing::Test {
 public:
  NearbyShareCertificateStorageImplTest() = default;
  ~NearbyShareCertificateStorageImplTest() override = default;
  NearbyShareCertificateStorageImplTest(
      NearbyShareCertificateStorageImplTest&) = delete;
  NearbyShareCertificateStorageImplTest& operator=(
      NearbyShareCertificateStorageImplTest&) = delete;

  void SetUp() override {
    auto db = std::make_unique<
        leveldb_proto::test::FakeDB<nearby::sharing::proto::PublicCertificate>>(
        &db_entries_);
    db_ = db.get();

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kNearbySharingPublicCertificateExpirationDictPrefName);
    pref_service_->registry()->RegisterListPref(
        prefs::kNearbySharingPrivateCertificateListPrefName);

    // Add public certificates to database before construction. Needed
    // to ensure test coverage of FetchPublicCertificateExpirations.
    PrepopulatePublicCertificates();

    cert_store_ = std::make_unique<NearbyShareCertificateStorageImpl>(
        pref_service_.get(), std::move(db));
  }

  void PrepopulatePublicCertificates() {
    std::vector<nearby::sharing::proto::PublicCertificate> pub_certs;
    pub_certs.emplace_back(CreatePublicCertificate(
        kSecretId1, kSecretKey1, kPublicKey1, kStartSeconds1, kStartNanos1,
        kEndSeconds1, kEndNanos1, kForSelectedContacts1,
        kMetadataEncryptionKey1, kEncryptedMetadataBytes1,
        kMetadataEncryptionKeyTag1));
    pub_certs.emplace_back(CreatePublicCertificate(
        kSecretId2, kSecretKey2, kPublicKey2, kStartSeconds2, kStartNanos2,
        kEndSeconds2, kEndNanos2, kForSelectedContacts2,
        kMetadataEncryptionKey2, kEncryptedMetadataBytes2,
        kMetadataEncryptionKeyTag2));
    pub_certs.emplace_back(CreatePublicCertificate(
        kSecretId3, kSecretKey3, kPublicKey3, kStartSeconds3, kStartNanos3,
        kEndSeconds3, kEndNanos3, kForSelectedContacts3,
        kMetadataEncryptionKey3, kEncryptedMetadataBytes3,
        kMetadataEncryptionKeyTag3));

    base::Value::Dict expiration_dict;
    db_entries_.clear();
    for (auto& cert : pub_certs) {
      expiration_dict.Set(EncodeString(cert.secret_id()),
                          base::TimeToValue(TimestampToTime(cert.end_time())));
      db_entries_.emplace(cert.secret_id(), std::move(cert));
    }
    pref_service_->SetDict(
        prefs::kNearbySharingPublicCertificateExpirationDictPrefName,
        std::move(expiration_dict));
  }

  void CaptureBoolCallback(bool* dest, bool src) { *dest = src; }

  void PublicCertificateCallback(
      std::vector<nearby::sharing::proto::PublicCertificate>*
          public_certificates,
      base::OnceClosure complete,
      bool success,
      std::unique_ptr<std::vector<nearby::sharing::proto::PublicCertificate>>
          result) {
    if (success && result) {
      public_certificates->swap(*result);
    }
    std::move(complete).Run();
  }

 protected:
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::map<std::string, nearby::sharing::proto::PublicCertificate> db_entries_;
  raw_ptr<
      leveldb_proto::test::FakeDB<nearby::sharing::proto::PublicCertificate>,
      DanglingUntriaged>
      db_;
  std::unique_ptr<NearbyShareCertificateStorage> cert_store_;
  std::vector<nearby::sharing::proto::PublicCertificate> public_certificates_;
};

TEST_F(NearbyShareCertificateStorageImplTest, DeferredCallbackQueue) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  std::vector<nearby::sharing::proto::PublicCertificate> public_certificates;

  cert_store_->GetPublicCertificates(base::BindOnce(
      &NearbyShareCertificateStorageImplTest::PublicCertificateCallback,
      base::Unretained(this), &public_certificates, run_loop.QuitClosure()));

  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  // These callbacks have to be posted to ensure that they run after the
  // deferred callbacks posted during initialization.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &leveldb_proto::test::FakeDB<
              nearby::sharing::proto::PublicCertificate>::LoadCallback,
          base::Unretained(db_), true));

  run_loop.Run();

  EXPECT_TRUE(public_certificates_.empty());
}

TEST_F(NearbyShareCertificateStorageImplTest, GetPublicCertificates) {
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  std::vector<nearby::sharing::proto::PublicCertificate> public_certificates;
  cert_store_->GetPublicCertificates(base::BindOnce(
      &NearbyShareCertificateStorageImplTest::PublicCertificateCallback,
      base::Unretained(this), &public_certificates, base::BindOnce([] {})));
  db_->LoadCallback(true);

  ASSERT_EQ(3u, public_certificates.size());
  for (nearby::sharing::proto::PublicCertificate& cert : public_certificates) {
    std::string expected_serialized, actual_serialized;
    ASSERT_TRUE(cert.SerializeToString(&expected_serialized));
    ASSERT_TRUE(db_entries_.find(cert.secret_id())
                    ->second.SerializeToString(&actual_serialized));
    ASSERT_EQ(expected_serialized, actual_serialized);
  }
}

TEST_F(NearbyShareCertificateStorageImplTest, AddPublicCertificates) {
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  std::vector<nearby::sharing::proto::PublicCertificate> new_certs = {
      CreatePublicCertificate(kSecretId3, kSecretKey2, kPublicKey2,
                              kStartSeconds2, kStartNanos2, kEndSeconds2,
                              kEndNanos2, kForSelectedContacts2,
                              kMetadataEncryptionKey2, kEncryptedMetadataBytes2,
                              kMetadataEncryptionKeyTag2),
      CreatePublicCertificate(kSecretId4, kSecretKey4, kPublicKey4,
                              kStartSeconds4, kStartNanos4, kEndSeconds4,
                              kEndNanos4, kForSelectedContacts4,
                              kMetadataEncryptionKey4, kEncryptedMetadataBytes4,
                              kMetadataEncryptionKeyTag4),
  };

  bool succeeded = false;
  cert_store_->AddPublicCertificates(
      new_certs,
      base::BindOnce(
          &NearbyShareCertificateStorageImplTest::CaptureBoolCallback,
          base::Unretained(this), &succeeded));
  db_->UpdateCallback(true);

  ASSERT_TRUE(succeeded);
  ASSERT_EQ(4u, db_entries_.size());
  ASSERT_EQ(1u, db_entries_.count(kSecretId3));
  ASSERT_EQ(1u, db_entries_.count(kSecretId4));
  auto& cert = db_entries_.find(kSecretId3)->second;
  EXPECT_EQ(kSecretKey2, cert.secret_key());
  EXPECT_EQ(kPublicKey2, cert.public_key());
  EXPECT_EQ(kStartSeconds2, cert.start_time().seconds());
  EXPECT_EQ(kStartNanos2, cert.start_time().nanos());
  EXPECT_EQ(kEndSeconds2, cert.end_time().seconds());
  EXPECT_EQ(kEndNanos2, cert.end_time().nanos());
  EXPECT_EQ(kForSelectedContacts2, cert.for_selected_contacts());
  EXPECT_EQ(kMetadataEncryptionKey2, cert.metadata_encryption_key());
  EXPECT_EQ(kEncryptedMetadataBytes2, cert.encrypted_metadata_bytes());
  EXPECT_EQ(kMetadataEncryptionKeyTag2, cert.metadata_encryption_key_tag());
  cert = db_entries_.find(kSecretId4)->second;
  EXPECT_EQ(kSecretKey4, cert.secret_key());
  EXPECT_EQ(kPublicKey4, cert.public_key());
  EXPECT_EQ(kStartSeconds4, cert.start_time().seconds());
  EXPECT_EQ(kStartNanos4, cert.start_time().nanos());
  EXPECT_EQ(kEndSeconds4, cert.end_time().seconds());
  EXPECT_EQ(kEndNanos4, cert.end_time().nanos());
  EXPECT_EQ(kForSelectedContacts4, cert.for_selected_contacts());
  EXPECT_EQ(kMetadataEncryptionKey4, cert.metadata_encryption_key());
  EXPECT_EQ(kEncryptedMetadataBytes4, cert.encrypted_metadata_bytes());
  EXPECT_EQ(kMetadataEncryptionKeyTag4, cert.metadata_encryption_key_tag());
}

TEST_F(NearbyShareCertificateStorageImplTest,
       RemoveExpiredPrivateCertificates) {
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  std::vector<NearbySharePrivateCertificate> certs = CreatePrivateCertificates(
      3, nearby_share::mojom::Visibility::kAllContacts);
  cert_store_->ReplacePrivateCertificates(certs);

  std::vector<base::Time> expiration_times;
  for (const NearbySharePrivateCertificate& cert : certs) {
    expiration_times.push_back(cert.not_after());
  }
  std::sort(expiration_times.begin(), expiration_times.end());

  // Set current time to exceed the expiration times of the first two
  // certificates.
  base::Time now = expiration_times[1];

  cert_store_->RemoveExpiredPrivateCertificates(now);

  certs = *cert_store_->GetPrivateCertificates();
  ASSERT_EQ(1u, certs.size());
  for (const NearbySharePrivateCertificate& cert : certs) {
    EXPECT_LE(now, cert.not_after());
  }
}

TEST_F(NearbyShareCertificateStorageImplTest, RemoveExpiredPublicCertificates) {
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  std::vector<base::Time> expiration_times;
  for (const auto& pair : db_entries_) {
    expiration_times.emplace_back(TimestampToTime(pair.second.end_time()));
  }
  std::sort(expiration_times.begin(), expiration_times.end());

  // The current time exceeds the expiration times of the first two certificates
  // even accounting for the expiration time tolerance applied to public
  // certificates to account for clock skew.
  base::Time now = expiration_times[1] +
                   kNearbySharePublicCertificateValidityBoundOffsetTolerance;

  bool succeeded = false;
  cert_store_->RemoveExpiredPublicCertificates(
      now, base::BindOnce(
               &NearbyShareCertificateStorageImplTest::CaptureBoolCallback,
               base::Unretained(this), &succeeded));
  db_->UpdateCallback(true);

  ASSERT_TRUE(succeeded);
  ASSERT_EQ(1u, db_entries_.size());
  for (const auto& pair : db_entries_) {
    EXPECT_LE(now - kNearbySharePublicCertificateValidityBoundOffsetTolerance,
              TimestampToTime(pair.second.end_time()));
  }
}

TEST_F(NearbyShareCertificateStorageImplTest, ReplaceGetPrivateCertificates) {
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  auto certs_before = CreatePrivateCertificates(
      3, nearby_share::mojom::Visibility::kAllContacts);
  cert_store_->ReplacePrivateCertificates(certs_before);
  auto certs_after = cert_store_->GetPrivateCertificates();

  ASSERT_TRUE(certs_after.has_value());
  ASSERT_EQ(certs_before.size(), certs_after->size());
  for (size_t i = 0; i < certs_before.size(); ++i) {
    EXPECT_EQ(certs_before[i].ToDictionary(), (*certs_after)[i].ToDictionary());
  }

  certs_before = CreatePrivateCertificates(
      1, nearby_share::mojom::Visibility::kAllContacts);
  cert_store_->ReplacePrivateCertificates(certs_before);
  certs_after = cert_store_->GetPrivateCertificates();

  ASSERT_TRUE(certs_after.has_value());
  ASSERT_EQ(certs_before.size(), certs_after->size());
  for (size_t i = 0; i < certs_before.size(); ++i) {
    EXPECT_EQ(certs_before[i].ToDictionary(), (*certs_after)[i].ToDictionary());
  }
}

TEST_F(NearbyShareCertificateStorageImplTest, UpdatePrivateCertificates) {
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  std::vector<NearbySharePrivateCertificate> initial_certs =
      CreatePrivateCertificates(3,
                                nearby_share::mojom::Visibility::kAllContacts);
  cert_store_->ReplacePrivateCertificates(initial_certs);

  NearbySharePrivateCertificate cert_to_update = initial_certs[1];
  EXPECT_EQ(initial_certs[1].ToDictionary(), cert_to_update.ToDictionary());
  cert_to_update.EncryptMetadataKey();
  EXPECT_NE(initial_certs[1].ToDictionary(), cert_to_update.ToDictionary());

  cert_store_->UpdatePrivateCertificate(cert_to_update);

  std::vector<NearbySharePrivateCertificate> new_certs =
      *cert_store_->GetPrivateCertificates();
  EXPECT_EQ(initial_certs.size(), new_certs.size());
  for (size_t i = 0; i < new_certs.size(); ++i) {
    NearbySharePrivateCertificate expected_cert =
        i == 1 ? cert_to_update : initial_certs[i];
    EXPECT_EQ(expected_cert.ToDictionary(), new_certs[i].ToDictionary());
  }
}

TEST_F(NearbyShareCertificateStorageImplTest,
       NextPrivateCertificateExpirationTime) {
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  auto certs = CreatePrivateCertificates(
      3, nearby_share::mojom::Visibility::kAllContacts);
  cert_store_->ReplacePrivateCertificates(certs);
  std::optional<base::Time> next_expiration =
      cert_store_->NextPrivateCertificateExpirationTime();

  ASSERT_TRUE(next_expiration.has_value());
  bool found = false;
  for (auto& cert : certs) {
    EXPECT_GE(cert.not_after(), *next_expiration);
    if (cert.not_after() == *next_expiration)
      found = true;
  }
  EXPECT_TRUE(found);
}

TEST_F(NearbyShareCertificateStorageImplTest,
       NextPublicCertificateExpirationTime) {
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  std::optional<base::Time> next_expiration =
      cert_store_->NextPublicCertificateExpirationTime();

  ASSERT_TRUE(next_expiration.has_value());
  bool found = false;
  for (const auto& pair : db_entries_) {
    base::Time curr_expiration = TimestampToTime(pair.second.end_time());
    EXPECT_GE(curr_expiration, *next_expiration);
    if (curr_expiration == *next_expiration)
      found = true;
  }
  EXPECT_TRUE(found);
}

TEST_F(NearbyShareCertificateStorageImplTest, ClearPrivateCertificates) {
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  std::vector<NearbySharePrivateCertificate> certs_before =
      CreatePrivateCertificates(3,
                                nearby_share::mojom::Visibility::kAllContacts);
  cert_store_->ReplacePrivateCertificates(certs_before);
  cert_store_->ClearPrivateCertificates();
  auto certs_after = cert_store_->GetPrivateCertificates();

  ASSERT_TRUE(certs_after.has_value());
  EXPECT_EQ(0u, certs_after->size());
}

TEST_F(NearbyShareCertificateStorageImplTest,
       ClearPrivateCertificatesOfVisibility) {
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  std::vector<NearbySharePrivateCertificate> certs_all_contacts =
      CreatePrivateCertificates(3,
                                nearby_share::mojom::Visibility::kAllContacts);
  std::vector<NearbySharePrivateCertificate> certs_selected_contacts =
      CreatePrivateCertificates(
          3, nearby_share::mojom::Visibility::kSelectedContacts);
  std::vector<NearbySharePrivateCertificate> all_certs;
  all_certs.reserve(certs_all_contacts.size() + certs_selected_contacts.size());
  all_certs.insert(all_certs.end(), certs_all_contacts.begin(),
                   certs_all_contacts.end());
  all_certs.insert(all_certs.end(), certs_selected_contacts.begin(),
                   certs_selected_contacts.end());

  // Remove all-contacts certs then selected-contacts certs.
  {
    cert_store_->ReplacePrivateCertificates(all_certs);
    cert_store_->ClearPrivateCertificatesOfVisibility(
        nearby_share::mojom::Visibility::kAllContacts);
    auto certs_after = cert_store_->GetPrivateCertificates();
    ASSERT_TRUE(certs_after.has_value());
    ASSERT_EQ(certs_selected_contacts.size(), certs_after->size());
    for (size_t i = 0; i < certs_selected_contacts.size(); ++i) {
      EXPECT_EQ(certs_selected_contacts[i].ToDictionary(),
                (*certs_after)[i].ToDictionary());
    }

    cert_store_->ClearPrivateCertificatesOfVisibility(
        nearby_share::mojom::Visibility::kSelectedContacts);
    certs_after = cert_store_->GetPrivateCertificates();
    ASSERT_TRUE(certs_after.has_value());
    EXPECT_EQ(0u, certs_after->size());
  }

  // Remove selected-contacts certs then all-contacts certs.
  {
    cert_store_->ReplacePrivateCertificates(all_certs);
    cert_store_->ClearPrivateCertificatesOfVisibility(
        nearby_share::mojom::Visibility::kSelectedContacts);
    auto certs_after = cert_store_->GetPrivateCertificates();
    ASSERT_TRUE(certs_after.has_value());
    ASSERT_EQ(certs_all_contacts.size(), certs_after->size());
    for (size_t i = 0; i < certs_all_contacts.size(); ++i) {
      EXPECT_EQ(certs_all_contacts[i].ToDictionary(),
                (*certs_after)[i].ToDictionary());
    }

    cert_store_->ClearPrivateCertificatesOfVisibility(
        nearby_share::mojom::Visibility::kAllContacts);
    certs_after = cert_store_->GetPrivateCertificates();
    ASSERT_TRUE(certs_after.has_value());
    EXPECT_EQ(0u, certs_after->size());
  }
}
