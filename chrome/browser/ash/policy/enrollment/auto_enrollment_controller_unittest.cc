// // Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_controller.h"

#include <memory>
#include <string_view>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/containers/fixed_flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chromeos/ash/components/dbus/device_management/fake_install_attributes_client.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/system_clock/fake_system_clock_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(b/294350413): Cover AutoEnrollmentController with unit tests.

namespace policy {

namespace {

constexpr auto kPortalStateToStateString =
    base::MakeFixedFlatMap<ash::NetworkState::PortalState, std::string_view>(
        {{ash::NetworkState::PortalState::kNoInternet,
          shill::kStateNoConnectivity},
         {ash::NetworkState::PortalState::kOnline, shill::kStateOnline}});

constexpr char kTestStateKey[] = "test-state-key";
constexpr int kMaxRequestStateKeysTries = 10;

constexpr base::TimeDelta kSafeguardTimeout = base::Seconds(90);

class TestingNetwork : public ash::NetworkStateTestHelper {
 public:
  TestingNetwork()
      : ash::NetworkStateTestHelper(
            /*use_default_devices_and_services=*/false),
        wifi_path_(ConfigureWiFi(shill::kStateIdle)) {}

  void GoOnline() { GoState(ash::NetworkState::PortalState::kOnline); }

  void GoOffline() { GoState(ash::NetworkState::PortalState::kNoInternet); }

  void GoState(ash::NetworkState::PortalState state) {
    DCHECK(kPortalStateToStateString.contains(state))
        << "Unknown state: " << state
        << ", add it to kPortalStateToStateString";
    SetServiceProperty(wifi_path_, shill::kStateProperty,
                       base::Value(kPortalStateToStateString.at(state)));
  }

 private:
  std::string wifi_path_;
};

class MockAutoEnrollmentClient {
 public:
  MOCK_METHOD(void, Start, ());
  MOCK_METHOD(void, Retry, ());

  void ReportAutoEnrollmentState(AutoEnrollmentState state) {
    DCHECK(!callback_.is_null());

    callback_.Run(state);
  }

  void SetProgressCallback(AutoEnrollmentClient::ProgressCallback callback) {
    callback_ = std::move(callback);
  }

 private:
  AutoEnrollmentClient::ProgressCallback callback_;
};

class ProxyAutoEnrollmentClient : public AutoEnrollmentClient {
 public:
  explicit ProxyAutoEnrollmentClient(MockAutoEnrollmentClient* mock)
      : mock_(mock) {}
  void Start() override { mock_->Start(); }
  void Retry() override { mock_->Retry(); }

 private:
  raw_ptr<MockAutoEnrollmentClient> mock_{nullptr};
};

class ProxyAutoEnrollmentClientFactory : public AutoEnrollmentClient::Factory {
 public:
  explicit ProxyAutoEnrollmentClientFactory(MockAutoEnrollmentClient* mock)
      : mock_(mock) {}

  std::unique_ptr<AutoEnrollmentClient> CreateForFRE(
      const AutoEnrollmentClient::ProgressCallback& progress_callback,
      DeviceManagementService* device_management_service,
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& server_backed_state_key,
      int power_initial,
      int power_limit) override {
    mock_->SetProgressCallback(progress_callback);
    return std::make_unique<ProxyAutoEnrollmentClient>(mock_);
  }

  std::unique_ptr<AutoEnrollmentClient> CreateForInitialEnrollment(
      const AutoEnrollmentClient::ProgressCallback& progress_callback,
      DeviceManagementService* device_management_service,
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& device_serial_number,
      const std::string& device_brand_code,
      std::unique_ptr<psm::RlweDmserverClient> psm_rlwe_dmserver_client,
      ash::OobeConfiguration* oobe_config) override {
    mock_->SetProgressCallback(progress_callback);
    return std::make_unique<ProxyAutoEnrollmentClient>(mock_);
  }

 private:
  raw_ptr<MockAutoEnrollmentClient> mock_{nullptr};
};

class MockDeviceSettingsService : public ash::DeviceSettingsService {
 public:
  MOCK_METHOD(void,
              GetOwnershipStatusAsync,
              (OwnershipStatusCallback callback),
              (override));
};

class MockStateKeyBroker : public ServerBackedStateKeysBroker {
 public:
  MockStateKeyBroker() : ServerBackedStateKeysBroker(nullptr) {}
  ~MockStateKeyBroker() override = default;

  MOCK_METHOD(void, RequestStateKeys, (StateKeysCallback), (override));
};

class AutoEnrollmentControllerForTesting : public AutoEnrollmentController {
 public:
  template <class... Args>
  explicit AutoEnrollmentControllerForTesting(Args... args)
      : AutoEnrollmentController(std::forward<Args>(args)...) {}

