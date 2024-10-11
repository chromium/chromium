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

#if BUILDFLAG(IS_CHROMEOS)
class ServerCertificateDatabaseServiceNSSMigratorTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = TestingProfile::Builder().Build();

    nss_service_ = FakeNssService::InitializeForBrowserContext(
        profile_.get(),
        /*enable_system_slot=*/false);
  }

  void TearDown() override { nss_service_ = nullptr; }

  TestingProfile* profile() { return profile_.get(); }
  FakeNssService* nss_service() { return nss_service_; }

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kEnableCertManagementUIV2Write};
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
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
    base::test::TestFuture<
        std::vector<net::ServerCertificateDatabase::CertInformation>>
        get_certs_waiter;
    cert_db_service->GetAllCertificates(get_certs_waiter.GetCallback());
    std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos =
        get_certs_waiter.Take();
    EXPECT_TRUE(cert_infos.empty());
    EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                  prefs::kNSSCertsMigratedToServerCertDb),
              static_cast<int>(ServerCertificateDatabaseService::
                                   NSSMigrationResultPref::kNotMigrated));
  }

  // Call GetAllCertificatesMigrateFromNSSFirstIfNeeded to begin the migration.
  {
    base::test::TestFuture<
        std::vector<net::ServerCertificateDatabase::CertInformation>>
        migrate_and_get_certs_waiter;
    cert_db_service->GetAllCertificatesMigrateFromNSSFirstIfNeeded(
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

  // Call GetAllCertificatesMigrateFromNSSFirstIfNeeded again. Since the
  // migration already completed, this should just get the current contents of
  // the database without re-doing the migration.
  {
    base::test::TestFuture<
        std::vector<net::ServerCertificateDatabase::CertInformation>>
        migrate_and_get_certs_waiter;
    cert_db_service->GetAllCertificatesMigrateFromNSSFirstIfNeeded(
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

  // Call GetAllCertificatesMigrateFromNSSFirstIfNeeded multiple times.
  base::test::TestFuture<
      std::vector<net::ServerCertificateDatabase::CertInformation>>
      waiter1;
  base::test::TestFuture<
      std::vector<net::ServerCertificateDatabase::CertInformation>>
      waiter2;
  cert_db_service->GetAllCertificatesMigrateFromNSSFirstIfNeeded(
      waiter1.GetCallback());
  cert_db_service->GetAllCertificatesMigrateFromNSSFirstIfNeeded(
      waiter2.GetCallback());
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
