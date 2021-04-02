// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/policy/enrollment_requisition_manager.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/policy/enrollment_status.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/dbus/userdataauth/fake_install_attributes_client.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/core/common/cloud/mock_signing_service.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::AtMost;
using testing::DoAll;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;
using testing::StrictMock;
using testing::WithArgs;

namespace em = enterprise_management;

namespace policy {
namespace {

MATCHER_P(HasJobType, job_type, "matches job type") {
  return arg->GetConfiguration()->GetType() == job_type;
}

void CopyLockResult(base::RunLoop* loop,
                    chromeos::InstallAttributes::LockResult* out,
                    chromeos::InstallAttributes::LockResult result) {
  *out = result;
  loop->Quit();
}

void CertCallbackSuccess(
    chromeos::attestation::AttestationFlow::CertificateCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     chromeos::attestation::ATTESTATION_SUCCESS, "fake_cert"));
}

class TestingDeviceCloudPolicyManagerChromeOS
    : public DeviceCloudPolicyManagerChromeOS {
 public:
  TestingDeviceCloudPolicyManagerChromeOS(
      std::unique_ptr<DeviceCloudPolicyStoreChromeOS> store,
      std::unique_ptr<CloudExternalDataManager> external_data_manager,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      ServerBackedStateKeysBroker* state_keys_broker)
      : DeviceCloudPolicyManagerChromeOS(std::move(store),
                                         std::move(external_data_manager),
                                         task_runner,
                                         state_keys_broker) {
    set_component_policy_disabled_for_testing(true);
  }

  ~TestingDeviceCloudPolicyManagerChromeOS() override {}
};

