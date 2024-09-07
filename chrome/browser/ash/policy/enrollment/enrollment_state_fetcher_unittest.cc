// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_state_fetcher.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_test_helper.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_test_support.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_device_state.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "private_membership_rlwe.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace policy {

namespace {

namespace em = enterprise_management;

const char kTestStateKey[] = "test-state-key";
const char kTestSerialNumber[] = "test-serial-number";
const char kTestBrandCode[] = "GOOG";
const char kTestPsmId[] = "474F4F47/test-serial-number";
const char kTestDisabledMessage[] = "test-disabled-message";

using base::test::RunOnceCallback;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;

class MockStateKeyBroker : public ServerBackedStateKeysBroker {
 public:
  MockStateKeyBroker() : ServerBackedStateKeysBroker(nullptr) {}
  ~MockStateKeyBroker() override = default;

  MOCK_METHOD(void, RequestStateKeys, (StateKeysCallback), (override));
  MOCK_METHOD(ErrorType, error_type, (), (const, override));
};

class MockDeviceSettingsService : public ash::DeviceSettingsService {
 public:
  MOCK_METHOD(void,
              GetOwnershipStatusAsync,
              (OwnershipStatusCallback callback),
              (override));
};

std::unique_ptr<EnrollmentStateFetcher::RlweClient> CreateRlweClientForTesting(
    const psm::testing::RlweTestCase& test_case,
    private_membership::rlwe::RlweUseCase use_case,
    const private_membership::rlwe::RlwePlaintextId& plaintext_id) {
  // Below we use params from test_case to create the client instead of the
  // values passed to this function to ensure that Query/OPRF requests and
  // responses in EXPECT_CALL invocations match the ones in the test_case.
  // Hence, to test that passed values are correct, we verify them here.
  EXPECT_EQ(use_case, private_membership::rlwe::CROS_DEVICE_STATE_UNIFIED);
  EXPECT_EQ(plaintext_id.sensitive_id(), kTestPsmId);
  EXPECT_EQ(plaintext_id.non_sensitive_id(), kTestPsmId);
  auto client =
      private_membership::rlwe::PrivateMembershipRlweClient::CreateForTesting(
          test_case.use_case(), {test_case.plaintext_id()},
          test_case.ec_cipher_key(), test_case.seed());
  return std::move(client.value());
}

em::DeviceManagementResponse CreatePsmOprfResponse(
    const psm::testing::RlweTestCase& test_case) {
  em::DeviceManagementResponse response;
  auto* rlwe_response = response.mutable_private_set_membership_response()
                            ->mutable_rlwe_response();
  *rlwe_response->mutable_oprf_response() = test_case.oprf_response();
  return response;
}

MATCHER_P(JobWithPsmRlweRequest, rlwe_request_matcher, "") {
  DeviceManagementService::JobConfiguration* config =
      arg.GetConfigurationForTesting();
  if (config->GetType() != DeviceManagementService::JobConfiguration::
                               TYPE_PSM_HAS_DEVICE_STATE_REQUEST) {
    return false;
  }
  em::DeviceManagementRequest request;
  request.ParseFromString(config->GetPayload());
  if (!request.private_set_membership_request().has_rlwe_request()) {
    return false;
  }
  return testing::Matches(rlwe_request_matcher)(
      request.private_set_membership_request().rlwe_request());
}

MATCHER_P3(JobWithStateRequest, state_key, serial_number, brand_code, "") {
  DeviceManagementService::JobConfiguration* config =
      arg.GetConfigurationForTesting();
  if (config->GetType() !=
      DeviceManagementService::JobConfiguration::TYPE_DEVICE_STATE_RETRIEVAL) {
    return false;
  }
  em::DeviceManagementRequest request;
  request.ParseFromString(config->GetPayload());
  if (!request.has_device_state_retrieval_request()) {
    return false;
  }
  const auto& state_request = request.device_state_retrieval_request();
  return state_request.server_backed_state_key() == state_key &&
         state_request.serial_number() == serial_number &&
         state_request.brand_code() == brand_code;
}

MATCHER_P3(JobWithEnrollmentTokenStateRequest,
           enrollment_token,
           serial_number,
           brand_code,
           "") {
  DeviceManagementService::JobConfiguration* config =
      arg.GetConfigurationForTesting();
  if (config->GetType() !=
      DeviceManagementService::JobConfiguration::TYPE_DEVICE_STATE_RETRIEVAL) {
    return false;
  }
  em::DeviceManagementRequest request;
  request.ParseFromString(config->GetPayload());
  if (!request.has_device_state_retrieval_request()) {
    return false;
  }
  const auto& state_request = request.device_state_retrieval_request();
  return state_request.enrollment_token() == enrollment_token &&
         state_request.serial_number() == serial_number &&
         state_request.brand_code() == brand_code;
}

MATCHER(JobWithStateRequestWithNoEnrollmentToken, "") {
  DeviceManagementService::JobConfiguration* config =
      arg.GetConfigurationForTesting();
  if (config->GetType() !=
      DeviceManagementService::JobConfiguration::TYPE_DEVICE_STATE_RETRIEVAL) {
    return false;
  }
  em::DeviceManagementRequest request;
  request.ParseFromString(config->GetPayload());
  if (!request.has_device_state_retrieval_request()) {
    return false;
  }
  return !request.device_state_retrieval_request().has_enrollment_token();
}

MATCHER(WithAnyOprfRequest, "") {
  return arg.has_oprf_request();
}

MATCHER_P(WithOprfRequestFor, test_case, "") {
  return arg.oprf_request().SerializeAsString() ==
         test_case->expected_oprf_request().SerializeAsString();
}

MATCHER_P(WithQueryRequestFor, test_case, "") {
  return arg.query_request().SerializeAsString() ==
         test_case->expected_query_request().SerializeAsString();
}

template <typename Error>
AutoEnrollmentState ToState(Error error) {
  return base::unexpected(error);
}

}  // namespace

