// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service_impl.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/ash/platform_keys/key_permissions/mock_key_permissions_manager.h"
#include "chrome/browser/ash/platform_keys/mock_platform_keys_service.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_test_util.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace platform_keys {
namespace {

using ::chromeos::platform_keys::Status;
using ::chromeos::platform_keys::TokenId;
using ::testing::_;

// Supports waiting for the result of KeyPermissionsService::IsCorporateKey.
class IsCorporateKeyExecutionWaiter
    : public base::test::TestFuture<std::optional<bool>, Status> {
 public:
  bool corporate() { return Get<0>().value(); }
  Status status() { return Get<1>(); }
};

}  // namespace

class KeyPermissionsServiceImplTest : public ::testing::Test {
 public:
  KeyPermissionsServiceImplTest() = default;
  KeyPermissionsServiceImplTest(const KeyPermissionsServiceImplTest&) = delete;
  KeyPermissionsServiceImplTest& operator=(
      const KeyPermissionsServiceImplTest&) = delete;
  ~KeyPermissionsServiceImplTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    platform_keys_service_ = std::make_unique<MockPlatformKeysService>();
    user_token_key_permissions_manager_ =
        std::make_unique<MockKeyPermissionsManager>();

    key_permissions_service_ = std::make_unique<KeyPermissionsServiceImpl>(
        /*is_regular_profile=*/true, /*profile_is_managed=*/true,
        platform_keys_service_.get(),
        user_token_key_permissions_manager_.get());

    // All test keys that reside on user token only are not marked as corporate
    // by default unless specified.
    EXPECT_CALL(*user_token_key_permissions_manager_,
                IsKeyAllowedForUsage(_, _, _))
        .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
            /*allowed=*/false, Status::kSuccess));

    system_token_key_permissions_manager_ =
        std::make_unique<platform_keys::MockKeyPermissionsManager>();

    // All test keys that reside on system token are marked for corporate usage
    // by default unless specified.
    EXPECT_CALL(*system_token_key_permissions_manager_,
                IsKeyAllowedForUsage(_, _, _))
        .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
            /*allowed=*/true, Status::kSuccess));

    platform_keys::KeyPermissionsManagerImpl::
        SetSystemTokenKeyPermissionsManagerForTesting(
            system_token_key_permissions_manager_.get());
  }

 protected:
  void SetKeyLocations(const std::vector<uint8_t>& public_key,
                       const std::vector<TokenId>& key_locations) {
    ON_CALL(*platform_keys_service_, GetKeyLocations(public_key, _))
        .WillByDefault(testing::Invoke(
            [key_locations](std::vector<uint8_t> public_key_spki_der,
                            GetKeyLocationsCallback callback) {
              std::move(callback).Run(std::move(key_locations),
                                      Status::kSuccess);
            }));
  }

  bool IsCorporateKey(std::vector<uint8_t> public_key) const {
    IsCorporateKeyExecutionWaiter is_corporate_key_waiter;
    key_permissions_service_->IsCorporateKey(
        std::move(public_key), is_corporate_key_waiter.GetCallback());
    EXPECT_TRUE(is_corporate_key_waiter.Wait());
    EXPECT_EQ(is_corporate_key_waiter.status(), Status::kSuccess);
    return is_corporate_key_waiter.corporate();
  }

  void SetCorporateKey(const std::vector<uint8_t>& public_key) {
    EXPECT_CALL(*user_token_key_permissions_manager_,
                AllowKeyForUsage(_, KeyUsage::kCorporate, public_key))
        .Times(1)
        .WillOnce(base::test::RunOnceCallback<0>(Status::kSuccess));
    EXPECT_CALL(*user_token_key_permissions_manager_,
                IsKeyAllowedForUsage(_, KeyUsage::kCorporate, public_key))
        .WillOnce(
            base::test::RunOnceCallback<0>(/*allowed=*/true, Status::kSuccess));
    test_util::StatusWaiter set_corporate_key_waiter;

    key_permissions_service_->SetCorporateKey(
        std::vector<uint8_t>(public_key.begin(), public_key.end()),
        set_corporate_key_waiter.GetCallback());
    EXPECT_TRUE(set_corporate_key_waiter.Wait());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  std::unique_ptr<KeyPermissionsServiceImpl> key_permissions_service_;
  std::unique_ptr<MockPlatformKeysService> platform_keys_service_;
  std::unique_ptr<MockKeyPermissionsManager>
      user_token_key_permissions_manager_;
  std::unique_ptr<MockKeyPermissionsManager>
      system_token_key_permissions_manager_;
};

TEST_F(KeyPermissionsServiceImplTest, SystemTokenKeyIsImplicitlyCorporate) {
  const std::vector<uint8_t> kPublicKey{1, 2, 3, 4, 5};

  SetKeyLocations(kPublicKey, {TokenId::kSystem});
  EXPECT_TRUE(IsCorporateKey(kPublicKey));

  SetKeyLocations(kPublicKey, {TokenId::kUser, TokenId::kSystem});
  EXPECT_TRUE(IsCorporateKey(kPublicKey));
}

TEST_F(KeyPermissionsServiceImplTest, CorporateRoundTrip) {
  const std::vector<uint8_t> kPublicKey{1, 2, 3, 4, 5};

  // By default, user-token keys are not corporate.
  SetKeyLocations(kPublicKey, {TokenId::kUser});
  EXPECT_FALSE(IsCorporateKey(kPublicKey));

  SetCorporateKey(kPublicKey);
  EXPECT_TRUE(IsCorporateKey(kPublicKey));

  // Check that a repeated call doesn't corrupt the stored state.
  SetCorporateKey(kPublicKey);
  EXPECT_TRUE(IsCorporateKey(kPublicKey));
}

}  // namespace platform_keys
}  // namespace ash