  ~AutoEnrollmentControllerForTesting() override = default;
};

template <typename DbusClient>
class ScopedFakeClientInitializer {
 public:
  ScopedFakeClientInitializer() { DbusClient::InitializeFake(); }

  ~ScopedFakeClientInitializer() { DbusClient::Shutdown(); }
};

void SetDevBootFlag(ash::FakeInstallAttributesClient* client,
                    bool is_disabled) {
  const int fwmp_flags =
      is_disabled ? cryptohome::DEVELOPER_DISABLE_BOOT : cryptohome::NONE;
  ::device_management::SetFirmwareManagementParametersRequest request;
  request.mutable_fwmp()->set_flags(fwmp_flags);
  base::test::TestFuture<
      std::optional<::device_management::SetFirmwareManagementParametersReply>>
      future_fwmp;
  client->SetFirmwareManagementParameters(request, future_fwmp.GetCallback());
  ASSERT_TRUE(future_fwmp.Get());
}

void ClearDevBootFlag(ash::FakeInstallAttributesClient* client) {
  base::test::TestFuture<std::optional<
      ::device_management::RemoveFirmwareManagementParametersReply>>
      future_removed_fwmp;
  client->RemoveFirmwareManagementParameters(
      ::device_management::RemoveFirmwareManagementParametersRequest(),
      future_removed_fwmp.GetCallback());

  ASSERT_TRUE(future_removed_fwmp.Get());
}

}  // namespace

class EnrollmentFwmpHelperTest : public testing::Test {
 protected:
  ash::FakeInstallAttributesClient* install_attributes_client() const {
    return ash::FakeInstallAttributesClient::Get();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment;

  ScopedFakeClientInitializer<ash::FakeInstallAttributesClient>
      scoped_fake_install_attributes_client_initializer_;
};

TEST_F(EnrollmentFwmpHelperTest, NoAvailability) {
  // Fake unavailability of install attributes.
  install_attributes_client()->SetServiceIsAvailable(false);
  install_attributes_client()->ReportServiceIsNotAvailable();

  // Verify that EnrollmentFwmpHelper yields `false`.
  EnrollmentFwmpHelper helper(install_attributes_client());
  base::test::TestFuture<bool> future_result_holder;
  helper.DetermineDevDisableBoot(future_result_holder.GetCallback());
  EXPECT_FALSE(future_result_holder.Get());
}

TEST_F(EnrollmentFwmpHelperTest, NoFwmpParameters) {
  // Fake FWMP starts out unset.
  ClearDevBootFlag(install_attributes_client());
  EnrollmentFwmpHelper helper(install_attributes_client());
  base::test::TestFuture<bool> future_result_holder;
  helper.DetermineDevDisableBoot(future_result_holder.GetCallback());
  EXPECT_FALSE(future_result_holder.Get());
}

TEST_F(EnrollmentFwmpHelperTest, NoDevDisableBoot) {
  // Fake FWMP.dev_disable_boot == 0.
  SetDevBootFlag(install_attributes_client(), /*is_disabled=*/false);

  // Verify that EnrollmentFwmpHelper yields `false`.
  EnrollmentFwmpHelper helper(install_attributes_client());
  base::test::TestFuture<bool> future_result_holder;
  helper.DetermineDevDisableBoot(future_result_holder.GetCallback());
  EXPECT_FALSE(future_result_holder.Get());
}

TEST_F(EnrollmentFwmpHelperTest, DevDisableBoot) {
  // Fake FWMP.dev_disable_boot == 1.
  SetDevBootFlag(install_attributes_client(), /*is_disabled=*/true);

  // Verify that EnrollmentFwmpHelper yields `true`.
  EnrollmentFwmpHelper helper(install_attributes_client());
  base::test::TestFuture<bool> future_result;
  helper.DetermineDevDisableBoot(future_result.GetCallback());
  EXPECT_TRUE(future_result.Get());
}

class AutoEnrollmentControllerBaseTest : public testing::Test {
 protected:
  AutoEnrollmentControllerForTesting CreateController() {
    return AutoEnrollmentControllerForTesting(
        &mock_device_settings_service_, &fake_dm_service_,
        &mock_state_keys_broker_, testing_network_.network_state_handler(),
        std::make_unique<ProxyAutoEnrollmentClientFactory>(
            &mock_auto_enrollment_client_),
        AutoEnrollmentController::RlweClientFactory(),
        EnrollmentStateFetcher::Factory(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
  }

  void RunAndWaitForStateUpdate(AutoEnrollmentController& controller,
                                base::OnceClosure callback) {
    base::test::TestFuture<AutoEnrollmentState> future;
    auto subscription =
        controller.RegisterProgressCallback(future.GetRepeatingCallback());

    std::move(callback).Run();

    ASSERT_TRUE(future.Wait());
  }

  void RunAndExpectNoStateUpdate(AutoEnrollmentController& controller,
                                 base::OnceClosure callback) {
    bool state_updated = false;
    auto subscription =
        controller.RegisterProgressCallback(base::BindLambdaForTesting(
            [&state_updated](AutoEnrollmentState) { state_updated = true; }));

    std::move(callback).Run();
    // We test negative branch (expecting no call) so there is no
    // synchronization point, just wait until all async calls are done.
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(state_updated);
  }

  void SetupUnifiedStateDetermination(bool enabled) {
    const std::string_view switch_value =
        enabled ? AutoEnrollmentTypeChecker::kUnifiedStateDeterminationAlways
                : AutoEnrollmentTypeChecker::kUnifiedStateDeterminationNever;
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnableUnifiedStateDetermination,
        switch_value);
  }

  void SetupForcedReenrollmentCheckType() {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kValid);
    fake_statistics_provider_.SetMachineStatistic(ash::system::kActivateDateKey,
                                                  "activated");
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kCheckEnrollmentKey, "1");

    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnableForcedReEnrollment,
        AutoEnrollmentTypeChecker::kForcedReEnrollmentAlways);
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnrollmentInitialModulus, "10");
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnrollmentModulusLimit, "20");
  }

  void SetupReenrollmentCheckType() {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kValid);
    fake_statistics_provider_.SetMachineStatistic(ash::system::kActivateDateKey,
                                                  "activated");

    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnableForcedReEnrollment,
        AutoEnrollmentTypeChecker::kForcedReEnrollmentAlways);
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnrollmentInitialModulus, "10");
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnrollmentModulusLimit, "20");
  }

  void SetupDevBoot(bool is_disabled) {
    ash::FakeInstallAttributesClient::Get()->SetServiceIsAvailable(true);
    SetDevBootFlag(ash::FakeInstallAttributesClient::Get(), is_disabled);
  }

  void SetupOwnership(ash::DeviceSettingsService::OwnershipStatus status) {
    ON_CALL(mock_device_settings_service_, GetOwnershipStatusAsync)
        .WillByDefault(base::test::RunOnceCallbackRepeatedly<0>(
            ash::DeviceSettingsService::OwnershipStatus::kOwnershipNone));
  }

  void SetupSystemClock(bool synchronized) {
    fake_system_clock_.SetNetworkSynchronized(synchronized);
  }

  void SetupStateKeysAvailable() {
    ON_CALL(mock_state_keys_broker_, RequestStateKeys)
        .WillByDefault(base::test::RunOnceCallbackRepeatedly<0>(
            std::vector<std::string>{kTestStateKey}));
  }

  void SetupCorrectFwmpRemoval() {
    ash::FakeInstallAttributesClient::Get()->SetServiceIsAvailable(true);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingNetwork testing_network_;

  base::test::ScopedCommandLine command_line_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;

  ash::FakeSystemClockClient fake_system_clock_;

  testing::NiceMock<MockDeviceSettingsService> mock_device_settings_service_;

  testing::NiceMock<MockJobCreationHandler> mock_job_creation_handler_;
  FakeDeviceManagementService fake_dm_service_{&mock_job_creation_handler_};

  testing::NiceMock<MockStateKeyBroker> mock_state_keys_broker_;

  testing::NiceMock<MockAutoEnrollmentClient> mock_auto_enrollment_client_;

  network::TestURLLoaderFactory test_url_loader_factory_;

  ash::ScopedStubInstallAttributes fake_install_attributes_{
      ash::StubInstallAttributes::CreateUnset()};
  ScopedFakeClientInitializer<ash::FakeInstallAttributesClient>
      scoped_fake_install_attributes_client_initializer_;

  ScopedFakeClientInitializer<ash::FakeSessionManagerClient>
      scoped_fake_session_manager_client_initializer_;
};

