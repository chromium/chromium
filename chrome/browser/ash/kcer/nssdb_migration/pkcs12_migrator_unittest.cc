// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/kcer/nssdb_migration/pkcs12_migrator.h"

#include <memory>

#include "ash/components/kcer/chaps/mock_high_level_chaps_client.h"
#include "ash/components/kcer/kcer.h"
#include "ash/components/kcer/kcer_impl.h"
#include "ash/components/kcer/kcer_nss/test_utils.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/kcer/kcer_factory_ash.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/net/fake_nss_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cert/scoped_nss_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using ObjectHandle = kcer::SessionChapsClient::ObjectHandle;
using base::Bucket;
using base::test::RunOnceCallback;
using base::test::RunOnceCallbackRepeatedly;

namespace kcer {
namespace {

enum class NssSlot {
  kPublic,
  kPrivate,
};

std::u16string GetPassword(const std::string& file_name) {
  if (file_name == "client.p12") {
    return u"12345";
  }
  if (file_name == "client_with_ec_key.p12") {
    return u"123456";
  }
  if (file_name == "client-empty-password.p12") {
    return u"";
  }
  ADD_FAILURE() << "GetPassword() is called with an unexpected file name";
  return u"";
}

std::unique_ptr<KeyedService> CreateKcer(
    base::WeakPtr<internal::KcerToken> user_token,
    content::BrowserContext* context) {
  auto kcer = std::make_unique<internal::KcerImpl>();
  kcer->Initialize(content::GetUIThreadTaskRunner(), user_token, nullptr);
  return std::make_unique<KcerFactoryAsh::KcerService>(std::move(kcer));
}

class KcerPkcs12MigratorTest : public testing::Test {
 public:
  KcerPkcs12MigratorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                          base::test::TaskEnvironment::MainThreadType::UI),
        fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()) {}

  void SetUp() override {
    profile_ = TestingProfile::Builder().Build();
    auto account = AccountId::FromUserEmail("test@example.com");
    fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account, false, user_manager::UserType::kRegular, profile_.get());
    fake_user_manager_->OnUserProfileCreated(account, profile_->GetPrefs());
    fake_user_manager_->LoginUser(account);

    migrator_ = std::make_unique<Pkcs12Migrator>(profile_.get());

    nss_service_ = FakeNssService::InitializeForBrowserContext(
        profile_.get(),
        /*enable_system_slot=*/false);

    InitKcer(profile_.get());

    // Sanity check that by default the flag is false for all tests.
    ASSERT_FALSE(GetDualWrittenFlag());
  }

  void TearDown() override { nss_service_ = nullptr; }

  void InitKcer(Profile* profile) {
    kcer_token_ =
        internal::KcerToken::CreateForNss(Token::kUser, &chaps_client_);
    kcer_token_->InitializeForNss(crypto::ScopedPK11Slot(
        PK11_ReferenceSlot(nss_service_->GetPrivateSlot())));
    KcerFactoryAsh::GetInstance()->SetTestingFactoryAndUse(
        profile, base::BindRepeating(&CreateKcer, kcer_token_->GetWeakPtr()));
  }

  void ImportPkcs12(const std::string& file_name, NssSlot slot) {
    base::test::TestFuture<net::NSSCertDatabase*> nss_waiter;
    nss_service_->UnsafelyGetNSSCertDatabaseForTesting(
        nss_waiter.GetCallback());
    net::NSSCertDatabase* nss_db = nss_waiter.Get();

    std::vector<uint8_t> pkcs12_bytes = ReadTestFile(file_name);
    std::string pkcs12_str(pkcs12_bytes.begin(), pkcs12_bytes.end());

    PK11SlotInfo* slot_info = nullptr;
    switch (slot) {
      case NssSlot::kPublic:
        slot_info = nss_db->GetPublicSlot().get();
        break;
      case NssSlot::kPrivate:
        slot_info = nss_db->GetPrivateSlot().get();
        break;
    }

    nss_db->ImportFromPKCS12(slot_info, std::move(pkcs12_str),
                             GetPassword(file_name), true, nullptr);
  }

  void ImportPkcs12PrivSlot(const std::string& file_name) {
    base::test::TestFuture<net::NSSCertDatabase*> nss_waiter;
    nss_service_->UnsafelyGetNSSCertDatabaseForTesting(
        nss_waiter.GetCallback());
    net::NSSCertDatabase* nss_db = nss_waiter.Get();

    std::vector<uint8_t> pkcs12_bytes = ReadTestFile(file_name);
    std::string pkcs12_str(pkcs12_bytes.begin(), pkcs12_bytes.end());

    nss_db->ImportFromPKCS12(nss_db->GetPrivateSlot().get(),
                             std::move(pkcs12_str), GetPassword(file_name),
                             true, nullptr);
  }

  bool GetDualWrittenFlag() {
    return user_manager::UserManager::Get()
        ->GetActiveUser()
        ->GetProfilePrefs()
        ->GetBoolean(prefs::kNssChapsDualWrittenCertsExist);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<internal::KcerToken> kcer_token_;
  std::unique_ptr<Pkcs12Migrator> migrator_;
  MockHighLevelChapsClient chaps_client_;
  raw_ptr<FakeNssService> nss_service_;
  base::HistogramTester histogram_tester_;
};

