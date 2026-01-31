// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/flex_attester.h"

#include <memory>
#include <set>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr char kFakeDeviceId[] = "fake_device_id";
constexpr char kFakeDomain[] = "fake_domain.com";
constexpr char kFakeUserEmail[] = "test@fake_domain.com";
constexpr char kFakeChallengeResponse[] = "fake_challenge_response";

}  // namespace

class FlexAttesterTest : public testing::Test {
 protected:
  FlexAttesterTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(user_manager_.get())) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");

    // TestingProfile creates its own ScopedStubInstallAttributes.
    // We get a pointer to the stub instance via Get() and cast it
    // so we can modify it.
    stub_attributes_ =
        static_cast<ash::StubInstallAttributes*>(ash::InstallAttributes::Get());

    flex_attester_ = std::make_unique<FlexAttester>(profile_);
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

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  // This points to the stub created and managed by TestingProfile's helpers.
  raw_ptr<ash::StubInstallAttributes> stub_attributes_;
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
  std::unique_ptr<FlexAttester> flex_attester_;
};

// Tests that DecorateKeyInfo adds the correct details for an enterprise-managed
// device.
TEST_F(FlexAttesterTest, DecorateKeyInfo_EnterpriseManaged) {
  SetEnterpriseManaged(true);

  KeyInfo key_info;
  base::RunLoop run_loop;
  flex_attester_->DecorateKeyInfo(std::set<DTCPolicyLevel>(), key_info,
                                  run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(key_info.device_id(), kFakeDeviceId);
  EXPECT_EQ(key_info.domain(), kFakeDomain);
  EXPECT_EQ(key_info.customer_id(), g_browser_process->platform_part()
                                        ->browser_policy_connector_ash()
                                        ->GetObfuscatedCustomerID());
}

// Tests that DecorateKeyInfo adds the correct details for a non-managed device
// with a user.
TEST_F(FlexAttesterTest, DecorateKeyInfo_UnmanagedWithUser) {
  SetEnterpriseManaged(false);
  AddUser();

  KeyInfo key_info;
  base::RunLoop run_loop;
  flex_attester_->DecorateKeyInfo(std::set<DTCPolicyLevel>(), key_info,
                                  run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(key_info.device_id(),
            "");  // Device ID is empty for consumer owned.
  EXPECT_EQ(key_info.domain(), kFakeUserEmail);
  EXPECT_FALSE(key_info.has_customer_id());
}

// Tests that DecorateKeyInfo adds the correct details for a non-managed device
// without a user.
TEST_F(FlexAttesterTest, DecorateKeyInfo_UnmanagedNoUser) {
  SetEnterpriseManaged(false);
  // No user added

  KeyInfo key_info;
  base::RunLoop run_loop;
  flex_attester_->DecorateKeyInfo(std::set<DTCPolicyLevel>(), key_info,
                                  run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(key_info.device_id(),
            "");  // Device ID is empty for consumer owned.
  EXPECT_FALSE(key_info.has_domain());
  EXPECT_FALSE(key_info.has_customer_id());
}

// Tests that SignResponse does not add a signature.
TEST_F(FlexAttesterTest, SignResponse_NoSignature) {
  SignedData signed_data;
  base::RunLoop run_loop;
  flex_attester_->SignResponse(std::set<DTCPolicyLevel>(),
                               kFakeChallengeResponse, signed_data,
                               run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(signed_data.has_signature());
}

}  // namespace enterprise_connectors
