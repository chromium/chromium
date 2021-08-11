// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/desktop_attestation_service.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/common/proto/device_trust_report_event.pb.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/ec_signing_key.h"
#endif

namespace {

constexpr char challenge[] =
    "{"
    "\"challenge\": "
    "\"CkEKFkVudGVycHJpc2VLZXlDaGFsbGVuZ2USIELlPXqh8+"
    "rZJ2VIqwPXtPFrr653QdRrIzHFwqP+"
    "b3L8GJTcufirLxKAAkindNwTfwYUcbCFDjiW3kXdmDPE0wC0J6b5ZI6X6vOVcSMXTpK7nxsAGK"
    "zFV+i80LCnfwUZn7Ne1bHzloAqBdpLOu53vQ63hKRk6MRPhc9jYVDsvqXfQ7s+"
    "FUA5r3lxdoluxwAUMFqcP4VgnMvKzKTPYbnnB+xj5h5BZqjQToXJYoP4VC3/"
    "ID+YHNsCWy5o7+G5jnq0ak3zeqWfo1+lCibMPsCM+"
    "2g7nCZIwvwWlfoKwv3aKvOVMBcJxPAIxH1w+hH+"
    "NWxqRi6qgZm84q0ylm0ybs6TFjdgLvSViAIp0Z9p/An/"
    "u3W4CMboCswxIxNYRCGrIIVPElE3Yb4QS65mKrg=\""
    "}";

}  // namespace

namespace enterprise_connectors {

class DesktopAttestationServiceTest : public testing::Test {
 public:
  DesktopAttestationServiceTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    testing::Test::SetUp();
    OSCryptMocker::SetUp();

    attestation_service_ = std::make_unique<DesktopAttestationService>();

#if defined(OS_WIN)
    // The test bots won't already have a signing key stored in platform
    // specific storage, so the attestation_service will have an empty signing
    // key.  Set a key now for testing.
    ECSigningKeyProvider provider;
    auto acceptable_algorithms = {
        crypto::SignatureVerifier::ECDSA_SHA256,
        crypto::SignatureVerifier::RSA_PKCS1_SHA256,
    };
    attestation_service_->SetKeyPairForTesting(
        provider.GenerateSigningKeySlowly(acceptable_algorithms));
#endif
  }

  void TearDown() override {
    testing::Test::TearDown();
    OSCryptMocker::TearDown();
  }

  DesktopAttestationService* attestation_service() {
    return attestation_service_.get();
  }

 private:
  ScopedTestingLocalState local_state_;
  base::test::TaskEnvironment task_environment_;
  policy::FakeBrowserDMTokenStorage dm_token_storage_;
  std::unique_ptr<DesktopAttestationService> attestation_service_;
};

TEST_F(DesktopAttestationServiceTest, BuildChallengeResponse) {
  SignEnterpriseChallengeRequest request;
  SignEnterpriseChallengeReply result;
  // Get the challenge from the SignedData json and create request.
  request.set_challenge(JsonChallengeToProtobufChallenge(challenge));
  // If challenge is equal to empty string, then
  // `JsonChallengeToProtobufChallenge()` failed.
  EXPECT_NE(request.challenge(), std::string());

  attestation_service()->SignEnterpriseChallenge(request, &result);
  // If challenge is equal to empty string, then
  // `JsonChallengeToProtobufChallenge()` failed.
  EXPECT_NE(result.challenge_response(), std::string());

  SignedData signed_data;
  EXPECT_TRUE(signed_data.ParseFromString(result.challenge_response()));

  EXPECT_NE(signed_data.data(), std::string());
  EXPECT_NE(signed_data.signature(), std::string());
}

// Tests that StampReport properly adds the credentials to the proto.
TEST_F(DesktopAttestationServiceTest, StampReport) {
  DeviceTrustReportEvent report;

  attestation_service()->StampReport(report);

  ASSERT_TRUE(report.has_attestation_credential());
  ASSERT_TRUE(report.attestation_credential().has_credential());

  const auto& credential = report.attestation_credential();
  EXPECT_EQ(credential.credential(), attestation_service()->ExportPublicKey());
  EXPECT_EQ(
      credential.format(),
      DeviceTrustReportEvent::Credential::EC_NID_X9_62_PRIME256V1_PUBLIC_DER);
}

}  // namespace enterprise_connectors
