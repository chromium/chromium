// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/browser_attestation_service.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/flex_attester.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
// #include
// "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

// A sample VerifiedAccess v2 challenge.
constexpr char kEncodedChallenge[] =
    "CkEKFkVudGVycHJpc2VLZXlDaGFsbGVuZ2USIELlPXqh8+"
    "rZJ2VIqwPXtPFrr653QdRrIzHFwqP+"
    "b3L8GJTcufirLxKAAkindNwTfwYUcbCFDjiW3kXdmDPE0wC0J6b5ZI6X6vOVcSMXTpK7nxsAGK"
    "zFV+i80LCnfwUZn7Ne1bHzloAqBdpLOu53vQ63hKRk6MRPhc9jYVDsvqXfQ7s+"
    "FUA5r3lxdoluxwAUMFqcP4VgnMvKzKTPYbnnB+xj5h5BZqjQToXJYoP4VC3/"
    "ID+YHNsCWy5o7+G5jnq0ak3zeqWfo1+lCibMPsCM+"
    "2g7nCZIwvwWlfoKwv3aKvOVMBcJxPAIxH1w+hH+"
    "NWxqRi6qgZm84q0ylm0ybs6TFjdgLvSViAIp0Z9p/An/"
    "u3W4CMboCswxIxNYRCGrIIVPElE3Yb4QS65mKrg=";

constexpr char kFakeDeviceId[] = "fake_device_id";
constexpr char kFakeDomain[] = "fake_domain.com";
constexpr char kFakeUserEmail[] = "test@example.com";

std::string GetSerializedSignedChallenge() {
  std::string serialized_signed_challenge;
  base::Base64Decode(kEncodedChallenge, &serialized_signed_challenge);
  return serialized_signed_challenge;
}

std::optional<SignedData> ParseDataFromResponse(const std::string& response) {
  std::optional<base::Value> data = base::JSONReader::Read(
      response, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  if (!data || !data->GetDict().FindString("challengeResponse")) {
    return std::nullopt;
  }

  std::string serialized_signed_challenge;
  if (!base::Base64Decode(*data->GetDict().FindString("challengeResponse"),
                          &serialized_signed_challenge)) {
    return std::nullopt;
  }

  SignedData signed_data;
  if (!signed_data.ParseFromString(serialized_signed_challenge)) {
    return std::nullopt;
  }
  return signed_data;
}

}  // namespace

class BrowserAttestationServiceChromeOSTest : public testing::Test {
 protected:
  BrowserAttestationServiceChromeOSTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(user_manager_.get())) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");

    stub_attributes_ =
        static_cast<ash::StubInstallAttributes*>(ash::InstallAttributes::Get());
  }

  void CreateService(VerifiedAccessFlow flow) {
    auto flex_attester = std::make_unique<FlexAttester>(profile_);

    std::vector<std::unique_ptr<Attester>> attesters;
    attesters.push_back(std::move(flex_attester));

    attestation_service_ =
        std::make_unique<BrowserAttestationService>(std::move(attesters), flow);
  }

  void SetEnterpriseManaged(bool is_managed) {
    if (is_managed) {
      stub_attributes_->SetCloudManaged(kFakeDomain, kFakeDeviceId);
    } else {
      stub_attributes_->SetConsumerOwned();
    }
  }

  void AddUser() {
    const AccountId account_id = AccountId::FromUserEmail(kFakeUserEmail);
    user_manager_->AddUser(account_id);
    user_manager_->LoginUser(account_id);
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user_manager_->GetPrimaryUser(), profile_);
  }

  void VerifyAttestationResponse(
      const AttestationResponse& attestation_response) {
    ASSERT_FALSE(attestation_response.challenge_response.empty());
    auto signed_data =
        ParseDataFromResponse(attestation_response.challenge_response);
    ASSERT_TRUE(signed_data);

    // The 'data' field contains the encrypted ChallengeResponse proto.
    EXPECT_FALSE(signed_data->data().empty());

    // FlexAttester never adds a signature.
    EXPECT_TRUE(signed_data->signature().empty());

    EXPECT_EQ(attestation_response.result_code,
              DTAttestationResult::kSuccessNoSignature);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  raw_ptr<ash::StubInstallAttributes> stub_attributes_;
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;

  std::unique_ptr<BrowserAttestationService> attestation_service_;
};

// Test FlexAttester in ENTERPRISE_MACHINE flow (Managed ChromeOS)
TEST_F(BrowserAttestationServiceChromeOSTest, BuildChallengeResponse_Managed) {
  SetEnterpriseManaged(true);
  CreateService(VerifiedAccessFlow::ENTERPRISE_MACHINE);

  auto levels = std::set<DTCPolicyLevel>({DTCPolicyLevel::kBrowser});

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      GetSerializedSignedChallenge(), base::DictValue(), levels,
      future.GetCallback());

  const auto& response = future.Get();
  VerifyAttestationResponse(response);
}

// Test FlexAttester in DEVICE_TRUST_CONNECTOR flow (Unmanaged ChromeOS Flex)
TEST_F(BrowserAttestationServiceChromeOSTest,
       BuildChallengeResponse_Unmanaged) {
  SetEnterpriseManaged(false);
  AddUser();
  CreateService(VerifiedAccessFlow::DEVICE_TRUST_CONNECTOR);

  auto levels = std::set<DTCPolicyLevel>({DTCPolicyLevel::kUser});

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      GetSerializedSignedChallenge(), base::DictValue(), levels,
      future.GetCallback());

  const auto& response = future.Get();
  VerifyAttestationResponse(response);
}

}  // namespace enterprise_connectors