// Test that Pkcs12Migrator doesn't migrate anything when there's nothing to
// migrate.
TEST_F(KcerPkcs12MigratorTest, NothingToMigrateSuccess) {
  migrator_->Start();
  task_environment_.FastForwardBy(base::Seconds(31));

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kKcerPkcs12MigrationUma),
      BucketsAre(Bucket(KcerPkcs12MigrationEvent::kMigrationStarted, 1),
                 Bucket(KcerPkcs12MigrationEvent::kkNothingToMigrate, 1)));

  EXPECT_FALSE(GetDualWrittenFlag());
}

// Test that Pkcs12Migrator can successfully migrate a single cert.
TEST_F(KcerPkcs12MigratorTest, OneCertMigratedSuccess) {
  ImportPkcs12("client.p12", NssSlot::kPublic);

  // The internal call to ImportPkcs12 tries to find the existing key before
  // importing it. The certs are checked by listing them from NSS (for
  // Kcer-over-NSS).
  EXPECT_CALL(chaps_client_, FindObjects)
      .WillOnce(RunOnceCallback<2>(std::vector<ObjectHandle>{},
                                   chromeos::PKCS11_CKR_OK));
  // Should create 3 objects - public key, private key, cert.
  EXPECT_CALL(chaps_client_, CreateObject)
      .Times(3)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(ObjectHandle(0),
                                                   chromeos::PKCS11_CKR_OK));

  migrator_->Start();
  task_environment_.FastForwardBy(base::Seconds(31));

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kKcerPkcs12MigrationUma),
      BucketsAre(Bucket(KcerPkcs12MigrationEvent::kMigrationStarted, 1),
                 Bucket(KcerPkcs12MigrationEvent::kMigrationFinishedSuccess, 1),
                 Bucket(KcerPkcs12MigrationEvent::kCertMigratedSuccess, 1)));

  EXPECT_TRUE(GetDualWrittenFlag());
}

// Test that Pkcs12Migrator can successfully migrate multiple certs.
TEST_F(KcerPkcs12MigratorTest, MultipleCertsMigratedSuccess) {
  ImportPkcs12("client.p12", NssSlot::kPublic);
#if defined(MEMORY_SANITIZER)
  // For whatever reason NSS behaves differently under memory sanitizer and
  // fails to import the client_with_ec_key.p12 file. It's working on a normal
  // build, and it's useful to test files with EC keys, so only replace the file
  // for the MSAN builds.
  ImportPkcs12("client-empty-password.p12", NssSlot::kPublic);
#else
  ImportPkcs12("client_with_ec_key.p12", NssSlot::kPublic);
#endif

  // The internal call to ImportPkcs12 tries to find the existing key for each
  // PKCS#12 file before importing it. The certs are checked by listing them
  // from NSS (for Kcer-over-NSS).
  EXPECT_CALL(chaps_client_, FindObjects)
      .Times(2)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(std::vector<ObjectHandle>{},
                                                   chromeos::PKCS11_CKR_OK));
  // Should create 6 objects - {public key, private key, cert} x2.
  EXPECT_CALL(chaps_client_, CreateObject)
      .Times(6)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(ObjectHandle(0),
                                                   chromeos::PKCS11_CKR_OK));

  migrator_->Start();
  task_environment_.FastForwardBy(base::Seconds(31));

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kKcerPkcs12MigrationUma),
      BucketsAre(Bucket(KcerPkcs12MigrationEvent::kMigrationStarted, 1),
                 Bucket(KcerPkcs12MigrationEvent::kMigrationFinishedSuccess, 1),
                 Bucket(KcerPkcs12MigrationEvent::kCertMigratedSuccess, 2)));

  EXPECT_TRUE(GetDualWrittenFlag());
}

