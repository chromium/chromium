// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/settings/install_attributes.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
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
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
using testing::_;

namespace em = enterprise_management;

namespace policy {
namespace {

void CopyLockResult(base::RunLoop* loop,
                    chromeos::InstallAttributes::LockResult* out,
                    chromeos::InstallAttributes::LockResult result) {
  *out = result;
  loop->Quit();
}

void CertCallbackSuccess(
    const chromeos::attestation::AttestationFlow::CertificateCallback&
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(callback, chromeos::attestation::ATTESTATION_SUCCESS,
                     "fake_cert"));
}

class TestingDeviceCloudPolicyManagerChromeOS
    : public DeviceCloudPolicyManagerChromeOS {
 public:
  TestingDeviceCloudPolicyManagerChromeOS(
      std::unique_ptr<DeviceCloudPolicyStoreChromeOS> store,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      ServerBackedStateKeysBroker* state_keys_broker)
      : DeviceCloudPolicyManagerChromeOS(std::move(store),
                                         task_runner,
                                         state_keys_broker) {
    set_component_policy_disabled_for_testing(true);
  }

  ~TestingDeviceCloudPolicyManagerChromeOS() override {}
};

class DeviceCloudPolicyManagerChromeOSTest
    : public chromeos::DeviceSettingsTestBase,
      public DeviceCloudPolicyManagerChromeOS::Observer {
 protected:
  DeviceCloudPolicyManagerChromeOSTest()
      : fake_cryptohome_client_(new chromeos::FakeCryptohomeClient()),
        state_keys_broker_(&fake_session_manager_client_),
        store_(nullptr),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    fake_statistics_provider_.SetMachineStatistic(
        chromeos::system::kSerialNumberKeyForTest, "test_sn");
    fake_statistics_provider_.SetMachineStatistic(
        chromeos::system::kHardwareClassKey, "test_hw");
    std::vector<std::string> state_keys;
    state_keys.push_back("1");
    state_keys.push_back("2");
    state_keys.push_back("3");
    fake_session_manager_client_.set_server_backed_state_keys(state_keys);
  }

  ~DeviceCloudPolicyManagerChromeOSTest() override {
    chromeos::system::StatisticsProvider::SetTestProvider(NULL);
  }

  virtual bool ShouldRegisterWithCert() const { return false; }

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();
    dbus_setter_->SetCryptohomeClient(
        std::unique_ptr<chromeos::CryptohomeClient>(fake_cryptohome_client_));
    chromeos::DBusThreadManager::Get()->GetCryptohomeClient();
    cryptohome::AsyncMethodCaller::Initialize();

    install_attributes_ =
        std::make_unique<chromeos::InstallAttributes>(fake_cryptohome_client_);
    store_ = new DeviceCloudPolicyStoreChromeOS(
        &device_settings_service_, install_attributes_.get(),
        base::ThreadTaskRunnerHandle::Get());
    manager_ = std::make_unique<TestingDeviceCloudPolicyManagerChromeOS>(
        base::WrapUnique(store_), base::ThreadTaskRunnerHandle::Get(),
        &state_keys_broker_);

    RegisterLocalState(local_state_.registry());
    manager_->Init(&schema_registry_);