class DeviceCloudPolicyManagerChromeOSTest
    : public ash::DeviceSettingsTestBase,
      public chromeos::SessionManagerClient::Observer {
 protected:
  DeviceCloudPolicyManagerChromeOSTest()
      : state_keys_broker_(&session_manager_client_), store_(nullptr) {
    fake_statistics_provider_.SetMachineStatistic(
        chromeos::system::kSerialNumberKeyForTest, "test_sn");
    fake_statistics_provider_.SetMachineStatistic(
        chromeos::system::kHardwareClassKey, "test_hw");
    std::vector<std::string> state_keys;
    state_keys.push_back("1");
    state_keys.push_back("2");
    state_keys.push_back("3");
    session_manager_client_.set_server_backed_state_keys(state_keys);
    session_manager_client_.AddObserver(this);
  }

  ~DeviceCloudPolicyManagerChromeOSTest() override {
    session_manager_client_.RemoveObserver(this);
    chromeos::system::StatisticsProvider::SetTestProvider(NULL);
  }

  virtual bool ShouldRegisterWithCert() const { return false; }

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();

    device_management_service_.ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();

    if (set_empty_system_salt_) {
      chromeos::FakeCryptohomeMiscClient::Get()->set_system_salt(
          std::vector<uint8_t>());
    }

    chromeos::InstallAttributesClient::InitializeFake();
    install_attributes_ = std::make_unique<chromeos::InstallAttributes>(
        chromeos::FakeInstallAttributesClient::Get());
    store_ = new DeviceCloudPolicyStoreChromeOS(
        device_settings_service_.get(), install_attributes_.get(),
        base::ThreadTaskRunnerHandle::Get());
    auto external_data_manager =
        std::make_unique<MockCloudExternalDataManager>();
    external_data_manager_ = external_data_manager.get();
    manager_ = std::make_unique<TestingDeviceCloudPolicyManagerChromeOS>(
        base::WrapUnique(store_), std::move(external_data_manager),
        base::ThreadTaskRunnerHandle::Get(), &state_keys_broker_);

    RegisterLocalState(local_state_.registry());
    manager_->Init(&schema_registry_);

    // SharedURLLoaderFactory and LocalState singletons have to be set since
    // they are accessed by EnrollmentHandlerChromeOS and StartupUtils.
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);

    // SystemSaltGetter is used in DeviceOAuth2TokenService.
    chromeos::SystemSaltGetter::Initialize();
    DeviceOAuth2TokenServiceFactory::Initialize(
        test_url_loader_factory_.GetSafeWeakWrapper(), &local_state_);

    url_fetcher_response_code_ = net::HTTP_OK;
    url_fetcher_response_string_ =
        "{\"access_token\":\"accessToken4Test\","
        "\"expires_in\":1234,"
        "\"refresh_token\":\"refreshToken4Test\"}";

    AllowUninterestingRemoteCommandFetches();
  }

  void TearDown() override {
    if (initializer_)
      initializer_->Shutdown();
    ShutdownManager();
    manager_.reset();
    install_attributes_.reset();

    DeviceOAuth2TokenServiceFactory::Shutdown();
    chromeos::SystemSaltGetter::Shutdown();
    chromeos::InstallAttributesClient::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);

    DeviceSettingsTestBase::TearDown();
  }

  StrictMock<chromeos::attestation::MockAttestationFlow>*
  CreateAttestationFlow() {
    mock_ = new StrictMock<chromeos::attestation::MockAttestationFlow>();
    if (ShouldRegisterWithCert()) {
      EXPECT_CALL(*mock_, GetCertificate(_, _, _, _, _, _))
          .WillOnce(WithArgs<5>(Invoke(CertCallbackSuccess)));
    }
    return mock_;
  }

  void LockDevice() {
    base::RunLoop loop;
    chromeos::InstallAttributes::LockResult result;
    install_attributes_->LockDevice(
        DEVICE_MODE_ENTERPRISE, PolicyBuilder::kFakeDomain,
        std::string(),  // realm
        PolicyBuilder::kFakeDeviceId,
        base::BindOnce(&CopyLockResult, &loop, &result));
    loop.Run();
    ASSERT_EQ(chromeos::InstallAttributes::LOCK_SUCCESS, result);
  }

  void ConnectManager(bool expectExternalDataManagerConnectCall = true) {
    if (expectExternalDataManagerConnectCall) {
      EXPECT_CALL(*external_data_manager_, Connect(_));
    }
    std::unique_ptr<chromeos::attestation::AttestationFlow> unique_flow(
        CreateAttestationFlow());
    manager_->Initialize(&local_state_);
    policy::EnrollmentRequisitionManager::Initialize();
    initializer_ = std::make_unique<DeviceCloudPolicyInitializer>(
        &local_state_, &device_management_service_,
        base::ThreadTaskRunnerHandle::Get(), install_attributes_.get(),
        &state_keys_broker_, store_, manager_.get(), std::move(unique_flow),
        &fake_statistics_provider_);
    initializer_->SetSigningServiceForTesting(
        std::make_unique<FakeSigningService>());
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
        device_management_service_,
        StartJob(HasJobType(
            DeviceManagementService::JobConfiguration::TYPE_REMOTE_COMMANDS)))
        .Times(AnyNumber())
        .WillRepeatedly(device_management_service_.StartJobAsync(
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

  std::unique_ptr<chromeos::InstallAttributes> install_attributes_;

  net::HttpStatusCode url_fetcher_response_code_;
  std::string url_fetcher_response_string_;
  TestingPrefServiceSimple local_state_;
  MockDeviceManagementService device_management_service_;
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  bool set_empty_system_salt_ = false;
  ServerBackedStateKeysBroker state_keys_broker_;
  StrictMock<chromeos::attestation::MockAttestationFlow>* mock_;

  DeviceCloudPolicyStoreChromeOS* store_;
  SchemaRegistry schema_registry_;
  MockCloudExternalDataManager* external_data_manager_;
  std::unique_ptr<TestingDeviceCloudPolicyManagerChromeOS> manager_;
  std::unique_ptr<DeviceCloudPolicyInitializer> initializer_;
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  // This property is required to instantiate the session manager, a singleton
  // which is used by the device status collector.
  session_manager::SessionManager session_manager_;

  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyManagerChromeOSTest);
};

TEST_F(DeviceCloudPolicyManagerChromeOSTest, FreshDevice) {
  owner_key_util_->Clear();
  FlushDeviceSettings();
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  manager_->Initialize(&local_state_);

  PolicyBundle bundle;
  EXPECT_TRUE(manager_->policies().Equals(bundle));
}