class AutoEnrollmentControllerSafeguardTimeoutTest
    : public AutoEnrollmentControllerBaseTest {
 protected:
  AutoEnrollmentControllerSafeguardTimeoutTest() {
    SetupUnifiedStateDetermination(/*enabled=*/false);
    SetupDevBoot(/*is_disabled=*/false);
    SetupSystemClock(/*synchronized=*/true);
    SetupOwnership(ash::DeviceSettingsService::OwnershipStatus::kOwnershipNone);
  }
};

// Tests that the controller times out with connection error when it performs
// forced re-enrollment check.
TEST_F(AutoEnrollmentControllerSafeguardTimeoutTest,
       ReportsErrorForForcedReenrollment) {
  SetupForcedReenrollmentCheckType();
  // Simulate long running request for state keys by doing nothing. We need to
  // preserve the callback as it holds weak pointer to controller which makes
  // sure that callback is not executed.
  ServerBackedStateKeysBroker::StateKeysCallback state_keys_callback;
  EXPECT_CALL(mock_state_keys_broker_, RequestStateKeys)
      .WillOnce([&state_keys_callback](
                    ServerBackedStateKeysBroker::StateKeysCallback callback) {
        state_keys_callback = std::move(callback);
      });

  auto controller = CreateController();

  EXPECT_FALSE(controller.SafeguardTimerForTesting().IsRunning());

  // Start auto-enrollment check and kick-off the tasks.
  controller.Start();
  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_FALSE(controller.state().has_value());
  EXPECT_TRUE(controller.SafeguardTimerForTesting().IsRunning());

  RunAndWaitForStateUpdate(controller, base::BindLambdaForTesting([this]() {
                             task_environment_.FastForwardBy(kSafeguardTimeout);
                           }));

  EXPECT_EQ(controller.auto_enrollment_check_type(),
            AutoEnrollmentTypeChecker::CheckType::
                kForcedReEnrollmentExplicitlyRequired);
  EXPECT_FALSE(controller.SafeguardTimerForTesting().IsRunning());
  EXPECT_EQ(controller.state(), base::unexpected(AutoEnrollmentError(
                                    AutoEnrollmentSafeguardTimeoutError{})));
}

