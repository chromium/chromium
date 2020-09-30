// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_manager_impl.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/state_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace platform_keys {

class KeyPermissionsManagerImplTest : public ::testing::Test {
 public:
  KeyPermissionsManagerImplTest() = default;
  KeyPermissionsManagerImplTest(const KeyPermissionsManagerImplTest&) = delete;
  KeyPermissionsManagerImplTest& operator=(
      const KeyPermissionsManagerImplTest&) = delete;
  ~KeyPermissionsManagerImplTest() override = default;

  void SetUp() override {
    auto mock_policy_service = std::make_unique<policy::MockPolicyService>();
    policy_service_ = mock_policy_service.get();
    TestingProfile::Builder builder;
    builder.SetPolicyService(std::move(mock_policy_service));
    profile_ = builder.Build();

    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_.get()));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(),
        /*install_directory=*/base::FilePath(),
        /*autoupdate_enabled=*/false);
    extensions_state_store_ = extension_system->state_store();

    key_permissions_manager_ = std::make_unique<KeyPermissionsManagerImpl>(
        /*profile_is_managed=*/true, profile_->GetPrefs(), policy_service_,
        extensions_state_store_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  // Owned by |profile_|.
  policy::MockPolicyService* policy_service_ = nullptr;
  // Owned by |profile_|.
  extensions::StateStore* extensions_state_store_ = nullptr;

  std::unique_ptr<KeyPermissionsManagerImpl> key_permissions_manager_;
};

TEST_F(KeyPermissionsManagerImplTest, SystemTokenKeyIsImplicitlyCorporate) {
  EXPECT_TRUE(key_permissions_manager_->IsCorporateKey("some_public_key",
                                                       {TokenId::kSystem}));
  EXPECT_TRUE(key_permissions_manager_->IsCorporateKey(
      "some_public_key", {TokenId::kUser, TokenId::kSystem}));
}

TEST_F(KeyPermissionsManagerImplTest, CorporateRoundTrip) {
  // By default, user-token keys are not corporate.
  EXPECT_FALSE(key_permissions_manager_->IsCorporateKey("some_public_key",
                                                        {TokenId::kUser}));

  key_permissions_manager_->SetCorporateKey("some_public_key", TokenId::kUser);

  EXPECT_TRUE(key_permissions_manager_->IsCorporateKey("some_public_key",
                                                       {TokenId::kUser}));

  // Check that a repeated call doesn't corrupt the stored state.
  key_permissions_manager_->SetCorporateKey("some_public_key", TokenId::kUser);

  EXPECT_TRUE(key_permissions_manager_->IsCorporateKey("some_public_key",
                                                       {TokenId::kUser}));
}

}  // namespace platform_keys
}  // namespace chromeos