class EnrollmentStateFetcherTest : public testing::Test {
 public:
  EnrollmentStateFetcherTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    psm_test_case_ = psm::testing::LoadTestCase(/*is_member=*/true);
    fake_dm_service_ =
        std::make_unique<FakeDeviceManagementService>(&job_creation_handler_);
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnableUnifiedStateDetermination,
        AutoEnrollmentTypeChecker::kUnifiedStateDeterminationAlways);

    DeviceCloudPolicyManagerAsh::RegisterPrefs(local_state_.registry());
    EnrollmentStateFetcher::RegisterPrefs(local_state_.registry());

    statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                             kTestSerialNumber);
    statistics_provider_.SetMachineStatistic(ash::system::kRlzBrandCodeKey,
                                             kTestBrandCode);
  }

  AutoEnrollmentState FetchEnrollmentState() {
    base::test::TestFuture<AutoEnrollmentState> future;
    auto fetcher = EnrollmentStateFetcher::Create(
        future.GetCallback(), &local_state_,
        base::BindRepeating(&CreateRlweClientForTesting, psm_test_case_),
        fake_dm_service_.get(), shared_url_loader_factory_, &state_key_broker_,
        &device_settings_service_,
        enrollment_test_helper_.oobe_configuration());
    fetcher->Start();
    return future.Get();
  }

 protected:
  void ExpectOwnershipCheck(base::TimeDelta time = base::TimeDelta()) {
    EXPECT_CALL(device_settings_service_, GetOwnershipStatusAsync)
        .WillOnce(DoAll(
            InvokeWithoutArgs(
                [=, this]() { task_environment_.AdvanceClock(time); }),
            RunOnceCallback<0>(
                ash::DeviceSettingsService::OwnershipStatus::kOwnershipNone)));
  }

  std::string GetTestStateKey() {
    return AutoEnrollmentTypeChecker::IsFREEnabled() ? kTestStateKey
                                                     : std::string();
  }

  void ExpectStateKeysRequestOrNotDependingOnFRESupport(
      base::TimeDelta time = base::TimeDelta()) {
    if (AutoEnrollmentTypeChecker::IsFREEnabled()) {
      EXPECT_CALL(state_key_broker_, RequestStateKeys)
          .WillOnce(DoAll(
              InvokeWithoutArgs(
                  [=, this]() { task_environment_.AdvanceClock(time); }),
              RunOnceCallback<0>(std::vector<std::string>{kTestStateKey})));
      EXPECT_CALL(state_key_broker_, error_type)
          .WillOnce(Return(ServerBackedStateKeysBroker::ErrorType::kNoError));
    } else {
      EXPECT_CALL(state_key_broker_, RequestStateKeys).Times(0);
    }
  }

  void ExpectOprfRequest(base::TimeDelta time = base::TimeDelta()) {
    EXPECT_CALL(job_creation_handler_,
                OnJobCreation(
                    JobWithPsmRlweRequest(WithOprfRequestFor(&psm_test_case_))))
        .WillOnce(DoAll(InvokeWithoutArgs([=, this]() {
                          task_environment_.AdvanceClock(time);
                        }),
                        fake_dm_service_->SendJobOKAsync(
                            CreatePsmOprfResponse(psm_test_case_))));
  }

  void ExpectQueryRequest(base::TimeDelta time = base::TimeDelta()) {
    em::DeviceManagementResponse response;
    auto* rlwe_response = response.mutable_private_set_membership_response()
                              ->mutable_rlwe_response();
    *rlwe_response->mutable_query_response() = psm_test_case_.query_response();
    EXPECT_CALL(job_creation_handler_,
                OnJobCreation(JobWithPsmRlweRequest(
                    WithQueryRequestFor(&psm_test_case_))))
        .WillOnce(DoAll(InvokeWithoutArgs([=, this]() {
                          task_environment_.AdvanceClock(time);
                        }),
                        fake_dm_service_->SendJobOKAsync(response)));
  }

  void ExpectStateRequest(base::TimeDelta time = base::TimeDelta()) {
    em::DeviceManagementResponse response;
    response.mutable_device_state_retrieval_response();
    EXPECT_CALL(job_creation_handler_,
                OnJobCreation(JobWithStateRequest(
                    GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
        .WillOnce(DoAll(InvokeWithoutArgs([=, this]() {
                          task_environment_.AdvanceClock(time);
                        }),
                        fake_dm_service_->SendJobOKAsync(response)));
  }

  em::DeviceManagementResponse CreateTokenEnrollmentStateResponse() {
    em::DeviceManagementResponse response;
    auto* state_response = response.mutable_device_state_retrieval_response()
                               ->mutable_initial_state_response();
    state_response->set_initial_enrollment_mode(
        em::DeviceInitialEnrollmentStateResponse::
            INITIAL_ENROLLMENT_MODE_TOKEN_ENROLLMENT_ENFORCED);
    return response;
  }

  void ExpectStateRetrievalRequestWithEnrollmentToken(
      const em::DeviceManagementResponse& response) {
    EXPECT_CALL(
        job_creation_handler_,
        OnJobCreation(JobWithEnrollmentTokenStateRequest(
            policy::test::kEnrollmentToken, kTestSerialNumber, kTestBrandCode)))
        .WillOnce(fake_dm_service_->SendJobOKAsync(response));
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedCommandLine command_line_;
  TestingPrefServiceSimple local_state_;
  ash::system::ScopedFakeStatisticsProvider statistics_provider_;
  ash::ScopedStubInstallAttributes install_attributes_;
  MockStateKeyBroker state_key_broker_;
  MockDeviceSettingsService device_settings_service_;
  psm::testing::RlweTestCase psm_test_case_;
  policy::test::EnrollmentTestHelper enrollment_test_helper_{
      &command_line_, &statistics_provider_};

  // Fake URL loader factories.
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  // Fake DMService.
  MockJobCreationHandler job_creation_handler_;
  std::unique_ptr<FakeDeviceManagementService> fake_dm_service_;
};

TEST_F(EnrollmentStateFetcherTest, RegisterPrefs) {
  TestingPrefServiceSimple local_state;
  auto* registry = local_state.registry();

  DeviceCloudPolicyManagerAsh::RegisterPrefs(registry);
  EnrollmentStateFetcher::RegisterPrefs(registry);

  const base::Value* value;
  auto defaults = registry->defaults();
  ASSERT_TRUE(defaults->GetValue(prefs::kServerBackedDeviceState, &value));
  ASSERT_TRUE(value->is_dict());
  EXPECT_TRUE(value->GetDict().empty());
  ASSERT_TRUE(defaults->GetValue(prefs::kEnrollmentPsmResult, &value));
  EXPECT_EQ(value->GetInt(), -1);
  ASSERT_TRUE(
      defaults->GetValue(prefs::kEnrollmentPsmDeterminationTime, &value));
  EXPECT_EQ(value->GetString(), "0");
}

TEST_F(EnrollmentStateFetcherTest, DisabledViaSwitches) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableUnifiedStateDetermination,
      AutoEnrollmentTypeChecker::kUnifiedStateDeterminationNever);

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kNoEnrollment);
}

