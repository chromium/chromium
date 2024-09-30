// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_command_line.h"
#include "base/time/time.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_client_factory_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/ash/policy/enrollment/device_cloud_policy_initializer.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_handler.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "chrome/browser/ash/policy/remote_commands/crd/fake_start_crd_session_job_delegate.h"
#include "chrome/browser/ash/policy/uploading/heartbeat_scheduler.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/attestation/fake_certificate.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "chromeos/ash/components/attestation/stub_attestation_features.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/device_management/fake_install_attributes_client.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/core/common/cloud/mock_signing_service.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/fake_user_manager.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::WithArgs;
namespace em = ::enterprise_management;

MATCHER_P(HasJobType, job_type, "matches job type") {
  return arg.GetConfigurationForTesting()->GetType() == job_type;
}

void CopyLockResult(base::RunLoop* loop,
                    ash::InstallAttributes::LockResult* out,
                    ash::InstallAttributes::LockResult result) {
  *out = result;
  loop->Quit();
}

void CertCallbackSuccess(
    ash::attestation::AttestationFlow::CertificateCallback callback,
    std::string certificate) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ash::attestation::ATTESTATION_SUCCESS,
                     std::move(certificate)));
}

void CertCallbackSuccessWithValidCertificate(
    ash::attestation::AttestationFlow::CertificateCallback callback) {
  std::string certificate;
  ash::attestation::GetFakeCertificatePEM(base::Days(10), &certificate);
  CertCallbackSuccess(std::move(callback), std::move(certificate));
}

class FakeSigningServiceProvider final
    : public EnrollmentHandler::SigningServiceProvider {
 public:
  explicit FakeSigningServiceProvider(bool success) : success_(success) {}

  std::unique_ptr<SigningService> CreateSigningService() const override {
    auto service = std::make_unique<FakeSigningService>();
    service->set_success(success_);
    return service;
  }

 private:
  bool success_;
};

class TestingDeviceCloudPolicyManagerAsh : public DeviceCloudPolicyManagerAsh {
 public:
  TestingDeviceCloudPolicyManagerAsh(
      std::unique_ptr<DeviceCloudPolicyStoreAsh> store,
      std::unique_ptr<CloudExternalDataManager> external_data_manager,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      ServerBackedStateKeysBroker* state_keys_broker)
      : DeviceCloudPolicyManagerAsh(std::move(store),
                                    std::move(external_data_manager),
                                    task_runner,
                                    state_keys_broker,
                                    crd_delegate_) {
    set_component_policy_disabled_for_testing(true);
  }

  ~TestingDeviceCloudPolicyManagerAsh() override = default;

  ManagedSessionService* GetManagedSessionService() {
    return managed_session_service_.get();
  }

  ash::reporting::LoginLogoutReporter* GetLoginLogoutReporter() {
    return login_logout_reporter_.get();
  }

  reporting::UserAddedRemovedReporter* GetUserAddedRemovedReporter() {
    return user_added_removed_reporter_.get();
  }

 private:
  FakeStartCrdSessionJobDelegate crd_delegate_;
};

