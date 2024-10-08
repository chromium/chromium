// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/server_certificate_database.h"

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "chrome/browser/net/server_certificate_database.pb.h"
#include "chrome/browser/net/server_certificate_database_test_util.h"
#include "net/test/cert_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using chrome_browser_server_certificate_database::CertificateTrust;
using ::testing::UnorderedElementsAre;

class ServerCertificateDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    CreateDatabase();
  }

  void TearDown() override {
    database_.reset();
    ASSERT_TRUE(temp_dir_.Delete());
  }

  void CreateDatabase() {
    database_ =
        std::make_unique<ServerCertificateDatabase>(temp_dir_.GetPath());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ServerCertificateDatabase> database_;
};

TEST_F(ServerCertificateDatabaseTest, StoreAndRetrieve) {
  EXPECT_TRUE(database_->RetrieveAllCertificates().empty());

  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  ServerCertificateDatabase::CertInformation root_cert_info = MakeCertInfo(
      root->GetDER(), CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED);
  ServerCertificateDatabase::CertInformation intermediate_cert_info =
      MakeCertInfo(intermediate->GetDER(),
                   CertificateTrust::CERTIFICATE_TRUST_TYPE_UNSPECIFIED);

  EXPECT_TRUE(database_->InsertOrUpdateCert(root_cert_info));
  EXPECT_TRUE(database_->InsertOrUpdateCert(intermediate_cert_info));

  EXPECT_THAT(
      database_->RetrieveAllCertificates(),
      UnorderedElementsAre(CertInfoEquals(std::ref(root_cert_info)),
                           CertInfoEquals(std::ref(intermediate_cert_info))));

  // Reopen the database, and it should have the entries.
  database_.reset();
  CreateDatabase();
  EXPECT_THAT(
      database_->RetrieveAllCertificates(),
      UnorderedElementsAre(CertInfoEquals(std::ref(intermediate_cert_info)),
                           CertInfoEquals(std::ref(root_cert_info))));
}

TEST_F(ServerCertificateDatabaseTest, Update) {
  EXPECT_TRUE(database_->RetrieveAllCertificates().empty());

  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  ServerCertificateDatabase::CertInformation root_cert_info = MakeCertInfo(
      root->GetDER(), CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED);
  ServerCertificateDatabase::CertInformation intermediate_cert_info =
      MakeCertInfo(intermediate->GetDER(),
                   CertificateTrust::CERTIFICATE_TRUST_TYPE_UNSPECIFIED);

  EXPECT_TRUE(database_->InsertOrUpdateCert(root_cert_info));
  EXPECT_TRUE(database_->InsertOrUpdateCert(intermediate_cert_info));

  EXPECT_THAT(
      database_->RetrieveAllCertificates(),
      UnorderedElementsAre(CertInfoEquals(std::ref(root_cert_info)),
                           CertInfoEquals(std::ref(intermediate_cert_info))));

  root_cert_info.cert_metadata.mutable_trust()->set_trust_type(
      CertificateTrust::CERTIFICATE_TRUST_TYPE_DISTRUSTED);

  EXPECT_TRUE(database_->InsertOrUpdateCert(root_cert_info));

  EXPECT_THAT(
      database_->RetrieveAllCertificates(),
      UnorderedElementsAre(CertInfoEquals(std::ref(root_cert_info)),
                           CertInfoEquals(std::ref(intermediate_cert_info))));
}

TEST(ServerCertificateDatabaseTrustTest, TestTrustMappings) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  EXPECT_EQ(bssl::CertificateTrustType::UNSPECIFIED,
            ServerCertificateDatabase::GetUserCertificateTrust(MakeCertInfo(
                intermediate->GetDER(),
                CertificateTrust::CERTIFICATE_TRUST_TYPE_UNSPECIFIED)));

  EXPECT_EQ(bssl::CertificateTrustType::DISTRUSTED,
            ServerCertificateDatabase::GetUserCertificateTrust(MakeCertInfo(
                root->GetDER(),
                CertificateTrust::CERTIFICATE_TRUST_TYPE_DISTRUSTED)));

  EXPECT_EQ(bssl::CertificateTrustType::TRUSTED_ANCHOR,
            ServerCertificateDatabase::GetUserCertificateTrust(MakeCertInfo(
                intermediate->GetDER(),
                CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED)));

  EXPECT_EQ(
      bssl::CertificateTrustType::TRUSTED_LEAF,
      ServerCertificateDatabase::GetUserCertificateTrust(MakeCertInfo(
          leaf->GetDER(), CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED)));

  leaf->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/-1);
  EXPECT_EQ(
      bssl::CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF,
      ServerCertificateDatabase::GetUserCertificateTrust(MakeCertInfo(
          leaf->GetDER(), CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED)));

  EXPECT_EQ(
      bssl::CertificateTrustType::TRUSTED_ANCHOR,
      ServerCertificateDatabase::GetUserCertificateTrust(MakeCertInfo(
          root->GetDER(), CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED)));

  EXPECT_EQ(
      std::nullopt,
      ServerCertificateDatabase::GetUserCertificateTrust(MakeCertInfo(
          "invalidcertder", CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED)));
}

}  // namespace net
