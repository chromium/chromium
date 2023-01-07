// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/active_directory/active_directory_policy_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chromeos/ash/components/dbus/authpolicy/fake_authpolicy_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/user_manager/scoped_user_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Note that session exit is asynchronous and thus ActiveDirectoryPolicyManager
// still needs to react reasonably to events happening after session exit has
// been fired.
class ActiveDirectoryPolicyManagerTest : public testing::Test {
 public:
  ActiveDirectoryPolicyManagerTest()
      : user_manager_enabler_(std::make_unique<ash::FakeChromeUserManager>()),
        install_attributes_(
            ash::StubInstallAttributes::CreateActiveDirectoryManaged(
                "realm.com",
                "device_id")) {}

  ActiveDirectoryPolicyManagerTest(const ActiveDirectoryPolicyManagerTest&) =
      delete;
  ActiveDirectoryPolicyManagerTest& operator=(
      const ActiveDirectoryPolicyManagerTest&) = delete;

  // testing::Test overrides:
  void SetUp() override {
    ash::AuthPolicyClient::InitializeFake();
    fake_client()->SetStarted(true);
    fake_client()->set_refresh_user_policy_error(authpolicy::ERROR_NONE);
  }

  void TearDown() override {
    if (mock_external_data_manager())
      EXPECT_CALL(*mock_external_data_manager(), Disconnect());
    policy_manager_->Shutdown();
    ash::AuthPolicyClient::Shutdown();
  }

 protected:
  // Gets the store passed to the policy manager during construction.
  MockCloudPolicyStore* mock_store() {
    DCHECK(policy_manager_);
    return static_cast<MockCloudPolicyStore*>(policy_manager_->store());
  }

  // Gets the data manager passed to the policy manager during construction.
  MockCloudExternalDataManager* mock_external_data_manager() {
    DCHECK(policy_manager_);
    return static_cast<MockCloudExternalDataManager*>(
        policy_manager_->external_data_manager());
  }

  // Initializes the policy manager and verifies expectations on mock classes.
  void InitPolicyManagerAndVerifyExpectations() {
    if (!mock_store()->is_initialized())
      EXPECT_CALL(*mock_store(), Load());
    if (mock_external_data_manager()) {
      EXPECT_CALL(*mock_external_data_manager(),
                  Connect(scoped_refptr<network::SharedURLLoaderFactory>()));
    }
    policy_manager_->Init(&schema_registry_);
    testing::Mock::VerifyAndClearExpectations(mock_store());
    if (mock_external_data_manager())
      testing::Mock::VerifyAndClearExpectations(mock_external_data_manager());
  }

  // Owned by the AuthPolicyClient global instance.
  ash::FakeAuthPolicyClient* fake_client() {
    return ash::FakeAuthPolicyClient::Get();
  }

  // Used to set FakeUserManager.
  user_manager::ScopedUserManager user_manager_enabler_;

  // Initialized by the individual tests but owned by the test class so that it
  // can be shut down automatically after the test has run.
  std::unique_ptr<ActiveDirectoryPolicyManager> policy_manager_;

  SchemaRegistry schema_registry_;

 private:
  base::test::TaskEnvironment task_environment_;
  ash::ScopedStubInstallAttributes install_attributes_;
};

