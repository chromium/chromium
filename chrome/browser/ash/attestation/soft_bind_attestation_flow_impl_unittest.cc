// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/attestation/soft_bind_attestation_flow_impl.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/securemessage/proto/securemessage.pb.h"
#include "third_party/securemessage/src/cpp/include/securemessage/crypto_ops.h"
#include "third_party/securemessage/src/cpp/include/securemessage/public_key_proto_util.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;

namespace ash {
namespace attestation {

namespace {

const AccountId kTestAccountId =
    AccountId::FromUserEmail("test_email@chromium.org");

}  // namespace

class SoftBindAttestationFlowImplTest : public ::testing::Test {
 public:
  SoftBindAttestationFlowImplTest() { AttestationClient::InitializeFake(); }

  ~SoftBindAttestationFlowImplTest() override { AttestationClient::Shutdown(); }

  void SetUp() override {
    auto mock_attestation_flow =
        std::make_unique<StrictMock<MockAttestationFlow>>();
    mock_attestation_flow_ = mock_attestation_flow.get();
    soft_bind_attestation_flow_ =
        std::make_unique<SoftBindAttestationFlowImpl>();
    soft_bind_attestation_flow_->SetAttestationFlowForTesting(
        std::move(mock_attestation_flow));
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    settings_helper_.SetBoolean(kAttestationForContentProtectionEnabled, true);
  }

  SoftBindAttestationFlow::Callback CreateCallback() {
    return base::BindOnce(
        &SoftBindAttestationFlowImplTest::OnAttestationCertificates,
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
    EXPECT_CALL(
        *mock_attestation_flow_,
        GetCertificate(PROFILE_SOFT_BIND_CERTIFICATE, _, _, _, _, _, _, _))
        .WillRepeatedly(WithArgs<7>(Invoke(
            this, &SoftBindAttestationFlowImplTest::FakeGetCertificate)));
  }

  void ExpectMockAttestationFlowGetCertificateTimeout() {
    EXPECT_CALL(
        *mock_attestation_flow_,
        GetCertificate(PROFILE_SOFT_BIND_CERTIFICATE, _, _, _, _, _, _, _))
        .WillRepeatedly(WithArgs<7>(Invoke(
            this,
            &SoftBindAttestationFlowImplTest::FakeGetCertificateTimeout)));
  }

  void FakeGetCertificate(AttestationFlow::CertificateCallback callback) {
    std::string cert = fake_cert_chain_read_index_ < fake_cert_chains_.size()
                           ? fake_cert_chains_[fake_cert_chain_read_index_]
                           : "";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
  raw_ptr<StrictMock<MockAttestationFlow>, DanglingUntriaged>
      mock_attestation_flow_;
  ScopedCrosSettingsTestHelper settings_helper_;
  std::unique_ptr<SoftBindAttestationFlowImpl> soft_bind_attestation_flow_;

  AttestationStatus fake_certificate_status_ = ATTESTATION_SUCCESS;
  std::vector<std::string> fake_cert_chains_;
  size_t fake_cert_chain_read_index_ = 0;

  std::vector<std::string> result_cert_chain_;
  bool result_validity_ = false;
};

TEST_F(SoftBindAttestationFlowImplTest, Success) {
  ExpectMockAttestationFlowGetCertificate();
  SetFakeCertChain(0, 90);
  const std::string user_key = CreateUserKey();
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, result_cert_chain_.size());
  EXPECT_TRUE(result_validity_);
}

TEST_F(SoftBindAttestationFlowImplTest, FeatureDisabledByPolicy) {
  settings_helper_.SetBoolean(kAttestationForContentProtectionEnabled, false);
  const std::string user_key = CreateUserKey();
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, result_cert_chain_.size());
  EXPECT_EQ("INVALID:attestationNotAllowed", result_cert_chain_[0]);
  EXPECT_FALSE(result_validity_);
}

TEST_F(SoftBindAttestationFlowImplTest, NotVerifiedDueToUnspecifiedFailure) {
  ExpectMockAttestationFlowGetCertificate();
  SetFakeCertChain(0, 90);
  fake_certificate_status_ = ATTESTATION_UNSPECIFIED_FAILURE;
  const std::string user_key = CreateUserKey();
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, result_cert_chain_.size());
  EXPECT_EQ("INVALID:notVerified", result_cert_chain_[0]);
  EXPECT_FALSE(result_validity_);
}

TEST_F(SoftBindAttestationFlowImplTest, NotVerifiedDueToBadRequestFailure) {
  ExpectMockAttestationFlowGetCertificate();
  SetFakeCertChain(0, 90);
  fake_certificate_status_ = ATTESTATION_SERVER_BAD_REQUEST_FAILURE;
  const std::string user_key = CreateUserKey();
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, result_cert_chain_.size());
  EXPECT_EQ("INVALID:notVerified", result_cert_chain_[0]);
  EXPECT_FALSE(result_validity_);
}

TEST_F(SoftBindAttestationFlowImplTest, Timeout) {
  ExpectMockAttestationFlowGetCertificateTimeout();
  const std::string user_key = CreateUserKey();
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, result_cert_chain_.size());
  EXPECT_EQ("INVALID:timeout", result_cert_chain_[0]);
  EXPECT_FALSE(result_validity_);
}

TEST_F(SoftBindAttestationFlowImplTest, NearlyExpiredCert) {
  ExpectMockAttestationFlowGetCertificate();
  SetFakeCertChain(-89, 1);
  SetFakeCertChain(0, 90);
  const std::string user_key = CreateUserKey();
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, result_cert_chain_.size());
  EXPECT_TRUE(result_validity_);
  EXPECT_EQ(2u, fake_cert_chain_read_index_);
}

TEST_F(SoftBindAttestationFlowImplTest, ExpiredCertRenewed) {
  ExpectMockAttestationFlowGetCertificate();
  SetFakeCertChain(-90, 0);
  SetFakeCertChain(0, 90);
  const std::string user_key = CreateUserKey();
  soft_bind_attestation_flow_->GetCertificate(CreateCallback(), kTestAccountId,
                                              user_key);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, result_cert_chain_.size());
  EXPECT_TRUE(result_validity_);
  EXPECT_EQ(2u, fake_cert_chain_read_index_);
}

TEST_F(SoftBindAttestationFlowImplTest, MultipleRenewalsExceedsMaxRetries) {
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
  EXPECT_EQ(1u, result_cert_chain_.size());
  EXPECT_EQ("INVALID:tooManyRetries", result_cert_chain_[0]);
  EXPECT_FALSE(result_validity_);
  EXPECT_EQ(4u, fake_cert_chain_read_index_);
}

TEST_F(SoftBindAttestationFlowImplTest, MultipleSuccessesSimultaneously) {
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
  EXPECT_EQ(2u, result_cert_chain_.size());
  EXPECT_TRUE(result_validity_);
  EXPECT_EQ(3u, fake_cert_chain_read_index_);
}

}  // namespace attestation
}  // namespace ash