class DeviceCloudPolicyManagerAshTest
    : public ash::DeviceSettingsTestBase,
      public ash::SessionManagerClient::Observer {
 public:
  DeviceCloudPolicyManagerAshTest(const DeviceCloudPolicyManagerAshTest&) =
      delete;
  DeviceCloudPolicyManagerAshTest& operator=(
      const DeviceCloudPolicyManagerAshTest&) = delete;

 protected:
  DeviceCloudPolicyManagerAshTest()
      : state_keys_broker_(&session_manager_client_), store_(nullptr) {
    fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                  "test_sn");
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kHardwareClassKey, "test_hw");
    session_manager_client_.AddObserver(this);
  }

  ~DeviceCloudPolicyManagerAshTest() override {
    session_manager_client_.RemoveObserver(this);
    ash::system::StatisticsProvider::SetTestProvider(nullptr);
  }

  virtual bool ShouldRegisterWithCert() const { return false; }

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();

    device_management_service_.ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();

    reporting_test_enviroment_ =
        reporting::ReportingClient::TestEnvironment::CreateWithStorageModule();

    if (set_empty_system_salt_) {
      ash::FakeCryptohomeMiscClient::Get()->set_system_salt(
          std::vector<uint8_t>());
    }

    ash::InstallAttributesClient::InitializeFake();
    install_attributes_ = std::make_unique<ash::InstallAttributes>(
        ash::FakeInstallAttributesClient::Get());
    store_ = new DeviceCloudPolicyStoreAsh(
        device_settings_service_.get(), install_attributes_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    auto external_data_manager =
        std::make_unique<MockCloudExternalDataManager>();
    external_data_manager_ = external_data_manager.get();
    manager_ = std::make_unique<TestingDeviceCloudPolicyManagerAsh>(
        base::WrapUnique(store_.get()), std::move(external_data_manager),
        base::SingleThreadTaskRunner::GetCurrentDefault(), &state_keys_broker_);

    RegisterLocalState(local_state_.registry());
    manager_->Init(&schema_registry_);
    manager_->SetSigninProfileSchemaRegistry(&schema_registry_);

    user_manager_ =
        std::make_unique<user_manager::FakeUserManager>(&local_state_);
    manager_->OnUserManagerCreated(user_manager_.get());

    // SharedURLLoaderFactory and LocalState singletons have to be set since
    // they are accessed by EnrollmentHandler and StartupUtils.
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);

    // SystemSaltGetter is used in DeviceOAuth2TokenService.
    ash::SystemSaltGetter::Initialize();
    DeviceOAuth2TokenServiceFactory::Initialize(
        test_url_loader_factory_.GetSafeWeakWrapper(), &local_state_);

    url_fetcher_response_code_ = net::HTTP_OK;
    url_fetcher_response_string_ =
        "{\"access_token\":\"accessToken4Test\","
        "\"expires_in\":1234,"
        "\"refresh_token\":\"refreshToken4Test\"}";

    // Set the verification key to be used for testing by the
    // CloudPolicyValidator.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(
        policy::switches::kPolicyVerificationKey,
        policy::PolicyBuilder::GetEncodedPolicyVerificationKey());

    AllowUninterestingRemoteCommandFetches();
  }

  void TearDown() override {
    if (initializer_) {
      initializer_->Shutdown();
    }
    ShutdownManager();

    manager_->OnUserManagerWillBeDestroyed();
    user_manager_.reset();

    manager_.reset();
    install_attributes_.reset();

    DeviceOAuth2TokenServiceFactory::Shutdown();
    ash::SystemSaltGetter::Shutdown();
    ash::InstallAttributesClient::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);

    reporting_test_enviroment_.reset();

    DeviceSettingsTestBase::TearDown();
  }

  void LockDevice() {
    base::RunLoop loop;
    ash::InstallAttributes::LockResult result;
    install_attributes_->LockDevice(
        DEVICE_MODE_ENTERPRISE, PolicyBuilder::kFakeDomain,
        std::string(),  // realm
        PolicyBuilder::kFakeDeviceId,
        base::BindOnce(&CopyLockResult, &loop, &result));
    loop.Run();
    ASSERT_EQ(ash::InstallAttributes::LOCK_SUCCESS, result);
  }

  void AddStateKeys() {
    std::vector<std::string> state_keys;
    state_keys.push_back("1");
    state_keys.push_back("2");
    state_keys.push_back("3");
    session_manager_client_.set_server_backed_state_keys(state_keys);
  }

  void ConnectManager(bool expectExternalDataManagerConnectCall = true) {
    if (expectExternalDataManagerConnectCall) {
      EXPECT_CALL(*external_data_manager_, Connect(_));
    }
    AddStateKeys();
    InitDeviceCloudPolicyInitializer();
  }

  void InitDeviceCloudPolicyInitializer() {
    manager_->Initialize(&local_state_);
    EnrollmentRequisitionManager::Initialize();
    initializer_ = std::make_unique<DeviceCloudPolicyInitializer>(
        &device_management_service_, install_attributes_.get(),
        &state_keys_broker_, store_, manager_.get(),
        &fake_statistics_provider_);
    initializer_->SetSystemURLLoaderFactoryForTesting(
        test_url_loader_factory_.GetSafeWeakWrapper());
    initializer_->Init();
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(external_data_manager_);
  }

  void ShutdownManager() {
    EXPECT_CALL(*external_data_manager_, Disconnect());
    manager_->Shutdown();
    Mock::VerifyAndClearExpectations(external_data_manager_);
  }

  void VerifyPolicyPopulated() {
    const auto* actual_policy =
        manager_->policies()
            .Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
            .Get(key::kDeviceMetricsReportingEnabled);
    EXPECT_TRUE(!!actual_policy);
    PolicyMap::Entry expected_policy(POLICY_LEVEL_MANDATORY,
                                     POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                                     base::Value(false), nullptr);
    EXPECT_TRUE(actual_policy->Equals(expected_policy));
  }

  // Should be called after EXPECT_CALL(..., StartJob(_)) so "any" case does
  // not override this one.
  void AllowUninterestingRemoteCommandFetches() {
    // We are not interested in remote command fetches that the client initiates
    // automatically. Make them fail and ignore them otherwise.
    EXPECT_CALL(
        job_creation_handler_,
        OnJobCreation(HasJobType(
            DeviceManagementService::JobConfiguration::TYPE_REMOTE_COMMANDS)))
        .Times(AnyNumber())
        .WillRepeatedly(device_management_service_.SendJobResponseAsync(
            net::OK, DeviceManagementService::kServiceUnavailable,
            em::DeviceManagementResponse()));
  }

  // SessionManagerClient::Observer:
  void OwnerKeySet(bool success) override {
    // Called when the owner key is set in SessionManagerClient. Make it
    // immediately available to |owner_key_util_| since code that "loads" the
    // key will load it through that instance.
    EXPECT_TRUE(success);
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_->GetNewSigningKey());
  }

  std::unique_ptr<reporting::ReportingClient::TestEnvironment>
      reporting_test_enviroment_;

  std::unique_ptr<ash::InstallAttributes> install_attributes_;

  net::HttpStatusCode url_fetcher_response_code_;
  std::string url_fetcher_response_string_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<user_manager::FakeUserManager> user_manager_;
  StrictMock<MockJobCreationHandler> job_creation_handler_;
  FakeDeviceManagementService device_management_service_{
      &job_creation_handler_};
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  bool set_empty_system_salt_ = false;
  ServerBackedStateKeysBroker state_keys_broker_;
  StrictMock<ash::attestation::MockAttestationFlow> mock_attestation_flow_;

  raw_ptr<DeviceCloudPolicyStoreAsh, DanglingUntriaged> store_;
  SchemaRegistry schema_registry_;
  ash::attestation::ScopedStubAttestationFeatures attestation_features_;
  raw_ptr<MockCloudExternalDataManager, DanglingUntriaged>
      external_data_manager_;
  std::unique_ptr<TestingDeviceCloudPolicyManagerAsh> manager_;
  std::unique_ptr<DeviceCloudPolicyInitializer> initializer_;
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  // This property is required to instantiate the session manager, a singleton
  // which is used by the device status collector.
  session_manager::SessionManager session_manager_;
};

