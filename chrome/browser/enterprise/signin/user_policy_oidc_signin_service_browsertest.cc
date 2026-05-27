// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/user_policy_oidc_signin_service.h"

#include <variant>

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/remote_commands/user_remote_commands_service.h"
#include "chrome/browser/enterprise/remote_commands/user_remote_commands_service_factory.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor_factory.h"
#include "chrome/browser/enterprise/signin/user_policy_oidc_signin_service.h"
#include "chrome/browser/enterprise/signin/user_policy_oidc_signin_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_test_util.h"
#include "chrome/browser/policy/device_management_service_configuration.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/core/common/cloud/mock_profile_cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
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

}  // namespace

class FakeGaiaPolicySigninService : public policy::UserPolicySigninService {
 public:
  FakeGaiaPolicySigninService(
      Profile* profile,
      PrefService* local_state,
      DeviceManagementService* device_management_service,
      UserCloudPolicyManager* policy_manager,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory,
      bool can_apply_policies)
      : UserPolicySigninService(profile,
                                local_state,
                                device_management_service,
                                policy_manager,
                                identity_manager,
                                system_url_loader_factory),
        can_apply_policies_(can_apply_policies) {}

  ~FakeGaiaPolicySigninService() override = default;

  void ShutdownCloudPolicyManager() override {
    if (!can_apply_policies_) {
      UserPolicySigninService::ShutdownCloudPolicyManager();
    }
  }

  bool CanApplyPolicies(bool check_for_refresh_token) override { return false; }

 private:
  bool can_apply_policies_;
};

