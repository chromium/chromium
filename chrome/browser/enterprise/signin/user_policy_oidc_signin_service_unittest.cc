// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/user_policy_oidc_signin_service.h"

#include <variant>

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/remote_commands/user_remote_commands_service.h"
#include "chrome/browser/enterprise/remote_commands/user_remote_commands_service_factory.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor_factory.h"
#include "chrome/browser/enterprise/signin/user_policy_oidc_signin_service.h"
#include "chrome/browser/enterprise/signin/user_policy_oidc_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_test_util.h"
#include "chrome/browser/policy/device_management_service_configuration.h"
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
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/core/common/cloud/mock_profile_cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/signin/public/base/consent_level.h"
#include "google_apis/gaia/gaia_id.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

using policy::DeviceManagementService;
using policy::ProfileCloudPolicyManager;
using policy::UserCloudPolicyManager;

namespace policy {
namespace {

const ProfileManagementOidcTokens kExampleOidcTokens =
    ProfileManagementOidcTokens{"example_encrypted_user_info"};
constexpr char kExampleUserEmail[] = "user@test.com";
constexpr char kExampleDmToken[] = "example_dm_token";
constexpr char kExampleClientId[] = "example_client_id";
constexpr GaiaId::Literal kExampleGaiaId("123");

std::unique_ptr<KeyedService> BuildFakeUserPolicySigninService(
    bool is_managed,
    DeviceManagementService* device_management_service,
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  // Non-empty dm token & client id will indicate to PolicyFetchCallback that
  // the account is managed.
  IdentityTestEnvironmentProfileAdaptor(profile)
      .identity_test_env()
      ->MakePrimaryAccountAvailable("123", signin::ConsentLevel::kSignin);

  auto fake_service = std::make_unique<policy::UserPolicySigninService>(
      profile, TestingBrowserProcess::GetGlobal()->local_state(),
      device_management_service, profile->GetUserCloudPolicyManager(),
      IdentityManagerFactory::GetForProfile(profile),
      TestingBrowserProcess::GetGlobal()->shared_url_loader_factory());

  fake_service->set_profile_can_be_managed_for_testing(is_managed);

  return fake_service;
}

// Customized profile manager that ensures the created profiles are properly set
// up for testing.
class UnittestProfileManager : public FakeProfileManager {
 public:
  explicit UnittestProfileManager(
      const base::FilePath& user_data_dir,
      bool is_managed,
      DeviceManagementService* device_management_service)
      : FakeProfileManager(user_data_dir),
        is_managed_(is_managed),
        device_management_service_(device_management_service) {}

  std::unique_ptr<TestingProfile> BuildTestingProfile(
      const base::FilePath& path,
      Delegate* delegate,
      Profile::CreateMode create_mode) override {
    TestingProfile::Builder builder;
    builder.SetPath(path);
    builder.SetDelegate(delegate);
    builder.SetCreateMode(create_mode);

    if (std::holds_alternative<std::unique_ptr<UserCloudPolicyManager>>(
            policy_manager_)) {
      builder.AddTestingFactory(
          policy::UserPolicySigninServiceFactory::GetInstance(),
          base::BindRepeating(&BuildFakeUserPolicySigninService, is_managed_,
                              device_management_service_));
      builder.SetUserCloudPolicyManager(std::move(
          std::get<std::unique_ptr<UserCloudPolicyManager>>(policy_manager_)));
    } else {
      builder.SetProfileCloudPolicyManager(
          std::move(std::get<std::unique_ptr<ProfileCloudPolicyManager>>(
              policy_manager_)));
    }

    return IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);
  }

  void SetPolicyManagerForNextProfile(
      std::variant<std::unique_ptr<UserCloudPolicyManager>,
                   std::unique_ptr<ProfileCloudPolicyManager>> policy_manager) {
    policy_manager_ = std::move(policy_manager);
  }

 private:
  bool is_managed_ = false;
  raw_ptr<DeviceManagementService> device_management_service_;
  std::variant<std::unique_ptr<UserCloudPolicyManager>,
               std::unique_ptr<ProfileCloudPolicyManager>>
      policy_manager_;
};

}  // namespace

