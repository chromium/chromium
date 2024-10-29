// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/server_certificate_database_service.h"

#include "base/test/test_future.h"
#include "chrome/browser/net/server_certificate_database_service.h"
#include "chrome/browser/net/server_certificate_database_service_factory.h"
#include "chrome/browser/net/server_certificate_database_test_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "net/test/cert_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/net/fake_nss_service.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_util_nss.h"
#endif

using chrome_browser_server_certificate_database::CertificateTrust;
using ::testing::UnorderedElementsAre;

namespace net {

class ServerCertificateDatabaseServiceTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = TestingProfile::Builder().Build();
  }

  TestingProfile* profile() { return profile_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kEnableCertManagementUIV2Write};
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(ServerCertificateDatabaseServiceTest, TestNotifications) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  net::ServerCertificateDatabaseService* cert_db_service =
      net::ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
          profile());

  base::test::TestFuture<void> update_waiter;

  auto scoped_observer_subscription =
      cert_db_service->AddObserver(update_waiter.GetRepeatingCallback());

  // Insert a new cert.
  {
    base::test::TestFuture<bool> insert_waiter;
    cert_db_service->AddOrUpdateUserCertificate(
        MakeCertInfo(root->GetDER(),
                     CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED),
        insert_waiter.GetCallback());
    // Insert should be successful.
    EXPECT_TRUE(insert_waiter.Take());
  }
  // Observer notification should have been delivered.
  EXPECT_TRUE(update_waiter.WaitAndClear());

  // Update metadata for existing cert.
  {
    base::test::TestFuture<bool> insert_waiter;
    cert_db_service->AddOrUpdateUserCertificate(
        MakeCertInfo(root->GetDER(),
                     CertificateTrust::CERTIFICATE_TRUST_TYPE_DISTRUSTED),
        insert_waiter.GetCallback());
    // Update should be successful.
    EXPECT_TRUE(insert_waiter.Take());
  }
  // Observer notification should have been delivered.
  EXPECT_TRUE(update_waiter.WaitAndClear());

  // Delete a cert.
  {
    base::test::TestFuture<bool> delete_waiter;
    auto cert_info = MakeCertInfo(
        root->GetDER(), CertificateTrust::CERTIFICATE_TRUST_TYPE_DISTRUSTED);
    cert_db_service->DeleteCertificate(cert_info.sha256hash_hex,
                                       delete_waiter.GetCallback());
    // Delete should be successful.
    EXPECT_TRUE(delete_waiter.Take());
  }
  // Observer notification should have been delivered.
  EXPECT_TRUE(update_waiter.WaitAndClear());

  // Try to delete a cert that doesn't exist.
  {
    base::test::TestFuture<bool> delete_waiter;
    auto cert_info = MakeCertInfo(
        root->GetDER(), CertificateTrust::CERTIFICATE_TRUST_TYPE_DISTRUSTED);
    cert_db_service->DeleteCertificate(cert_info.sha256hash_hex,
                                       delete_waiter.GetCallback());
    // Delete should fail since the cert doesn't exist in the database.
    EXPECT_FALSE(delete_waiter.Take());
  }
  // Observer notification should not be delivered since nothing was actually
  // changed.
  EXPECT_FALSE(update_waiter.IsReady());
}

#if BUILDFLAG(IS_CHROMEOS)
class ServerCertificateDatabaseServiceNSSMigratorTest
    : public ServerCertificateDatabaseServiceTest {
 public:
  void SetUp() override {
    ServerCertificateDatabaseServiceTest::SetUp();

    nss_service_ = FakeNssService::InitializeForBrowserContext(
        profile(), /*enable_system_slot=*/false);
  }

  void TearDown() override { nss_service_ = nullptr; }

  FakeNssService* nss_service() { return nss_service_; }

 private:
  raw_ptr<FakeNssService> nss_service_;
};

