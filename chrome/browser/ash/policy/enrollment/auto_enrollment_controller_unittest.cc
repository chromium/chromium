// // Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_controller.h"

#include <memory>
#include <string_view>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_state_fetcher.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chromeos/ash/components/dbus/device_management/fake_install_attributes_client.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
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

// This exists only to access the protected AutoEnrollmentController
// constructor.
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

}  // namespace

class MockEnrollmentStateFetcher : public EnrollmentStateFetcher {
 public:
  MOCK_METHOD(void, Start, ());

  base::WeakPtr<MockEnrollmentStateFetcher> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockEnrollmentStateFetcher> weak_ptr_factory_{this};
};

class MockEnrollmentStateFetcherFactory {
 public:
  // Create a state fetcher before it is requested by the
  // `AutoEnrollmentController`. This allows interacting with the mocked `Start`
  // method.
  void PreCreate() {
    prepared_state_fetcher_ = std::make_unique<MockEnrollmentStateFetcher>();
    weak_state_fetcher_ = prepared_state_fetcher_->GetWeakPtr();
  }

  // Factory method that can be used by `AutoEnrollmentController`.
  // If a fetcher has been created beforehand, it will be returned. Otherwise a
  // new fetcher will be created.
  std::unique_ptr<EnrollmentStateFetcher> Create(
      base::OnceCallback<void(AutoEnrollmentState)> report_result,
      PrefService* local_state,
      EnrollmentStateFetcher::RlweClientFactory rlwe_client_factory,
      DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      ServerBackedStateKeysBroker* state_key_broker,
      ash::DeviceSettingsService* device_settings_service,
      ash::OobeConfiguration* oobe_configuration) {
    report_result_ = std::move(report_result);
    auto state_fetcher = prepared_state_fetcher_
                             ? std::move(prepared_state_fetcher_)
                             : std::make_unique<MockEnrollmentStateFetcher>();
    weak_state_fetcher_ = state_fetcher->GetWeakPtr();
    return state_fetcher;
  }

  // Get a raw pointer to the current state fetcher.
  // Note that this /could/ be NULL if the fetcher was destroyed.
  MockEnrollmentStateFetcher* Get() { return weak_state_fetcher_.get(); }

  // Run the `report_result_` callback provided by the `Create` method.
  void ReportResult(AutoEnrollmentState state) {
    std::move(report_result_).Run(state);
  }

 private:
  std::unique_ptr<MockEnrollmentStateFetcher> prepared_state_fetcher_;
  base::WeakPtr<MockEnrollmentStateFetcher> weak_state_fetcher_;
  base::OnceCallback<void(AutoEnrollmentState)> report_result_;
};