TEST_F(DeviceCloudPolicyManagerChromeOSTest, EnrolledDevice) {
  LockDevice();
  FlushDeviceSettings();
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  VerifyPolicyPopulated();

  // Trigger a policy refresh - this triggers a policy update.
  DeviceManagementService::JobControl* policy_job = nullptr;
  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(device_management_service_, StartJob(_))
      .WillOnce(
          DoAll(device_management_service_.CaptureJobType(&job_type),
                device_management_service_.StartJobFullControl(&policy_job)));
  AllowUninterestingRemoteCommandFetches();
  ConnectManager();
  Mock::VerifyAndClearExpectations(&device_management_service_);
  ASSERT_TRUE(policy_job);
  ASSERT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type);
  // Should create a status uploader for reporting on enrolled devices.
  EXPECT_TRUE(manager_->GetStatusUploader());
  VerifyPolicyPopulated();

  ShutdownManager();
  VerifyPolicyPopulated();

  EXPECT_EQ(store_->policy()->service_account_identity(),
            PolicyBuilder::kFakeServiceAccountIdentity);
}

TEST_F(DeviceCloudPolicyManagerChromeOSTest, UnmanagedDevice) {
  device_policy_->policy_data().set_state(em::PolicyData::UNMANAGED);
  device_policy_->Build();
  session_manager_client_.set_device_policy(device_policy_->GetBlob());

  LockDevice();
  FlushDeviceSettings();
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(store_->is_managed());

  // Policy settings should be ignored for UNMANAGED devices.
  PolicyBundle bundle;
  EXPECT_TRUE(manager_->policies().Equals(bundle));

  // Trigger a policy refresh.
  DeviceManagementService::JobControl* policy_job = nullptr;
  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(device_management_service_, StartJob(_))
      .WillOnce(
          DoAll(device_management_service_.CaptureJobType(&job_type),
                device_management_service_.StartJobFullControl(&policy_job)));
  AllowUninterestingRemoteCommandFetches();
  ConnectManager();
  Mock::VerifyAndClearExpectations(&device_management_service_);
  ASSERT_TRUE(policy_job);
  ASSERT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type);
  // Should create a status provider for reporting on enrolled devices, even
  // those that aren't managed.
  EXPECT_TRUE(manager_->GetStatusUploader());

  // Switch back to ACTIVE, service the policy fetch and let it propagate.
  device_policy_->policy_data().set_state(em::PolicyData::ACTIVE);
  device_policy_->Build();
  session_manager_client_.set_device_policy(device_policy_->GetBlob());
  em::DeviceManagementResponse policy_fetch_response;
  policy_fetch_response.mutable_policy_response()->add_responses()->CopyFrom(
      device_policy_->policy());
  device_management_service_.DoURLCompletion(&policy_job, net::OK,
                                             DeviceManagementService::kSuccess,
                                             policy_fetch_response);
  EXPECT_EQ(nullptr, policy_job);
  FlushDeviceSettings();

  // Policy state should now be active and the policy map should be populated.
  EXPECT_TRUE(store_->is_managed());
  VerifyPolicyPopulated();
}

TEST_F(DeviceCloudPolicyManagerChromeOSTest, ConsumerDevice) {
  FlushDeviceSettings();
  EXPECT_EQ(CloudPolicyStore::STATUS_BAD_STATE, store_->status());
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  PolicyBundle bundle;
  EXPECT_TRUE(manager_->policies().Equals(bundle));

  ConnectManager(false);
  EXPECT_TRUE(manager_->policies().Equals(bundle));
  // Should not create a status provider for reporting on consumer devices.
  EXPECT_FALSE(manager_->GetStatusUploader());

  ShutdownManager();
  EXPECT_TRUE(manager_->policies().Equals(bundle));
}

