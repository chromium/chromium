// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/ash_attestation_cleanup_manager.h"
#include <memory>

#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/fake_attestation_client.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kTestUserEmail[] = "test@google.com";

std::string GetDTCDefaultKeyName(std::string username) {
  return ash::attestation::kDeviceTrustConnectorKeyPrefix + username;
}
}  // namespace

namespace enterprise_connectors {

class AshAttestationCleanupManagerTest : public testing::Test {
 public:
  AshAttestationCleanupManagerTest()
      : account_id_(AccountId::FromUserEmail(kTestUserEmail)) {
    ash::AttestationClient::InitializeFake();

    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    user_manager->AddUser(account_id_);

    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
  }

  ~AshAttestationCleanupManagerTest() override {
    ash::AttestationClient::Shutdown();
  }

  void SetDeviceManagement(bool is_managed) {
    if (is_managed) {
      StubInstallAttributes()->SetCloudManaged("test_domain", "test_device_id");
    } else {
      StubInstallAttributes()->SetConsumerOwned();
    }
  }

  ash::StubInstallAttributes* StubInstallAttributes() {
    return stub_install_attributes_.Get();
  }

  AccountId account_id_;
  ash::ScopedStubInstallAttributes stub_install_attributes_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(AshAttestationCleanupManagerTest, UnmanagedDeviceDeleteKeys) {
  SetDeviceManagement(false);

  AshAttestationCleanupManager attestation_cleanup_manager;

  user_manager::UserManager::Get()->RemoveUser(
      account_id_, user_manager::UserRemovalReason::LOCAL_USER_INITIATED);

  auto delete_keys_history =
      ash::AttestationClient::Get()->GetTestInterface()->delete_keys_history();

  EXPECT_EQ(delete_keys_history.size(), 1u);
  EXPECT_EQ(delete_keys_history.front().key_label_match(),
            GetDTCDefaultKeyName(kTestUserEmail));
}

TEST_F(AshAttestationCleanupManagerTest, ManagedDeviceNoCleanup) {
  SetDeviceManagement(true);

  AshAttestationCleanupManager attestation_cleanup_manager;

  user_manager::UserManager::Get()->RemoveUser(
      account_id_, user_manager::UserRemovalReason::LOCAL_USER_INITIATED);

  EXPECT_TRUE(ash::AttestationClient::Get()
                  ->GetTestInterface()
                  ->delete_keys_history()
                  .empty());
}

}  // namespace enterprise_connectors