TEST_F(DeviceCloudPolicyManagerAshTest, FreshDevice) {
  owner_key_util_->Clear();
  // Normally this happens at signin screen profile creation. But
  // TestingProfile doesn't do that.
  device_settings_service_->LoadImmediately();
  FlushDeviceSettings();
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  manager_->Initialize(&local_state_);

  PolicyBundle bundle;
  EXPECT_TRUE(manager_->policies().Equals(bundle));
}

TEST_F(DeviceCloudPolicyManagerAshTest, EnrolledDevice) {
  LockDevice();
  // Normally this happens at signin screen profile creation. But
  // TestingProfile doesn't do that.
  device_settings_service_->LoadImmediately();
  FlushDeviceSettings();
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  VerifyPolicyPopulated();

  // Trigger a policy refresh - this triggers a policy update.
  DeviceManagementService::JobForTesting policy_job;
  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(device_management_service_.CaptureJobType(&job_type),
                      SaveArg<0>(&policy_job)));
  AllowUninterestingRemoteCommandFetches();
  ConnectManager();
  Mock::VerifyAndClearExpectations(&device_management_service_);
  ASSERT_TRUE(policy_job.IsActive());
  ASSERT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type);
  // Should create a status uploader for reporting on enrolled devices.
  EXPECT_TRUE(manager_->GetStatusUploader());
  EXPECT_TRUE(manager_->GetManagedSessionService());
  EXPECT_TRUE(manager_->GetLoginLogoutReporter());
  EXPECT_TRUE(manager_->GetUserAddedRemovedReporter());
  VerifyPolicyPopulated();

  ShutdownManager();
  VerifyPolicyPopulated();

  EXPECT_EQ(store_->policy()->service_account_identity(),
            PolicyBuilder::kFakeServiceAccountIdentity);
}

TEST_F(DeviceCloudPolicyManagerAshTest, EnrolledDevicePolicyFetchSHA256) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(policy::kPolicyFetchWithSha256);
  device_policy_->SetSignatureType(em::PolicyFetchRequest::SHA256_RSA);
  device_policy_->Build();
  session_manager_client_.set_device_policy(device_policy_->GetBlob());
  LockDevice();
  // Normally this happens at signin screen profile creation. But
  // TestingProfile doesn't do that.
  device_settings_service_->LoadImmediately();
  FlushDeviceSettings();
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  VerifyPolicyPopulated();

  // Trigger a policy refresh - this triggers a policy update.
  DeviceManagementService::JobForTesting policy_job;
  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(device_management_service_.CaptureJobType(&job_type),
                      SaveArg<0>(&policy_job)));
  AllowUninterestingRemoteCommandFetches();
  ConnectManager();
  Mock::VerifyAndClearExpectations(&device_management_service_);
  ASSERT_TRUE(policy_job.IsActive());
  ASSERT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type);
  VerifyPolicyPopulated();
}

