// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service_impl.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/platform_keys/mock_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/state_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

    platform_keys_service_ = std::make_unique<MockPlatformKeysService>();
    key_permissions_service_ = std::make_unique<KeyPermissionsServiceImpl>(
        /*profile_is_managed=*/true, profile_->GetPrefs(), policy_service_,
        extensions_state_store_, platform_keys_service_.get());
  }

 protected:
  void SetKeyLocations(const std::string& public_key,
                       const std::vector<TokenId>& key_locations) {
    ON_CALL(*platform_keys_service_, GetKeyLocations(public_key, testing::_))
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
    SetCorporateKeyExecutionWaiter set_corporate_key_waiter;
    key_permissions_service_->SetCorporateKey(
        public_key, set_corporate_key_waiter.GetCallback());
    set_corporate_key_waiter.Wait();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  // Owned by |profile_|.
  policy::MockPolicyService* policy_service_ = nullptr;
  // Owned by |profile_|.
  extensions::StateStore* extensions_state_store_ = nullptr;

  std::unique_ptr<KeyPermissionsServiceImpl> key_permissions_service_;
  std::unique_ptr<platform_keys::MockPlatformKeysService>
      platform_keys_service_;
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