TEST_F(EnrollmentStateFetcherTest, RlzBrandCodeMissing) {
  base::HistogramTester histograms;
  statistics_provider_.ClearMachineStatistic(ash::system::kRlzBrandCodeKey);

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kNoEnrollment);
  histograms.ExpectUniqueSample(kUMAStateDeterminationDeviceIdentifierStatus,
                                2 /*kRlzBrandCodeMissing*/, 1);
}

TEST_F(EnrollmentStateFetcherTest, SerialNumberMissing) {
  base::HistogramTester histograms;
  statistics_provider_.ClearMachineStatistic(ash::system::kSerialNumberKey);

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kNoEnrollment);
  histograms.ExpectUniqueSample(kUMAStateDeterminationDeviceIdentifierStatus,
                                1 /*kRlzBrandCodeMissing*/, 1);
}

TEST_F(EnrollmentStateFetcherTest, RlzBrandCodeAndSerialNumberMissing) {
  base::HistogramTester histograms;
  statistics_provider_.ClearMachineStatistic(ash::system::kRlzBrandCodeKey);
  statistics_provider_.ClearMachineStatistic(ash::system::kSerialNumberKey);

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kNoEnrollment);
  histograms.ExpectUniqueSample(kUMAStateDeterminationDeviceIdentifierStatus,
                                3 /*kAllMissing*/, 1);
}

TEST_F(EnrollmentStateFetcherTest, OwnershipTaken) {
  EXPECT_CALL(device_settings_service_, GetOwnershipStatusAsync)
      .WillOnce(RunOnceCallback<0>(
          ash::DeviceSettingsService::OwnershipStatus::kOwnershipTaken));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kNoEnrollment);
}

TEST_F(EnrollmentStateFetcherTest, OwnershipUnknown) {
  EXPECT_CALL(device_settings_service_, GetOwnershipStatusAsync)
      .WillOnce(RunOnceCallback<0>(
          ash::DeviceSettingsService::OwnershipStatus::kOwnershipUnknown));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kNoEnrollment);
}

TEST_F(EnrollmentStateFetcherTest, StateKeysMissingDueToCommunicationError) {
  if (!AutoEnrollmentTypeChecker::IsFREEnabled()) {
    // State keys are not requested, this test doesn't apply.
    return;
  }

  base::HistogramTester histograms;
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  EXPECT_CALL(state_key_broker_, RequestStateKeys)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>(std::vector<std::string>{}));
  EXPECT_CALL(state_key_broker_, error_type)
      .WillRepeatedly(
          Return(ServerBackedStateKeysBroker::ErrorType::kCommunicationError));

  const AutoEnrollmentState state = FetchEnrollmentState();

  histograms.ExpectUniqueSample(
      kUMAStateDeterminationStateKeysRetrievalErrorType,
      ServerBackedStateKeysBroker::ErrorType::kCommunicationError, 1);
  EXPECT_EQ(state, ToState(AutoEnrollmentStateKeysRetrievalError{}));
}

