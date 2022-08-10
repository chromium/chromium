// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "ash/components/settings/cros_settings_names.h"
#include "base/bind.h"
#include "chrome/browser/ash/attestation/soft_bind_attestation_flow.h"
#include "chrome/browser/ash/login/users/mock_user_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/attestation/fake_certificate.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "chromeos/ash/components/dbus/attestation/attestation.pb.h"
#include "chromeos/ash/components/dbus/attestation/fake_attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/securemessage/proto/securemessage.pb.h"
#include "third_party/securemessage/src/cpp/include/securemessage/crypto_ops.h"
#include "third_party/securemessage/src/cpp/include/securemessage/public_key_proto_util.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;
using testing::WithArgs;

namespace ash {
namespace attestation {

namespace {

const AccountId& kTestAccountId =
    AccountId::FromUserEmail("test_email@chromium.org");

}  // namespace

class SoftBindAttestationFlowTest : public ::testing::Test {
 public:
  SoftBindAttestationFlowTest()
      : fake_certificate_status_(ATTESTATION_SUCCESS),
        fake_cert_chains_({}),
        fake_cert_chain_read_index_(0),
        result_cert_chain_({}) {
    AttestationClient::InitializeFake();
  }

  ~SoftBindAttestationFlowTest() override { AttestationClient::Shutdown(); }

  void SetUp() override {
    auto mock_attestation_flow =
        std::make_unique<StrictMock<MockAttestationFlow>>();
    mock_attestation_flow_ = mock_attestation_flow.get();
    soft_bind_attestation_flow_ =
        std::make_unique<ash::attestation::SoftBindAttestationFlow>();
    soft_bind_attestation_flow_->SetAttestationFlowForTesting(
        std::move(mock_attestation_flow));
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    settings_helper_.SetBoolean(kAttestationForContentProtectionEnabled, true);
  }

  SoftBindAttestationFlow::Callback CreateCallback() {
    return base::BindOnce(
        &SoftBindAttestationFlowTest::OnAttestationCertificates,
        base::Unretained(this));
  }

  std::string CreateUserKey() {
    std::unique_ptr<securemessage::CryptoOps::KeyPair> key_pair =
        securemessage::CryptoOps::GenerateEcP256KeyPair();
    std::unique_ptr<securemessage::GenericPublicKey> generic_public_key =
        securemessage::PublicKeyProtoUtil::EncodePublicKey(
            *((key_pair->public_key).get()));
    return generic_public_key->SerializeAsString();
  }

  void OnAttestationCertificates(const std::vector<std::string>& cert_chain,
                                 bool valid) {
    result_cert_chain_ = cert_chain;
    result_validity_ = valid;
  }

  void ExpectMockAttestationFlowGetCertificate() {
    EXPECT_CALL(*mock_attestation_flow_,
                GetCertificate(PROFILE_SOFT_BIND_CERTIFICATE, _, _, _, _, _))
        .WillRepeatedly(WithArgs<5>(
            Invoke(this, &SoftBindAttestationFlowTest::FakeGetCertificate)));
  }

  void ExpectMockAttestationFlowGetCertificateTimeout() {
    EXPECT_CALL(*mock_attestation_flow_,
                GetCertificate(PROFILE_SOFT_BIND_CERTIFICATE, _, _, _, _, _))
        .WillRepeatedly(WithArgs<5>(Invoke(
            this, &SoftBindAttestationFlowTest::FakeGetCertificateTimeout)));
  }

  void FakeGetCertificate(AttestationFlow::CertificateCallback callback) {
    std::string cert = fake_cert_chain_read_index_ < fake_cert_chains_.size()
                           ? fake_cert_chains_[fake_cert_chain_read_index_]
                           : "";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), fake_certificate_status_, cert));
    fake_cert_chain_read_index_++;
  }

  void FakeGetCertificateTimeout(
      AttestationFlow::CertificateCallback callback) {
    task_environment_.FastForwardBy(base::Seconds(35));
  }

  void SetFakeCertChain(int start_time_offset_days, int end_time_offset_days) {
    base::Time now = base::Time::Now();
    std::string der_encoded_cert;
    std::string fake_cert_chain;
    net::x509_util::CreateSelfSignedCert(
        crypto::RSAPrivateKey::Create(1024)->key(),
        net::x509_util::DIGEST_SHA256, "CN=test",
        /* serial_number=*/1, now + base::Days(start_time_offset_days),
        now + base::Days(end_time_offset_days),
        /* extension_specs */ {}, &der_encoded_cert);
    net::X509Certificate::GetPEMEncodedFromDER(der_encoded_cert,
                                               &fake_cert_chain);
    fake_cert_chains_.push_back(fake_cert_chain);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  StrictMock<MockAttestationFlow>* mock_attestation_flow_;
  ScopedCrosSettingsTestHelper settings_helper_;
  std::unique_ptr<ash::attestation::SoftBindAttestationFlow>
      soft_bind_attestation_flow_;

  AttestationStatus fake_certificate_status_;
  std::vector<std::string> fake_cert_chains_;
  int fake_cert_chain_read_index_;

  std::vector<std::string> result_cert_chain_;
  bool result_validity_;
};