class DeviceCloudPolicyManagerChromeOSObserverTest
    : public DeviceCloudPolicyManagerChromeOSTest,
      public DeviceCloudPolicyManagerChromeOS::Observer {
 protected:
  DeviceCloudPolicyManagerChromeOSObserverTest() {}

  void SetUp() override {
    DeviceCloudPolicyManagerChromeOSTest::SetUp();
    manager_->AddDeviceCloudPolicyManagerObserver(this);
  }

  void TearDown() override {
    manager_->RemoveDeviceCloudPolicyManagerObserver(this);
    DeviceCloudPolicyManagerChromeOSTest::TearDown();
  }

  // DeviceCloudPolicyManagerChromeOS::Observer:
  MOCK_METHOD0(OnDeviceCloudPolicyManagerConnected, void());
  MOCK_METHOD0(OnDeviceCloudPolicyManagerDisconnected, void());
};

TEST_F(DeviceCloudPolicyManagerChromeOSObserverTest, ConnectAndDisconnect) {
  LockDevice();
  FlushDeviceSettings();
  EXPECT_FALSE(manager_->core()->service());  // Not connected.

  // Connect the manager.
  DeviceManagementService::JobControl* policy_job = nullptr;
  EXPECT_CALL(device_management_service_, StartJob(_))
      .WillOnce(device_management_service_.StartJobFullControl(&policy_job));
  AllowUninterestingRemoteCommandFetches();
  EXPECT_CALL(*this, OnDeviceCloudPolicyManagerConnected());
  ConnectManager();
  Mock::VerifyAndClearExpectations(&device_management_service_);
  Mock::VerifyAndClearExpectations(this);
  EXPECT_TRUE(manager_->core()->service());  // Connected.

  // Disconnect the manager.
  EXPECT_CALL(*this, OnDeviceCloudPolicyManagerDisconnected());
  manager_->Disconnect();
  EXPECT_FALSE(manager_->core()->service());  // Not connnected.
}