TEST_F(EnrollmentStateFetcherTest, StateKeysMissingDueToMissingIdentifiers) {
  if (!AutoEnrollmentTypeChecker::IsFREEnabled()) {
    // State keys are not requested, this test doesn't apply.
    return;
  }

  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  EXPECT_CALL(state_key_broker_, RequestStateKeys)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>(std::vector<std::string>{}));
  EXPECT_CALL(state_key_broker_, error_type)
      .WillRepeatedly(
          Return(ServerBackedStateKeysBroker::ErrorType::kMissingIdentifiers));
  EXPECT_CALL(job_creation_handler_, OnJobCreation(JobWithStateRequest(
                                         /*state_key=*/std::string(),
                                         kTestSerialNumber, kTestBrandCode)))
      .WillOnce(
          fake_dm_service_->SendJobOKAsync(em::DeviceManagementResponse()));

  std::ignore = FetchEnrollmentState();
}

TEST_F(EnrollmentStateFetcherTest, StateKeysRetrievalSucceedOnRetry) {
  if (!AutoEnrollmentTypeChecker::IsFREEnabled()) {
    // State keys are not requested, this test doesn't apply.
    return;
  }

  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  EXPECT_CALL(state_key_broker_, RequestStateKeys)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>{}))
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>{kTestStateKey}));
  EXPECT_CALL(state_key_broker_, error_type)
      .WillOnce(
          Return(ServerBackedStateKeysBroker::ErrorType::kMissingIdentifiers))
      .WillOnce(Return(ServerBackedStateKeysBroker::ErrorType::kNoError));
  ExpectStateRequest();

  std::ignore = FetchEnrollmentState();
}

TEST_F(EnrollmentStateFetcherTest, EmptyOprfResponse) {
  ExpectOwnershipCheck();
  EXPECT_CALL(
      job_creation_handler_,
      OnJobCreation(JobWithPsmRlweRequest(WithOprfRequestFor(&psm_test_case_))))
      .WillOnce(fake_dm_service_->SendJobOKAsync(""));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, ToState(AutoEnrollmentStateAvailabilityResponseError{}));
}

TEST_F(EnrollmentStateFetcherTest, ConnectionErrorOnOprfRequest) {
  ExpectOwnershipCheck();
  EXPECT_CALL(
      job_creation_handler_,
      OnJobCreation(JobWithPsmRlweRequest(WithOprfRequestFor(&psm_test_case_))))
      .WillOnce(fake_dm_service_->SendJobResponseAsync(net::ERR_FAILED, 0));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, ToState(AutoEnrollmentDMServerError{
                       .dm_error = DM_STATUS_REQUEST_FAILED,
                       .network_error = net::ERR_FAILED}));
}

TEST_F(EnrollmentStateFetcherTest, ServerErrorOnOprfRequest) {
  ExpectOwnershipCheck();
  EXPECT_CALL(
      job_creation_handler_,
      OnJobCreation(JobWithPsmRlweRequest(WithOprfRequestFor(&psm_test_case_))))
      .WillOnce(fake_dm_service_->SendJobResponseAsync(
          net::OK, DM_STATUS_HTTP_STATUS_ERROR));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, ToState(AutoEnrollmentDMServerError{
                       .dm_error = DM_STATUS_HTTP_STATUS_ERROR}));
}

TEST_F(EnrollmentStateFetcherTest, FailToCreateQueryRequest) {
  ExpectOwnershipCheck();
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithPsmRlweRequest(WithAnyOprfRequest())))
      .WillOnce(fake_dm_service_->SendJobOKAsync(
          CreatePsmOprfResponse(psm_test_case_)));
  base::test::TestFuture<AutoEnrollmentState> future;
  auto fetcher = EnrollmentStateFetcher::Create(
      future.GetCallback(), &local_state_,
      base::BindRepeating(
          [](private_membership::rlwe::RlweUseCase use_case,
             const private_membership::rlwe::RlwePlaintextId& plaintext_id) {
            return private_membership::rlwe::PrivateMembershipRlweClient::
                CreateForTesting(
                       use_case,
                       // Using fake ID, cipher key and seed will cause failure
                       // to create query request since it won't match the
                       // encrypted ID in OPRF response from the psm_test_case_.
                       {plaintext_id}, "ec_cipher_key",
                       "seed4567890123456789012345678912")
                    .value();
          }),
      fake_dm_service_.get(), shared_url_loader_factory_, &state_key_broker_,
      &device_settings_service_, enrollment_test_helper_.oobe_configuration());

  fetcher->Start();
  AutoEnrollmentState state = future.Get();

  EXPECT_EQ(state, AutoEnrollmentResult::kNoEnrollment);
}