class UserPolicyOidcSigninServiceTestBase
    : public InProcessBrowserTest,
      public ProfileAttributesStorageObserver,
      public ProfileManagerObserver {
 public:
  UserPolicyOidcSigninServiceTestBase() = default;

  ~UserPolicyOidcSigninServiceTestBase() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    job_creation_handler_ =
        std::make_unique<testing::StrictMock<policy::MockJobCreationHandler>>();
    device_management_service_ =
        std::make_unique<policy::FakeDeviceManagementService>(
            job_creation_handler_.get());
    device_management_service_->ScheduleInitialization(0);
    // Inject device management service.
    g_browser_process->browser_policy_connector()
        ->SetDeviceManagementServiceForTesting(
            device_management_service_.get());

    ProfileAttributesStorage& storage =
        g_browser_process->profile_manager()->GetProfileAttributesStorage();
    profile_observation_.Observe(&storage);

    g_browser_process->profile_manager()->AddObserver(this);

    // Generate profile path.
    base::FilePath user_data_dir =
        g_browser_process->profile_manager()->user_data_dir();
    profile_path_ = user_data_dir.AppendASCII("OidcTestProfile");

    // Set policy data on the main profile to prevent it from creating jobs.
    Profile* main_profile = browser()->profile();
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_gaia_id(kExampleGaiaId.ToString());
    policy_data->set_command_invalidation_topic("fake-topic");
    policy_data->set_cec_enabled(true);
    if (main_profile->GetUserCloudPolicyManager()) {
      main_profile->GetUserCloudPolicyManager()
          ->core()
          ->store()
          ->set_policy_data_for_testing(std::move(policy_data));
    } else if (main_profile->GetProfileCloudPolicyManager()) {
      main_profile->GetProfileCloudPolicyManager()
          ->core()
          ->store()
          ->set_policy_data_for_testing(std::move(policy_data));
    }
  }

  void TearDownOnMainThread() override {
    g_browser_process->profile_manager()->RemoveObserver(this);
    profile_observation_.Reset();
    if (oidc_signin_service_) {
      oidc_signin_service_->Shutdown();
    }

    // Reset testing factory to prevent affecting other tests.
    ProfileImpl::SetCloudPolicyManagerFactoryForTesting({});

    oidc_signin_service_ = nullptr;
    mock_profile_cloud_policy_store_ = nullptr;
    mock_user_cloud_policy_store_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  // If the 3P identity is not synced to Google, the interceptor should
  // follow the Dasherless workflow.
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
      CloudPolicyStore::Status store_status,
      Profile* profile) {
    auto mock_user_cloud_policy_store =
        std::make_unique<MockUserCloudPolicyStore>(
            dm_protocol::GetChromeUserPolicyType());
    mock_user_cloud_policy_store_ = mock_user_cloud_policy_store.get();

    if (has_policy()) {
      SetPolicyData();
    }

    ConfigureMockStore(mock_user_cloud_policy_store_.get(),
                       is_store_initialized, store_status);

    ON_CALL(*mock_user_cloud_policy_store_, Clear()).WillByDefault([this]() {
      mock_user_cloud_policy_store_->set_policy_data_for_testing(nullptr);
      mock_user_cloud_policy_store_->NotifyStoreLoaded();
    });

    std::unique_ptr<MockUserCloudPolicyStore>
        mock_user_cloud_policy_extension_install_store;
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    mock_user_cloud_policy_extension_install_store =
        std::make_unique<MockUserCloudPolicyStore>(
            dm_protocol::kChromeExtensionInstallUserCloudPolicyType);
    EXPECT_CALL(*mock_user_cloud_policy_extension_install_store, Load())
        .Times(testing::AnyNumber());
#endif

    auto manager = std::make_unique<UserCloudPolicyManager>(
        std::move(mock_user_cloud_policy_store),
        std::move(mock_user_cloud_policy_extension_install_store),
        base::FilePath(),
        /*cloud_external_data_manager=*/nullptr,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        network::TestNetworkConnectionTracker::CreateGetter());
    manager->Init(&schema_registry_);
    return manager;
  }

  std::unique_ptr<ProfileCloudPolicyManager> BuildProfileCloudPolicyManager(
      bool is_store_initialized,
      CloudPolicyStore::Status store_status,
      Profile* profile) {
    auto mock_profile_cloud_policy_store =
        std::make_unique<MockProfileCloudPolicyStore>(
            dm_protocol::GetChromeUserPolicyType(),
            /*is_dasherless=*/true);
    mock_profile_cloud_policy_store_ = mock_profile_cloud_policy_store.get();

    SetPolicyData();

    ConfigureMockStore(mock_profile_cloud_policy_store_.get(),
                       is_store_initialized, store_status);

    std::unique_ptr<MockProfileCloudPolicyStore>
        mock_profile_cloud_policy_extension_install_store;
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    mock_profile_cloud_policy_extension_install_store =
        std::make_unique<MockProfileCloudPolicyStore>(
            dm_protocol::kChromeExtensionInstallUserCloudPolicyType,
            /*is_dasherless=*/true);
    EXPECT_CALL(*mock_profile_cloud_policy_extension_install_store, Load())
        .Times(testing::AnyNumber());
#endif

    auto manager = std::make_unique<ProfileCloudPolicyManager>(
        std::move(mock_profile_cloud_policy_store),
        std::move(mock_profile_cloud_policy_extension_install_store),
        base::FilePath(),
        /*cloud_external_data_manager=*/nullptr,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        network::TestNetworkConnectionTracker::CreateGetter(),
        /*is_dasherless=*/true);
    manager->Init(&schema_registry_);
    return manager;
  }

  void CreateProfileAndInitializeSigninService(
      bool user_email_missing = false,
      bool is_store_initialized = true,
      CloudPolicyStore::Status store_status =
          CloudPolicyStore::Status::STATUS_OK) {
    user_email_missing_ = user_email_missing;
    ON_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillByDefault(testing::Return(true));
    ON_CALL(policy_provider_, IsFirstPolicyLoadComplete(testing::_))
        .WillByDefault(testing::Return(true));
    policy::PushProfilePolicyConnectorProviderForTesting(&policy_provider_);

    if (is_3p_identity_synced()) {
      ProfileImpl::SetCloudPolicyManagerFactoryForTesting(base::BindRepeating(
          [](UserPolicyOidcSigninServiceTestBase* test,
             bool is_store_initialized, CloudPolicyStore::Status store_status,
             Profile* profile)
              -> std::variant<
                  std::unique_ptr<policy::UserCloudPolicyManager>,
                  std::unique_ptr<policy::ProfileCloudPolicyManager>> {
            return test->BuildUserCloudPolicyManager(is_store_initialized,
                                                     store_status, profile);
          },
          base::Unretained(this), is_store_initialized, store_status));
    } else {
      ProfileImpl::SetCloudPolicyManagerFactoryForTesting(base::BindRepeating(
          [](UserPolicyOidcSigninServiceTestBase* test,
             bool is_store_initialized, CloudPolicyStore::Status store_status,
             Profile* profile)
              -> std::variant<
                  std::unique_ptr<policy::UserCloudPolicyManager>,
                  std::unique_ptr<policy::ProfileCloudPolicyManager>> {
            return test->BuildProfileCloudPolicyManager(is_store_initialized,
                                                        store_status, profile);
          },
          base::Unretained(this), is_store_initialized, store_status));
    }

    Profile& profile = profiles::testing::CreateProfileSync(
        g_browser_process->profile_manager(), profile_path_);
    Profile* profile_ptr = &profile;

    std::variant<UserCloudPolicyManager*, ProfileCloudPolicyManager*>
        policy_manager;

    if (is_3p_identity_synced()) {
      policy_manager = profile_ptr->GetUserCloudPolicyManager();
    } else {
      policy_manager = profile_ptr->GetProfileCloudPolicyManager();
    }

    if (is_store_initialized && has_policy()) {
      SetPolicyData();
      if (mock_profile_cloud_policy_store_) {
        mock_profile_cloud_policy_store_->NotifyStoreLoaded();
      } else {
        mock_user_cloud_policy_store_->NotifyStoreLoaded();
      }
    }

    oidc_signin_service_ =
        UserPolicyOidcSigninServiceFactory::GetForProfile(profile_ptr);
    CHECK(oidc_signin_service_);
  }

  void ConfirmHasPolicy() {
    if (mock_profile_cloud_policy_store_) {
      CHECK(mock_profile_cloud_policy_store_->has_policy());
    } else if (mock_user_cloud_policy_store_) {
      CHECK(mock_user_cloud_policy_store_->has_policy());
    }
  }

  using ProfileAttributesStorageObserver::OnProfileAdded;
  using ProfileManagerObserver::OnProfileAdded;

  // ProfileAttributesStorage::Observer::
  void OnProfileAdded(const base::FilePath& profile_path) override {
    auto* entry = g_browser_process->profile_manager()
                      ->GetProfileAttributesStorage()
                      .GetProfileAttributesWithPath(profile_path);

    entry->SetProfileManagementOidcTokens(kExampleOidcTokens);
    entry->SetDasherlessManagement(!is_3p_identity_synced());
  }

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override {
    if (profile->GetPath() == profile_path_) {
      profile->GetPrefs()->SetString(
          enterprise_signin::prefs::kPolicyRecoveryToken, kExampleDmToken);
      profile->GetPrefs()->SetString(
          enterprise_signin::prefs::kPolicyRecoveryClientId, kExampleClientId);
      if (!user_email_missing_) {
        profile->GetPrefs()->SetString(
            enterprise_signin::prefs::kProfileUserEmail, kExampleUserEmail);
      }
    }
  }

  void OnPolicyFetchCompleteInNewProfile(
      base::OnceClosure quit_closure = base::OnceClosure()) {
    CHECK(oidc_signin_service_);
    oidc_signin_service_->OnPolicyFetchCompleteInNewProfile(
        "123", base::TimeTicks(), false,
        quit_closure ? base::IgnoreArgs<bool>(std::move(quit_closure))
                     : base::BindOnce([](bool) {}),
        true);
  }

  void CompleteJobAsynchronously(DeviceManagementService::JobForTesting job) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](FakeDeviceManagementService* service,
               DeviceManagementService::JobForTesting job) {
              service->SendJobOKNow(&job, "");
            },
            base::Unretained(device_management_service_.get()), job));
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

  void OnProfileCreationStarted(Profile* profile) override {
    // Only intercept OIDC profiles created during the test bodies.
    if (profile->GetPath() == profile_path_) {
      if (is_3p_identity_synced()) {
        UserPolicySigninServiceFactory::GetInstance()->SetTestingFactory(
            profile, base::BindRepeating(&UserPolicyOidcSigninServiceTestBase::
                                             BuildFakeUserPolicySigninService,
                                         base::Unretained(this), is_managed()));
      }

      UserPolicyOidcSigninServiceFactory::GetInstance()->SetTestingFactory(
          profile, base::BindRepeating(&UserPolicyOidcSigninServiceTestBase::
                                           BuildFakeUserPolicyOidcSigninService,
                                       base::Unretained(this)));
    }
  }

  std::unique_ptr<KeyedService> BuildFakeUserPolicySigninService(
      bool is_managed,
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    if (set_primary_account_for_gaia_) {
      signin::SetPrimaryAccount(IdentityManagerFactory::GetForProfile(profile),
                                kExampleUserEmail,
                                signin::ConsentLevel::kSignin);
      auto service = std::make_unique<UserPolicySigninService>(
          profile, g_browser_process->local_state(),
          device_management_service_.get(),
          profile->GetUserCloudPolicyManager(),
          IdentityManagerFactory::GetForProfile(profile),
          g_browser_process->shared_url_loader_factory());
      service->set_profile_can_be_managed_for_testing(is_managed);
      return service;
    } else {
      return std::make_unique<FakeGaiaPolicySigninService>(
          profile, g_browser_process->local_state(),
          device_management_service_.get(),
          profile->GetUserCloudPolicyManager(),
          IdentityManagerFactory::GetForProfile(profile),
          g_browser_process->shared_url_loader_factory(), is_managed);
    }
  }

  std::unique_ptr<KeyedService> BuildFakeUserPolicyOidcSigninService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);

    std::variant<UserCloudPolicyManager*, ProfileCloudPolicyManager*> manager;
    if (!is_3p_identity_synced()) {
      manager = profile->GetProfileCloudPolicyManager();
    } else {
      manager = profile->GetUserCloudPolicyManager();
    }

    return std::make_unique<UserPolicyOidcSigninService>(
        profile, g_browser_process->local_state(),
        device_management_service_.get(),  // Inject faked DM service directly!
        manager, IdentityManagerFactory::GetForProfile(profile),
        test_url_loader_factory_.GetSafeWeakWrapper());
  }

  base::WeakPtr<UserPolicyOidcSigninServiceTestBase> GetWeakPtr() {
    return weakptr_factory_.GetWeakPtr();
  }

 protected:
  bool is_3p_identity_synced_ = true;
  bool has_policy_ = true;
  bool is_managed_ = true;
  bool user_email_missing_ = false;
  bool set_primary_account_for_gaia_ = false;
  base::test::ScopedFeatureList scoped_feature_list_;
  policy::MockConfigurationPolicyProvider policy_provider_;
  raw_ptr<MockProfileCloudPolicyStore> mock_profile_cloud_policy_store_;
  raw_ptr<MockUserCloudPolicyStore> mock_user_cloud_policy_store_;
  raw_ptr<policy::UserPolicyOidcSigninService> oidc_signin_service_;
  policy::SchemaRegistry schema_registry_;
  base::FilePath profile_path_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<testing::StrictMock<policy::MockJobCreationHandler>>
      job_creation_handler_;
  std::unique_ptr<policy::FakeDeviceManagementService>
      device_management_service_;

  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_observation_{this};
  base::WeakPtrFactory<UserPolicyOidcSigninServiceTestBase> weakptr_factory_{
      this};
};