// Tests that the controller times out with no enrollment state when it performs
// non-forced re-enrollment check.
TEST_F(AutoEnrollmentControllerSafeguardTimeoutTest,
       ReportsNoEnrollmentForReenrollment) {
  SetupReenrollmentCheckType();
  // Simulate long running request for state keys by doing nothing. We need to
  // preserve the callback as it holds weak pointer to controller which makes
  // sure that callback is not executed.
  ServerBackedStateKeysBroker::StateKeysCallback state_keys_callback;
  EXPECT_CALL(mock_state_keys_broker_, RequestStateKeys)
      .WillOnce([&state_keys_callback](
                    ServerBackedStateKeysBroker::StateKeysCallback callback) {
        state_keys_callback = std::move(callback);
      });

  auto controller = CreateController();

  EXPECT_FALSE(controller.SafeguardTimerForTesting().IsRunning());

  // Start auto-enrollment check and kick-off the tasks.
  controller.Start();
  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_FALSE(controller.state().has_value());
  EXPECT_TRUE(controller.SafeguardTimerForTesting().IsRunning());

  RunAndWaitForStateUpdate(controller, base::BindLambdaForTesting([this]() {
                             task_environment_.FastForwardBy(kSafeguardTimeout);
                           }));

  EXPECT_EQ(controller.auto_enrollment_check_type(),
            AutoEnrollmentTypeChecker::CheckType::
                kForcedReEnrollmentImplicitlyRequired);
  EXPECT_FALSE(controller.SafeguardTimerForTesting().IsRunning());
  EXPECT_EQ(controller.state(), AutoEnrollmentResult::kNoEnrollment);
}

// Tests that the controller times out with connection error when runs out of
// attempts to retry state keys retrieval.
TEST_F(AutoEnrollmentControllerSafeguardTimeoutTest,
       TimesOutWithErrorWhenCannotObtainStateKeys) {
  SetupForcedReenrollmentCheckType();
  ServerBackedStateKeysBroker::StateKeysCallback last_state_keys_callback;

  // Capture state keys callback with the last attempt to request state keys.
  EXPECT_CALL(mock_state_keys_broker_, RequestStateKeys)
      .WillOnce([&last_state_keys_callback](
                    ServerBackedStateKeysBroker::StateKeysCallback callback) {
        last_state_keys_callback = std::move(callback);
      });
  // Fail state keys request for all the previous attempts by returning empty
  // state keys vector.
  EXPECT_CALL(mock_state_keys_broker_, RequestStateKeys)
      .Times(kMaxRequestStateKeysTries - 1)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>(std::vector<std::string>{}))
      .RetiresOnSaturation();

  auto controller = CreateController();

  EXPECT_FALSE(controller.SafeguardTimerForTesting().IsRunning());

  // Start auto-enrollment check and kick-off the tasks.
  controller.Start();
  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_EQ(controller.auto_enrollment_check_type(),
            AutoEnrollmentTypeChecker::CheckType::
                kForcedReEnrollmentExplicitlyRequired);
  EXPECT_FALSE(controller.state().has_value());
  EXPECT_TRUE(controller.SafeguardTimerForTesting().IsRunning());
  ASSERT_TRUE(last_state_keys_callback);

  // Run out of state keys attempts and check that timeout is triggered.
  std::move(last_state_keys_callback).Run({});

  EXPECT_FALSE(controller.SafeguardTimerForTesting().IsRunning());
  EXPECT_EQ(controller.state(), base::unexpected(AutoEnrollmentError(
                                    AutoEnrollmentSafeguardTimeoutError{})));
}