    // SharedURLLoaderFactory and LocalState singletons have to be set since
    // they are accessed by EnrollmentHandlerChromeOS and StartupUtils.
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_shared_loader_factory_);
    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);

    // SystemSaltGetter is used in DeviceOAuth2TokenService.
    chromeos::SystemSaltGetter::Initialize();
    chromeos::DeviceOAuth2TokenServiceFactory::Initialize(
        test_shared_loader_factory_, &local_state_);

    url_fetcher_response_code_ = net::HTTP_OK;
    url_fetcher_response_string_ = "{\"access_token\":\"accessToken4Test\","
                                   "\"expires_in\":1234,"
                                   "\"refresh_token\":\"refreshToken4Test\"}";

    AllowUninterestingRemoteCommandFetches();
  }

  StrictMock<chromeos::attestation::MockAttestationFlow>*
  CreateAttestationFlow() {
    mock_ = new StrictMock<chromeos::attestation::MockAttestationFlow>();
    if (ShouldRegisterWithCert()) {
      EXPECT_CALL(*mock_, GetCertificate(_, _, _, _, _))
          .WillOnce(WithArgs<4>(Invoke(CertCallbackSuccess)));
    }
    return mock_;
  }

  void TearDown() override {
    cryptohome::AsyncMethodCaller::Shutdown();

    manager_->RemoveDeviceCloudPolicyManagerObserver(this);
    manager_->Shutdown();
    if (initializer_)
      initializer_->Shutdown();
    DeviceSettingsTestBase::TearDown();

    chromeos::DeviceOAuth2TokenServiceFactory::Shutdown();
    chromeos::SystemSaltGetter::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(NULL);
    test_shared_loader_factory_->Detach();
  }

  void LockDevice() {
    base::RunLoop loop;
    chromeos::InstallAttributes::LockResult result;
    install_attributes_->LockDevice(
        DEVICE_MODE_ENTERPRISE,
        PolicyBuilder::kFakeDomain,
        std::string(),  // realm
        PolicyBuilder::kFakeDeviceId,
        base::Bind(&CopyLockResult, &loop, &result));
    loop.Run();
    ASSERT_EQ(chromeos::InstallAttributes::LOCK_SUCCESS, result);
  }

  void ConnectManager() {
    std::unique_ptr<chromeos::attestation::AttestationFlow> unique_flow(
        CreateAttestationFlow());
    manager_->Initialize(&local_state_);
    manager_->AddDeviceCloudPolicyManagerObserver(this);
    initializer_ = std::make_unique<DeviceCloudPolicyInitializer>(
        &local_state_, &device_management_service_,
        base::ThreadTaskRunnerHandle::Get(), install_attributes_.get(),
        &state_keys_broker_, store_, manager_.get(),
        cryptohome::AsyncMethodCaller::GetInstance(), std::move(unique_flow),
        &fake_statistics_provider_);
    initializer_->SetSigningServiceForTesting(
        std::make_unique<FakeSigningService>());
    initializer_->SetSystemURLLoaderFactoryForTesting(
        test_shared_loader_factory_);
    initializer_->Init();
  }

  void VerifyPolicyPopulated() {
    PolicyBundle bundle;
    bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
        .Set(key::kDeviceMetricsReportingEnabled, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(false), nullptr);
    EXPECT_TRUE(manager_->policies().Equals(bundle));
  }

  void AllowUninterestingRemoteCommandFetches() {
    // We are not interested in remote command fetches that the client initiates
    // automatically. Make them fail and ignore them otherwise.
    EXPECT_CALL(device_management_service_,
                CreateJob(DeviceManagementRequestJob::TYPE_REMOTE_COMMANDS, _))
        .Times(AnyNumber())
        .WillRepeatedly(device_management_service_.FailJob(
            DM_STATUS_TEMPORARY_UNAVAILABLE));
    EXPECT_CALL(
        device_management_service_,
        StartJob(dm_protocol::kValueRequestRemoteCommands, _, _, _, _, _, _))
        .Times(AnyNumber());
  }

  MOCK_METHOD0(OnDeviceCloudPolicyManagerConnected, void());
  MOCK_METHOD0(OnDeviceCloudPolicyManagerDisconnected, void());

  std::unique_ptr<chromeos::InstallAttributes> install_attributes_;

  net::HttpStatusCode url_fetcher_response_code_;
  std::string url_fetcher_response_string_;
  TestingPrefServiceSimple local_state_;
  MockDeviceManagementService device_management_service_;
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  chromeos::FakeSessionManagerClient fake_session_manager_client_;
  chromeos::FakeCryptohomeClient* fake_cryptohome_client_;
  ServerBackedStateKeysBroker state_keys_broker_;
  StrictMock<chromeos::attestation::MockAttestationFlow>* mock_;

  DeviceCloudPolicyStoreChromeOS* store_;
  SchemaRegistry schema_registry_;
  std::unique_ptr<TestingDeviceCloudPolicyManagerChromeOS> manager_;
  std::unique_ptr<DeviceCloudPolicyInitializer> initializer_;
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_loader_factory_;
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
  MockDeviceManagementJob* policy_fetch_job = NULL;
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
      .Times(AtMost(1))
      .WillOnce(device_management_service_.CreateAsyncJob(&policy_fetch_job));
  EXPECT_CALL(device_management_service_,
              StartJob(dm_protocol::kValueRequestPolicy, _, _, _, _, _, _))
      .Times(AtMost(1));
  ConnectManager();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&device_management_service_);
  ASSERT_TRUE(policy_fetch_job);
  // Should create a status uploader for reporting on enrolled devices.
  EXPECT_TRUE(manager_->GetStatusUploader());
  VerifyPolicyPopulated();

  manager_->Shutdown();
  VerifyPolicyPopulated();

  EXPECT_EQ(store_->policy()->service_account_identity(),
            PolicyBuilder::kFakeServiceAccountIdentity);
}