TEST_F(SoftBindAttestationFlowTest, Success) {
  ExpectMockAttestationFlowGetCertificate();
  SetFakeCertChain(0, 90);
  const std::string user_key = CreateUserKey();
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, result_cert_chain_.size());
  EXPECT_TRUE(result_validity_);
}

TEST_F(SoftBindAttestationFlowTest, FeatureDisabledByPolicy) {
  settings_helper_.SetBoolean(kAttestationForContentProtectionEnabled, false);
  const std::string user_key = CreateUserKey();
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, result_cert_chain_.size());
  EXPECT_EQ("INVALID:attestationNotAllowed", result_cert_chain_[0]);
  EXPECT_FALSE(result_validity_);
}

TEST_F(SoftBindAttestationFlowTest, NotVerifiedDueToUnspecifiedFailure) {
  ExpectMockAttestationFlowGetCertificate();
  SetFakeCertChain(0, 90);
  fake_certificate_status_ = ATTESTATION_UNSPECIFIED_FAILURE;
  const std::string user_key = CreateUserKey();
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, result_cert_chain_.size());
  EXPECT_EQ("INVALID:notVerified", result_cert_chain_[0]);
  EXPECT_FALSE(result_validity_);
}

TEST_F(SoftBindAttestationFlowTest, NotVerifiedDueToBadRequestFailure) {
  ExpectMockAttestationFlowGetCertificate();
  SetFakeCertChain(0, 90);
  fake_certificate_status_ = ATTESTATION_SERVER_BAD_REQUEST_FAILURE;
  const std::string user_key = CreateUserKey();
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, result_cert_chain_.size());
  EXPECT_EQ("INVALID:notVerified", result_cert_chain_[0]);
  EXPECT_FALSE(result_validity_);
}

TEST_F(SoftBindAttestationFlowTest, Timeout) {
  ExpectMockAttestationFlowGetCertificateTimeout();
  const std::string user_key = CreateUserKey();
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, result_cert_chain_.size());
  EXPECT_EQ("INVALID:timeout", result_cert_chain_[0]);
  EXPECT_FALSE(result_validity_);
}

TEST_F(SoftBindAttestationFlowTest, NearlyExpiredCert) {
  ExpectMockAttestationFlowGetCertificate();
  SetFakeCertChain(-89, 1);
  SetFakeCertChain(0, 90);
  const std::string user_key = CreateUserKey();
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, result_cert_chain_.size());
  EXPECT_TRUE(result_validity_);
  EXPECT_EQ(2, fake_cert_chain_read_index_);
}

TEST_F(SoftBindAttestationFlowTest, ExpiredCertRenewed) {
  ExpectMockAttestationFlowGetCertificate();
  SetFakeCertChain(-90, 0);
  SetFakeCertChain(0, 90);
  const std::string user_key = CreateUserKey();
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, result_cert_chain_.size());
  EXPECT_TRUE(result_validity_);
  EXPECT_EQ(2, fake_cert_chain_read_index_);
}

TEST_F(SoftBindAttestationFlowTest, MultipleRenewalsExceedsMaxRetries) {
  ExpectMockAttestationFlowGetCertificate();
  SetFakeCertChain(-90, 0);
  SetFakeCertChain(-90, 0);
  SetFakeCertChain(-90, 0);
  SetFakeCertChain(-90, 0);
  SetFakeCertChain(-90, 0);
  const std::string user_key = CreateUserKey();
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, result_cert_chain_.size());
  EXPECT_EQ("INVALID:tooManyRetries", result_cert_chain_[0]);
  EXPECT_FALSE(result_validity_);
  EXPECT_EQ(4, fake_cert_chain_read_index_);
}

TEST_F(SoftBindAttestationFlowTest, MultipleSuccessesSimultaneously) {
  ExpectMockAttestationFlowGetCertificate();
  SetFakeCertChain(0, 90);
  SetFakeCertChain(0, 90);
  SetFakeCertChain(0, 90);
  const std::string user_key = CreateUserKey();
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, result_cert_chain_.size());
  EXPECT_TRUE(result_validity_);
  EXPECT_EQ(3, fake_cert_chain_read_index_);
}

}  // namespace attestation
}  // namespace ash