class AutoEnrollmentControllerNetworkTest
    : public AutoEnrollmentControllerBaseTest {
 protected:
  AutoEnrollmentControllerNetworkTest() {
    SetupUnifiedStateDetermination(/*enabled=*/false);
    SetupForcedReenrollmentCheckType();
    SetupDevBoot(/*is_disabled=*/false);
    SetupSystemClock(/*synchronized=*/true);
    SetupOwnership(ash::DeviceSettingsService::OwnershipStatus::kOwnershipNone);
    SetupStateKeysAvailable();
  }
};

// Tests that network changes do not trigger auto-enrollment check.
TEST_F(AutoEnrollmentControllerNetworkTest, DoesNotStartWhenGoesOnline) {
  EXPECT_CALL(mock_auto_enrollment_client_, Start).Times(0);
  EXPECT_CALL(mock_auto_enrollment_client_, Retry).Times(0);

  testing_network_.GoOffline();

  auto controller = CreateController();

  EXPECT_FALSE(controller.state().has_value());
  EXPECT_FALSE(controller.SafeguardTimerForTesting().IsRunning());

  testing_network_.GoOnline();

  EXPECT_FALSE(controller.state().has_value());
  EXPECT_FALSE(controller.SafeguardTimerForTesting().IsRunning());
}

// Tests that the controller retries when the network goes online.
TEST_F(AutoEnrollmentControllerNetworkTest, RetriesWhenGoesOnline) {
  testing_network_.GoOnline();
  auto controller = CreateController();

  // Start auto-enrollment check.
  {
    EXPECT_CALL(mock_auto_enrollment_client_, Start).Times(1);
    EXPECT_CALL(mock_auto_enrollment_client_, Retry).Times(0);

    controller.Start();
    task_environment_.FastForwardBy(base::TimeDelta());

    testing::Mock::VerifyAndClearExpectations(&mock_auto_enrollment_client_);
    EXPECT_FALSE(controller.state().has_value());
    EXPECT_TRUE(controller.SafeguardTimerForTesting().IsRunning());
  }

  // Flip-flop the network state and check that retry is triggered.
  {
    EXPECT_CALL(mock_auto_enrollment_client_, Start).Times(0);
    EXPECT_CALL(mock_auto_enrollment_client_, Retry).Times(1);

    testing_network_.GoOffline();
    testing_network_.GoOnline();
    task_environment_.FastForwardBy(base::TimeDelta());

    testing::Mock::VerifyAndClearExpectations(&mock_auto_enrollment_client_);
    EXPECT_FALSE(controller.state().has_value());
    EXPECT_TRUE(controller.SafeguardTimerForTesting().IsRunning());
  }

  // Stop the client with a response error so the controller can retry.
  {
    mock_auto_enrollment_client_.ReportAutoEnrollmentState(
        base::unexpected(AutoEnrollmentStateRetrievalResponseError{}));

    EXPECT_EQ(controller.state(),
              AutoEnrollmentState(base::unexpected(
                  AutoEnrollmentStateRetrievalResponseError{})));
  }

  // Flip-flop the network state and check that retry is triggered.
  {
    EXPECT_CALL(mock_auto_enrollment_client_, Start).Times(0);
    EXPECT_CALL(mock_auto_enrollment_client_, Retry).Times(1);

    RunAndWaitForStateUpdate(
        controller, base::BindLambdaForTesting([this]() {
          testing_network_.GoOffline();
          testing_network_.GoOnline();

          mock_auto_enrollment_client_.ReportAutoEnrollmentState(
              AutoEnrollmentResult::kNoEnrollment);
        }));

    testing::Mock::VerifyAndClearExpectations(&mock_auto_enrollment_client_);
    EXPECT_EQ(controller.state(), AutoEnrollmentResult::kNoEnrollment);
  }
}

}  // namespace policy
