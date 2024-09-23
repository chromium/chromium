// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/user_policy_oidc_signin_service.h"

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor_factory.h"
#include "chrome/browser/enterprise/signin/user_policy_oidc_signin_service.h"
#include "chrome/browser/enterprise/signin/user_policy_oidc_signin_service_factory.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_profile_cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Invoke;
using testing::Return;

using policy::CloudPolicyStore;
using policy::DeviceManagementService;
using policy::MockProfileCloudPolicyStore;
using policy::MockUserCloudPolicyStore;
using policy::ProfileCloudPolicyManager;
using policy::UserCloudPolicyManager;

namespace {
const ProfileManagementOidcTokens kExampleOidcTokens =
    ProfileManagementOidcTokens{"example_auth_token", "example_id_token",
                                u"Test User"};

constexpr char kExampleUserEmail[] = "user@test.com";
constexpr char kExampleDmToken[] = "example_dm_token";
constexpr char kExampleClientId[] = "example_client_id";
constexpr char kExampleGaiaId[] = "123";

class FakeUserPolicyOidcSigninService
    : public policy::UserPolicyOidcSigninService {
 public:
  FakeUserPolicyOidcSigninService(
      Profile* profile,
      PrefService* local_state,
      DeviceManagementService* device_management_service,
      absl::variant<UserCloudPolicyManager*, ProfileCloudPolicyManager*>
          policy_manager,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory,
      bool expect_restore)
      : policy::UserPolicyOidcSigninService(
            profile,
            local_state,
            device_management_service,
            policy_manager,
            identity_manager,
            std::move(system_url_loader_factory)),
        test_profile_(profile),
        expect_restore_(expect_restore) {}

  // policy::UserPolicySigninServiceBase:
  void FetchPolicyForSignedInUser(
      const AccountId& account_id,
      const std::string& dm_token,
      const std::string& client_id,
      const std::vector<std::string>& user_affiliation_ids,
      scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory,
      PolicyFetchCallback callback) override {
    // This function should not be invoked if policy restore is unexpected.
    CHECK(expect_restore_);

    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_gaia_id(kExampleGaiaId);
    if (test_profile_->GetProfileCloudPolicyManager()) {
      static_cast<MockProfileCloudPolicyStore*>(
          test_profile_->GetProfileCloudPolicyManager()->core()->store())
          ->set_policy_data_for_testing(std::move(policy_data));
    } else {
      static_cast<MockUserCloudPolicyStore*>(
          test_profile_->GetUserCloudPolicyManager()->core()->store())
          ->set_policy_data_for_testing(std::move(policy_data));
    }

    std::move(callback).Run(/*success=*/true);
  }

  raw_ptr<Profile> test_profile_ = nullptr;
  bool expect_restore_;
};

// Customized profile manager that ensures the created profiles are properly set
// up for testing.
class UnittestProfileManager : public FakeProfileManager {
 public:
  explicit UnittestProfileManager(const base::FilePath& user_data_dir)
      : FakeProfileManager(user_data_dir) {}

  std::unique_ptr<TestingProfile> BuildTestingProfile(
      const base::FilePath& path,
      Delegate* delegate,
      Profile::CreateMode create_mode) override {
    TestingProfile::Builder builder;
    builder.SetPath(path);
    builder.SetDelegate(delegate);
    builder.SetCreateMode(create_mode);

    if (absl::holds_alternative<std::unique_ptr<UserCloudPolicyManager>>(
            policy_manager_)) {
      builder.SetUserCloudPolicyManager(std::move(
          absl::get<std::unique_ptr<UserCloudPolicyManager>>(policy_manager_)));
    } else {
      builder.SetProfileCloudPolicyManager(
          std::move(absl::get<std::unique_ptr<ProfileCloudPolicyManager>>(
              policy_manager_)));
    }

    return IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);
  }

  void SetPolicyManagerForNextProfile(
      absl::variant<std::unique_ptr<UserCloudPolicyManager>,
                    std::unique_ptr<ProfileCloudPolicyManager>>
          policy_manager) {
    policy_manager_ = std::move(policy_manager);
  }

 private:
  absl::variant<std::unique_ptr<UserCloudPolicyManager>,
                std::unique_ptr<ProfileCloudPolicyManager>>
      policy_manager_;
};

}  // namespace

