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

std::unique_ptr<KeyedService> CreateProfileIDService(
    content::BrowserContext* context) {
  return std::make_unique<enterprise::ProfileIdService>(kFakeProfileId);
}

}  // namespace

class ProfileAttesterTest : public testing::Test {
 protected:
  ProfileAttesterTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
    enterprise::ProfileIdServiceFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&CreateProfileIDService));
  }

  void SetUp() override {
    testing::Test::SetUp();

    profile_attester_ = std::make_unique<ProfileAttester>(
        enterprise::ProfileIdServiceFactory::GetForProfile(profile_),
        &mock_profile_cloud_policy_store_);

    levels_.insert(DTCPolicyLevel::kUser);
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

// Tests that no details are added when the profile attester decorates the key
// info.
TEST_F(ProfileAttesterTest, DecorateKeyInfo_NoKeyDetails) {
  profile_attester_->DecorateKeyInfo(levels_, key_info_,
                                     run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(key_info_.has_key_type());
  EXPECT_FALSE(key_info_.has_domain());
  EXPECT_FALSE(key_info_.has_device_id());
  EXPECT_FALSE(key_info_.has_certificate());
  EXPECT_FALSE(key_info_.has_signed_public_key_and_challenge());
  EXPECT_FALSE(key_info_.has_customer_id());
  EXPECT_FALSE(key_info_.has_browser_instance_public_key());
  EXPECT_FALSE(key_info_.has_signing_scheme());
  EXPECT_FALSE(key_info_.has_device_trust_signals_json());
  EXPECT_FALSE(key_info_.has_dm_token());
}

// Tests that no profile level signature is added to the signature map.
TEST_F(ProfileAttesterTest, SignResponse_NoSignature) {
  profile_attester_->SignResponse(levels_, kFakeChallengeResponse, signed_data_,
                                  run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(signed_data_.has_signature());
}

}  // namespace enterprise_connectors