TEST_F(EnrollmentStateFetcherTest, EmptyQueryResponse) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  EXPECT_CALL(job_creation_handler_, OnJobCreation(JobWithPsmRlweRequest(
                                         WithQueryRequestFor(&psm_test_case_))))
      .WillOnce(fake_dm_service_->SendJobOKAsync(""));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, ToState(AutoEnrollmentStateAvailabilityResponseError{}));
}

TEST_F(EnrollmentStateFetcherTest, ConnectionErrorOnQueryRequest) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  EXPECT_CALL(job_creation_handler_, OnJobCreation(JobWithPsmRlweRequest(
                                         WithQueryRequestFor(&psm_test_case_))))
      .WillOnce(fake_dm_service_->SendJobResponseAsync(net::ERR_FAILED, 0));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, ToState(AutoEnrollmentDMServerError{
                       .dm_error = DM_STATUS_REQUEST_FAILED,
                       .network_error = net::ERR_FAILED}));
}

TEST_F(EnrollmentStateFetcherTest, ServerErrorOnQueryRequest) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  EXPECT_CALL(job_creation_handler_, OnJobCreation(JobWithPsmRlweRequest(
                                         WithQueryRequestFor(&psm_test_case_))))
      .WillOnce(fake_dm_service_->SendJobResponseAsync(
          net::OK, DM_STATUS_HTTP_STATUS_ERROR));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, ToState(AutoEnrollmentDMServerError{
                       .dm_error = DM_STATUS_HTTP_STATUS_ERROR}));
}

TEST_F(EnrollmentStateFetcherTest, PsmReportsNoState) {
  base::HistogramTester histograms;
  psm_test_case_ = psm::testing::LoadTestCase(/*is_member=*/false);
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kNoEnrollment);
  histograms.ExpectUniqueSample(kUMAStateDeterminationPsmReportedAvailableState,
                                false, 1);
}

TEST_F(EnrollmentStateFetcherTest, EmptyEnrollmentStateResponse) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(
          fake_dm_service_->SendJobOKAsync(em::DeviceManagementResponse()));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, ToState(AutoEnrollmentStateRetrievalResponseError{}));
}

TEST_F(EnrollmentStateFetcherTest, ConnectionErrorOnEnrollmentStateRequest) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobResponseAsync(net::ERR_FAILED, 0));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, ToState(AutoEnrollmentDMServerError{
                       .dm_error = DM_STATUS_REQUEST_FAILED,
                       .network_error = net::ERR_FAILED}));
}

TEST_F(EnrollmentStateFetcherTest, ServerErrorOnEnrollmentStateRequest) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobResponseAsync(
          0, DM_STATUS_HTTP_STATUS_ERROR));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, ToState(AutoEnrollmentDMServerError{
                       .dm_error = DM_STATUS_HTTP_STATUS_ERROR}));
}

TEST_F(EnrollmentStateFetcherTest, NoEnrollment) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  response.mutable_device_state_retrieval_response();
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kNoEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  EXPECT_TRUE(device_state.empty());
}

TEST_F(EnrollmentStateFetcherTest, UmaHistogramsTimes) {
  base::HistogramTester histograms;
  ExpectOwnershipCheck(/*time=*/base::Seconds(1));
  ExpectOprfRequest(/*time=*/base::Seconds(2));
  ExpectQueryRequest(/*time=*/base::Seconds(3));
  ExpectStateKeysRequestOrNotDependingOnFRESupport(/*time=*/base::Seconds(4));
  ExpectStateRequest(/*time=*/base::Seconds(5));

  std::ignore = FetchEnrollmentState();

  const char* ds = kUMAStateDeterminationTotalDurationByState;
  histograms.ExpectUniqueTimeSample(
      base::StrCat({ds, kUMASuffixNoEnrollment}),
      base::Seconds(AutoEnrollmentTypeChecker::IsFREEnabled() ? 15 : 11), 1);
  histograms.ExpectTotalCount(
      base::StrCat({ds, kUMASuffixStateKeysRetrievalError}), 0);
  histograms.ExpectTotalCount(base::StrCat({ds, kUMASuffixConnectionError}), 0);
  histograms.ExpectTotalCount(base::StrCat({ds, kUMASuffixDisabled}), 0);
  histograms.ExpectTotalCount(base::StrCat({ds, kUMASuffixEnrollment}), 0);
  histograms.ExpectTotalCount(base::StrCat({ds, kUMASuffixServerError}), 0);
  histograms.ExpectTotalCount(kUMAStateDeterminationTotalDuration, 1);

  const char* step_d = kUMAStateDeterminationStepDuration;
  histograms.ExpectUniqueTimeSample(
      base::StrCat({step_d, kUMASuffixOwnershipCheck}), base::Seconds(1), 1);
  histograms.ExpectUniqueTimeSample(
      base::StrCat({step_d, kUMASuffixOPRFRequest}), base::Seconds(2), 1);
  histograms.ExpectUniqueTimeSample(
      base::StrCat({step_d, kUMASuffixQueryRequest}), base::Seconds(3), 1);
  if (AutoEnrollmentTypeChecker::IsFREEnabled()) {
    histograms.ExpectUniqueTimeSample(
        base::StrCat({step_d, kUMASuffixStateKeysRetrieval}), base::Seconds(4),
        1);
  }
  histograms.ExpectUniqueTimeSample(
      base::StrCat({step_d, kUMASuffixStateRequest}), base::Seconds(5), 1);
}