class UserPolicyOidcSigninServiceTestBase
    : public BrowserWithTestWindowTest,
      public ProfileAttributesStorageObserver {
 public:
  UserPolicyOidcSigninServiceTestBase() = default;

  ~UserPolicyOidcSigninServiceTestBase() override = default;

  void SetUp() override {
    device_management_service_.ScheduleInitialization(0);
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
    auto path = base::MakeAbsoluteFilePath(
        base::CreateUniqueTempDirectoryScopedToTest());
    auto profile_manager_unique = std::make_unique<UnittestProfileManager>(
        path, is_managed_, &device_management_service_);
    unit_test_profile_manager_ = profile_manager_unique.get();
    SetUpProfileManager(path, std::move(profile_manager_unique));

    BrowserWithTestWindowTest::SetUp();
    ProfileAttributesStorage& storage = TestingBrowserProcess::GetGlobal()
                                            ->profile_manager()
                                            ->GetProfileAttributesStorage();
    profile_observation_.Observe(&storage);
    profile_path_ =
        unit_test_profile_manager_->GenerateNextProfileDirectoryPath();
    // Create the first tab so that web_contents() exists.
    AddTab(browser(), GURL("http://foo/1"));
  }

  void TearDown() override {
    profile_observation_.Reset();
    if (oidc_signin_service_) {
      oidc_signin_service_->Shutdown();
    }
    auto* profile = unit_test_profile_manager_->GetProfileByPath(profile_path_);

    if (profile) {
      auto* profile_policy_manager = profile->GetProfileCloudPolicyManager();
      if (profile_policy_manager) {
        profile_policy_manager->Shutdown();
      }

      auto* user_policy_manager = profile->GetUserCloudPolicyManager();
      if (user_policy_manager) {
        user_policy_manager->Shutdown();
      }
    }

    oidc_signin_service_.reset();
    unit_test_profile_manager_ = nullptr;
    mock_profile_cloud_policy_store_ = nullptr;
    mock_user_cloud_policy_store_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  // If the 3P identity is not synced to Google, the interceptor should follow
  // the Dasherless workflow.
  bool is_3p_identity_synced() { return is_3p_identity_synced_; }

  // GAIA service cannot apply policies if profile is unmanaged.
  bool is_managed() { return is_managed_; }

  bool has_policy() { return has_policy_; }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetPolicyData() {
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_gaia_id(kExampleGaiaId.ToString());
    policy_data->set_command_invalidation_topic("fake-topic");
    policy_data->set_cec_enabled(true);
    if (mock_profile_cloud_policy_store_) {
      mock_profile_cloud_policy_store_->set_policy_data_for_testing(
          std::move(policy_data));
    } else {
      mock_user_cloud_policy_store_->set_policy_data_for_testing(
          std::move(policy_data));
    }
  }

  // Build a test version CloudPolicyManager for testing profiles.
  std::unique_ptr<UserCloudPolicyManager> BuildUserCloudPolicyManager(
      bool is_store_initialized,
      CloudPolicyStore::Status store_status) {
    auto mock_user_cloud_policy_store =
        std::make_unique<MockUserCloudPolicyStore>(
            dm_protocol::GetChromeUserPolicyType());
    mock_user_cloud_policy_store_ = mock_user_cloud_policy_store.get();

    ConfigureMockStore(mock_user_cloud_policy_store_.get(),
                       is_store_initialized, store_status);

    ON_CALL(*mock_user_cloud_policy_store_, Clear()).WillByDefault([this]() {
      mock_user_cloud_policy_store_->set_policy_data_for_testing(nullptr);
      mock_user_cloud_policy_store_->NotifyStoreLoaded();
    });

    std::unique_ptr<MockUserCloudPolicyStore>
        mock_user_cloud_policy_extension_install_store;
#if BUILDFLAG(ENABLE_EXTENSIONS)
    mock_user_cloud_policy_extension_install_store =
        std::make_unique<MockUserCloudPolicyStore>(
            dm_protocol::kChromeExtensionInstallUserCloudPolicyType);
    EXPECT_CALL(*mock_user_cloud_policy_extension_install_store, Load())
        .Times(testing::AnyNumber());
#endif

    if (has_policy()) {
      SetPolicyData();
    }

    return std::make_unique<UserCloudPolicyManager>(
        std::move(mock_user_cloud_policy_store),
        std::move(mock_user_cloud_policy_extension_install_store),
        base::FilePath(),
        /*cloud_external_data_manager=*/nullptr,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        network::TestNetworkConnectionTracker::CreateGetter());
  }

  std::unique_ptr<ProfileCloudPolicyManager> BuildProfileCloudPolicyManager(
      bool is_store_initialized,
      CloudPolicyStore::Status store_status) {
    auto mock_profile_cloud_policy_store =
        std::make_unique<MockProfileCloudPolicyStore>(
            dm_protocol::kChromeMachineLevelUserCloudPolicyType);
    mock_profile_cloud_policy_store_ = mock_profile_cloud_policy_store.get();

    ConfigureMockStore(mock_profile_cloud_policy_store_.get(),
                       is_store_initialized, store_status);

    std::unique_ptr<MockProfileCloudPolicyStore>
        mock_profile_cloud_policy_extension_install_store;
#if BUILDFLAG(ENABLE_EXTENSIONS)
    mock_profile_cloud_policy_extension_install_store =
        std::make_unique<MockProfileCloudPolicyStore>(
            dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType);
    EXPECT_CALL(*mock_profile_cloud_policy_extension_install_store, Load())
        .Times(testing::AnyNumber());
#endif

    SetPolicyData();

    return std::make_unique<ProfileCloudPolicyManager>(
        std::move(mock_profile_cloud_policy_store),
        std::move(mock_profile_cloud_policy_extension_install_store),
        base::FilePath(),
        /*cloud_external_data_manager=*/nullptr,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        network::TestNetworkConnectionTracker::CreateGetter());
  }

  void CreateProfileAndInitializeSigninService(
      bool user_email_missing = false) {
    Profile& profile = profiles::testing::CreateProfileSync(
        unit_test_profile_manager_, profile_path_);
    Profile* profile_ptr = &profile;

    profile_ptr->GetPrefs()->SetString(
        enterprise_signin::prefs::kPolicyRecoveryToken, kExampleDmToken);
    profile_ptr->GetPrefs()->SetString(
        enterprise_signin::prefs::kPolicyRecoveryClientId, kExampleClientId);
    if (!user_email_missing) {
      profile_ptr->GetPrefs()->SetString(
          enterprise_signin::prefs::kProfileUserEmail, kExampleUserEmail);
    }

    std::variant<UserCloudPolicyManager*, ProfileCloudPolicyManager*>
        policy_manager;

    if (is_3p_identity_synced()) {
      policy_manager = profile_ptr->GetUserCloudPolicyManager();
    } else {
      policy_manager = profile_ptr->GetProfileCloudPolicyManager();
    }

    std::visit([&](auto* manager) { manager->Init(&schema_registry_); },
               policy_manager);

    oidc_signin_service_ =
        std::make_unique<policy::UserPolicyOidcSigninService>(
            profile_ptr, TestingBrowserProcess::GetGlobal()->local_state(),
            &device_management_service_, policy_manager,
            IdentityManagerFactory::GetForProfile(profile_ptr),
            test_url_loader_factory_.GetSafeWeakWrapper());

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

  void OnPolicyFetchCompleteInNewProfile() {
    oidc_signin_service_->OnPolicyFetchCompleteInNewProfile(
        "123", base::TimeTicks(), false, base::BindOnce([](bool) {}), true);
  }

  void CallOnPolicyFetchCompleteInNewProfile(std::string user_email,
                                             base::TimeTicks start_time,
                                             bool switch_to_entry,
                                             PolicyFetchCallback callback,
                                             bool success) {
    oidc_signin_service_->OnPolicyFetchCompleteInNewProfile(
        std::move(user_email), start_time, switch_to_entry, std::move(callback),
        success);
  }

  template <typename MockStore>
  void ConfigureMockStore(MockStore* store,
                          bool is_store_initialized,
                          CloudPolicyStore::Status store_status) {
    store->status_ = store_status;
    if (is_store_initialized &&
        store_status == CloudPolicyStore::Status::STATUS_OK) {
      store->NotifyStoreLoaded();
    } else if (is_store_initialized) {
      store->NotifyStoreError();
    }

    EXPECT_CALL(*store, Load()).Times(testing::AnyNumber());
  }

 protected:
  bool is_3p_identity_synced_ = true;
  bool has_policy_ = true;
  bool is_managed_ = true;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<UnittestProfileManager> unit_test_profile_manager_;
  raw_ptr<MockProfileCloudPolicyStore> mock_profile_cloud_policy_store_;
  raw_ptr<MockUserCloudPolicyStore> mock_user_cloud_policy_store_;
  std::unique_ptr<policy::UserPolicyOidcSigninService> oidc_signin_service_;
  policy::SchemaRegistry schema_registry_;
  base::FilePath profile_path_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  testing::StrictMock<policy::MockJobCreationHandler> job_creation_handler_;
  policy::FakeDeviceManagementService device_management_service_{
      &job_creation_handler_};

  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_observation_{this};
};

class UserPolicyOidcSigninServiceTest
    : public UserPolicyOidcSigninServiceTestBase,
      public testing::WithParamInterface<std::tuple<bool>> {
 public:
  UserPolicyOidcSigninServiceTest() {
    is_3p_identity_synced_ = std::get<0>(GetParam());
  }

  ~UserPolicyOidcSigninServiceTest() override = default;

  void SetupPolicyRecoveryExpectations(
      DeviceManagementService::JobConfiguration::JobType* job_type_1,
      DeviceManagementService::JobConfiguration::JobType* job_type_2,
      DeviceManagementService::JobForTesting* job,
      base::RunLoop* run_loop) {
    if (is_3p_identity_synced()) {
      EXPECT_CALL(job_creation_handler_, OnJobCreation)
          .WillOnce(DoAll(
              device_management_service_.CaptureJobType(job_type_1),
              [this](auto&&...) { SetPolicyData(); },
              [this](auto&&...) { OnPolicyFetchCompleteInNewProfile(); },
              SaveArg<0>(job)))
          .WillOnce(DoAll(device_management_service_.CaptureJobType(job_type_2),
                          SaveArg<0>(job), [run_loop] { run_loop->Quit(); }));
    } else {
      EXPECT_CALL(job_creation_handler_, OnJobCreation)
          .WillOnce(DoAll(device_management_service_.CaptureJobType(job_type_1),
                          SaveArg<0>(job)))
          .WillOnce(DoAll(
              device_management_service_.CaptureJobType(job_type_2),
              [this](auto&&...) { SetPolicyData(); },
              [this](auto&&...) { OnPolicyFetchCompleteInNewProfile(); },
              SaveArg<0>(job), [run_loop] { run_loop->Quit(); }));
    }
  }

  void SetupBrokenProfileRecoveryExpectation(
      DeviceManagementService::JobConfiguration::JobType* job_type_1,
      DeviceManagementService::JobConfiguration::JobType* job_type_2,
      DeviceManagementService::JobForTesting* job,
      base::RunLoop* run_loop) {
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(
            device_management_service_.CaptureJobType(job_type_1),
            [this](DeviceManagementService::JobForTesting job) {
              SetPolicyData();

              // Set primary account available just before policy fetch
              // completion to simulate the case where primary account already
              // exists.
              auto* profile =
                  unit_test_profile_manager_->GetProfileByPath(profile_path_);
              IdentityTestEnvironmentProfileAdaptor adaptor(profile);
              adaptor.identity_test_env()->MakePrimaryAccountAvailable(
                  kExampleUserEmail, signin::ConsentLevel::kSignin);

              // Pass empty email to simulate broken profile state.
              CallOnPolicyFetchCompleteInNewProfile(
                  std::string(), base::TimeTicks(), false,
                  base::BindOnce([](bool) {}), true);
            },
            SaveArg<0>(job)))
        .WillOnce(DoAll(device_management_service_.CaptureJobType(job_type_2),
                        SaveArg<0>(job), [run_loop] { run_loop->Quit(); }));
  }

  void VerifyPolicyRecoveryJobTypes(
      DeviceManagementService::JobConfiguration::JobType job_type_1,
      DeviceManagementService::JobConfiguration::JobType job_type_2) {
    if (is_3p_identity_synced()) {
      EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
                job_type_1);
      EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REMOTE_COMMANDS,
                job_type_2);
    } else {
      EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REMOTE_COMMANDS,
                job_type_1);
      EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
                job_type_2);
    }
  }
};