class UserPolicyOidcSigninServiceTest
    : public BrowserWithTestWindowTest,
      public testing::WithParamInterface<bool>,
      public ProfileAttributesStorageObserver {
 public:
  UserPolicyOidcSigninServiceTest() {
    scoped_feature_list_.InitWithFeatureState(
        profile_management::features::kOidcAuthProfileManagement, true);
  }

  ~UserPolicyOidcSigninServiceTest() override = default;

  void SetUp() override {
    auto profile_path = base::MakeAbsoluteFilePath(
        base::CreateUniqueTempDirectoryScopedToTest());
    auto profile_manager_unique =
        std::make_unique<UnittestProfileManager>(profile_path);
    unit_test_profile_manager_ = profile_manager_unique.get();
    SetUpProfileManager(profile_path, std::move(profile_manager_unique));

    BrowserWithTestWindowTest::SetUp();
    ProfileAttributesStorage& storage = TestingBrowserProcess::GetGlobal()
                                            ->profile_manager()
                                            ->GetProfileAttributesStorage();
    profile_observation_.Observe(&storage);

    // Create the first tab so that web_contents() exists.
    AddTab(browser(), GURL("http://foo/1"));
  }

  void TearDown() override {
    profile_observation_.Reset();
    oidc_signin_service_.reset();
    unit_test_profile_manager_ = nullptr;
    mock_profile_cloud_policy_store_ = nullptr;
    mock_user_cloud_policy_store_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  // If the 3P identity is not synced to Google, the interceptor should follow
  // the Dasherless workflow.
  bool is_3p_identity_synced() { return GetParam(); }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Build a test version CloudPolicyManager for testing profiles.
  std::unique_ptr<UserCloudPolicyManager> BuildUserCloudPolicyManager(
      bool is_store_initialized,
      CloudPolicyStore::Status store_status) {
    auto mock_user_cloud_policy_store =
        std::make_unique<MockUserCloudPolicyStore>();
    mock_user_cloud_policy_store_ = mock_user_cloud_policy_store.get();

    mock_user_cloud_policy_store->status_ = store_status;
    if (is_store_initialized &&
        store_status == CloudPolicyStore::Status::STATUS_OK) {
      mock_user_cloud_policy_store->NotifyStoreLoaded();
    } else if (is_store_initialized) {
      mock_user_cloud_policy_store->NotifyStoreError();
    }

    EXPECT_CALL(*mock_user_cloud_policy_store, Load())
        .Times(testing::AnyNumber());

    return std::make_unique<UserCloudPolicyManager>(
        std::move(mock_user_cloud_policy_store), base::FilePath(),
        /*cloud_external_data_manager=*/nullptr,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        network::TestNetworkConnectionTracker::CreateGetter());
  }

  std::unique_ptr<ProfileCloudPolicyManager> BuildProfileCloudPolicyManager(
      bool is_store_initialized,
      CloudPolicyStore::Status store_status) {
    auto mock_profile_cloud_policy_store =
        std::make_unique<MockProfileCloudPolicyStore>();
    mock_profile_cloud_policy_store_ = mock_profile_cloud_policy_store.get();

    mock_profile_cloud_policy_store_->status_ = store_status;
    if (is_store_initialized &&
        store_status == CloudPolicyStore::Status::STATUS_OK) {
      mock_profile_cloud_policy_store_->NotifyStoreLoaded();
    } else if (is_store_initialized) {
      mock_profile_cloud_policy_store_->NotifyStoreError();
    }

    EXPECT_CALL(*mock_profile_cloud_policy_store, Load())
        .Times(testing::AnyNumber());

    return std::make_unique<ProfileCloudPolicyManager>(
        std::move(mock_profile_cloud_policy_store), base::FilePath(),
        /*cloud_external_data_manager=*/nullptr,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        network::TestNetworkConnectionTracker::CreateGetter());
  }

  void CreateProfileAndInitializeSigninService(bool expect_restore) {
    Profile& profile = profiles::testing::CreateProfileSync(
        unit_test_profile_manager_,
        unit_test_profile_manager_->GenerateNextProfileDirectoryPath());
    Profile* profile_ptr = &profile;

    profile_ptr->GetPrefs()->SetString(
        enterprise_signin::prefs::kPolicyRecoveryToken, kExampleDmToken);
    profile_ptr->GetPrefs()->SetString(
        enterprise_signin::prefs::kPolicyRecoveryClientId, kExampleClientId);
    profile_ptr->GetPrefs()->SetString(
        enterprise_signin::prefs::kProfileUserEmail, kExampleUserEmail);

    absl::variant<UserCloudPolicyManager*, ProfileCloudPolicyManager*>
        policy_manager;

    if (is_3p_identity_synced()) {
      policy_manager = profile_ptr->GetUserCloudPolicyManager();
    } else {
      policy_manager = profile_ptr->GetProfileCloudPolicyManager();
    }

    oidc_signin_service_ = std::make_unique<FakeUserPolicyOidcSigninService>(
        profile_ptr, TestingBrowserProcess::GetGlobal()->local_state(), nullptr,
        policy_manager, IdentityManagerFactory::GetForProfile(profile_ptr),
        nullptr, expect_restore);

    oidc_signin_service_->OnProfileReady(profile_ptr);
  }

  void ConfirmHasPolicy() {
    if (mock_profile_cloud_policy_store_) {
      CHECK(mock_profile_cloud_policy_store_->has_policy());
    } else if (mock_user_cloud_policy_store_) {
      CHECK(mock_user_cloud_policy_store_->has_policy());
    }
  }

  // ManagedProfileCreator::
  void OnProfileAdded(const base::FilePath& profile_path) override {
    auto* entry = TestingBrowserProcess::GetGlobal()
                      ->profile_manager()
                      ->GetProfileAttributesStorage()
                      .GetProfileAttributesWithPath(profile_path);

    entry->SetProfileManagementOidcTokens(kExampleOidcTokens);
    entry->SetDasherlessManagement(!is_3p_identity_synced());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<UnittestProfileManager> unit_test_profile_manager_;
  raw_ptr<MockProfileCloudPolicyStore> mock_profile_cloud_policy_store_;
  raw_ptr<MockUserCloudPolicyStore> mock_user_cloud_policy_store_;
  std::unique_ptr<FakeUserPolicyOidcSigninService> oidc_signin_service_;

  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_observation_{this};
};

TEST_P(UserPolicyOidcSigninServiceTest, UninitializedStorePolicyRecovery) {
  is_3p_identity_synced()
      ? unit_test_profile_manager_->SetPolicyManagerForNextProfile(
            BuildUserCloudPolicyManager(/*is_store_initialized=*/false,
                                        CloudPolicyStore::Status::STATUS_OK))
      : unit_test_profile_manager_->SetPolicyManagerForNextProfile(
            BuildProfileCloudPolicyManager(
                /*is_store_initialized=*/false,
                CloudPolicyStore::Status::STATUS_OK));

  CreateProfileAndInitializeSigninService(/*expect_restore=*/true);

  if (mock_profile_cloud_policy_store_) {
    mock_profile_cloud_policy_store_->NotifyStoreError();
  } else if (mock_user_cloud_policy_store_) {
    mock_user_cloud_policy_store_->NotifyStoreError();
  }

  base::RunLoop().RunUntilIdle();

  ConfirmHasPolicy();
}

TEST_P(UserPolicyOidcSigninServiceTest, InitializedStorePolicyRecovery) {
  is_3p_identity_synced()
      ? unit_test_profile_manager_->SetPolicyManagerForNextProfile(
            BuildUserCloudPolicyManager(
                /*is_store_initialized=*/true,
                CloudPolicyStore::Status::STATUS_LOAD_ERROR))
      : unit_test_profile_manager_->SetPolicyManagerForNextProfile(
            BuildProfileCloudPolicyManager(
                /*is_store_initialized=*/true,
                CloudPolicyStore::Status::STATUS_LOAD_ERROR));

  CreateProfileAndInitializeSigninService(/*expect_restore=*/true);
  base::RunLoop().RunUntilIdle();

  ConfirmHasPolicy();
}

TEST_P(UserPolicyOidcSigninServiceTest, InitializedSuccessLoad) {
  is_3p_identity_synced()
      ? unit_test_profile_manager_->SetPolicyManagerForNextProfile(
            BuildUserCloudPolicyManager(
                /*is_store_initialized=*/true,
                CloudPolicyStore::Status::STATUS_OK))
      : unit_test_profile_manager_->SetPolicyManagerForNextProfile(
            BuildProfileCloudPolicyManager(
                /*is_store_initialized=*/true,
                CloudPolicyStore::Status::STATUS_OK));

  CreateProfileAndInitializeSigninService(/*expect_restore=*/false);
  base::RunLoop().RunUntilIdle();
}

TEST_P(UserPolicyOidcSigninServiceTest, UninitializedSuccessLoad) {
  is_3p_identity_synced()
      ? unit_test_profile_manager_->SetPolicyManagerForNextProfile(
            BuildUserCloudPolicyManager(
                /*is_store_initialized=*/false,
                CloudPolicyStore::Status::STATUS_LOAD_ERROR))
      : unit_test_profile_manager_->SetPolicyManagerForNextProfile(
            BuildProfileCloudPolicyManager(
                /*is_store_initialized=*/false,
                CloudPolicyStore::Status::STATUS_LOAD_ERROR));

  CreateProfileAndInitializeSigninService(/*expect_restore=*/false);

  if (mock_profile_cloud_policy_store_) {
    mock_profile_cloud_policy_store_->NotifyStoreLoaded();
  } else if (mock_user_cloud_policy_store_) {
    mock_user_cloud_policy_store_->NotifyStoreLoaded();
  }
  base::RunLoop().RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(All,
                         UserPolicyOidcSigninServiceTest,
                         /*is_3p_identity_synced=*/testing::Bool());