// Test that Pkcs12Migrator doesn't migrate certs that are already present in
// the private slot (i.e. in Chaps).
TEST_F(KcerPkcs12MigratorTest, CertAlreadyExists) {
  ImportPkcs12("client.p12", NssSlot::kPublic);
  ImportPkcs12("client.p12", NssSlot::kPrivate);

  migrator_->Start();
  task_environment_.FastForwardBy(base::Seconds(31));

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kKcerPkcs12MigrationUma),
      BucketsAre(Bucket(KcerPkcs12MigrationEvent::kMigrationStarted, 1),
                 Bucket(KcerPkcs12MigrationEvent::kkNothingToMigrate, 1)));

  EXPECT_FALSE(GetDualWrittenFlag());
}

// Test that Pkcs12Migrator doesn't migrate certs that are already present in
// the private slot (i.e. in Chaps), but migrates the other ones.
TEST_F(KcerPkcs12MigratorTest, SomeCertsAlreadyExist) {
  ImportPkcs12("client.p12", NssSlot::kPublic);
  ImportPkcs12("client.p12", NssSlot::kPrivate);

#if defined(MEMORY_SANITIZER)
  // For whatever reason NSS behaves differently under memory sanitizer and
  // fails to import the client_with_ec_key.p12 file. It's working on a normal
  // build, and it's useful to test files with EC keys, so only replace the file
  // for the MSAN builds.
  ImportPkcs12("client-empty-password.p12", NssSlot::kPublic);
#else
  ImportPkcs12("client_with_ec_key.p12", NssSlot::kPublic);
#endif

  // For files that should be imported, the internal call to ImportPkcs12 will
  // try to find the existing key before importing it for each file. The certs
  // are checked by listing them from NSS (for Kcer-over-NSS).
  EXPECT_CALL(chaps_client_, FindObjects)
      .WillOnce(RunOnceCallback<2>(std::vector<ObjectHandle>{},
                                   chromeos::PKCS11_CKR_OK));
  // Should create 3 objects - public key, private key, cert.
  EXPECT_CALL(chaps_client_, CreateObject)
      .Times(3)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(ObjectHandle(0),
                                                   chromeos::PKCS11_CKR_OK));

  migrator_->Start();
  task_environment_.FastForwardBy(base::Seconds(31));

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kKcerPkcs12MigrationUma),
      BucketsAre(Bucket(KcerPkcs12MigrationEvent::kMigrationStarted, 1),
                 Bucket(KcerPkcs12MigrationEvent::kMigrationFinishedSuccess, 1),
                 Bucket(KcerPkcs12MigrationEvent::kCertMigratedSuccess, 1)));

  EXPECT_TRUE(GetDualWrittenFlag());
}

// Test that Pkcs12Migrator correctly handles errors from re-importing a cert.
TEST_F(KcerPkcs12MigratorTest, CertMigrationFailed) {
  ImportPkcs12("client.p12", NssSlot::kPublic);

  // The internal call to ImportPkcs12 tries to find the existing key before
  // importing it. The certs are checked by listing them from NSS (for
  // Kcer-over-NSS).
  EXPECT_CALL(chaps_client_, FindObjects)
      .WillOnce(RunOnceCallback<2>(std::vector<ObjectHandle>{},
                                   chromeos::PKCS11_CKR_OK));
  EXPECT_CALL(chaps_client_, CreateObject)
      .Times(1)
      .WillRepeatedly(RunOnceCallbackRepeatedly<2>(
          ObjectHandle(0), chromeos::PKCS11_CKR_GENERAL_ERROR));

  migrator_->Start();
  task_environment_.FastForwardBy(base::Seconds(31));

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kKcerPkcs12MigrationUma),
      BucketsAre(Bucket(KcerPkcs12MigrationEvent::kMigrationStarted, 1),
                 Bucket(KcerPkcs12MigrationEvent::kMigrationFinishedFailure, 1),
                 Bucket(KcerPkcs12MigrationEvent::kFailedToReimportCert, 1)));

  // True because even when it fails, some Chaps objects in theory might have
  // been created and not deleted.
  EXPECT_TRUE(GetDualWrittenFlag());
}

}  // namespace
}  // namespace kcer