class DeviceCloudPolicyManagerChromeOSEnrollmentTest
    : public DeviceCloudPolicyManagerChromeOSTest,
      public testing::WithParamInterface<bool> {
 public:
  void Done(EnrollmentStatus status) {
    status_ = status;
    done_ = true;
  }

  MOCK_METHOD1(OnUnregistered, void(bool));

 protected:
  DeviceCloudPolicyManagerChromeOSEnrollmentTest()
      : register_status_(DM_STATUS_SUCCESS),
        policy_fetch_status_(DM_STATUS_SUCCESS),
        robot_auth_fetch_status_(DM_STATUS_SUCCESS),
        status_(EnrollmentStatus::ForStatus(EnrollmentStatus::SUCCESS)),
        expect_robot_auth_fetch_failure_(false),
        done_(false) {}

  void SetUp() override {
    DeviceCloudPolicyManagerChromeOSTest::SetUp();

    // Set up test data.
    device_policy_->SetDefaultNewSigningKey();
    device_policy_->policy_data().set_timestamp(
        base::Time::NowFromSystemTime().ToJavaTime());
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
    FlushDeviceSettings();
    EXPECT_EQ(CloudPolicyStore::STATUS_BAD_STATE, store_->status());
    EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

    PolicyBundle bundle;
    EXPECT_TRUE(manager_->policies().Equals(bundle));

    ConnectManager(false);
  }

  void ExpectFailedEnrollment(EnrollmentStatus::Status status) {
    EXPECT_EQ(status, status_.status());
    EXPECT_FALSE(store_->is_managed());
    PolicyBundle empty_bundle;
    EXPECT_TRUE(manager_->policies().Equals(empty_bundle));
  }

  void ExpectSuccessfulEnrollment() {
    EXPECT_EQ(EnrollmentStatus::SUCCESS, status_.status());
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
    DeviceManagementService::JobControl* register_job = nullptr;
    DeviceManagementService::JobConfiguration::JobType register_job_type;
    EXPECT_CALL(device_management_service_, StartJob(_))
        .Times(AtMost(1))
        .WillOnce(DoAll(
            device_management_service_.CaptureJobType(&register_job_type),
            device_management_service_.CaptureQueryParams(&query_params_),
            device_management_service_.CaptureRequest(&register_request_),
            device_management_service_.StartJobFullControl(&register_job)));
    AllowUninterestingRemoteCommandFetches();

    ash::OwnerSettingsServiceAsh* owner_settings_service =
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
            profile_.get());
    ASSERT_TRUE(owner_settings_service);

    EnrollmentConfig enrollment_config;
    enrollment_config.auth_mechanism =
        EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;
    enrollment_config.mode = with_cert ? EnrollmentConfig::MODE_ATTESTATION
                                       : EnrollmentConfig::MODE_MANUAL;
    DMAuth auth =
        with_cert ? DMAuth::NoAuth() : DMAuth::FromOAuthToken("auth token");
    initializer_->PrepareEnrollment(
        &device_management_service_, nullptr, enrollment_config,
        std::move(auth),
        base::BindOnce(&DeviceCloudPolicyManagerChromeOSEnrollmentTest::Done,
                       base::Unretained(this)));
    initializer_->StartEnrollment();
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&device_management_service_);

    if (done_)
      return;

    // Process registration.
    ASSERT_TRUE(register_job);
    ASSERT_EQ(
        with_cert
            ? DeviceManagementService::JobConfiguration::
                  TYPE_CERT_BASED_REGISTRATION
            : DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
        register_job_type);
    DeviceManagementService::JobControl* fetch_job = nullptr;
    DeviceManagementService::JobConfiguration::JobType fetch_job_type;
    EXPECT_CALL(device_management_service_, StartJob(_))
        .Times(AtMost(1))
        .WillOnce(
            DoAll(device_management_service_.CaptureJobType(&fetch_job_type),
                  device_management_service_.StartJobFullControl(&fetch_job)));
    AllowUninterestingRemoteCommandFetches();
    device_management_service_.DoURLCompletion(
        &register_job,
        register_status_ == DM_STATUS_SUCCESS ? net::OK : net::ERR_FAILED,
        DeviceManagementService::kSuccess, register_response_);
    EXPECT_EQ(nullptr, register_job);
    Mock::VerifyAndClearExpectations(&device_management_service_);

    if (done_)
      return;

    // Process policy fetch.
    ASSERT_TRUE(fetch_job);
    ASSERT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
              fetch_job_type);
    device_management_service_.DoURLCompletion(
        &fetch_job,
        policy_fetch_status_ == DM_STATUS_SUCCESS ? net::OK : net::ERR_FAILED,
        DeviceManagementService::kSuccess, policy_fetch_response_);
    EXPECT_EQ(nullptr, fetch_job);

    if (done_)
      return;

    // Process verification.
    DeviceManagementService::JobControl* robot_auth_fetch_job = nullptr;
    DeviceManagementService::JobConfiguration::JobType robot_job_type;
    EXPECT_CALL(device_management_service_, StartJob(_))
        .Times(AtMost(1))
        .WillOnce(
            DoAll(device_management_service_.CaptureJobType(&robot_job_type),
                  device_management_service_.StartJobFullControl(
                      &robot_auth_fetch_job)));
    AllowUninterestingRemoteCommandFetches();
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&device_management_service_);

    if (done_)
      return;

    // Process robot auth token fetch.
    ASSERT_TRUE(robot_auth_fetch_job);
    ASSERT_EQ(
        DeviceManagementService::JobConfiguration::TYPE_API_AUTH_CODE_FETCH,
        robot_job_type);
    device_management_service_.DoURLCompletion(
        &robot_auth_fetch_job,
        robot_auth_fetch_status_ == DM_STATUS_SUCCESS ? net::OK
                                                      : net::ERR_FAILED,
        DeviceManagementService::kSuccess, robot_auth_fetch_response_);
    EXPECT_EQ(nullptr, robot_auth_fetch_job);
    Mock::VerifyAndClearExpectations(&device_management_service_);

    if (done_)
      return;

    // Set expectations for the second policy refresh that happens after the
    // enrollment completes.
    DeviceManagementService::JobControl* component_fetch_job = nullptr;
    DeviceManagementService::JobConfiguration::JobType component_job_type;
    EXPECT_CALL(device_management_service_, StartJob(_))
        .Times(AtMost(1))
        .WillOnce(DoAll(
            device_management_service_.CaptureJobType(&component_job_type),
            device_management_service_.StartJobFullControl(
                &component_fetch_job)));
    AllowUninterestingRemoteCommandFetches();

    // Process robot refresh token fetch if the auth code fetch succeeded.
    // DeviceCloudPolicyInitializer holds an EnrollmentHandlerChromeOS which
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

    if (!done_ || status_.status() == EnrollmentStatus::SUCCESS) {
      // Verify the state only if the task is not yet failed.
      // Note that, if the flow is not yet |done_| here, assume that it is
      // in the "succeeding" flow, so verify here, too.
      DeviceOAuth2TokenService* token_service =
          DeviceOAuth2TokenServiceFactory::Get();

      // For the refresh token for the robot account to be visible, the robot
      // account ID must not be empty.
      token_service->set_robot_account_id_for_testing(CoreAccountId("dummy"));

      EXPECT_TRUE(token_service->RefreshTokenIsAvailable());
      EXPECT_EQ(device_policy_->GetBlob(),
                session_manager_client_.device_policy());
    }
    if (done_)
      return;

    // Policy load.

    // Reloading device settings will call StartJob() a few times but the test
    // simply ignores those calls.
    EXPECT_CALL(device_management_service_, StartJob(_))
        .WillRepeatedly(device_management_service_.StartJobAsync(
            net::OK, DeviceManagementService::kSuccess,
            em::DeviceManagementResponse()));
    AllowUninterestingRemoteCommandFetches();

    ReloadDeviceSettings();

    // Respond to the second policy refresh.
    if (component_fetch_job) {
      ASSERT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
                component_job_type);
      device_management_service_.DoURLCompletion(
          &robot_auth_fetch_job,
          policy_fetch_status_ == DM_STATUS_SUCCESS ? net::OK : net::ERR_FAILED,
          DeviceManagementService::kSuccess, policy_fetch_response_);
      EXPECT_EQ(nullptr, robot_auth_fetch_job);
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

  // Set to true if the robot auth fetch is expected to fail.
  bool expect_robot_auth_fetch_failure_;

  bool done_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyManagerChromeOSEnrollmentTest);
};

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest, Success) {
  RunTest();
  ExpectSuccessfulEnrollment();
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest, Reenrollment) {
  LockDevice();
  RunTest();
  ExpectSuccessfulEnrollment();
  EXPECT_TRUE(GetDeviceRegisterRequest()->reregister());
  EXPECT_EQ(PolicyBuilder::kFakeDeviceId,
            query_params_[dm_protocol::kParamDeviceID]);
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest, RegistrationFailed) {
  register_status_ = DM_STATUS_REQUEST_FAILED;
  expect_robot_auth_fetch_failure_ = true;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::REGISTRATION_FAILED);
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, status_.client_status());
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest,
       RobotAuthCodeFetchFailed) {
  robot_auth_fetch_status_ = DM_STATUS_REQUEST_FAILED;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::ROBOT_AUTH_FETCH_FAILED);
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest,
       RobotRefreshTokenFetchResponseCodeFailed) {
  url_fetcher_response_code_ = net::HTTP_BAD_REQUEST;
  expect_robot_auth_fetch_failure_ = true;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::ROBOT_REFRESH_FETCH_FAILED);
  EXPECT_EQ(net::HTTP_BAD_REQUEST, status_.http_status());
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest,
       RobotRefreshTokenFetchResponseStringFailed) {
  url_fetcher_response_string_ = "invalid response json";
  expect_robot_auth_fetch_failure_ = true;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::ROBOT_REFRESH_FETCH_FAILED);
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest,
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
  ExpectFailedEnrollment(EnrollmentStatus::ROBOT_REFRESH_STORE_FAILED);
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest, PolicyFetchFailed) {
  policy_fetch_status_ = DM_STATUS_REQUEST_FAILED;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::POLICY_FETCH_FAILED);
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, status_.client_status());
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest, ValidationFailed) {
  device_policy_->policy().set_policy_data_signature("bad");
  policy_fetch_response_.clear_policy_response();
  policy_fetch_response_.mutable_policy_response()->add_responses()->CopyFrom(
      device_policy_->policy());
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::VALIDATION_FAILED);
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_INITIAL_SIGNATURE,
            status_.validation_status());
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest, StoreError) {
  session_manager_client_.ForceStorePolicyFailure(true);
  expect_robot_auth_fetch_failure_ = true;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::STORE_ERROR);
  EXPECT_EQ(CloudPolicyStore::STATUS_STORE_ERROR, status_.store_status());
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest, LoadError) {
  session_manager_client_.ForceRetrievePolicyLoadError(true);
  expect_robot_auth_fetch_failure_ = true;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::STORE_ERROR);
  EXPECT_EQ(CloudPolicyStore::STATUS_LOAD_ERROR, status_.store_status());
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest, UnregisterSucceeds) {
  // Enroll first.
  RunTest();
  ExpectSuccessfulEnrollment();

  // Set up mock objects for the upcoming unregistration job.
  em::DeviceManagementResponse response;
  response.mutable_unregister_response();
  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(device_management_service_, StartJob(_))
      .WillOnce(DoAll(device_management_service_.CaptureJobType(&job_type),
                      device_management_service_.StartJobOKAsync(response)));
  AllowUninterestingRemoteCommandFetches();
  EXPECT_CALL(*this, OnUnregistered(true));

  // Start unregistering.
  manager_->Unregister(base::BindOnce(
      &DeviceCloudPolicyManagerChromeOSEnrollmentTest::OnUnregistered,
      base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(DeviceManagementService::JobConfiguration::TYPE_UNREGISTRATION,
            job_type);
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest, UnregisterFails) {
  // Enroll first.
  RunTest();
  ExpectSuccessfulEnrollment();

  // Set up mock objects for the upcoming unregistration job.
  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(device_management_service_, StartJob(_))
      .WillOnce(DoAll(device_management_service_.CaptureJobType(&job_type),
                      device_management_service_.StartJobAsync(
                          net::ERR_FAILED, DeviceManagementService::kSuccess)));
  AllowUninterestingRemoteCommandFetches();
  EXPECT_CALL(*this, OnUnregistered(false));

  // Start unregistering.
  manager_->Unregister(base::BindOnce(
      &DeviceCloudPolicyManagerChromeOSEnrollmentTest::OnUnregistered,
      base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(DeviceManagementService::JobConfiguration::TYPE_UNREGISTRATION,
            job_type);
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest, DisableMachineCertReq) {
  // Simulate the flag --disable-machine-cert-request being provided to Chrome.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kDisableMachineCertRequest);

  // Set expecation that a request for a machine cert is never made.
  EXPECT_CALL(*mock_, GetCertificate(
                          chromeos::attestation::AttestationCertificateProfile::
                              PROFILE_ENTERPRISE_MACHINE_CERTIFICATE,
                          _, _, _, _, _))
      .Times(0);

  RunTest();
  ExpectSuccessfulEnrollment();
}

// A subclass that runs with a blank system salt.
class DeviceCloudPolicyManagerChromeOSEnrollmentBlankSystemSaltTest
    : public DeviceCloudPolicyManagerChromeOSEnrollmentTest {
 protected:
  DeviceCloudPolicyManagerChromeOSEnrollmentBlankSystemSaltTest() {
    set_empty_system_salt_ = true;
  }
};

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentBlankSystemSaltTest,
       RobotRefreshSaveFailed) {
  // Without the system salt, the robot token can't be stored.
  expect_robot_auth_fetch_failure_ = true;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::ROBOT_REFRESH_STORE_FAILED);
}

INSTANTIATE_TEST_SUITE_P(Cert,
                         DeviceCloudPolicyManagerChromeOSEnrollmentTest,
                         ::testing::Values(false, true));

INSTANTIATE_TEST_SUITE_P(
    Cert,
    DeviceCloudPolicyManagerChromeOSEnrollmentBlankSystemSaltTest,
    ::testing::Values(false, true));

}  // namespace
}  // namespace policy