TEST_F(DeviceCloudPolicyManagerChromeOSTest, UnmanagedDevice) {
  device_policy_.policy_data().set_state(em::PolicyData::UNMANAGED);
  device_policy_.Build();
  session_manager_client_.set_device_policy(device_policy_.GetBlob());

  LockDevice();
  FlushDeviceSettings();
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(store_->is_managed());

  // Policy settings should be ignored for UNMANAGED devices.
  PolicyBundle bundle;
  EXPECT_TRUE(manager_->policies().Equals(bundle));

  // Trigger a policy refresh.
  MockDeviceManagementJob* policy_fetch_job = NULL;
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
      .Times(AtMost(1))
      .WillOnce(device_management_service_.CreateAsyncJob(&policy_fetch_job));
  EXPECT_CALL(device_management_service_,
              StartJob(dm_protocol::kValueRequestPolicy, _, _, _, _, _, _))
      .Times(AtMost(1));
  ConnectManager();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&device_management_service_);
  ASSERT_TRUE(policy_fetch_job);
  // Should create a status provider for reporting on enrolled devices, even
  // those that aren't managed.
  EXPECT_TRUE(manager_->GetStatusUploader());

  // Switch back to ACTIVE, service the policy fetch and let it propagate.
  device_policy_.policy_data().set_state(em::PolicyData::ACTIVE);
  device_policy_.Build();
  session_manager_client_.set_device_policy(device_policy_.GetBlob());
  em::DeviceManagementResponse policy_fetch_response;
  policy_fetch_response.mutable_policy_response()->add_response()->CopyFrom(
      device_policy_.policy());
  policy_fetch_job->SendResponse(DM_STATUS_SUCCESS, policy_fetch_response);
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

  ConnectManager();
  EXPECT_TRUE(manager_->policies().Equals(bundle));
  // Should not create a status provider for reporting on consumer devices.
  EXPECT_FALSE(manager_->GetStatusUploader());

  manager_->Shutdown();
  EXPECT_TRUE(manager_->policies().Equals(bundle));
}