TEST_F(EnrollmentStateFetcherTest, PackagedLicenseWithoutEnrollment) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::INITIAL_ENROLLMENT_MODE_NONE);
  state_response->set_license_packaging_sku(
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          CHROME_TERMINAL);
  state_response->set_is_license_packaged_with_device(true);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kNoEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  EXPECT_FALSE(device_state.FindString(kDeviceStateMode));
  ASSERT_TRUE(device_state.FindBool(kDeviceStatePackagedLicense));
  EXPECT_EQ(*device_state.FindBool(kDeviceStatePackagedLicense), true);
  ASSERT_TRUE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateLicenseType),
            kDeviceStateLicenseTypeTerminal);
}

TEST_F(EnrollmentStateFetcherTest, InitialEnrollmentEnforced) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ENROLLMENT_ENFORCED);
  state_response->set_management_domain("example.org");
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  EXPECT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateInitialModeEnrollmentEnforced);
  ASSERT_TRUE(device_state.FindString(kDeviceStateManagementDomain));
  EXPECT_EQ(*device_state.FindString(kDeviceStateManagementDomain),
            "example.org");
  EXPECT_FALSE(device_state.FindString(kDeviceStateDisabledMessage));
  EXPECT_FALSE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_FALSE(device_state.FindString(kDeviceStatePackagedLicense));
  EXPECT_FALSE(device_state.FindString(kDeviceStateAssignedUpgradeType));
}

TEST_F(EnrollmentStateFetcherTest, InitialEnrollmentDisabled) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_DISABLED);
  state_response->mutable_disabled_state()->set_message(kTestDisabledMessage);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kDisabled);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  EXPECT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateModeDisabled);
  ASSERT_TRUE(device_state.FindString(kDeviceStateDisabledMessage));
  EXPECT_EQ(*device_state.FindString(kDeviceStateDisabledMessage),
            kTestDisabledMessage);
}

TEST_F(EnrollmentStateFetcherTest, ZTEWithPackagedEnterpriseLicense) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED);
  state_response->set_is_license_packaged_with_device(true);
  state_response->set_license_packaging_sku(
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          CHROME_ENTERPRISE);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  EXPECT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateInitialModeEnrollmentZeroTouch);
  ASSERT_TRUE(device_state.FindBool(kDeviceStatePackagedLicense));
  EXPECT_EQ(*device_state.FindBool(kDeviceStatePackagedLicense), true);
  ASSERT_TRUE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateLicenseType),
            kDeviceStateLicenseTypeEnterprise);
}

TEST_F(EnrollmentStateFetcherTest, ZTEWithEducationLicense) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED);
  state_response->set_is_license_packaged_with_device(false);
  state_response->set_license_packaging_sku(
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          CHROME_EDUCATION);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindBool(kDeviceStatePackagedLicense));
  EXPECT_EQ(*device_state.FindBool(kDeviceStatePackagedLicense), false);
  ASSERT_TRUE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateLicenseType),
            kDeviceStateLicenseTypeEducation);
}

TEST_F(EnrollmentStateFetcherTest, ZTEWithTerminalLicense) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED);
  state_response->set_license_packaging_sku(
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          CHROME_TERMINAL);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateLicenseType),
            kDeviceStateLicenseTypeTerminal);
}

TEST_F(EnrollmentStateFetcherTest, ZTEWithUnspecifiedUpgrade) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED);
  state_response->set_assigned_upgrade_type(
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          ASSIGNED_UPGRADE_TYPE_UNSPECIFIED);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateAssignedUpgradeType));
  EXPECT_TRUE(
      device_state.FindString(kDeviceStateAssignedUpgradeType)->empty());
}

TEST_F(EnrollmentStateFetcherTest, ZTEWithChromeEnterpriseUpgrade) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED);
  state_response->set_assigned_upgrade_type(
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          ASSIGNED_UPGRADE_TYPE_CHROME_ENTERPRISE);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateAssignedUpgradeType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateAssignedUpgradeType),
            kDeviceStateAssignedUpgradeTypeChromeEnterprise);
}

TEST_F(EnrollmentStateFetcherTest, ZTEWithKioskAndSignageUpgrade) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED);
  state_response->set_assigned_upgrade_type(
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          ASSIGNED_UPGRADE_TYPE_KIOSK_AND_SIGNAGE);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateAssignedUpgradeType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateAssignedUpgradeType),
            kDeviceStateAssignedUpgradeTypeKiosk);
}

TEST_F(EnrollmentStateFetcherTest, ReEnrollmentRequested) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_REQUESTED);
  state_response->set_management_domain("example.org");
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  EXPECT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateRestoreModeReEnrollmentRequested);
  ASSERT_TRUE(device_state.FindString(kDeviceStateManagementDomain));
  EXPECT_EQ(*device_state.FindString(kDeviceStateManagementDomain),
            "example.org");
  EXPECT_FALSE(device_state.FindString(kDeviceStateDisabledMessage));
  EXPECT_FALSE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_FALSE(device_state.FindString(kDeviceStatePackagedLicense));
  EXPECT_FALSE(device_state.FindString(kDeviceStateAssignedUpgradeType));
}

