// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/server_certificate_database_nss_migrator.h"

#include "base/test/test_future.h"
#include "chrome/browser/net/fake_nss_service.h"
#include "chrome/browser/net/server_certificate_database.h"
#include "chrome/browser/net/server_certificate_database.pb.h"
#include "chrome/browser/net/server_certificate_database_service.h"
#include "chrome/browser/net/server_certificate_database_service_factory.h"
#include "chrome/browser/net/server_certificate_database_test_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

using chrome_browser_server_certificate_database::CertificateTrust;
using ::testing::UnorderedElementsAre;

namespace net {

class ServerCertificateDatabaseNSSMigratorTest : public testing::Test {
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

TEST_F(ServerCertificateDatabaseNSSMigratorTest, MigrateEmptyNssDb) {
  ServerCertificateDatabaseNSSMigrator migrator(profile());
  base::test::TestFuture<ServerCertificateDatabaseNSSMigrator::MigrationResult>
      migrate_certs_waiter;
  migrator.MigrateCerts(migrate_certs_waiter.GetCallback());
  ServerCertificateDatabaseNSSMigrator::MigrationResult result =
      migrate_certs_waiter.Take();
  EXPECT_EQ(0, result.cert_count);
  EXPECT_EQ(0, result.error_count);

  net::ServerCertificateDatabaseService* cert_db_service =
      net::ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
          profile());
  base::test::TestFuture<
      std::vector<net::ServerCertificateDatabase::CertInformation>>
      get_certs_waiter;
  cert_db_service->GetAllCertificates(get_certs_waiter.GetCallback());
  std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos =
      get_certs_waiter.Take();

  EXPECT_TRUE(cert_infos.empty());
}

TEST_F(ServerCertificateDatabaseNSSMigratorTest, MigrateCerts) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Import test certificates into NSS user database.
  base::test::TestFuture<net::NSSCertDatabase*> nss_waiter;
  nss_service()->UnsafelyGetNSSCertDatabaseForTesting(nss_waiter.GetCallback());
  net::NSSCertDatabase* nss_db = nss_waiter.Get();

  NSSCertDatabase::ImportCertFailureList not_imported;
  EXPECT_TRUE(nss_db->ImportServerCert(
      x509_util::CreateCERTCertificateListFromX509Certificate(
          leaf->GetX509Certificate().get()),
      NSSCertDatabase::DISTRUSTED_SSL, &not_imported));
  EXPECT_TRUE(not_imported.empty());

  EXPECT_TRUE(nss_db->ImportCACerts(
      x509_util::CreateCERTCertificateListFromX509Certificate(
          intermediate->GetX509Certificate().get()),
      NSSCertDatabase::TRUST_DEFAULT, &not_imported));
  EXPECT_TRUE(not_imported.empty());

  EXPECT_TRUE(nss_db->ImportCACerts(
      x509_util::CreateCERTCertificateListFromX509Certificate(
          root->GetX509Certificate().get()),
      NSSCertDatabase::TRUSTED_SSL, &not_imported));
  EXPECT_TRUE(not_imported.empty());

  // Import a client cert to NSS also. It shouldn't be migrated.
  EXPECT_TRUE(net::ImportClientCertAndKeyFromFile(
      net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8",
      nss_db->GetPublicSlot().get()));

  // Do the migration from NSS to ServerCertificateDatabase.
  {
    ServerCertificateDatabaseNSSMigrator migrator(profile());
    base::test::TestFuture<
        ServerCertificateDatabaseNSSMigrator::MigrationResult>
        migrate_certs_waiter;
    migrator.MigrateCerts(migrate_certs_waiter.GetCallback());
    ServerCertificateDatabaseNSSMigrator::MigrationResult result =
        migrate_certs_waiter.Take();
    EXPECT_EQ(3, result.cert_count);
    EXPECT_EQ(0, result.error_count);
  }

  // Test that the certs in the ServerCertificateDatabase match the expected
  // certs and trust settings, and that the client cert was not migrated.
  const ServerCertificateDatabase::CertInformation expected_leaf_info =
      MakeCertInfo(leaf->GetDER(),
                   CertificateTrust::CERTIFICATE_TRUST_TYPE_DISTRUSTED);
  const ServerCertificateDatabase::CertInformation expected_intermediate_info =
      MakeCertInfo(intermediate->GetDER(),
                   CertificateTrust::CERTIFICATE_TRUST_TYPE_UNSPECIFIED);
  const ServerCertificateDatabase::CertInformation expected_root_info =
      MakeCertInfo(root->GetDER(),
                   CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED);
  net::ServerCertificateDatabaseService* cert_db_service =
      net::ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
          profile());
  {
    base::test::TestFuture<
        std::vector<net::ServerCertificateDatabase::CertInformation>>
        get_certs_waiter;
    cert_db_service->GetAllCertificates(get_certs_waiter.GetCallback());
    std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos =
        get_certs_waiter.Take();
    EXPECT_THAT(cert_infos,
                UnorderedElementsAre(
                    CertInfoEquals(std::ref(expected_leaf_info)),
                    CertInfoEquals(std::ref(expected_intermediate_info)),
                    CertInfoEquals(std::ref(expected_root_info))));
  }

  // Run the migration again. This simulates what would happen if the migration
  // completed but the browser was closed before the pref could be updated to
  // record that migration had been done. The migration should run again,
  // replacing the entries in the ServerCertificateDatabase with exactly the
  // same value, so the end result should not change.
  {
    ServerCertificateDatabaseNSSMigrator migrator(profile());
    base::test::TestFuture<
        ServerCertificateDatabaseNSSMigrator::MigrationResult>
        migrate_certs_waiter;
    migrator.MigrateCerts(migrate_certs_waiter.GetCallback());
    ServerCertificateDatabaseNSSMigrator::MigrationResult result =
        migrate_certs_waiter.Take();
    EXPECT_EQ(3, result.cert_count);
    EXPECT_EQ(0, result.error_count);
  }

  // Test that the ServerCertificateDatabase still contains the expected certs
  // and nothing else.
  {
    base::test::TestFuture<
        std::vector<net::ServerCertificateDatabase::CertInformation>>
        get_certs_waiter;
    cert_db_service->GetAllCertificates(get_certs_waiter.GetCallback());
    std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos =
        get_certs_waiter.Take();
    EXPECT_THAT(cert_infos,
                UnorderedElementsAre(
                    CertInfoEquals(std::ref(expected_leaf_info)),
                    CertInfoEquals(std::ref(expected_intermediate_info)),
                    CertInfoEquals(std::ref(expected_root_info))));
  }
}

}  // namespace net