TEST_F(DeviceCloudPolicyManagerAshTest, UnmanagedDevice) {
  device_policy_->policy_data().set_state(em::PolicyData::UNMANAGED);
  device_policy_->Build();
  session_manager_client_.set_device_policy(device_policy_->GetBlob());

  LockDevice();
  // Normally this happens at signin screen profile creation. But
  // TestingProfile doesn't do that.
  device_settings_service_->LoadImmediately();
  FlushDeviceSettings();
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(store_->is_managed());

  // Policy settings should be ignored for UNMANAGED devices.
  PolicyBundle bundle;
  EXPECT_TRUE(manager_->policies().Equals(bundle));

  // Trigger a policy refresh.
  DeviceManagementService::JobForTesting policy_job;
  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(device_management_service_.CaptureJobType(&job_type),
                      SaveArg<0>(&policy_job)));
  AllowUninterestingRemoteCommandFetches();
  ConnectManager();
  Mock::VerifyAndClearExpectations(&device_management_service_);
  ASSERT_TRUE(policy_job.IsActive());
  ASSERT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type);
  // Should create a status provider for reporting on enrolled devices, even
  // those that aren't managed.
  EXPECT_TRUE(manager_->GetStatusUploader());
  EXPECT_TRUE(manager_->GetManagedSessionService());
  EXPECT_TRUE(manager_->GetLoginLogoutReporter());
  EXPECT_TRUE(manager_->GetUserAddedRemovedReporter());

  // Switch back to ACTIVE, service the policy fetch and let it propagate.
  device_policy_->policy_data().set_state(em::PolicyData::ACTIVE);
  device_policy_->Build();
  session_manager_client_.set_device_policy(device_policy_->GetBlob());
  em::DeviceManagementResponse policy_fetch_response;
  policy_fetch_response.mutable_policy_response()->add_responses()->CopyFrom(
      device_policy_->policy());
  device_management_service_.SendJobResponseNow(
      &policy_job, net::OK, DeviceManagementService::kSuccess,
      policy_fetch_response);
  EXPECT_FALSE(policy_job.IsActive());
  device_settings_service_->LoadImmediately();
  FlushDeviceSettings();

  // Policy state should now be active and the policy map should be populated.
  EXPECT_TRUE(store_->is_managed());
  VerifyPolicyPopulated();
}

TEST_F(DeviceCloudPolicyManagerAshTest, ConsumerDevice) {
  device_settings_service_->LoadImmediately();
  FlushDeviceSettings();
  EXPECT_EQ(CloudPolicyStore::STATUS_BAD_STATE, store_->status());
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  PolicyBundle bundle;
  EXPECT_TRUE(manager_->policies().Equals(bundle));

  ConnectManager(false);
  EXPECT_TRUE(manager_->policies().Equals(bundle));
  // Should not create a status provider for reporting on consumer devices.
  EXPECT_FALSE(manager_->GetStatusUploader());
  EXPECT_FALSE(manager_->GetManagedSessionService());
  EXPECT_FALSE(manager_->GetLoginLogoutReporter());
  EXPECT_FALSE(manager_->GetUserAddedRemovedReporter());

  ShutdownManager();
  EXPECT_TRUE(manager_->policies().Equals(bundle));
}

TEST_F(DeviceCloudPolicyManagerAshTest, EnrolledDeviceNoStateKeysGenerated) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment,
      AutoEnrollmentTypeChecker::kForcedReEnrollmentAlways);

  LockDevice();
  device_settings_service_->LoadImmediately();
  FlushDeviceSettings();
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  VerifyPolicyPopulated();

  EXPECT_CALL(job_creation_handler_, OnJobCreation).Times(0);
  AllowUninterestingRemoteCommandFetches();

  EXPECT_FALSE(manager_->GetManagedSessionService());
  EXPECT_FALSE(manager_->GetLoginLogoutReporter());
  EXPECT_FALSE(manager_->GetUserAddedRemovedReporter());

  InitDeviceCloudPolicyInitializer();

  // Status uploader for reporting on enrolled devices is only created on
  // connect call.
  EXPECT_FALSE(manager_->GetStatusUploader());
  // Managed session service and reporters are created when notified by
  // |DeviceCloudPolicyInitializer| that the policy store is ready.
  EXPECT_TRUE(manager_->GetManagedSessionService());
  EXPECT_TRUE(manager_->GetLoginLogoutReporter());
  EXPECT_TRUE(manager_->GetUserAddedRemovedReporter());

  ShutdownManager();

  EXPECT_EQ(store_->policy()->service_account_identity(),
            PolicyBuilder::kFakeServiceAccountIdentity);
}

class DeviceCloudPolicyManagerAshObserverTest
    : public DeviceCloudPolicyManagerAshTest,
      public DeviceCloudPolicyManagerAsh::Observer {
 protected:
  DeviceCloudPolicyManagerAshObserverTest() {}

  void SetUp() override {
    DeviceCloudPolicyManagerAshTest::SetUp();
    manager_->AddDeviceCloudPolicyManagerObserver(this);
  }

  void TearDown() override {
    manager_->RemoveDeviceCloudPolicyManagerObserver(this);
    DeviceCloudPolicyManagerAshTest::TearDown();
  }

  // DeviceCloudPolicyManagerAsh::Observer:
  MOCK_METHOD0(OnDeviceCloudPolicyManagerConnected, void());
  MOCK_METHOD0(OnDeviceCloudPolicyManagerGotRegistry, void());
};

TEST_F(DeviceCloudPolicyManagerAshObserverTest, Connect) {
  LockDevice();
  device_settings_service_->LoadImmediately();
  FlushDeviceSettings();
  EXPECT_FALSE(manager_->IsConnected());

  // Connect the manager.
  DeviceManagementService::JobForTesting policy_job;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(SaveArg<0>(&policy_job));
  AllowUninterestingRemoteCommandFetches();
  EXPECT_CALL(*this, OnDeviceCloudPolicyManagerConnected());
  ConnectManager();
  Mock::VerifyAndClearExpectations(&device_management_service_);
  Mock::VerifyAndClearExpectations(this);
  EXPECT_TRUE(manager_->IsConnected());
}