class UserActiveDirectoryPolicyManagerTest
    : public ActiveDirectoryPolicyManagerTest {
 public:
  ~UserActiveDirectoryPolicyManagerTest() override {
    EXPECT_EQ(session_exit_expected_, session_exited_);
  }

 protected:
  UserActiveDirectoryPolicyManager* user_policy_manager() const {
    return static_cast<UserActiveDirectoryPolicyManager*>(
        policy_manager_.get());
  }

  // Creates |policy_manager_| with fake AD account id and
  // |initial_policy_fetch_timeout| as timeout.
  void CreatePolicyManager(base::TimeDelta initial_policy_fetch_timeout) {
    auto account_id = AccountId::AdFromUserEmailObjGuid("bla", "ble");

    base::OnceClosure exit_session =
        base::BindOnce(&UserActiveDirectoryPolicyManagerTest::ExitSession,
                       base::Unretained(this));

    policy_manager_ = std::make_unique<UserActiveDirectoryPolicyManager>(
        account_id, true /* policy_required */, initial_policy_fetch_timeout,
        std::move(exit_session), std::make_unique<MockCloudPolicyStore>(),
        std::make_unique<MockCloudExternalDataManager>());

    EXPECT_FALSE(
        policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  }

  // Expect that session exit will be called below. (Must only be called once.)
  void ExpectSessionExit() {
    ASSERT_FALSE(session_exit_expected_);
    EXPECT_FALSE(session_exited_);
    session_exit_expected_ = true;
  }

  // Expect that session exit has been called above. (Must only be called after
  // ExpectSessionExit().)
  void ExpectSessionExited() {
    ASSERT_TRUE(session_exit_expected_);
    EXPECT_TRUE(session_exited_);
  }

  // Closure to exit the session.
  void ExitSession() {
    EXPECT_TRUE(session_exit_expected_);
    session_exited_ = true;
  }

  bool session_exited_ = false;
  bool session_exit_expected_ = false;
};

TEST_F(UserActiveDirectoryPolicyManagerTest, InitializationChromadEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(ash::features::kChromadAvailable);

  CreatePolicyManager(base::TimeDelta());
  fake_client()->set_refresh_user_policy_error(authpolicy::ERROR_NONE);
  mock_store()->set_policy_data_for_testing(
      std::make_unique<enterprise_management::PolicyData>());
  mock_store()->NotifyStoreLoaded();
  InitPolicyManagerAndVerifyExpectations();

  EXPECT_EQ(policy_manager_->scheduler()->interval(),
            ActiveDirectoryPolicyManager::kFetchIntervalChromadEnabled);
}

TEST_F(UserActiveDirectoryPolicyManagerTest, InitializationChromadDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(ash::features::kChromadAvailable);

  CreatePolicyManager(base::TimeDelta());
  fake_client()->set_refresh_user_policy_error(authpolicy::ERROR_NONE);
  mock_store()->set_policy_data_for_testing(
      std::make_unique<enterprise_management::PolicyData>());
  mock_store()->NotifyStoreLoaded();
  InitPolicyManagerAndVerifyExpectations();

  EXPECT_EQ(policy_manager_->scheduler()->interval(),
            ActiveDirectoryPolicyManager::kFetchIntervalChromadDisabled);
}

TEST_F(UserActiveDirectoryPolicyManagerTest, DontWaitHasCachedPolicy) {
  CreatePolicyManager(base::TimeDelta());

  // Configure policy fetch to fail.
  fake_client()->set_refresh_user_policy_error(authpolicy::ERROR_UNKNOWN);
  mock_store()->set_policy_data_for_testing(
      std::make_unique<enterprise_management::PolicyData>());
  mock_store()->NotifyStoreLoaded();
  // Trigger mock policy fetch from authpolicyd.
  InitPolicyManagerAndVerifyExpectations();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

TEST_F(UserActiveDirectoryPolicyManagerTest, DontWaitNoCachedPolicy) {
  CreatePolicyManager(base::TimeDelta());

  // Configure policy fetch to fail.
  fake_client()->set_refresh_user_policy_error(authpolicy::ERROR_UNKNOWN);

  mock_store()->NotifyStoreError();

  ExpectSessionExit();
  // Trigger mock policy fetch from authpolicyd.
  InitPolicyManagerAndVerifyExpectations();

  ExpectSessionExited();
}

// Initialization is only complete after policy has been fetched and after that
// has been loaded.
TEST_F(UserActiveDirectoryPolicyManagerTest,
       WaitFiniteLoadSuccessFetchSuccessLoadSuccess) {
  CreatePolicyManager(base::Days(365));

  // Configure policy fetch to succeed.
  fake_client()->set_refresh_user_policy_error(authpolicy::ERROR_NONE);

  // Trigger mock policy fetch from authpolicyd.
  InitPolicyManagerAndVerifyExpectations();

  // Simulate successful store load.
  mock_store()->set_policy_data_for_testing(
      std::make_unique<enterprise_management::PolicyData>());
  mock_store()->NotifyStoreLoaded();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Process reply for mock policy fetch.
  EXPECT_CALL(*mock_store(), Load());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Simulate successful store load. At this point initialization is complete.
  mock_store()->NotifyStoreLoaded();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

// Initialization is only complete after policy has been fetched and after that
// has failed to load. Load failure should not prevent session start as long as
// we have cached policy.
TEST_F(UserActiveDirectoryPolicyManagerTest,
       WaitFinite_LoadSuccessFetchSuccessLoadFail) {
  CreatePolicyManager(base::Days(365));

  // Configure policy fetch to succeed.
  fake_client()->set_refresh_user_policy_error(authpolicy::ERROR_NONE);

  // Trigger mock policy fetch from authpolicyd.
  InitPolicyManagerAndVerifyExpectations();

  // Simulate successful store load.
  mock_store()->set_policy_data_for_testing(
      std::make_unique<enterprise_management::PolicyData>());
  mock_store()->NotifyStoreLoaded();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Process reply for mock policy fetch.
  EXPECT_CALL(*mock_store(), Load());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Simulate failed store load.
  mock_store()->NotifyStoreError();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

// If the initial fetch timeout is not infinite, we're in best-effort mode but
// still require the policy load to succeed so that there's *some* policy
// present (though possibly outdated).
TEST_F(UserActiveDirectoryPolicyManagerTest, WaitFiniteLoadSuccessFetchFail) {
  CreatePolicyManager(base::Days(365));

  // Configure policy fetch to fail.
  fake_client()->set_refresh_user_policy_error(authpolicy::ERROR_UNKNOWN);

  // Trigger mock policy fetch from authpolicyd.
  InitPolicyManagerAndVerifyExpectations();

  // Simulate successful store load.
  mock_store()->set_policy_data_for_testing(
      std::make_unique<enterprise_management::PolicyData>());
  mock_store()->NotifyStoreLoaded();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Process reply for mock policy fetch. At this point initialization is
  // complete (we have waited for the fetch but now that we know it has failed
  // we continue).
  EXPECT_CALL(*mock_store(), Load());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Simulate successful store load.
  mock_store()->NotifyStoreLoaded();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

// If the initial fetch timeout is not infinite, we're in best-effort mode but
// still require the policy load to succeed so that there's *some* policy
// present (though possibly outdated). Here the sequence is inverted: Fetch
// returns before load.
TEST_F(UserActiveDirectoryPolicyManagerTest, WaitFiniteFetchFailLoadSuccess) {
  CreatePolicyManager(base::Days(365));

  // Configure policy fetch to fail.
  fake_client()->set_refresh_user_policy_error(authpolicy::ERROR_UNKNOWN);

  // Trigger mock policy fetch from authpolicyd.
  InitPolicyManagerAndVerifyExpectations();

  // Process reply for mock policy fetch.
  EXPECT_CALL(*mock_store(), Load());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Simulate successful store load.
  mock_store()->set_policy_data_for_testing(
      std::make_unique<enterprise_management::PolicyData>());
  mock_store()->NotifyStoreLoaded();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

// If the initial fetch timeout is not infinite, we're in best-effort mode but
// if we can't load existing policy from disk we have to give up.
TEST_F(UserActiveDirectoryPolicyManagerTest, WaitFiniteLoadFailFetchFail) {
  CreatePolicyManager(base::Days(365));

  // Configure policy fetch to fail.
  fake_client()->set_refresh_user_policy_error(authpolicy::ERROR_UNKNOWN);

  // Trigger mock policy fetch from authpolicyd.
  InitPolicyManagerAndVerifyExpectations();

  // Simulate failed store load.
  mock_store()->NotifyStoreError();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  ExpectSessionExit();

  // Process reply for mock policy fetch.
  EXPECT_CALL(*mock_store(), Load());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  ExpectSessionExited();

  // Simulate successful store load.
  mock_store()->set_policy_data_for_testing(
      std::make_unique<enterprise_management::PolicyData>());
  mock_store()->NotifyStoreLoaded();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

// If the initial fetch timeout is not infinite, we're in best-effort mode and
// upon timeout initialization is complete if any policy could be loaded from
// disk.
TEST_F(UserActiveDirectoryPolicyManagerTest,
       WaitFiniteLoadSuccessFetchTimeout) {
  CreatePolicyManager(base::Days(365));

  // Configure policy fetch to fail.
  fake_client()->set_refresh_user_policy_error(authpolicy::ERROR_UNKNOWN);

  // Trigger mock policy fetch from authpolicyd.
  InitPolicyManagerAndVerifyExpectations();

  // Simulate successful store load.
  mock_store()->set_policy_data_for_testing(
      std::make_unique<enterprise_management::PolicyData>());
  mock_store()->NotifyStoreLoaded();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Simulate policy fetch timeout.
  user_policy_manager()->ForceTimeoutForTesting();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

// If load fails and fetch times out, we wait until fetch response is called.
TEST_F(UserActiveDirectoryPolicyManagerTest,
       WaitFiniteLoadFailFetchTimeoutFetchSuccess) {
  CreatePolicyManager(base::Days(365));

  // Configure policy fetch to return success.
  fake_client()->set_refresh_user_policy_error(authpolicy::ERROR_NONE);

  // Trigger mock policy fetch from authpolicyd.
  InitPolicyManagerAndVerifyExpectations();

  mock_store()->NotifyStoreError();

  // Simulate policy fetch timeout.
  user_policy_manager()->ForceTimeoutForTesting();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Process reply for mock policy fetch.
  EXPECT_CALL(*mock_store(), Load());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Simulate successful store load.
  mock_store()->set_policy_data_for_testing(
      std::make_unique<enterprise_management::PolicyData>());
  mock_store()->NotifyStoreLoaded();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

// If load fails and fetch times out, we wait until fetch response is called.
// Failure should end the session.
TEST_F(UserActiveDirectoryPolicyManagerTest,
       WaitFiniteLoadFailFetchTimeoutFetchFail) {
  CreatePolicyManager(base::Days(365));

  // Configure policy fetch to fail.
  fake_client()->set_refresh_user_policy_error(authpolicy::ERROR_UNKNOWN);

  // Trigger mock policy fetch from authpolicyd.
  InitPolicyManagerAndVerifyExpectations();

  mock_store()->NotifyStoreError();

  // Simulate policy fetch timeout.
  user_policy_manager()->ForceTimeoutForTesting();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  ExpectSessionExit();
  // Process reply for mock policy fetch.
  EXPECT_CALL(*mock_store(), Load());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  ExpectSessionExited();
}

// Simulate long load by policy store.
TEST_F(UserActiveDirectoryPolicyManagerTest, WaitFiniteFetchSuccessLongLoad) {
  CreatePolicyManager(base::Days(365));
  // Trigger mock policy fetch from authpolicyd.
  InitPolicyManagerAndVerifyExpectations();
  // Process reply for mock policy fetch.
  EXPECT_CALL(*mock_store(), Load());
  base::RunLoop().RunUntilIdle();
  user_policy_manager()->ForceTimeoutForTesting();
  EXPECT_FALSE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Simulate successful store load.
  mock_store()->set_policy_data_for_testing(
      std::make_unique<enterprise_management::PolicyData>());
  mock_store()->NotifyStoreLoaded();
  EXPECT_TRUE(policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

class DeviceActiveDirectoryPolicyManagerTest
    : public ActiveDirectoryPolicyManagerTest {
 protected:
  // Creates |mock_store()| and |policy_manager_|.
  void CreatePolicyManager() {
    policy_manager_ = std::make_unique<DeviceActiveDirectoryPolicyManager>(
        std::make_unique<MockCloudPolicyStore>());

    EXPECT_FALSE(
        policy_manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  }
};

TEST_F(DeviceActiveDirectoryPolicyManagerTest, InitializationChromadEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(ash::features::kChromadAvailable);

  CreatePolicyManager();
  InitPolicyManagerAndVerifyExpectations();

  EXPECT_EQ(policy_manager_->scheduler()->interval(),
            ActiveDirectoryPolicyManager::kFetchIntervalChromadEnabled);
}

TEST_F(DeviceActiveDirectoryPolicyManagerTest, InitializationChromadDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(ash::features::kChromadAvailable);

  CreatePolicyManager();
  InitPolicyManagerAndVerifyExpectations();

  EXPECT_EQ(policy_manager_->scheduler()->interval(),
            ActiveDirectoryPolicyManager::kFetchIntervalChromadDisabled);
}

}  // namespace policy
