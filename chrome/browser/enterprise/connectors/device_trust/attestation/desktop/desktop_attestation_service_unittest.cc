// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/desktop_attestation_service.h"

#include <memory>

#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/memory_signing_key_pair.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/signing_key_pair.h"
#include "components/enterprise/common/proto/device_trust_report_event.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A sample VerifiedAccess v2 challenge rerepsented as a JSON string.
constexpr char kJsonChallenge[] =
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
 protected:
  DesktopAttestationServiceTest() = default;

  void SetUp() override {
    testing::Test::SetUp();

    // Make sure a signing key exists for the tests. This won't actual require
    // admin rights because of MemorySigningKeyPair.
    auto key_pair = test::CreateInMemorySigningKeyPair(nullptr, nullptr);
    ASSERT_TRUE(key_pair->RotateWithAdminRights("fake_dm_token"));

    attestation_service_ =
        std::make_unique<DesktopAttestationService>(std::move(key_pair));
  }

  DesktopAttestationService* attestation_service() {
    return attestation_service_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<DesktopAttestationService> attestation_service_;
  test::ScopedMemorySigningKeyPairPersistence persistence_scope_;
};

TEST_F(DesktopAttestationServiceTest, BuildChallengeResponse) {
  SignEnterpriseChallengeRequest request;
  SignEnterpriseChallengeReply result;

  // TODO(crbug.com/1208881): Add signals and validate they effectively get
  // added to the signed data.
  auto signals = std::make_unique<DeviceTrustSignals>();

  // Get the challenge from the SignedData json and create request.
  request.set_challenge(JsonChallengeToProtobufChallenge(kJsonChallenge));
  // If challenge is equal to empty string, then
  // `JsonChallengeToProtobufChallenge()` failed.
  EXPECT_NE(request.challenge(), std::string());

  attestation_service()->SignEnterpriseChallenge(request, std::move(signals),
                                                 &result);
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