TEST_F(EnrollmentStateFetcherTest, ReEnrollmentEnforced) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  EXPECT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateRestoreModeReEnrollmentEnforced);
}

TEST_F(EnrollmentStateFetcherTest, ReEnrollmentDisabled) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_DISABLED);
  state_response->mutable_disabled_state()->set_message(kTestDisabledMessage);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kDisabled);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  EXPECT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateModeDisabled);
  ASSERT_TRUE(device_state.FindString(kDeviceStateDisabledMessage));
  EXPECT_EQ(*device_state.FindString(kDeviceStateDisabledMessage),
            kTestDisabledMessage);
}

TEST_F(EnrollmentStateFetcherTest, AutoREWithPerpetualLicense) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ZERO_TOUCH);
  state_response->mutable_license_type()->set_license_type(
      em::LicenseType::CDM_PERPETUAL);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  EXPECT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateRestoreModeReEnrollmentZeroTouch);
  ASSERT_TRUE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateLicenseType),
            kDeviceStateLicenseTypeEnterprise);
}

TEST_F(EnrollmentStateFetcherTest, AutoREWithUndefinedLicense) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ZERO_TOUCH);
  state_response->mutable_license_type()->set_license_type(
      em::LicenseType::UNDEFINED);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_TRUE(device_state.FindString(kDeviceStateLicenseType)->empty());
}

TEST_F(EnrollmentStateFetcherTest, AutoREWithAnnualLicense) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ZERO_TOUCH);
  state_response->mutable_license_type()->set_license_type(
      em::LicenseType::CDM_ANNUAL);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateLicenseType),
            kDeviceStateLicenseTypeEnterprise);
}

TEST_F(EnrollmentStateFetcherTest, AutoREWithKioskLicense) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ZERO_TOUCH);
  state_response->mutable_license_type()->set_license_type(
      em::LicenseType::KIOSK);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateLicenseType),
            kDeviceStateLicenseTypeTerminal);
}

TEST_F(EnrollmentStateFetcherTest, AutoREWithPackagedLicense) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ZERO_TOUCH);
  state_response->mutable_license_type()->set_license_type(
      em::LicenseType::CDM_PACKAGED);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateLicenseType),
            kDeviceStateLicenseTypeEnterprise);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(EnrollmentStateFetcherTest,
       DeviceNotOnFlexDoesNotSendEnrollmentTokenInStateRequest) {
  enrollment_test_helper_.SetUpEnrollmentTokenConfig();
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequestWithNoEnrollmentToken()))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
}

TEST_F(EnrollmentStateFetcherTest,
       EnrollmentTokenPresentWithFREDisabledSkipsStateKeys) {
  enrollment_test_helper_.SetUpFlexDevice();
  enrollment_test_helper_.SetUpEnrollmentTokenConfig();
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  ExpectStateRetrievalRequestWithEnrollmentToken(
      CreateTokenEnrollmentStateResponse());

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  ASSERT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateInitialModeTokenEnrollment);
  EXPECT_FALSE(device_state.FindString(kDeviceStateModeDisabled));
  EXPECT_FALSE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_FALSE(device_state.FindString(kDeviceStatePackagedLicense));
  EXPECT_FALSE(device_state.FindString(kDeviceStateAssignedUpgradeType));
}

TEST_F(EnrollmentStateFetcherTest,
       EnrollmentTokenPresentWithFREEnabledRetrievesStateKeys) {
  enrollment_test_helper_.EnableFREOnFlex();
  enrollment_test_helper_.SetUpFlexDevice();
  enrollment_test_helper_.SetUpEnrollmentTokenConfig();
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  ExpectStateRetrievalRequestWithEnrollmentToken(
      CreateTokenEnrollmentStateResponse());

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  ASSERT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateInitialModeTokenEnrollment);
}

TEST_F(EnrollmentStateFetcherTest,
       EnrollmentTokenPresentNoPsmStateStillDoesStateRetrieval) {
  psm_test_case_ = psm::testing::LoadTestCase(false);
  enrollment_test_helper_.SetUpFlexDevice();
  enrollment_test_helper_.SetUpEnrollmentTokenConfig();
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  ExpectStateRetrievalRequestWithEnrollmentToken(
      CreateTokenEnrollmentStateResponse());

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  ASSERT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateInitialModeTokenEnrollment);
}

TEST_F(EnrollmentStateFetcherTest,
       EnrollmentTokenPresentServerRespondsWithKioskUpgradeType) {
  enrollment_test_helper_.SetUpFlexDevice();
  enrollment_test_helper_.SetUpEnrollmentTokenConfig();
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_TOKEN_ENROLLMENT_ENFORCED);
  state_response->set_assigned_upgrade_type(
      em::DeviceInitialEnrollmentStateResponse::
          ASSIGNED_UPGRADE_TYPE_KIOSK_AND_SIGNAGE);
  ExpectStateRetrievalRequestWithEnrollmentToken(response);

  const AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  ASSERT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateInitialModeTokenEnrollment);
  ASSERT_EQ(*device_state.FindString(kDeviceStateAssignedUpgradeType),
            kDeviceStateAssignedUpgradeTypeKiosk);
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// An enum for the kind of Chromium OS running on the device.
enum class DeviceOs { Chrome = 0, Flex = 1 };

