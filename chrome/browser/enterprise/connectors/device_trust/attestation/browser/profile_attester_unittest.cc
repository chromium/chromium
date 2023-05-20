// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/profile_attester.h"

#include "base/run_loop.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr char kFakeProfileId[] = "fake-profile-id";
constexpr char kFakeChallengeResponse[] = "fake_challenge_response";
constexpr char kFakeCustomerId[] = "fake_obfuscated_customer_id";
constexpr char kFakeGaiaId[] = "fake_obfuscated_gaia_id";

std::unique_ptr<KeyedService> CreateProfileIDService(
    content::BrowserContext* context) {
  return std::make_unique<enterprise::ProfileIdService>(kFakeProfileId);
}

}  // namespace

class ProfileAttesterTest
    : public ::testing::TestWithParam<std::tuple<bool, bool>> {
 protected:
  ProfileAttesterTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  void SetUp() override {
    enterprise::ProfileIdServiceFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&CreateProfileIDService));

    profile_attester_ = std::make_unique<ProfileAttester>(
        enterprise::ProfileIdServiceFactory::GetForProfile(profile_),
        &mock_profile_cloud_policy_store_);

    levels_.insert(DTCPolicyLevel::kUser);
  }

  bool has_customer_id() { return std::get<0>(GetParam()); }
  bool has_gaia_id() { return std::get<1>(GetParam()); }

  void SetFakeUserPolicyData() {
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    if (has_customer_id()) {
      policy_data->set_obfuscated_customer_id(kFakeCustomerId);
    }
    if (has_gaia_id()) {
      policy_data->set_gaia_id(kFakeGaiaId);
    }

    mock_profile_cloud_policy_store_.set_policy_data_for_testing(
        std::move(policy_data));
  }

  content::BrowserTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  KeyInfo key_info_;
  SignedData signed_data_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  policy::MockCloudPolicyStore mock_profile_cloud_policy_store_;
  std::unique_ptr<ProfileAttester> profile_attester_;
  std::set<DTCPolicyLevel> levels_;
};

// Tests that the correct device details are added when the profile attester
// decorates the key info.
TEST_P(ProfileAttesterTest, DecorateKeyInfo_Success) {
  SetFakeUserPolicyData();

  profile_attester_->DecorateKeyInfo(levels_, key_info_,
                                     run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_EQ(key_info_.profile_id(), kFakeProfileId);
  EXPECT_EQ(key_info_.user_customer_id(),
            has_customer_id() ? kFakeCustomerId : "");
  EXPECT_EQ(key_info_.obfuscated_gaia_id(), has_gaia_id() ? kFakeGaiaId : "");
}

// Tests that no policy data is added when the user cloud policy store is
// null.
TEST_F(ProfileAttesterTest, DecorateKeyInfo_MissingUserCloudPolicyStore) {
  profile_attester_->DecorateKeyInfo(levels_, key_info_,
                                     run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_EQ(key_info_.profile_id(), kFakeProfileId);
  EXPECT_FALSE(key_info_.has_user_customer_id());
  EXPECT_FALSE(key_info_.has_obfuscated_gaia_id());
}

// Tests that no user details are added when the user policy level is missing.
TEST_P(ProfileAttesterTest, DecorateKeyInfo__MissingUserPolicyLevel) {
  SetFakeUserPolicyData();

  profile_attester_->DecorateKeyInfo(std::set<DTCPolicyLevel>(), key_info_,
                                     run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(key_info_.has_profile_id());
  EXPECT_FALSE(key_info_.has_user_customer_id());
  EXPECT_FALSE(key_info_.has_obfuscated_gaia_id());
}

// Tests that no profile level signature is added to the signed data.
TEST_F(ProfileAttesterTest, SignResponse_NoSignature) {
  profile_attester_->SignResponse(levels_, kFakeChallengeResponse, signed_data_,
                                  run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(signed_data_.has_signature());
}

INSTANTIATE_TEST_SUITE_P(,
                         ProfileAttesterTest,
                         testing::Combine(testing::Bool(), testing::Bool()));
}  // namespace enterprise_connectors