TEST_P(UserPolicyOidcSigninServiceTest, UninitializedStorePolicyRecovery) {
  DeviceManagementService::JobConfiguration::JobType job_type_1 =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobConfiguration::JobType job_type_2 =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobForTesting job;

  base::RunLoop run_loop;

  SetupPolicyRecoveryExpectations(&job_type_1, &job_type_2, &job, &run_loop);

  is_3p_identity_synced()
      ? unit_test_profile_manager_->SetPolicyManagerForNextProfile(
            BuildUserCloudPolicyManager(/*is_store_initialized=*/false,
                                        CloudPolicyStore::Status::STATUS_OK))
      : unit_test_profile_manager_->SetPolicyManagerForNextProfile(
            BuildProfileCloudPolicyManager(
                /*is_store_initialized=*/false,
                CloudPolicyStore::Status::STATUS_OK));

  CreateProfileAndInitializeSigninService();

  if (mock_profile_cloud_policy_store_) {
    mock_profile_cloud_policy_store_->NotifyStoreError();
  } else if (mock_user_cloud_policy_store_) {
    mock_user_cloud_policy_store_->NotifyStoreError();
  }

  run_loop.Run();

  VerifyPolicyRecoveryJobTypes(job_type_1, job_type_2);

  ConfirmHasPolicy();
}