class AutoEnrollmentControllerTest : public testing::Test {
 protected:
  AutoEnrollmentControllerForTesting CreateController() {
    return AutoEnrollmentControllerForTesting(
        &mock_device_settings_service_, &fake_dm_service_,
        &mock_state_keys_broker_, testing_network_.network_state_handler(),
        AutoEnrollmentController::RlweClientFactory(),
        // The factory will stay alive until the test is destroyed.
        base::BindRepeating(&MockEnrollmentStateFetcherFactory::Create,
                            base::Unretained(&state_fetcher_factory_)),
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

  void SetupUnifiedStateDetermination(bool enabled) {
    const std::string_view switch_value =
        enabled ? AutoEnrollmentTypeChecker::kUnifiedStateDeterminationAlways
                : AutoEnrollmentTypeChecker::kUnifiedStateDeterminationNever;
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnableUnifiedStateDetermination,
        switch_value);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  MockEnrollmentStateFetcherFactory state_fetcher_factory_;

  TestingNetwork testing_network_;

  base::test::ScopedCommandLine command_line_;
  testing::NiceMock<MockDeviceSettingsService> mock_device_settings_service_;

  testing::NiceMock<MockJobCreationHandler> mock_job_creation_handler_;
  FakeDeviceManagementService fake_dm_service_{&mock_job_creation_handler_};

  testing::NiceMock<MockStateKeyBroker> mock_state_keys_broker_;

  network::TestURLLoaderFactory test_url_loader_factory_;

  ScopedFakeClientInitializer<ash::FakeInstallAttributesClient>
      scoped_fake_install_attributes_client_initializer_;

  ScopedFakeClientInitializer<ash::FakeSessionManagerClient>
      scoped_fake_session_manager_client_initializer_;
};

TEST_F(AutoEnrollmentControllerTest, NoEarlyGuestMode) {
  auto controller = CreateController();

  // Guest signin should not be allowed before finishing state determination.
  // In particular, it should not be allowed before even starting state
  // determination.
  EXPECT_FALSE(controller.IsGuestSigninAllowed());
}

// Tests that the controller does not start Unified State Determination if it is
// disabled by command line switch.
TEST_F(AutoEnrollmentControllerTest, NoUsdIfDisabled) {
  SetupUnifiedStateDetermination(/*enabled=*/false);
  auto controller = CreateController();

  EXPECT_FALSE(controller.SafeguardTimerForTesting().IsRunning());

  // Start auto-enrollment check and kick-off the tasks.
  state_fetcher_factory_.PreCreate();
  EXPECT_CALL(*state_fetcher_factory_.Get(), Start).Times(0);
  controller.Start();
}

// Tests that the controller times out with timeout error when unified state
// determination takes too long.
TEST_F(AutoEnrollmentControllerTest, ReportsSafeguardTimeout) {
  SetupUnifiedStateDetermination(/*enabled=*/true);
  auto controller = CreateController();

  EXPECT_FALSE(controller.SafeguardTimerForTesting().IsRunning());

  // Start auto-enrollment check and kick-off the tasks.
  {
    state_fetcher_factory_.PreCreate();
    EXPECT_CALL(*state_fetcher_factory_.Get(), Start).Times(1);
    controller.Start();
  }
  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_FALSE(controller.state().has_value());
  EXPECT_TRUE(controller.SafeguardTimerForTesting().IsRunning());

  RunAndWaitForStateUpdate(controller, base::BindLambdaForTesting([this]() {
                             task_environment_.FastForwardBy(kSafeguardTimeout);
                           }));

  EXPECT_FALSE(controller.SafeguardTimerForTesting().IsRunning());
  EXPECT_EQ(controller.state(), base::unexpected(AutoEnrollmentError(
                                    AutoEnrollmentSafeguardTimeoutError{})));
}

// Tests that the controller forwards
// `AutoEnrollmentResult::kDeviceAlreadyOwned` to registered callbacks and does
// not change block-devmode settings.
TEST_F(AutoEnrollmentControllerTest, ReportsDeviceAlreadyOwned) {
  SetupUnifiedStateDetermination(/*enabled=*/true);
  auto controller = CreateController();

  // Register progress callback to record reported state.
  AutoEnrollmentState result;
  auto subscription =
      controller.RegisterProgressCallback(base::BindLambdaForTesting(
          [&result](AutoEnrollmentState state) { result = state; }));

  // Create mock state fetcher to allow reporting results.
  controller.Start();

  state_fetcher_factory_.ReportResult(
      AutoEnrollmentResult::kDeviceAlreadyOwned);
  task_environment_.FastForwardBy(base::TimeDelta());

  // Expect no changes to block-devmode
  EXPECT_EQ(ash::FakeInstallAttributesClient::Get()
                ->remove_firmware_management_parameters_from_tpm_call_count(),
            0);
  EXPECT_EQ(ash::FakeSessionManagerClient::Get()
                ->clear_block_devmode_vpd_call_count(),
            0);

  // Expect result being passed to progress callbacks unchanged.
  EXPECT_EQ(result, AutoEnrollmentResult::kDeviceAlreadyOwned);
}

// Tests that the controller forwards
// `AutoEnrollmentResult::kNoEnrollment` to registered callbacks and
// clears block-devmode settings.
TEST_F(AutoEnrollmentControllerTest, ReportsNoEnrollment) {
  SetupUnifiedStateDetermination(/*enabled=*/true);
  auto controller = CreateController();

  // Register progress callback to record reported state.
  AutoEnrollmentState result;
  auto subscription =
      controller.RegisterProgressCallback(base::BindLambdaForTesting(
          [&result](AutoEnrollmentState state) { result = state; }));

  // Create mock state fetcher to allow reporting results.
  controller.Start();

  state_fetcher_factory_.ReportResult(AutoEnrollmentResult::kNoEnrollment);
  task_environment_.FastForwardBy(base::TimeDelta());

  // Expect no changes to block-devmode
  EXPECT_EQ(ash::FakeInstallAttributesClient::Get()
                ->remove_firmware_management_parameters_from_tpm_call_count(),
            1);
  EXPECT_EQ(ash::FakeSessionManagerClient::Get()
                ->clear_block_devmode_vpd_call_count(),
            1);

  // Expect result being passed to progress callbacks unchanged.
  EXPECT_EQ(result, AutoEnrollmentResult::kNoEnrollment);
}

// Tests that the Safeguard Timeout is stopped in case state determination
// reports an error, e.g. missing state keys.
TEST_F(AutoEnrollmentControllerTest, TimeoutInterruptedByOtherError) {
  SetupUnifiedStateDetermination(/*enabled=*/true);
  auto controller = CreateController();

  EXPECT_FALSE(controller.SafeguardTimerForTesting().IsRunning());

  // Start auto-enrollment check and kick-off the tasks.
  {
    state_fetcher_factory_.PreCreate();
    EXPECT_CALL(*state_fetcher_factory_.Get(), Start).Times(1);
    controller.Start();
  }
  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_FALSE(controller.state().has_value());
  EXPECT_TRUE(controller.SafeguardTimerForTesting().IsRunning());

  state_fetcher_factory_.ReportResult(
      base::unexpected(AutoEnrollmentStateKeysRetrievalError{}));

  EXPECT_FALSE(controller.SafeguardTimerForTesting().IsRunning());
  EXPECT_EQ(controller.state(), base::unexpected(AutoEnrollmentError(
                                    AutoEnrollmentStateKeysRetrievalError{})));
}

// Tests that network changes do not trigger auto-enrollment check.
TEST_F(AutoEnrollmentControllerTest, DoesNotStartWhenGoesOnline) {
  SetupUnifiedStateDetermination(/*enabled=*/true);
  state_fetcher_factory_.PreCreate();
  EXPECT_CALL(*state_fetcher_factory_.Get(), Start).Times(0);

  testing_network_.GoOffline();

  auto controller = CreateController();

  EXPECT_FALSE(controller.state().has_value());
  EXPECT_FALSE(controller.SafeguardTimerForTesting().IsRunning());

  testing_network_.GoOnline();

  EXPECT_FALSE(controller.state().has_value());
  EXPECT_FALSE(controller.SafeguardTimerForTesting().IsRunning());
}

// Tests that the controller retries after failure when the network goes online.
TEST_F(AutoEnrollmentControllerTest, RetriesWhenGoesOnline) {
  SetupUnifiedStateDetermination(/*enabled=*/true);

  testing_network_.GoOnline();
  auto controller = CreateController();

  // Start auto-enrollment check.
  {
    state_fetcher_factory_.PreCreate();
    EXPECT_CALL(*state_fetcher_factory_.Get(), Start).Times(1);

    controller.Start();
    task_environment_.FastForwardBy(base::TimeDelta());

    EXPECT_FALSE(controller.state().has_value());
    EXPECT_TRUE(controller.SafeguardTimerForTesting().IsRunning());
  }

  // Flip-flop the network state and check that it has no effect as the state
  // fetcher is still running.
  {
    EXPECT_CALL(*state_fetcher_factory_.Get(), Start).Times(0);

    testing_network_.GoOffline();
    testing_network_.GoOnline();
    task_environment_.FastForwardBy(base::TimeDelta());

    EXPECT_FALSE(controller.state().has_value());
    EXPECT_TRUE(controller.SafeguardTimerForTesting().IsRunning());
  }

  // Stop the client with a response error so the controller can retry.
  {
    state_fetcher_factory_.ReportResult(
        base::unexpected(AutoEnrollmentStateRetrievalResponseError{}));

    EXPECT_EQ(controller.state(),
              AutoEnrollmentState(base::unexpected(
                  AutoEnrollmentStateRetrievalResponseError{})));
  }

  // Flip-flop the network state and check that retry is triggered.
  {
    state_fetcher_factory_.PreCreate();
    EXPECT_CALL(*state_fetcher_factory_.Get(), Start).Times(1);

    testing_network_.GoOffline();
    testing_network_.GoOnline();
  }
}

}  // namespace policy