TEST_F(DeviceCloudPolicyManagerAshObserverTest, GetSchemaRegistry) {
  EXPECT_CALL(*this, OnDeviceCloudPolicyManagerGotRegistry()).Times(1);

  manager_->Shutdown();
  manager_->Init(&schema_registry_);

  EXPECT_FALSE(manager_->HasSchemaRegistry());

  manager_->SetSigninProfileSchemaRegistry(&schema_registry_);

  EXPECT_TRUE(manager_->HasSchemaRegistry());
}

class DeviceCloudPolicyManagerAshEnrollmentTest
    : public DeviceCloudPolicyManagerAshTest,
      public testing::WithParamInterface<bool> {
 public:
  DeviceCloudPolicyManagerAshEnrollmentTest(
      const DeviceCloudPolicyManagerAshEnrollmentTest&) = delete;
  DeviceCloudPolicyManagerAshEnrollmentTest& operator=(
      const DeviceCloudPolicyManagerAshEnrollmentTest&) = delete;

  void Done(EnrollmentStatus status) {
    enrollment_handler_.reset();
    ConnectManager(false);
    status_ = status;
    done_ = true;
  }

 protected:
  DeviceCloudPolicyManagerAshEnrollmentTest()
      : register_status_(DM_STATUS_SUCCESS),
        policy_fetch_status_(DM_STATUS_SUCCESS),
        robot_auth_fetch_status_(DM_STATUS_SUCCESS),
        status_(EnrollmentStatus::ForEnrollmentCode(
            EnrollmentStatus::Code::kSuccess)),
        expect_robot_auth_fetch_failure_(false),
        done_(false) {}

  void SetUp() override {
    DeviceCloudPolicyManagerAshTest::SetUp();

    // Set up test data.
    device_policy_->SetDefaultNewSigningKey();
    device_policy_->policy_data().set_timestamp(
        base::Time::NowFromSystemTime().InMillisecondsSinceUnixEpoch());
    device_policy_->Build();

    register_response_.mutable_register_response()->set_device_management_token(
        PolicyBuilder::kFakeToken);
    register_response_.mutable_register_response()->set_enrollment_type(
        em::DeviceRegisterResponse::ENTERPRISE);
    policy_fetch_response_.mutable_policy_response()->add_responses()->CopyFrom(
        device_policy_->policy());
    robot_auth_fetch_response_.mutable_service_api_access_response()
        ->set_auth_code("auth_code_for_test");

    // Initialize the manager.
    device_settings_service_->LoadImmediately();
    FlushDeviceSettings();
    // Since the install attributes is not locked, the store status is
    // BAD_STATE.
    EXPECT_EQ(CloudPolicyStore::STATUS_BAD_STATE, store_->status());
    EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

    PolicyBundle bundle;
    EXPECT_TRUE(manager_->policies().Equals(bundle));

    if (ShouldRegisterWithCert()) {
      // TODO(crbug.com/1298989): This expectation tests implementation details
      // of EnrollmentHandler which is not the scope of this tests. Remove it
      // once EnrollmentHandler has its own unit tests.
      EXPECT_CALL(
          mock_attestation_flow_,
          GetCertificate(
              ash::attestation::PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
              /*force_new_key=*/true, _, _, _, _))
          .WillOnce(
              WithArgs<7>(Invoke(CertCallbackSuccessWithValidCertificate)));
    }
    AddStateKeys();
  }

  void ExpectFailedEnrollment(EnrollmentStatus::Code enrollment_code) {
    EXPECT_EQ(enrollment_code, status_.enrollment_code());
    EXPECT_FALSE(store_->is_managed());
    PolicyBundle empty_bundle;
    EXPECT_TRUE(manager_->policies().Equals(empty_bundle));
  }

  void ExpectSuccessfulEnrollment() {
    EXPECT_EQ(EnrollmentStatus::Code::kSuccess, status_.enrollment_code());
    ASSERT_TRUE(manager_->core()->client());
    EXPECT_TRUE(manager_->core()->client()->is_registered());
    EXPECT_EQ(DEVICE_MODE_ENTERPRISE, install_attributes_->GetMode());
    EXPECT_TRUE(store_->has_policy());
    EXPECT_TRUE(store_->is_managed());
    VerifyPolicyPopulated();
  }

  void RunTest() {
    const bool with_cert = ShouldRegisterWithCert();
    // Trigger enrollment.
    DeviceManagementService::JobForTesting register_job;
    DeviceManagementService::JobConfiguration::JobType register_job_type;
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .Times(AtMost(1))
        .WillOnce(
            DoAll(device_management_service_.CaptureJobType(&register_job_type),
                  device_management_service_.CaptureQueryParams(&query_params_),
                  device_management_service_.CaptureRequest(&register_request_),
                  SaveArg<0>(&register_job)));
    AllowUninterestingRemoteCommandFetches();

    ash::OwnerSettingsServiceAsh* owner_settings_service =
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
            profile_.get());
    ASSERT_TRUE(owner_settings_service);

    EnrollmentConfig enrollment_config;
    enrollment_config.mode = with_cert ? EnrollmentConfig::MODE_ATTESTATION
                                       : EnrollmentConfig::MODE_MANUAL;
    DMAuth auth =
        with_cert ? DMAuth::NoAuth() : DMAuth::FromOAuthToken("auth token");

    auto client = CreateDeviceCloudPolicyClientAsh(
        &fake_statistics_provider_, &device_management_service_,
        test_url_loader_factory_.GetSafeWeakWrapper(),
        CloudPolicyClient::DeviceDMTokenCallback());

    enrollment_handler_ = std::make_unique<EnrollmentHandler>(
        store_, install_attributes_.get(), &state_keys_broker_,
        &mock_attestation_flow_, std::move(client),
        base::SingleThreadTaskRunner::GetCurrentDefault(), enrollment_config,
        std::move(auth), install_attributes_->GetDeviceId(),
        EnrollmentRequisitionManager::GetDeviceRequisition(),
        EnrollmentRequisitionManager::GetSubOrganization(),

        base::BindOnce(&DeviceCloudPolicyManagerAshEnrollmentTest::Done,
                       base::Unretained(this)));
    enrollment_handler_->SetSigningServiceProviderForTesting(
        std::make_unique<FakeSigningServiceProvider>(/*success=*/true));
    enrollment_handler_->StartEnrollment();
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&device_management_service_);

    if (done_) {
      return;
    }

    // Process registration.
    ASSERT_TRUE(register_job.IsActive());
    ASSERT_EQ(
        with_cert
            ? DeviceManagementService::JobConfiguration::
                  TYPE_CERT_BASED_REGISTRATION
            : DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
        register_job_type);
    DeviceManagementService::JobForTesting fetch_job;
    DeviceManagementService::JobConfiguration::JobType fetch_job_type;
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .Times(AtMost(1))
        .WillOnce(
            DoAll(device_management_service_.CaptureJobType(&fetch_job_type),
                  SaveArg<0>(&fetch_job)));
    AllowUninterestingRemoteCommandFetches();
    device_management_service_.SendJobResponseNow(
        &register_job,
        register_status_ == DM_STATUS_SUCCESS ? net::OK : net::ERR_FAILED,
        DeviceManagementService::kSuccess, register_response_);
    EXPECT_FALSE(register_job.IsActive());
    Mock::VerifyAndClearExpectations(&device_management_service_);

    if (done_) {
      return;
    }

    // Process policy fetch.
    ASSERT_TRUE(fetch_job.IsActive());
    ASSERT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
              fetch_job_type);
    device_management_service_.SendJobResponseNow(
        &fetch_job,
        policy_fetch_status_ == DM_STATUS_SUCCESS ? net::OK : net::ERR_FAILED,
        DeviceManagementService::kSuccess, policy_fetch_response_);
    EXPECT_FALSE(fetch_job.IsActive());

    if (done_) {
      return;
    }

    // Process verification.
    DeviceManagementService::JobForTesting robot_auth_fetch_job;
    DeviceManagementService::JobConfiguration::JobType robot_job_type;
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .Times(AtMost(1))
        .WillOnce(
            DoAll(device_management_service_.CaptureJobType(&robot_job_type),
                  SaveArg<0>(&robot_auth_fetch_job)));
    AllowUninterestingRemoteCommandFetches();
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&device_management_service_);

    if (done_) {
      return;
    }

    // Process robot auth token fetch.
    ASSERT_TRUE(robot_auth_fetch_job.IsActive());
    ASSERT_EQ(
        DeviceManagementService::JobConfiguration::TYPE_API_AUTH_CODE_FETCH,
        robot_job_type);
    device_management_service_.SendJobResponseNow(
        &robot_auth_fetch_job,
        robot_auth_fetch_status_ == DM_STATUS_SUCCESS ? net::OK
                                                      : net::ERR_FAILED,
        DeviceManagementService::kSuccess, robot_auth_fetch_response_);
    EXPECT_FALSE(robot_auth_fetch_job.IsActive());
    Mock::VerifyAndClearExpectations(&device_management_service_);

    if (done_) {
      return;
    }

    // Set expectations for the second policy refresh that happens after the
    // enrollment completes.
    DeviceManagementService::JobForTesting component_fetch_job;
    DeviceManagementService::JobConfiguration::JobType component_job_type;
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .Times(AtMost(1))
        .WillOnce(DoAll(
            device_management_service_.CaptureJobType(&component_job_type),
            SaveArg<0>(&component_fetch_job)));
    AllowUninterestingRemoteCommandFetches();

    // Process robot refresh token fetch if the auth code fetch succeeded.
    // DeviceCloudPolicyInitializer holds an EnrollmentHandler which
    // holds a GaiaOAuthClient that fetches the refresh token during enrollment.
    // We return a successful OAuth response via a TestURLLoaderFactory to
    // trigger the happy path for these classes so that enrollment can continue.
    if (robot_auth_fetch_status_ == DM_STATUS_SUCCESS) {
      if (!expect_robot_auth_fetch_failure_) {
        EXPECT_CALL(*external_data_manager_, Connect(_));
      }
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          GaiaUrls::GetInstance()->oauth2_token_url(),
          network::URLLoaderCompletionStatus(net::OK),
          network::CreateURLResponseHead(url_fetcher_response_code_),
          url_fetcher_response_string_);
      content::RunAllTasksUntilIdle();
      Mock::VerifyAndClearExpectations(external_data_manager_);
    }

    // Process robot refresh token store and policy store.
    base::RunLoop().RunUntilIdle();

    if (!done_ ||
        status_.enrollment_code() == EnrollmentStatus::Code::kSuccess) {
      // Verify the state only if the task is not yet failed.
      // Note that, if the flow is not yet |done_| here, assume that it is
      // in the "succeeding" flow, so verify here, too.
      DeviceOAuth2TokenService* token_service =
          DeviceOAuth2TokenServiceFactory::Get();

      // For the refresh token for the robot account to be visible, the robot
      // account ID must not be empty.
      token_service->set_robot_account_id_for_testing(
          CoreAccountId::FromRobotEmail("robot_account@gserviceaccount.com"));

      EXPECT_TRUE(token_service->RefreshTokenIsAvailable());
      EXPECT_EQ(device_policy_->GetBlob(),
                session_manager_client_.device_policy());
    }
    if (done_) {
      return;
    }

    // Policy load.

    // Reloading device settings will call StartJob() a few times but the test
    // simply ignores those calls.
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillRepeatedly(device_management_service_.SendJobOKAsync(
            em::DeviceManagementResponse()));
    AllowUninterestingRemoteCommandFetches();

    ReloadDeviceSettings();

    // Respond to the second policy refresh.
    if (component_fetch_job.IsActive()) {
      ASSERT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
                component_job_type);
      device_management_service_.SendJobResponseNow(
          &robot_auth_fetch_job,
          policy_fetch_status_ == DM_STATUS_SUCCESS ? net::OK : net::ERR_FAILED,
          DeviceManagementService::kSuccess, policy_fetch_response_);
      EXPECT_FALSE(robot_auth_fetch_job.IsActive());
    }
    Mock::VerifyAndClearExpectations(&device_management_service_);
  }

  bool ShouldRegisterWithCert() const override { return GetParam(); }

  const std::unique_ptr<em::DeviceRegisterRequest> GetDeviceRegisterRequest() {
    auto req = std::make_unique<em::DeviceRegisterRequest>();
    if (ShouldRegisterWithCert()) {
      em::CertificateBasedDeviceRegistrationData data;
      const em::SignedData& signed_request =
          register_request_.certificate_based_register_request()
              .signed_request();
      EXPECT_TRUE(data.ParseFromString(signed_request.data().substr(
          0,
          signed_request.data().size() - signed_request.extra_data_bytes())));
      EXPECT_EQ(em::CertificateBasedDeviceRegistrationData::
                    ENTERPRISE_ENROLLMENT_CERTIFICATE,
                data.certificate_type());
      req->CopyFrom(data.device_register_request());
    } else {
      req->CopyFrom(register_request_.register_request());
    }
    return req;
  }

  DeviceManagementStatus register_status_;
  em::DeviceManagementResponse register_response_;

  DeviceManagementStatus policy_fetch_status_;
  em::DeviceManagementResponse policy_fetch_response_;

  DeviceManagementStatus robot_auth_fetch_status_;
  em::DeviceManagementResponse robot_auth_fetch_response_;

  em::DeviceManagementRequest register_request_;
  DeviceManagementService::JobConfiguration::ParameterMap query_params_;
  EnrollmentStatus status_;

  std::unique_ptr<EnrollmentHandler> enrollment_handler_;

  // Set to true if the robot auth fetch is expected to fail.
  bool expect_robot_auth_fetch_failure_;

  bool done_;
};