class UserPolicyOidcSigninServiceTest
    : public UserPolicyOidcSigninServiceTestBase,
      public testing::WithParamInterface<bool> {
 public:
  UserPolicyOidcSigninServiceTest() { is_3p_identity_synced_ = GetParam(); }

  ~UserPolicyOidcSigninServiceTest() override = default;

  void SetupPolicyRecoveryExpectations(
      DeviceManagementService::JobConfiguration::JobType* job_type_1,
      DeviceManagementService::JobConfiguration::JobType* job_type_2,
      DeviceManagementService::JobForTesting* job,
      base::RunLoop* run_loop) {
    if (is_3p_identity_synced()) {
      EXPECT_CALL(*job_creation_handler_, OnJobCreation)
          .WillOnce(DoAll(
              device_management_service_->CaptureJobType(job_type_1),
              [this](auto&&...) {
                SetPolicyData();
                mock_user_cloud_policy_store_->NotifyStoreLoaded();
              },
              [this](auto&&...) {
                base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                    FROM_HERE,
                    base::BindOnce(&UserPolicyOidcSigninServiceTestBase::
                                       OnPolicyFetchCompleteInNewProfile,
                                   GetWeakPtr(), base::OnceClosure()));
              },
              [this](DeviceManagementService::JobForTesting job) {
                CompleteJobAsynchronously(job);
              },
              SaveArg<0>(job)))
          .WillOnce(DoAll(
              device_management_service_->CaptureJobType(job_type_2),
              [this](DeviceManagementService::JobForTesting job) {
                CompleteJobAsynchronously(job);
              },
              SaveArg<0>(job), [run_loop] { run_loop->Quit(); }));
    } else {
      EXPECT_CALL(*job_creation_handler_, OnJobCreation)
          .WillOnce(DoAll(
              device_management_service_->CaptureJobType(job_type_1),
              [this](DeviceManagementService::JobForTesting job) {
                CompleteJobAsynchronously(job);
              },
              SaveArg<0>(job)))
          .WillOnce(DoAll(
              device_management_service_->CaptureJobType(job_type_2),
              [this](auto&&...) {
                SetPolicyData();
                mock_profile_cloud_policy_store_->NotifyStoreLoaded();
              },
              [this, run_loop](auto&&...) {
                base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                    FROM_HERE,
                    base::BindOnce(&UserPolicyOidcSigninServiceTestBase::
                                       OnPolicyFetchCompleteInNewProfile,
                                   GetWeakPtr(), run_loop->QuitClosure()));
              },
              [this](DeviceManagementService::JobForTesting job) {
                CompleteJobAsynchronously(job);
              },
              SaveArg<0>(job)));
    }
  }

  void SetupBrokenProfileRecoveryExpectation(
      DeviceManagementService::JobConfiguration::JobType* job_type_1,
      DeviceManagementService::JobConfiguration::JobType* job_type_2,
      DeviceManagementService::JobForTesting* job,
      base::RunLoop* run_loop) {
    EXPECT_CALL(*job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(
            device_management_service_->CaptureJobType(job_type_1),
            [this](DeviceManagementService::JobForTesting job) {
              SetPolicyData();
              mock_user_cloud_policy_store_->NotifyStoreLoaded();

              // Set primary account available just before policy fetch
              // completion to simulate the case where primary account
              // already exists.
              auto* profile =
                  g_browser_process->profile_manager()->GetProfileByPath(
                      profile_path_);
              signin::SetPrimaryAccount(
                  IdentityManagerFactory::GetForProfile(profile),
                  kExampleUserEmail, signin::ConsentLevel::kSignin);

              // Pass empty email to simulate broken profile state
              // asynchronously.
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(&UserPolicyOidcSigninServiceTestBase::
                                     CallOnPolicyFetchCompleteInNewProfile,
                                 GetWeakPtr(), std::string(), base::TimeTicks(),
                                 false, base::BindOnce([](bool) {}), true));

              CompleteJobAsynchronously(job);
            },
            SaveArg<0>(job)))
        .WillOnce(DoAll(
            device_management_service_->CaptureJobType(job_type_2),
            [this](DeviceManagementService::JobForTesting job) {
              CompleteJobAsynchronously(job);
            },
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

IN_PROC_BROWSER_TEST_P(UserPolicyOidcSigninServiceTest,
                       UninitializedStorePolicyRecovery) {
  DeviceManagementService::JobConfiguration::JobType job_type_1 =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobConfiguration::JobType job_type_2 =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobForTesting job;

  base::RunLoop run_loop;

  SetupPolicyRecoveryExpectations(&job_type_1, &job_type_2, &job, &run_loop);

  CreateProfileAndInitializeSigninService(
      /*user_email_missing=*/false,
      /*is_store_initialized=*/false, CloudPolicyStore::Status::STATUS_OK);

  if (mock_profile_cloud_policy_store_) {
    mock_profile_cloud_policy_store_->NotifyStoreError();
  } else if (mock_user_cloud_policy_store_) {
    mock_user_cloud_policy_store_->NotifyStoreError();
  }

  run_loop.Run();

  VerifyPolicyRecoveryJobTypes(job_type_1, job_type_2);

  ConfirmHasPolicy();
}

IN_PROC_BROWSER_TEST_P(UserPolicyOidcSigninServiceTest,
                       InitializedStorePolicyRecovery) {
  DeviceManagementService::JobConfiguration::JobType job_type_1 =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobConfiguration::JobType job_type_2 =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobForTesting job;

  base::RunLoop run_loop;

  SetupPolicyRecoveryExpectations(&job_type_1, &job_type_2, &job, &run_loop);

  CreateProfileAndInitializeSigninService(
      /*user_email_missing=*/false,
      /*is_store_initialized=*/true,
      CloudPolicyStore::Status::STATUS_LOAD_ERROR);

  run_loop.Run();

  VerifyPolicyRecoveryJobTypes(job_type_1, job_type_2);

  ConfirmHasPolicy();
}

IN_PROC_BROWSER_TEST_P(UserPolicyOidcSigninServiceTest,
                       PolicyRecoverySelfHealing) {
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

  CreateProfileAndInitializeSigninService(
      /*user_email_missing=*/true,
      /*is_store_initialized=*/true,
      CloudPolicyStore::Status::STATUS_LOAD_ERROR);

  run_loop.Run();

  VerifyPolicyRecoveryJobTypes(job_type_1, job_type_2);

  ConfirmHasPolicy();

  auto* profile =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path_);
  EXPECT_EQ(kExampleUserEmail,
            profile->GetPrefs()->GetString(
                enterprise_signin::prefs::kProfileUserEmail));
}

IN_PROC_BROWSER_TEST_P(UserPolicyOidcSigninServiceTest,
                       InitializedSuccessLoad) {
  // The main profile is silenced via SetPolicyData in SetUpOnMainThread and
  // never creates jobs. Since the store is initialized, OIDC service skips
  // recovery. So we expect exactly 0 jobs.
  EXPECT_CALL(*job_creation_handler_, OnJobCreation).Times(0);
  CreateProfileAndInitializeSigninService(
      /*user_email_missing=*/false,
      /*is_store_initialized=*/true, CloudPolicyStore::Status::STATUS_OK);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_P(UserPolicyOidcSigninServiceTest,
                       UninitializedSuccessLoad) {
  // Store remains uninitialized, so OIDC service skips recovery.
  // We expect exactly 0 jobs.
  EXPECT_CALL(*job_creation_handler_, OnJobCreation).Times(0);
  CreateProfileAndInitializeSigninService(
      /*user_email_missing=*/false,
      /*is_store_initialized=*/false,
      CloudPolicyStore::Status::STATUS_LOAD_ERROR);

  if (mock_profile_cloud_policy_store_) {
    mock_profile_cloud_policy_store_->NotifyStoreLoaded();
  } else if (mock_user_cloud_policy_store_) {
    mock_user_cloud_policy_store_->NotifyStoreLoaded();
  }
  base::RunLoop().RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(All, UserPolicyOidcSigninServiceTest, testing::Bool());

class UserPolicySigninServicesInteractionTest
    : public UserPolicyOidcSigninServiceTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  UserPolicySigninServicesInteractionTest() {
    is_managed_ = std::get<0>(GetParam());
    has_policy_ = std::get<1>(GetParam());
    set_primary_account_for_gaia_ = is_managed_;
  }

  ~UserPolicySigninServicesInteractionTest() override = default;
};

IN_PROC_BROWSER_TEST_P(UserPolicySigninServicesInteractionTest,
                       RecoverPolicyIfMissing) {
  base::RunLoop run_loop;
  DeviceManagementService::JobConfiguration::JobType job_type =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;

  // We expect a recovery job only if the profile is unmanaged OR policies
  // are missing.
  if (!is_managed() || !has_policy()) {
    EXPECT_CALL(*job_creation_handler_, OnJobCreation)
        .Times(1)
        .WillOnce(DoAll(
            device_management_service_->CaptureJobType(&job_type),
            [this](auto&&...) {
              SetPolicyData();
              if (mock_profile_cloud_policy_store_) {
                mock_profile_cloud_policy_store_->NotifyStoreLoaded();
              } else {
                mock_user_cloud_policy_store_->NotifyStoreLoaded();
              }
            },
            [this](DeviceManagementService::JobForTesting job) {
              device_management_service_->SendJobOKNow(&job, "");
            },
            [&run_loop] { run_loop.Quit(); }));
  } else {
    EXPECT_CALL(*job_creation_handler_, OnJobCreation).Times(0);
  }

  CreateProfileAndInitializeSigninService(
      /*user_email_missing=*/false,
      /*is_store_initialized=*/true, CloudPolicyStore::Status::STATUS_OK);

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