TEST_P(UserPolicyOidcSigninServiceTest, InitializedStorePolicyRecovery) {
  DeviceManagementService::JobConfiguration::JobType job_type_1 =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobConfiguration::JobType job_type_2 =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobForTesting job;

  base::RunLoop run_loop;

  SetupPolicyRecoveryExpectations(&job_type_1, &job_type_2, &job, &run_loop);

  is_3p_identity_synced()
      ? unit_test_profile_manager_->SetPolicyManagerForNextProfile(
            BuildUserCloudPolicyManager(
                /*is_store_initialized=*/true,
                CloudPolicyStore::Status::STATUS_LOAD_ERROR))
      : unit_test_profile_manager_->SetPolicyManagerForNextProfile(
            BuildProfileCloudPolicyManager(
                /*is_store_initialized=*/true,
                CloudPolicyStore::Status::STATUS_LOAD_ERROR));

  CreateProfileAndInitializeSigninService();

  run_loop.Run();

  VerifyPolicyRecoveryJobTypes(job_type_1, job_type_2);

  ConfirmHasPolicy();
}

TEST_P(UserPolicyOidcSigninServiceTest, PolicyRecoverySelfHealing) {
  // Only relevant for dasher-based profiles where we try to add primary
  // account.
  if (!is_3p_identity_synced()) {
    return;
  }

  DeviceManagementService::JobConfiguration::JobType job_type_1 =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobConfiguration::JobType job_type_2 =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobForTesting job;
  base::RunLoop run_loop;

  SetupBrokenProfileRecoveryExpectation(&job_type_1, &job_type_2, &job,
                                        &run_loop);

  unit_test_profile_manager_->SetPolicyManagerForNextProfile(
      BuildUserCloudPolicyManager(/*is_store_initialized=*/true,
                                  CloudPolicyStore::Status::STATUS_LOAD_ERROR));

  CreateProfileAndInitializeSigninService(/*user_email_missing=*/true);

  run_loop.Run();

  VerifyPolicyRecoveryJobTypes(job_type_1, job_type_2);

  ConfirmHasPolicy();

  auto* profile = unit_test_profile_manager_->GetProfileByPath(profile_path_);
  EXPECT_EQ(kExampleUserEmail,
            profile->GetPrefs()->GetString(
                enterprise_signin::prefs::kProfileUserEmail));
}