TEST_P(DeviceCloudPolicyManagerAshEnrollmentTest, Success) {
  RunTest();
  ExpectSuccessfulEnrollment();
}

TEST_P(DeviceCloudPolicyManagerAshEnrollmentTest,
       EnabledKioskHeartbeatsViaERP) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kKioskHeartbeatsViaERP);

  RunTest();
  EXPECT_FALSE(manager_->GetHeartbeatSchedulerForTesting());
}

TEST_P(DeviceCloudPolicyManagerAshEnrollmentTest,
       DisabledKioskHeartbeatsViaERP) {
  RunTest();
  EXPECT_EQ(manager_->GetHeartbeatSchedulerForTesting()->last_heartbeat(),
            base::Time());
}

TEST_P(DeviceCloudPolicyManagerAshEnrollmentTest, Reenrollment) {
  LockDevice();
  // Normally this happens at signin screen profile creation. But
  // TestingProfile doesn't do that.
  device_settings_service_->LoadImmediately();
  RunTest();
  ExpectSuccessfulEnrollment();
  EXPECT_TRUE(GetDeviceRegisterRequest()->reregister());
  EXPECT_EQ(PolicyBuilder::kFakeDeviceId,
            query_params_[dm_protocol::kParamDeviceID]);
}

TEST_P(DeviceCloudPolicyManagerAshEnrollmentTest, RegistrationFailed) {
  register_status_ = DM_STATUS_REQUEST_FAILED;
  expect_robot_auth_fetch_failure_ = true;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::Code::kRegistrationFailed);
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, status_.client_status());
}