TEST_F(ServerCertificateDatabaseServiceNSSMigratorTest, TestMigration) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  // Import test certificate into NSS user database.
  base::test::TestFuture<net::NSSCertDatabase*> nss_waiter;
  nss_service()->UnsafelyGetNSSCertDatabaseForTesting(nss_waiter.GetCallback());
  net::NSSCertDatabase* nss_db = nss_waiter.Get();
  NSSCertDatabase::ImportCertFailureList not_imported;
  EXPECT_TRUE(nss_db->ImportCACerts(
      x509_util::CreateCERTCertificateListFromX509Certificate(
          root->GetX509Certificate().get()),
      NSSCertDatabase::TRUSTED_SSL, &not_imported));
  EXPECT_TRUE(not_imported.empty());

  net::ServerCertificateDatabaseService* cert_db_service =
      net::ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
          profile());

  // Verify that server cert database starts empty and migration pref default
  // is false.
  {
    base::test::TestFuture<uint32_t> get_certs_count_waiter;
    cert_db_service->GetCertificatesCount(get_certs_count_waiter.GetCallback());
    EXPECT_EQ(0U, get_certs_count_waiter.Get());
    EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                  prefs::kNSSCertsMigratedToServerCertDb),
              static_cast<int>(ServerCertificateDatabaseService::
                                   NSSMigrationResultPref::kNotMigrated));
  }

  // Call GetAllCertificates to begin the migration.
  {
    base::test::TestFuture<
        std::vector<net::ServerCertificateDatabase::CertInformation>>
        migrate_and_get_certs_waiter;
    cert_db_service->GetAllCertificates(
        migrate_and_get_certs_waiter.GetCallback());
    std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos =
        migrate_and_get_certs_waiter.Take();

    // Test that the the result includes the migrated cert.
    ServerCertificateDatabase::CertInformation expected_nss_root_info =
        MakeCertInfo(root->GetDER(),
                     CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED);
    EXPECT_THAT(
        cert_infos,
        UnorderedElementsAre(CertInfoEquals(std::ref(expected_nss_root_info))));
    // Migration pref should be true now.
    EXPECT_EQ(
        profile()->GetPrefs()->GetInteger(
            prefs::kNSSCertsMigratedToServerCertDb),
        static_cast<int>(ServerCertificateDatabaseService::
                             NSSMigrationResultPref::kMigratedSuccessfully));
  }

  // Change the settings of the cert that was imported.
  {
    base::test::TestFuture<bool> update_cert_waiter;
    cert_db_service->AddOrUpdateUserCertificate(
        MakeCertInfo(root->GetDER(),
                     CertificateTrust::CERTIFICATE_TRUST_TYPE_DISTRUSTED),
        update_cert_waiter.GetCallback());
    EXPECT_TRUE(update_cert_waiter.Take());
  }

  // Call GetAllCertificates again. Since the migration already completed, this
  // should just get the current contents of the database without re-doing the
  // migration.
  {
    base::test::TestFuture<
        std::vector<net::ServerCertificateDatabase::CertInformation>>
        migrate_and_get_certs_waiter;
    cert_db_service->GetAllCertificates(
        migrate_and_get_certs_waiter.GetCallback());
    std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos =
        migrate_and_get_certs_waiter.Take();

    // Test that the the result still includes the modified cert data and hasn't
    // been overwritten by the NSS settings (which is what would happen if the
    // migration was repeated).
    ServerCertificateDatabase::CertInformation expected_modified_root_info =
        MakeCertInfo(root->GetDER(),
                     CertificateTrust::CERTIFICATE_TRUST_TYPE_DISTRUSTED);
    EXPECT_THAT(cert_infos, UnorderedElementsAre(CertInfoEquals(
                                std::ref(expected_modified_root_info))));
  }
}

TEST_F(ServerCertificateDatabaseServiceNSSMigratorTest, SimultaneousCalls) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  // Import test certificate into NSS user database.
  base::test::TestFuture<net::NSSCertDatabase*> nss_waiter;
  nss_service()->UnsafelyGetNSSCertDatabaseForTesting(nss_waiter.GetCallback());
  net::NSSCertDatabase* nss_db = nss_waiter.Get();
  NSSCertDatabase::ImportCertFailureList not_imported;
  EXPECT_TRUE(nss_db->ImportCACerts(
      x509_util::CreateCERTCertificateListFromX509Certificate(
          root->GetX509Certificate().get()),
      NSSCertDatabase::TRUSTED_SSL, &not_imported));
  EXPECT_TRUE(not_imported.empty());

  net::ServerCertificateDatabaseService* cert_db_service =
      net::ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
          profile());

  // Call GetAllCertificates multiple times.
  base::test::TestFuture<
      std::vector<net::ServerCertificateDatabase::CertInformation>>
      waiter1;
  base::test::TestFuture<
      std::vector<net::ServerCertificateDatabase::CertInformation>>
      waiter2;
  cert_db_service->GetAllCertificates(waiter1.GetCallback());
  cert_db_service->GetAllCertificates(waiter2.GetCallback());
  // Migration pref should be false still.
  EXPECT_EQ(
      profile()->GetPrefs()->GetInteger(prefs::kNSSCertsMigratedToServerCertDb),
      static_cast<int>(ServerCertificateDatabaseService::
                           NSSMigrationResultPref::kNotMigrated));

  // Both callbacks should get run and both should have the migrated cert.
  ServerCertificateDatabase::CertInformation expected_nss_root_info =
      MakeCertInfo(root->GetDER(),
                   CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED);

  std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos1 =
      waiter1.Take();
  std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos2 =
      waiter2.Take();

  EXPECT_THAT(
      cert_infos1,
      UnorderedElementsAre(CertInfoEquals(std::ref(expected_nss_root_info))));
  EXPECT_THAT(
      cert_infos2,
      UnorderedElementsAre(CertInfoEquals(std::ref(expected_nss_root_info))));

  // Migration pref should be true now.
  EXPECT_EQ(
      profile()->GetPrefs()->GetInteger(prefs::kNSSCertsMigratedToServerCertDb),
      static_cast<int>(ServerCertificateDatabaseService::
                           NSSMigrationResultPref::kMigratedSuccessfully));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace net