TEST_P(UserPolicyOidcSigninServiceTest, InitializedSuccessLoad) {
  EXPECT_CALL(job_creation_handler_, OnJobCreation).Times(0);
  is_3p_identity_synced()
      ? unit_test_profile_manager_->SetPolicyManagerForNextProfile(
            BuildUserCloudPolicyManager(
                /*is_store_initialized=*/true,
                CloudPolicyStore::Status::STATUS_OK))
      : unit_test_profile_manager_->SetPolicyManagerForNextProfile(
            BuildProfileCloudPolicyManager(
                /*is_store_initialized=*/true,
                CloudPolicyStore::Status::STATUS_OK));

  CreateProfileAndInitializeSigninService();
  base::RunLoop().RunUntilIdle();
}

TEST_P(UserPolicyOidcSigninServiceTest, UninitializedSuccessLoad) {
  EXPECT_CALL(job_creation_handler_, OnJobCreation).Times(0);
  is_3p_identity_synced()
      ? unit_test_profile_manager_->SetPolicyManagerForNextProfile(
            BuildUserCloudPolicyManager(
                /*is_store_initialized=*/false,
                CloudPolicyStore::Status::STATUS_LOAD_ERROR))
      : unit_test_profile_manager_->SetPolicyManagerForNextProfile(
            BuildProfileCloudPolicyManager(
                /*is_store_initialized=*/false,
                CloudPolicyStore::Status::STATUS_LOAD_ERROR));

  CreateProfileAndInitializeSigninService();

  if (mock_profile_cloud_policy_store_) {
    mock_profile_cloud_policy_store_->NotifyStoreLoaded();
  } else if (mock_user_cloud_policy_store_) {
    mock_user_cloud_policy_store_->NotifyStoreLoaded();
  }
  base::RunLoop().RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(All,
                         UserPolicyOidcSigninServiceTest,
                         testing::Combine(
                             /*is_3p_identity_synced=*/testing::Bool()));

class UserPolicySigninServicesInteractionTest
    : public UserPolicyOidcSigninServiceTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  UserPolicySigninServicesInteractionTest() {
    is_managed_ = std::get<0>(GetParam());
    has_policy_ = std::get<1>(GetParam());
  }

  ~UserPolicySigninServicesInteractionTest() override = default;
};

TEST_P(UserPolicySigninServicesInteractionTest, RecoverPolicyIfMissing) {
  DeviceManagementService::JobConfiguration::JobType job_type =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobForTesting job;

  base::RunLoop run_loop;

  if (!is_managed() || !has_policy()) {
    EXPECT_CALL(job_creation_handler_, OnJobCreation).Times(0);

    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(
            device_management_service_.CaptureJobType(&job_type),
            SaveArg<0>(&job), [this](auto&&...) { SetPolicyData(); },
            [&run_loop] { run_loop.Quit(); }));
  }

  unit_test_profile_manager_->SetPolicyManagerForNextProfile(
      BuildUserCloudPolicyManager(
          /*is_store_initialized=*/true, CloudPolicyStore::Status::STATUS_OK));

  CreateProfileAndInitializeSigninService();

  if (!is_managed() || !has_policy()) {
    run_loop.Run();

    EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
              job_type);
  }

  ConfirmHasPolicy();
}

INSTANTIATE_TEST_SUITE_P(All,
                         UserPolicySigninServicesInteractionTest,
                         testing::Combine(
                             /*is_managed=*/testing::Bool(),
                             /*has_policy=*/testing::Bool()));

}  // namespace policy