TEST_P(DeviceCloudPolicyManagerAshEnrollmentTest, RobotAuthCodeFetchFailed) {
  robot_auth_fetch_status_ = DM_STATUS_REQUEST_FAILED;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::Code::kRobotAuthFetchFailed);
}

TEST_P(DeviceCloudPolicyManagerAshEnrollmentTest,
       RobotRefreshTokenFetchResponseCodeFailed) {
  url_fetcher_response_code_ = net::HTTP_BAD_REQUEST;
  expect_robot_auth_fetch_failure_ = true;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::Code::kRobotRefreshFetchFailed);
}

TEST_P(DeviceCloudPolicyManagerAshEnrollmentTest,
       RobotRefreshTokenFetchResponseStringFailed) {
  url_fetcher_response_string_ = "invalid response json";
  expect_robot_auth_fetch_failure_ = true;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::Code::kRobotRefreshFetchFailed);
}

TEST_P(DeviceCloudPolicyManagerAshEnrollmentTest,
       RobotRefreshEncryptionFailed) {
  // The encryption lib is a noop for tests, but empty results from encryption
  // is an error, so we simulate an encryption error by returning an empty
  // refresh token.
  url_fetcher_response_string_ =
      "{\"access_token\":\"accessToken4Test\","
      "\"expires_in\":1234,"
      "\"refresh_token\":\"\"}";
  expect_robot_auth_fetch_failure_ = true;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::Code::kRobotRefreshStoreFailed);
}