TEST_F(DeviceCloudPolicyManagerChromeOSTest, ConnectAndDisconnect) {
  LockDevice();
  FlushDeviceSettings();
  EXPECT_FALSE(manager_->core()->service());  // Not connected.

  // Connect the manager.
  MockDeviceManagementJob* policy_fetch_job = nullptr;
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
      .WillOnce(device_management_service_.CreateAsyncJob(&policy_fetch_job));
  EXPECT_CALL(device_management_service_,
              StartJob(dm_protocol::kValueRequestPolicy, _, _, _, _, _, _));
  EXPECT_CALL(*this, OnDeviceCloudPolicyManagerConnected());
  ConnectManager();
  base::RunLoop().RunUntilIdle();
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
        done_(false) {}

  void SetUp() override {
    DeviceCloudPolicyManagerChromeOSTest::SetUp();

    // Set up test data.
    device_policy_.SetDefaultNewSigningKey();
    device_policy_.policy_data().set_timestamp(
        base::Time::NowFromSystemTime().ToJavaTime());
    device_policy_.Build();

    register_response_.mutable_register_response()->set_device_management_token(
        PolicyBuilder::kFakeToken);
    register_response_.mutable_register_response()->set_enrollment_type(
        em::DeviceRegisterResponse::ENTERPRISE);
    policy_fetch_response_.mutable_policy_response()->add_response()->CopyFrom(
        device_policy_.policy());
    robot_auth_fetch_response_.mutable_service_api_access_response()
        ->set_auth_code("auth_code_for_test");
    loaded_blob_ = device_policy_.GetBlob();

    // Initialize the manager.
    FlushDeviceSettings();
    EXPECT_EQ(CloudPolicyStore::STATUS_BAD_STATE, store_->status());
    EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

    PolicyBundle bundle;
    EXPECT_TRUE(manager_->policies().Equals(bundle));

    ConnectManager();
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
    MockDeviceManagementJob* register_job = NULL;
    EXPECT_CALL(
        device_management_service_,
        CreateJob(with_cert
                      ? DeviceManagementRequestJob::TYPE_CERT_BASED_REGISTRATION
                      : DeviceManagementRequestJob::TYPE_REGISTRATION,
                  _))
        .Times(AtMost(1))
        .WillOnce(device_management_service_.CreateAsyncJob(&register_job));
    EXPECT_CALL(device_management_service_,
                StartJob(with_cert ? dm_protocol::kValueRequestCertBasedRegister
                                   : dm_protocol::kValueRequestRegister,
                         _, _, _, _, _, _))
        .Times(AtMost(1))
        .WillOnce(
            DoAll(SaveArg<5>(&client_id_), SaveArg<6>(&register_request_)));

    chromeos::OwnerSettingsServiceChromeOS* owner_settings_service =
        chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
            profile_.get());
    ASSERT_TRUE(owner_settings_service);

    EnrollmentConfig enrollment_config;
    enrollment_config.auth_mechanism =
        EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;
    enrollment_config.mode = with_cert ? EnrollmentConfig::MODE_ATTESTATION
                                       : EnrollmentConfig::MODE_MANUAL;
    std::unique_ptr<DMAuth> auth =
        with_cert ? DMAuth::NoAuth() : DMAuth::FromOAuthToken("auth token");
    initializer_->PrepareEnrollment(
        &device_management_service_, nullptr, enrollment_config,
        std::move(auth),
        base::Bind(&DeviceCloudPolicyManagerChromeOSEnrollmentTest::Done,
                   base::Unretained(this)));
    initializer_->StartEnrollment();
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&device_management_service_);
    AllowUninterestingRemoteCommandFetches();

    if (done_)
      return;

    // Process registration.
    ASSERT_TRUE(register_job);
    MockDeviceManagementJob* policy_fetch_job = NULL;
    EXPECT_CALL(device_management_service_,
                CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
        .Times(AtMost(1))
        .WillOnce(device_management_service_.CreateAsyncJob(&policy_fetch_job));
    EXPECT_CALL(device_management_service_,
                StartJob(dm_protocol::kValueRequestPolicy, _, _, _, _, _, _))
        .Times(AtMost(1));
    register_job->SendResponse(register_status_, register_response_);
    Mock::VerifyAndClearExpectations(&device_management_service_);
    AllowUninterestingRemoteCommandFetches();

    if (done_)
      return;

    // Process policy fetch.
    ASSERT_TRUE(policy_fetch_job);
    policy_fetch_job->SendResponse(policy_fetch_status_,
                                   policy_fetch_response_);

    if (done_)
      return;

    // Process verification.
    MockDeviceManagementJob* robot_auth_fetch_job = NULL;
    EXPECT_CALL(device_management_service_, CreateJob(
        DeviceManagementRequestJob::TYPE_API_AUTH_CODE_FETCH, _))
        .Times(AtMost(1))
        .WillOnce(device_management_service_.CreateAsyncJob(
            &robot_auth_fetch_job));
    EXPECT_CALL(
        device_management_service_,
        StartJob(dm_protocol::kValueRequestApiAuthorization, _, _, _, _, _, _))
        .Times(AtMost(1));
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&device_management_service_);
    AllowUninterestingRemoteCommandFetches();

    if (done_)
      return;

    // Process robot auth token fetch.
    ASSERT_TRUE(robot_auth_fetch_job);
    robot_auth_fetch_job->SendResponse(robot_auth_fetch_status_,
                                       robot_auth_fetch_response_);
    Mock::VerifyAndClearExpectations(&device_management_service_);
    AllowUninterestingRemoteCommandFetches();

    if (done_)
      return;

    // Process robot refresh token fetch if the auth code fetch succeeded.
    // DeviceCloudPolicyInitializer holds an EnrollmentHandlerChromeOS which
    // holds a GaiaOAuthClient that fetches the refresh token during enrollment.
    // We return a successful OAuth response via a TestURLLoaderFactory to
    // trigger the happy path for these classes so that enrollment can continue.
    if (robot_auth_fetch_status_ == DM_STATUS_SUCCESS) {
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          GaiaUrls::GetInstance()->oauth2_token_url(),
          network::URLLoaderCompletionStatus(net::OK),
          network::CreateResourceResponseHead(url_fetcher_response_code_),
          url_fetcher_response_string_);
    }

    // Process robot refresh token store and policy store.
    base::RunLoop().RunUntilIdle();
    if (!done_ || status_.status() == EnrollmentStatus::SUCCESS) {
      // Verify the state only if the task is not yet failed.
      // Note that, if the flow is not yet |done_| here, assume that it is
      // in the "succeeding" flow, so verify here, too.
      chromeos::DeviceOAuth2TokenService* token_service =
          chromeos::DeviceOAuth2TokenServiceFactory::Get();
      EXPECT_TRUE(token_service->RefreshTokenIsAvailable(
          token_service->GetRobotAccountId()));
      EXPECT_EQ(device_policy_.GetBlob(),
                session_manager_client_.device_policy());
    }
    if (done_)
      return;

    // Process the second policy refresh that happens after the enrollment
    // completes.
    MockDeviceManagementJob* component_policy_fetch_job = NULL;
    EXPECT_CALL(device_management_service_,
                CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
        .Times(AtMost(1))
        .WillOnce(device_management_service_.CreateAsyncJob(
            &component_policy_fetch_job));
    EXPECT_CALL(device_management_service_,
                StartJob(dm_protocol::kValueRequestPolicy, _, _, _, _, _, _))
        .Times(AtMost(1));

    // Key installation and policy load.
    session_manager_client_.set_device_policy(loaded_blob_);
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetNewSigningKey());
    ReloadDeviceSettings();

    // Respond to the second policy refresh.
    if (component_policy_fetch_job) {
      component_policy_fetch_job->SendResponse(policy_fetch_status_,
                                               policy_fetch_response_);
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
      req->CopyFrom(
          register_request_.register_request());
    }
    return req;
  }

  DeviceManagementStatus register_status_;
  em::DeviceManagementResponse register_response_;

  DeviceManagementStatus policy_fetch_status_;
  em::DeviceManagementResponse policy_fetch_response_;

  DeviceManagementStatus robot_auth_fetch_status_;
  em::DeviceManagementResponse robot_auth_fetch_response_;

  std::string loaded_blob_;

  em::DeviceManagementRequest register_request_;
  std::string client_id_;
  EnrollmentStatus status_;

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
  EXPECT_EQ(PolicyBuilder::kFakeDeviceId, client_id_);
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest, RegistrationFailed) {
  register_status_ = DM_STATUS_REQUEST_FAILED;
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
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::ROBOT_REFRESH_FETCH_FAILED);
  EXPECT_EQ(net::HTTP_BAD_REQUEST, status_.http_status());
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest,
       RobotRefreshTokenFetchResponseStringFailed) {
  url_fetcher_response_string_ = "invalid response json";
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::ROBOT_REFRESH_FETCH_FAILED);
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest,
       RobotRefreshEncryptionFailed) {
  // The encryption lib is a noop for tests, but empty results from encryption
  // is an error, so we simulate an encryption error by returning an empty
  // refresh token.
  url_fetcher_response_string_ = "{\"access_token\":\"accessToken4Test\","
                                 "\"expires_in\":1234,"
                                 "\"refresh_token\":\"\"}";
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
  device_policy_.policy().set_policy_data_signature("bad");
  policy_fetch_response_.clear_policy_response();
  policy_fetch_response_.mutable_policy_response()->add_response()->CopyFrom(
      device_policy_.policy());
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::VALIDATION_FAILED);
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_INITIAL_SIGNATURE,
            status_.validation_status());
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest, StoreError) {
  session_manager_client_.set_store_policy_success(false);
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::STORE_ERROR);
  EXPECT_EQ(CloudPolicyStore::STATUS_STORE_ERROR,
            status_.store_status());
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest, LoadError) {
  loaded_blob_.clear();
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::STORE_ERROR);
  EXPECT_EQ(CloudPolicyStore::STATUS_LOAD_ERROR,
            status_.store_status());
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest, UnregisterSucceeds) {
  // Enroll first.
  RunTest();
  ExpectSuccessfulEnrollment();

  // Set up mock objects for the upcoming unregistration job.
  em::DeviceManagementResponse response;
  response.mutable_unregister_response();
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_UNREGISTRATION, _))
      .WillOnce(device_management_service_.SucceedJob(response));
  EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _));
  EXPECT_CALL(*this, OnUnregistered(true));

  // Start unregistering.
  manager_->Unregister(base::Bind(
      &DeviceCloudPolicyManagerChromeOSEnrollmentTest::OnUnregistered,
      base::Unretained(this)));
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest, UnregisterFails) {
  // Enroll first.
  RunTest();
  ExpectSuccessfulEnrollment();

  // Set up mock objects for the upcoming unregistration job.
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_UNREGISTRATION, _))
      .WillOnce(device_management_service_.FailJob(DM_STATUS_REQUEST_FAILED));
  EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _));
  EXPECT_CALL(*this, OnUnregistered(false));

  // Start unregistering.
  manager_->Unregister(base::Bind(
      &DeviceCloudPolicyManagerChromeOSEnrollmentTest::OnUnregistered,
      base::Unretained(this)));
}

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentTest, DisableMachineCertReq) {
  // Simulate the flag --disable-machine-cert-request being provided to Chrome.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kDisableMachineCertRequest);

  // Set expecation that a request for a machine cert is never made.
  EXPECT_CALL(*mock_, GetCertificate(
                          chromeos::attestation::AttestationCertificateProfile::
                              PROFILE_ENTERPRISE_MACHINE_CERTIFICATE,
                          _, _, _, _))
      .Times(0);

  RunTest();
  ExpectSuccessfulEnrollment();
}

// A subclass that runs with a blank system salt.
class DeviceCloudPolicyManagerChromeOSEnrollmentBlankSystemSaltTest
    : public DeviceCloudPolicyManagerChromeOSEnrollmentTest {
 protected:
  DeviceCloudPolicyManagerChromeOSEnrollmentBlankSystemSaltTest() {
    // Set up a FakeCryptohomeClient with a blank system salt.
    fake_cryptohome_client_->set_system_salt(std::vector<uint8_t>());
  }
};

TEST_P(DeviceCloudPolicyManagerChromeOSEnrollmentBlankSystemSaltTest,
       RobotRefreshSaveFailed) {
  // Without the system salt, the robot token can't be stored.
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::ROBOT_REFRESH_STORE_FAILED);
}

INSTANTIATE_TEST_CASE_P(Cert,
                        DeviceCloudPolicyManagerChromeOSEnrollmentTest,
                        ::testing::Values(false, true));

INSTANTIATE_TEST_CASE_P(
    Cert,
    DeviceCloudPolicyManagerChromeOSEnrollmentBlankSystemSaltTest,
    ::testing::Values(false, true));

}  // namespace
}  // namespace policy