// This is parameterized by device OS.
class EnrollmentStateFetcherTestP
    : public EnrollmentStateFetcherTest,
      public testing::WithParamInterface<DeviceOs> {
 public:
  void SetUp() override {
    EnrollmentStateFetcherTest::SetUp();
    if (device_os_ == DeviceOs::Flex) {
      enrollment_test_helper_.SetUpFlexDevice();
      enrollment_test_helper_.EnableFREOnFlex();
    }
  }

 protected:
  const DeviceOs device_os_ = GetParam();
};

TEST_P(EnrollmentStateFetcherTestP, ReEnrollmentRequested) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_REQUESTED);
  state_response->set_management_domain("example.org");
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  EXPECT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateRestoreModeReEnrollmentRequested);
  ASSERT_TRUE(device_state.FindString(kDeviceStateManagementDomain));
  EXPECT_EQ(*device_state.FindString(kDeviceStateManagementDomain),
            "example.org");
  EXPECT_FALSE(device_state.FindString(kDeviceStateDisabledMessage));
  EXPECT_FALSE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_FALSE(device_state.FindString(kDeviceStatePackagedLicense));
  EXPECT_FALSE(device_state.FindString(kDeviceStateAssignedUpgradeType));
}

TEST_P(EnrollmentStateFetcherTestP, ReEnrollmentEnforced) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  EXPECT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateRestoreModeReEnrollmentEnforced);
}

TEST_P(EnrollmentStateFetcherTestP, ReEnrollmentDisabled) {
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_DISABLED);
  state_response->mutable_disabled_state()->set_message(kTestDisabledMessage);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentResult::kDisabled);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  EXPECT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateModeDisabled);
  ASSERT_TRUE(device_state.FindString(kDeviceStateDisabledMessage));
  EXPECT_EQ(*device_state.FindString(kDeviceStateDisabledMessage),
            kTestDisabledMessage);
}

TEST_P(EnrollmentStateFetcherTestP, UmaHistogramsCounts) {
  base::HistogramTester histograms;
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  ExpectStateRequest();

  std::ignore = FetchEnrollmentState();

  histograms.ExpectUniqueSample(kUMAStateDeterminationDeviceIdentifierStatus,
                                0 /*kAllPresent*/, 1);
  histograms.ExpectUniqueSample(kUMAStateDeterminationEnabled, true, 1);
  histograms.ExpectUniqueSample(kUMAStateDeterminationOnFlex,
                                device_os_ == DeviceOs::Flex, 1);
  histograms.ExpectUniqueSample(
      kUMAStateDeterminationOwnershipStatus,
      ash::DeviceSettingsService::OwnershipStatus::kOwnershipNone, 1);
  histograms.ExpectUniqueSample(
      kUMAStateDeterminationStateKeysRetrievalErrorType,
      ServerBackedStateKeysBroker::ErrorType::kNoError, 1);
  histograms.ExpectUniqueSample(
      kUMAStateDeterminationPsmRlweOprfRequestDmStatusCode, DM_STATUS_SUCCESS,
      1);
  histograms.ExpectUniqueSample(
      kUMAStateDeterminationPsmRlweOprfRequestNetworkErrorCode, net::OK, 1);
  histograms.ExpectUniqueSample(
      kUMAStateDeterminationPsmRlweQueryRequestDmStatusCode, DM_STATUS_SUCCESS,
      1);
  histograms.ExpectUniqueSample(
      kUMAStateDeterminationPsmRlweQueryRequestNetworkErrorCode, net::OK, 1);
  histograms.ExpectUniqueSample(kUMAStateDeterminationPsmReportedAvailableState,
                                true, 1);
  histograms.ExpectUniqueSample(kUMAStateDeterminationStateRequestDmStatusCode,
                                DM_STATUS_SUCCESS, 1);
  histograms.ExpectUniqueSample(
      kUMAStateDeterminationStateRequestNetworkErrorCode, net::OK, 1);
  histograms.ExpectUniqueSample(kUMAStateDeterminationStateReturned, true, 1);
  histograms.ExpectUniqueSample(
      base::StrCat(
          {kUMAStateDeterminationIsInitialByState, kUMASuffixNoEnrollment}),
      true, 1);
}

TEST_P(EnrollmentStateFetcherTestP, UmaHistogramsCountsOnReEnrollmentDisabled) {
  base::HistogramTester histograms;
  ExpectOwnershipCheck();
  ExpectOprfRequest();
  ExpectQueryRequest();
  ExpectStateKeysRequestOrNotDependingOnFRESupport();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_DISABLED);
  state_response->mutable_disabled_state()->set_message(kTestDisabledMessage);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  GetTestStateKey(), kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  std::ignore = FetchEnrollmentState();

  histograms.ExpectUniqueSample(
      base::StrCat(
          {kUMAStateDeterminationIsInitialByState, kUMASuffixDisabled}),
      false, 1);
}

INSTANTIATE_TEST_SUITE_P(EnrollmentStateFetcherTestSuite,
                         EnrollmentStateFetcherTestP,
                         testing::Values(DeviceOs::Chrome, DeviceOs::Flex));

}  // namespace policy