TEST_P(DeviceCloudPolicyManagerAshEnrollmentTest, PolicyFetchFailed) {
  policy_fetch_status_ = DM_STATUS_REQUEST_FAILED;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::Code::kPolicyFetchFailed);
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, status_.client_status());
}

TEST_P(DeviceCloudPolicyManagerAshEnrollmentTest, ValidationFailed) {
  device_policy_->policy().set_policy_data_signature("bad");
  policy_fetch_response_.clear_policy_response();
  policy_fetch_response_.mutable_policy_response()->add_responses()->CopyFrom(
      device_policy_->policy());
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::Code::kValidationFailed);
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_INITIAL_SIGNATURE,
            status_.validation_status());
}

TEST_P(DeviceCloudPolicyManagerAshEnrollmentTest, StoreError) {
  session_manager_client_.ForceStorePolicyFailure(true);
  expect_robot_auth_fetch_failure_ = true;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::Code::kStoreError);
  EXPECT_EQ(CloudPolicyStore::STATUS_STORE_ERROR, status_.store_status());
}

TEST_P(DeviceCloudPolicyManagerAshEnrollmentTest, LoadError) {
  session_manager_client_.ForceRetrievePolicyLoadError(true);
  expect_robot_auth_fetch_failure_ = true;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::Code::kStoreError);
  EXPECT_EQ(CloudPolicyStore::STATUS_LOAD_ERROR, status_.store_status());
}

TEST_P(DeviceCloudPolicyManagerAshEnrollmentTest, DisableMachineCertReq) {
  // Simulate the flag --disable-machine-cert-request being provided to Chrome.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kDisableMachineCertRequest);

  // Set expectation that a request for a machine cert is never made.
  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(ash::attestation::AttestationCertificateProfile::
                                 PROFILE_ENTERPRISE_MACHINE_CERTIFICATE,
                             _, _, _, _, _, _, _))
      .Times(0);

  RunTest();
  ExpectSuccessfulEnrollment();
}

// A subclass that runs with a blank system salt.
class DeviceCloudPolicyManagerAshEnrollmentBlankSystemSaltTest
    : public DeviceCloudPolicyManagerAshEnrollmentTest {
 protected:
  DeviceCloudPolicyManagerAshEnrollmentBlankSystemSaltTest() {
    set_empty_system_salt_ = true;
  }
};

TEST_P(DeviceCloudPolicyManagerAshEnrollmentBlankSystemSaltTest,
       RobotRefreshSaveFailed) {
  // Without the system salt, the robot token can't be stored.
  expect_robot_auth_fetch_failure_ = true;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::Code::kRobotRefreshStoreFailed);
}

INSTANTIATE_TEST_SUITE_P(Cert,
                         DeviceCloudPolicyManagerAshEnrollmentTest,
                         ::testing::Values(false, true));

INSTANTIATE_TEST_SUITE_P(
    Cert,
    DeviceCloudPolicyManagerAshEnrollmentBlankSystemSaltTest,
    ::testing::Values(false, true));

}  // namespace
}  // namespace policy
