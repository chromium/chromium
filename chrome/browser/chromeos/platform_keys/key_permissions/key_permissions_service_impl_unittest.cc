// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service_impl.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/mock_key_permissions_manager.h"
#include "chrome/browser/chromeos/platform_keys/mock_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace chromeos {
namespace platform_keys {
namespace {

// A helper that waits until execution of an asynchronous KeyPermissionsService
// operation has finished and provides access to the results.
template <typename... ResultCallbackArgs>
class ExecutionWaiter {
 public:
  ExecutionWaiter() = default;
  ~ExecutionWaiter() = default;
  ExecutionWaiter(const ExecutionWaiter& other) = delete;
  ExecutionWaiter& operator=(const ExecutionWaiter& other) = delete;

  // Returns the callback to be passed to the KeyPermissionsService operation
  // invocation.
  base::OnceCallback<void(ResultCallbackArgs... result_callback_args)>
  GetCallback() {
    return base::BindOnce(&ExecutionWaiter::OnExecutionDone,
                          weak_ptr_factory_.GetWeakPtr());
  }

  // Waits until the callback returned by GetCallback() has been called.
  void Wait() { run_loop_.Run(); }

 protected:
  // A std::tuple that is capable of storing the arguments passed to the result
  // callback.
  using ResultCallbackArgsTuple =
      std::tuple<std::decay_t<ResultCallbackArgs>...>;

  // Access to the arguments passed to the callback.
  const ResultCallbackArgsTuple& result_callback_args() const {
    EXPECT_TRUE(done_);
    return result_callback_args_;
  }

 private:
  void OnExecutionDone(ResultCallbackArgs... result_callback_args) {
    EXPECT_FALSE(done_);
    done_ = true;
    result_callback_args_ = ResultCallbackArgsTuple(
        std::forward<ResultCallbackArgs>(result_callback_args)...);
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  bool done_ = false;
  ResultCallbackArgsTuple result_callback_args_;

  base::WeakPtrFactory<ExecutionWaiter> weak_ptr_factory_{this};
};

// Supports waiting for the result of KeyPermissionsService::IsCorporateKey.
class IsCorporateKeyExecutionWaiter : public ExecutionWaiter<bool> {
 public:
  IsCorporateKeyExecutionWaiter() = default;
  ~IsCorporateKeyExecutionWaiter() = default;

  bool corporate() const { return std::get<0>(result_callback_args()); }
};

// Supports waiting for the result of KeyPermissionsService::SetCorporateKey.
class SetCorporateKeyExecutionWaiter : public ExecutionWaiter<Status> {
 public:
  SetCorporateKeyExecutionWaiter() = default;
  ~SetCorporateKeyExecutionWaiter() = default;

  Status status() const { return std::get<0>(result_callback_args()); }
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
        .WillRepeatedly(base::test::RunOnceCallback<0>(/*allowed=*/false,
                                                       Status::kSuccess));

    system_token_key_permissions_manager_ =
        std::make_unique<platform_keys::MockKeyPermissionsManager>();

    // All test keys that reside on system token are marked for corporate usage
    // by default unless specified.
    EXPECT_CALL(*system_token_key_permissions_manager_,
                IsKeyAllowedForUsage(_, _, _))
        .WillRepeatedly(
            base::test::RunOnceCallback<0>(/*allowed=*/true, Status::kSuccess));

    platform_keys::KeyPermissionsManagerImpl::
        SetSystemTokenKeyPermissionsManagerForTesting(
            system_token_key_permissions_manager_.get());
  }

 protected:
  void SetKeyLocations(const std::string& public_key,
                       const std::vector<TokenId>& key_locations) {
    ON_CALL(*platform_keys_service_, GetKeyLocations(public_key, _))
        .WillByDefault(testing::Invoke(
            [key_locations](const std::string& public_key_spki_der,
                            GetKeyLocationsCallback callback) {
              std::move(callback).Run(std::move(key_locations),
                                      Status::kSuccess);
            }));
  }

  bool IsCorporateKey(const std::string& public_key) const {
    IsCorporateKeyExecutionWaiter is_corporate_key_waiter;
    key_permissions_service_->IsCorporateKey(
        public_key, is_corporate_key_waiter.GetCallback());
    is_corporate_key_waiter.Wait();
    return is_corporate_key_waiter.corporate();
  }

  void SetCorporateKey(const std::string& public_key) {
    EXPECT_CALL(*user_token_key_permissions_manager_,
                AllowKeyForUsage(_, KeyUsage::kCorporate, public_key))
        .Times(1)
        .WillOnce(base::test::RunOnceCallback<0>(Status::kSuccess));
    EXPECT_CALL(*user_token_key_permissions_manager_,
                IsKeyAllowedForUsage(_, KeyUsage::kCorporate, public_key))
        .WillOnce(
            base::test::RunOnceCallback<0>(/*allowed=*/true, Status::kSuccess));
    SetCorporateKeyExecutionWaiter set_corporate_key_waiter;
    key_permissions_service_->SetCorporateKey(
        public_key, set_corporate_key_waiter.GetCallback());
    set_corporate_key_waiter.Wait();
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
  const std::string kPublicKey = "test_public_key";

  SetKeyLocations(kPublicKey, {TokenId::kSystem});
  EXPECT_TRUE(IsCorporateKey(kPublicKey));

  SetKeyLocations(kPublicKey, {TokenId::kUser, TokenId::kSystem});
  EXPECT_TRUE(IsCorporateKey(kPublicKey));
}

TEST_F(KeyPermissionsServiceImplTest, CorporateRoundTrip) {
  const std::string kPublicKey = "test_public_key";

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
}  // namespace chromeos
