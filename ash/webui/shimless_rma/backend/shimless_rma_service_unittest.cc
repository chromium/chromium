// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/backend/shimless_rma_service.h"

#include <map>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/rmad/fake_rmad_client.h"
#include "chromeos/dbus/rmad/rmad_client.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_certificate_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-shared.h"
#include "components/onc/onc_constants.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace shimless_rma {

namespace {
using chromeos::DBusMethodCallback;
using chromeos::FakeRmadClient;

class FakeRmadClientForTest : public FakeRmadClient {
 public:
  static void Initialize() {
    // The this pointer is stored in rmad_client.cc:g_instance and deleted when
    // chromeos::RmadClient::Shutdown() is called in
    // ShimlessRmaServiceTest::TearDown()
    new FakeRmadClientForTest();
  }

  FakeRmadClientForTest() = default;
  FakeRmadClientForTest(const FakeRmadClientForTest&) = delete;
  FakeRmadClientForTest& operator=(const FakeRmadClientForTest&) = delete;

  ~FakeRmadClientForTest() override = default;

  void TransitionNextState(
      const rmad::RmadState& state,
      DBusMethodCallback<rmad::GetStateReply> callback) override {
    if (check_state_callback) {
      check_state_callback.Run(state);
    }

    FakeRmadClient::TransitionNextState(state, std::move(callback));
  }

  base::RepeatingCallback<void(const rmad::RmadState& state)>
      check_state_callback;
};

}  // namespace

class ShimlessRmaServiceTest : public testing::Test {
 public:
  ShimlessRmaServiceTest() {
    // InitializeManagedNetworkConfigurationHandler();
    cros_network_config_test_helper().Initialize(
        managed_network_configuration_handler_.get());
    // Wait until |cros_network_config_test_helper_| has initialized.
    base::RunLoop().RunUntilIdle();
  }

  ~ShimlessRmaServiceTest() override {
    base::RunLoop().RunUntilIdle();
    // managed_network_configuration_handler should come first to avoid dcheck
    // on remaining observers.
    managed_network_configuration_handler_.reset();
    network_configuration_handler_.reset();
    network_profile_handler_.reset();
    ui_proxy_config_service_.reset();
  }

  void SetUp() override {
    FakeRmadClientForTest::Initialize();
    rmad_client_ = chromeos::RmadClient::Get();
    // ShimlessRmaService has to be created after RmadClient or there will be a
    // null ptr dereference in the service constructor.
    shimless_rma_provider_ = std::make_unique<ShimlessRmaService>();
  }

  void TearDown() override {
    // ShimlessRmaService has to be shutdown before RmadClient or there will be
    // a null ptr dereference in the service destructor.
    shimless_rma_provider_.reset();
    chromeos::RmadClient::Shutdown();
  }

  rmad::RmadState* CreateState(rmad::RmadState::StateCase state_case) {
    rmad::RmadState* state = new rmad::RmadState();
    switch (state_case) {
      case rmad::RmadState::kWelcome:
        state->set_allocated_welcome(new rmad::WelcomeState());
        break;
      case rmad::RmadState::kComponentsRepair:
        state->set_allocated_components_repair(
            new rmad::ComponentsRepairState());
        break;
      case rmad::RmadState::kDeviceDestination:
        state->set_allocated_device_destination(
            new rmad::DeviceDestinationState());
        break;
      case rmad::RmadState::kWpDisableMethod:
        state->set_allocated_wp_disable_method(
            new rmad::WriteProtectDisableMethodState());
        break;
      case rmad::RmadState::kWpDisableRsu:
        state->set_allocated_wp_disable_rsu(
            new rmad::WriteProtectDisableRsuState());
        break;
      case rmad::RmadState::kWpDisablePhysical:
        state->set_allocated_wp_disable_physical(
            new rmad::WriteProtectDisablePhysicalState());
        break;
      case rmad::RmadState::kWpDisableComplete:
        state->set_allocated_wp_disable_complete(
            new rmad::WriteProtectDisableCompleteState());
        break;
      case rmad::RmadState::kUpdateRoFirmware:
        state->set_allocated_update_ro_firmware(
            new rmad::UpdateRoFirmwareState());
        break;
      case rmad::RmadState::kRestock:
        state->set_allocated_restock(new rmad::RestockState());
        break;
      case rmad::RmadState::kUpdateDeviceInfo:
        state->set_allocated_update_device_info(
            new rmad::UpdateDeviceInfoState());
        break;
      case rmad::RmadState::kCheckCalibration:
        state->set_allocated_check_calibration(
            new rmad::CheckCalibrationState());
        break;
      case rmad::RmadState::kSetupCalibration:
        state->set_allocated_setup_calibration(
            new rmad::SetupCalibrationState());
        break;
      case rmad::RmadState::kRunCalibration:
        state->set_allocated_run_calibration(new rmad::RunCalibrationState());
        break;
      case rmad::RmadState::kProvisionDevice:
        state->set_allocated_provision_device(new rmad::ProvisionDeviceState());
        break;
      case rmad::RmadState::kWpEnablePhysical:
        state->set_allocated_wp_enable_physical(
            new rmad::WriteProtectEnablePhysicalState());
        break;
      case rmad::RmadState::kFinalize:
        state->set_allocated_finalize(new rmad::FinalizeState());
        break;
      default:
        assert(false);
    }
    EXPECT_EQ(state->state_case(), state_case);
    return state;
  }

  rmad::GetStateReply CreateStateReply(rmad::RmadState::StateCase state,
                                       rmad::RmadErrorCode error) {
    rmad::GetStateReply reply;
    reply.set_allocated_state(CreateState(state));
    reply.set_error(error);
    return reply;
  }

  void InitializeManagedNetworkConfigurationHandler() {
    network_profile_handler_ = NetworkProfileHandler::InitializeForTesting();
    network_configuration_handler_ =
        base::WrapUnique<NetworkConfigurationHandler>(
            NetworkConfigurationHandler::InitializeForTest(
                network_state_helper().network_state_handler(),
                cros_network_config_test_helper().network_device_handler()));

    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
    ::onc::RegisterProfilePrefs(user_prefs_.registry());
    ::onc::RegisterPrefs(local_state_.registry());

    ui_proxy_config_service_ = std::make_unique<chromeos::UIProxyConfigService>(
        &user_prefs_, &local_state_,
        network_state_helper().network_state_handler(),
        network_profile_handler_.get());

    managed_network_configuration_handler_ =
        ManagedNetworkConfigurationHandler::InitializeForTesting(
            network_state_helper().network_state_handler(),
            network_profile_handler_.get(),
            cros_network_config_test_helper().network_device_handler(),
            network_configuration_handler_.get(),
            ui_proxy_config_service_.get());

    managed_network_configuration_handler_->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY,
        /*userhash=*/std::string(),
        /*network_configs_onc=*/base::ListValue(),
        /*global_network_config=*/base::DictionaryValue());

    // Wait until the |managed_network_configuration_handler_| is initialized
    // and set up.
    base::RunLoop().RunUntilIdle();
  }

  FakeRmadClientForTest* fake_rmad_client_() {
    return google::protobuf::down_cast<FakeRmadClientForTest*>(rmad_client_);
  }

  void SetupWiFiNetwork() {
    network_state_helper().ConfigureService(
        R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "online",
            "Strength": 50, "AutoConnect": true, "WiFi.HiddenSSID": false})");

    base::RunLoop().RunUntilIdle();
  }

 protected:
  std::unique_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<UIProxyConfigService> ui_proxy_config_service_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;

  network_config::CrosNetworkConfigTestHelper&
  cros_network_config_test_helper() {
    return cros_network_config_test_helper_;
  }

  chromeos::NetworkStateTestHelper& network_state_helper() {
    return cros_network_config_test_helper_.network_state_helper();
  }

  std::unique_ptr<ShimlessRmaService> shimless_rma_provider_;
  chromeos::RmadClient* rmad_client_ = nullptr;  // Unowned convenience pointer.

 private:
  // TaskEnvironment must be declared before CrosNetworkConfigTestHelper so the
  // single threaded test environment is set up when needed.
  base::test::TaskEnvironment task_environment_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_test_helper_{
      false};
};

TEST_F(ShimlessRmaServiceTest, WelcomeHasNetworkConnection) {
  SetupWiFiNetwork();
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kComponentsRepair,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // With a WiFi network it should redirect to kUpdateOs
  shimless_rma_provider_->BeginFinalization(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kUpdateOs);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, WelcomeHasNoNetworkConnection) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kComponentsRepair,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // No network should prompt select network page
  shimless_rma_provider_->BeginFinalization(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kConfigureNetwork);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// TODO(gavindodd): Add tests of transitions back from rmad states through
// the mojom chrome update and network selection states when implemented.

TEST_F(ShimlessRmaServiceTest, GetCurrentState) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(CreateStateReply(rmad::RmadState::kDeviceDestination,
                                         rmad::RMAD_ERROR_OK));
  fake_states.push_back(CreateStateReply(rmad::RmadState::kComponentsRepair,
                                         rmad::RMAD_ERROR_OK));
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetCurrentStateNoRma) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kUnknown);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_RMA_NOT_REQUIRED);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, TransitionNextState) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(CreateStateReply(rmad::RmadState::kDeviceDestination,
                                         rmad::RMAD_ERROR_OK));
  fake_states.push_back(CreateStateReply(rmad::RmadState::kComponentsRepair,
                                         rmad::RMAD_ERROR_OK));
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->TransitionNextState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kSelectComponents);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, TransitionNextStateWithoutCurrentStateInvalid) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(CreateStateReply(rmad::RmadState::kDeviceDestination,
                                         rmad::RMAD_ERROR_OK));
  fake_states.push_back(CreateStateReply(rmad::RmadState::kComponentsRepair,
                                         rmad::RMAD_ERROR_OK));
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->TransitionNextState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, TransitionNextStateWithNoNextStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->TransitionNextState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_TRANSITION_FAILED);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, TransitionPreviousState) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(CreateStateReply(rmad::RmadState::kDeviceDestination,
                                         rmad::RMAD_ERROR_OK));
  fake_states.push_back(CreateStateReply(rmad::RmadState::kComponentsRepair,
                                         rmad::RMAD_ERROR_OK));
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->TransitionNextState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kSelectComponents);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->TransitionPreviousState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest,
       TransitionPreviousStateWithoutCurrentStateFails) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(CreateStateReply(rmad::RmadState::kDeviceDestination,
                                         rmad::RMAD_ERROR_OK));
  fake_states.push_back(CreateStateReply(rmad::RmadState::kComponentsRepair,
                                         rmad::RMAD_ERROR_OK));
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->TransitionPreviousState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_TRANSITION_FAILED);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, TransitionPreviousStateWithNoPrevStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->TransitionPreviousState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_TRANSITION_FAILED);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, CanCancelRma) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->SetAbortable(true);
  base::RunLoop run_loop;
  shimless_rma_provider_->AbortRma(
      base::BindLambdaForTesting([&](rmad::RmadErrorCode error) {
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, CannotCancelRma) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->SetAbortable(false);
  base::RunLoop run_loop;
  shimless_rma_provider_->AbortRma(
      base::BindLambdaForTesting([&](rmad::RmadErrorCode error) {
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_CANNOT_CANCEL_RMA);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetSameOwner) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kRestock, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kDeviceDestination);
        EXPECT_EQ(state.device_destination().destination(),
                  rmad::DeviceDestinationState::RMAD_DESTINATION_SAME);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetSameOwner(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetSameOwnerFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRestock, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetSameOwner(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetDifferentOwner) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kRestock, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kDeviceDestination);
        EXPECT_EQ(state.device_destination().destination(),
                  rmad::DeviceDestinationState::RMAD_DESTINATION_DIFFERENT);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetDifferentOwner(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetDifferentOwnerFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRestock, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->SetDifferentOwner(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetManuallyDisableWriteProtect) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWpDisableMethod, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kWpDisableMethod);
        EXPECT_EQ(
            state.wp_disable_method().disable_method(),
            rmad::WriteProtectDisableMethodState::RMAD_WP_DISABLE_PHYSICAL);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseWriteProtectDisableMethod);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ChooseManuallyDisableWriteProtect(
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, rmad::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
            EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest,
       SetManuallyDisableWriteProtectFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ChooseManuallyDisableWriteProtect(
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, rmad::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
            EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetRsuDisableWriteProtect) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWpDisableMethod, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kWpDisableMethod);
        EXPECT_EQ(state.wp_disable_method().disable_method(),
                  rmad::WriteProtectDisableMethodState::RMAD_WP_DISABLE_RSU);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseWriteProtectDisableMethod);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ChooseRsuDisableWriteProtect(
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, rmad::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
            EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetRsuDisableWriteProtectFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ChooseRsuDisableWriteProtect(
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, rmad::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
            EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetRsuDisableWriteProtectChallenge) {
  rmad::GetStateReply write_protect_disable_rsu_state =
      CreateStateReply(rmad::RmadState::kWpDisableRsu, rmad::RMAD_ERROR_OK);
  write_protect_disable_rsu_state.mutable_state()
      ->mutable_wp_disable_rsu()
      ->set_challenge_code("rsu write protect challenge code");
  std::vector<rmad::GetStateReply> fake_states = {
      write_protect_disable_rsu_state};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kEnterRSUWPDisableCode);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetRsuDisableWriteProtectChallenge(
      base::BindLambdaForTesting([&](const std::string& challenge) {
        EXPECT_EQ("rsu write protect challenge code", challenge);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetRsuDisableWriteProtectCode) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWpDisableRsu, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kWpDisableRsu);
        EXPECT_EQ(state.wp_disable_rsu().unlock_code(), "test RSU unlock code");
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kEnterRSUWPDisableCode);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetRsuDisableWriteProtectCode(
      "test RSU unlock code",
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, rmad::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
            EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest,
       SetRsuDisableWriteProtectCodeFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetRsuDisableWriteProtectCode(
      "test RSU unlock code",
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, rmad::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
            EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetComponentList) {
  rmad::GetStateReply components_repair_state =
      CreateStateReply(rmad::RmadState::kComponentsRepair, rmad::RMAD_ERROR_OK);
  // first component
  rmad::ComponentsRepairState::ComponentRepairStatus* component =
      components_repair_state.mutable_state()
          ->mutable_components_repair()
          ->add_component_repair();
  component->set_component(rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD);
  component->set_repair_status(
      rmad::ComponentsRepairState::ComponentRepairStatus::
          RMAD_REPAIR_STATUS_ORIGINAL);
  // second component
  component = components_repair_state.mutable_state()
                  ->mutable_components_repair()
                  ->add_component_repair();
  component->set_component(rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD);
  component->set_repair_status(
      rmad::ComponentsRepairState::ComponentRepairStatus::
          RMAD_REPAIR_STATUS_REPLACED);
  std::vector<rmad::GetStateReply> fake_states = {
      components_repair_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kSelectComponents);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetComponentList(base::BindLambdaForTesting(
      [&](const std::vector<rmad::ComponentsRepairState::ComponentRepairStatus>&
              components) {
        EXPECT_EQ(2UL, components.size());
        EXPECT_EQ(components[0].component(),
                  rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD);
        EXPECT_EQ(components[0].repair_status(),
                  rmad::ComponentsRepairState::ComponentRepairStatus::
                      RMAD_REPAIR_STATUS_ORIGINAL);
        EXPECT_EQ(components[1].component(),
                  rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD);
        EXPECT_EQ(components[1].repair_status(),
                  rmad::ComponentsRepairState::ComponentRepairStatus::
                      RMAD_REPAIR_STATUS_REPLACED);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetComponentListFromWrongStateEmpty) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetComponentList(base::BindLambdaForTesting(
      [&](const std::vector<rmad::ComponentsRepairState::ComponentRepairStatus>&
              components) {
        EXPECT_EQ(0UL, components.size());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetComponentList) {
  rmad::GetStateReply components_repair_state =
      CreateStateReply(rmad::RmadState::kComponentsRepair, rmad::RMAD_ERROR_OK);
  // first component
  rmad::ComponentsRepairState::ComponentRepairStatus* component =
      components_repair_state.mutable_state()
          ->mutable_components_repair()
          ->add_component_repair();
  component->set_component(rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD);
  component->set_repair_status(
      rmad::ComponentsRepairState::ComponentRepairStatus::
          RMAD_REPAIR_STATUS_ORIGINAL);
  // second component
  component = components_repair_state.mutable_state()
                  ->mutable_components_repair()
                  ->add_component_repair();
  component->set_component(rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD);
  component->set_repair_state(
      rmad::ComponentsRepairState::ComponentRepairStatus::RMAD_REPAIR_ORIGINAL);
  std::vector<rmad::GetStateReply> fake_states = {
      components_repair_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kComponentsRepair);
        EXPECT_EQ(2, state.components_repair().component_repair_size());
        EXPECT_EQ(state.components_repair().component_repair(0).component(),
                  rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD);
        EXPECT_EQ(state.components_repair().component_repair(0).repair_status(),
                  rmad::ComponentsRepairState::ComponentRepairStatus::
                      RMAD_REPAIR_STATUS_REPLACED);
        EXPECT_EQ(state.components_repair().component_repair(1).component(),
                  rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD);
        EXPECT_EQ(state.components_repair().component_repair(1).repair_status(),
                  rmad::ComponentsRepairState::ComponentRepairStatus::
                      RMAD_REPAIR_STATUS_ORIGINAL);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kSelectComponents);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  std::vector<rmad::ComponentsRepairState::ComponentRepairStatus> components(2);
  components[0].set_component(rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD);
  components[0].set_repair_status(
      rmad::ComponentsRepairState::ComponentRepairStatus::
          RMAD_REPAIR_STATUS_REPLACED);
  components[1].set_component(rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD);
  components[1].set_repair_status(
      rmad::ComponentsRepairState::ComponentRepairStatus::
          RMAD_REPAIR_STATUS_ORIGINAL);

  shimless_rma_provider_->SetComponentList(
      std::move(components),
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, rmad::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
            EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetComponentListFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  std::vector<rmad::ComponentsRepairState::ComponentRepairStatus> components(1);
  components[0].set_component(rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD);
  components[0].set_repair_status(
      rmad::ComponentsRepairState::ComponentRepairStatus::
          RMAD_REPAIR_STATUS_REPLACED);

  shimless_rma_provider_->SetComponentList(
      std::move(components),
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, rmad::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
            EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReworkMainboard) {
  rmad::GetStateReply components_repair_state =
      CreateStateReply(rmad::RmadState::kComponentsRepair, rmad::RMAD_ERROR_OK);
  // first component
  rmad::ComponentsRepairState::ComponentRepairStatus* component =
      components_repair_state.mutable_state()
          ->mutable_components_repair()
          ->add_component_repair();
  component->set_component(rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD);
  component->set_repair_status(
      rmad::ComponentsRepairState::ComponentRepairStatus::
          RMAD_REPAIR_STATUS_ORIGINAL);
  // second component
  component = components_repair_state.mutable_state()
                  ->mutable_components_repair()
                  ->add_component_repair();
  component->set_component(rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD);
  component->set_repair_status(
      rmad::ComponentsRepairState::ComponentRepairStatus::
          RMAD_REPAIR_STATUS_ORIGINAL);
  std::vector<rmad::GetStateReply> fake_states = {
      components_repair_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        // TODO(gavindodd): Check the rework_mainboard flag when it is in
        // rmad.proto.
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kSelectComponents);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReworkMainboard(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}  // namespace shimless_rma

TEST_F(ShimlessRmaServiceTest, ReworkMainboardFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReworkMainboard(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReimageRequiredTrue) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kUpdateRoFirmware, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageRequired(
      base::BindLambdaForTesting([&](bool required) {
        EXPECT_EQ(true, required);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReimageRequiredFalse) {
  rmad::GetStateReply update_firmware_state =
      CreateStateReply(rmad::RmadState::kUpdateRoFirmware, rmad::RMAD_ERROR_OK);
  update_firmware_state.mutable_state()
      ->mutable_update_ro_firmware()
      ->set_optional(true);
  std::vector<rmad::GetStateReply> fake_states = {
      update_firmware_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageRequired(
      base::BindLambdaForTesting([&](bool required) {
        EXPECT_EQ(false, required);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReimageRequiredFromWrongStateTrue) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageRequired(
      base::BindLambdaForTesting([&](bool required) {
        EXPECT_EQ(true, required);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReimageSkipped) {
  rmad::GetStateReply update_firmware_state =
      CreateStateReply(rmad::RmadState::kUpdateRoFirmware, rmad::RMAD_ERROR_OK);
  update_firmware_state.mutable_state()
      ->mutable_update_ro_firmware()
      ->set_optional(true);
  std::vector<rmad::GetStateReply> fake_states = {
      update_firmware_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kUpdateRoFirmware);
        EXPECT_EQ(state.update_ro_firmware().update(),
                  rmad::UpdateRoFirmwareState::RMAD_UPDATE_SKIP);
        EXPECT_EQ(state.update_ro_firmware().optional(), true);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageSkipped(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReimageSkippedWhenRequiredFails) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kUpdateRoFirmware, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageSkipped(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReimageSkippedFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageRequired(
      base::BindLambdaForTesting([&](bool required) {
        EXPECT_EQ(true, required);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReimageFromDownload) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kUpdateRoFirmware, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kUpdateRoFirmware);
        EXPECT_EQ(state.update_ro_firmware().update(),
                  rmad::UpdateRoFirmwareState::RMAD_UPDATE_FIRMWARE_DOWNLOAD);
        EXPECT_EQ(state.update_ro_firmware().optional(), false);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageFromDownload(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReimageFromDownloadFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageFromDownload(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReimageFromUsb) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kUpdateRoFirmware, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kUpdateRoFirmware);
        EXPECT_EQ(
            state.update_ro_firmware().update(),
            rmad::UpdateRoFirmwareState::RMAD_UPDATE_FIRMWARE_RECOVERY_UTILITY);
        EXPECT_EQ(state.update_ro_firmware().optional(), false);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageFromUsb(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReimageFromUsbFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageFromUsb(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalSerialNumber) {
  rmad::GetStateReply update_device_info_state =
      CreateStateReply(rmad::RmadState::kUpdateDeviceInfo, rmad::RMAD_ERROR_OK);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->set_original_serial_number("original serial number");
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->set_serial_number("serial number");
  std::vector<rmad::GetStateReply> fake_states = {
      update_device_info_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kUpdateDeviceInformation);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalSerialNumber(
      base::BindLambdaForTesting([&](const std::string& serial_number) {
        EXPECT_EQ("original serial number", serial_number);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalSerialNumberFromWrongStateTrue) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalSerialNumber(
      base::BindLambdaForTesting([&](const std::string& serial_number) {
        EXPECT_EQ("", serial_number);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalRegion) {
  rmad::GetStateReply update_device_info_state =
      CreateStateReply(rmad::RmadState::kUpdateDeviceInfo, rmad::RMAD_ERROR_OK);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->set_original_region_index(3);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->set_region_index(1);
  std::vector<rmad::GetStateReply> fake_states = {
      update_device_info_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kUpdateDeviceInformation);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalRegion(
      base::BindLambdaForTesting([&](uint8_t region) {
        EXPECT_EQ(3, region);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalRegionFromWrongStateTrue) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalRegion(
      base::BindLambdaForTesting([&](uint8_t region) {
        EXPECT_EQ(0, region);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalSku) {
  rmad::GetStateReply update_device_info_state =
      CreateStateReply(rmad::RmadState::kUpdateDeviceInfo, rmad::RMAD_ERROR_OK);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->set_original_sku_index(4);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->set_sku_index(2);
  std::vector<rmad::GetStateReply> fake_states = {
      update_device_info_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kUpdateDeviceInformation);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalSku(
      base::BindLambdaForTesting([&](uint8_t sku) {
        EXPECT_EQ(4, sku);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalSkuFromWrongStateTrue) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalSku(
      base::BindLambdaForTesting([&](uint8_t sku) {
        EXPECT_EQ(0, sku);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetDeviceInformation) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kUpdateDeviceInfo, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kUpdateDeviceInfo);
        EXPECT_EQ(state.update_device_info().serial_number(), "serial number");
        EXPECT_EQ(state.update_device_info().region_index(), 1UL);
        EXPECT_EQ(state.update_device_info().sku_index(), 2UL);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kUpdateDeviceInformation);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetDeviceInformation(
      "serial number", 1, 2,
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, rmad::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
            EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetDeviceInformationFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetDeviceInformation(
      "serial number", 1, 2,
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, rmad::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
            EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, FinalizeAndReboot) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kFinalize, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kFinalize);
        EXPECT_EQ(state.finalize().shutdown(),
                  rmad::FinalizeState::RMAD_FINALIZE_REBOOT);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRepairComplete);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->FinalizeAndReboot(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, FinalizeAndRebootFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->FinalizeAndReboot(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, FinalizeAndShutdown) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kFinalize, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kFinalize);
        EXPECT_EQ(state.finalize().shutdown(),
                  rmad::FinalizeState::RMAD_FINALIZE_SHUTDOWN);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRepairComplete);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->FinalizeAndShutdown(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, FinalizeAndShutdownFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->FinalizeAndShutdown(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, CutoffBattery) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kFinalize, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kFinalize);
        EXPECT_EQ(state.finalize().shutdown(),
                  rmad::FinalizeState::RMAD_FINALIZE_BATERY_CUTOFF);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRepairComplete);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->CutoffBattery(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, CutoffBatteryFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->CutoffBattery(base::BindLambdaForTesting(
      [&](mojom::RmaState state, rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

class FakeErrorObserver : public mojom::ErrorObserver {
 public:
  void OnError(rmad::RmadErrorCode error) override {
    observations.push_back(error);
  }

  std::vector<rmad::RmadErrorCode> observations;
  mojo::Receiver<mojom::ErrorObserver> receiver{this};
};

TEST_F(ShimlessRmaServiceTest, NoErrorObserver) {
  // FakeErrorObserver fake_observer;
  // shimless_rma_provider_->ObserveError(
  //     fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  fake_rmad_client_()->TriggerErrorObservation(
      rmad::RmadErrorCode::RMAD_ERROR_REIMAGING_UNKNOWN_FAILURE);
  run_loop.RunUntilIdle();
  // EXPECT_EQ(fake_observer.observations.size(), 1UL);
}

TEST_F(ShimlessRmaServiceTest, ObserveError) {
  FakeErrorObserver fake_observer;
  shimless_rma_provider_->ObserveError(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  fake_rmad_client_()->TriggerErrorObservation(
      rmad::RmadErrorCode::RMAD_ERROR_REIMAGING_UNKNOWN_FAILURE);
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
}

class FakeCalibrationObserver : public mojom::CalibrationObserver {
 public:
  void OnCalibrationUpdated(
      rmad::CheckCalibrationState::CalibrationStatus::Component component,
      float progress) override {
    observations.push_back(
        std::pair<rmad::CheckCalibrationState::CalibrationStatus::Component,
                  float>(component, progress));
  }

  std::vector<
      std::pair<rmad::CheckCalibrationState::CalibrationStatus::Component,
                float>>
      observations;
  mojo::Receiver<mojom::CalibrationObserver> receiver{this};
};

TEST_F(ShimlessRmaServiceTest, ObserveCalibration) {
  FakeCalibrationObserver fake_observer;
  shimless_rma_provider_->ObserveCalibrationProgress(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  fake_rmad_client_()->TriggerCalibrationProgressObservation(
      rmad::CheckCalibrationState::CalibrationStatus::
          RMAD_CALIBRATION_COMPONENT_ACCELEROMETER,
      0.25);
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
}

class FakeProvisioningObserver : public mojom::ProvisioningObserver {
 public:
  void OnProvisioningUpdated(rmad::ProvisionDeviceState_ProvisioningStep step,
                             float progress) override {
    observations.push_back(
        std::pair<rmad::ProvisionDeviceState_ProvisioningStep, float>(
            step, progress));
  }

  std::vector<std::pair<rmad::ProvisionDeviceState_ProvisioningStep, float>>
      observations;
  mojo::Receiver<mojom::ProvisioningObserver> receiver{this};
};

TEST_F(ShimlessRmaServiceTest, ObserveProvisioning) {
  FakeProvisioningObserver fake_observer;
  shimless_rma_provider_->ObserveProvisioningProgress(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  fake_rmad_client_()->TriggerProvisioningProgressObservation(
      rmad::ProvisionDeviceState::RMAD_PROVISIONING_STEP_IN_PROGRESS, 0.75);
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
}

class FakeHardwareWriteProtectionStateObserver
    : public mojom::HardwareWriteProtectionStateObserver {
 public:
  void OnHardwareWriteProtectionStateChanged(bool enabled) override {
    observations.push_back(enabled);
  }

  std::vector<bool> observations;
  mojo::Receiver<mojom::HardwareWriteProtectionStateObserver> receiver{this};
};

TEST_F(ShimlessRmaServiceTest, ObserveHardwareWriteProtectionState) {
  FakeHardwareWriteProtectionStateObserver fake_observer;
  shimless_rma_provider_->ObserveHardwareWriteProtectionState(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  fake_rmad_client_()->TriggerHardwareWriteProtectionStateObservation(false);
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
}

class FakePowerCableStateObserver : public mojom::PowerCableStateObserver {
 public:
  void OnPowerCableStateChanged(bool enabled) override {
    observations.push_back(enabled);
  }

  std::vector<bool> observations;
  mojo::Receiver<mojom::PowerCableStateObserver> receiver{this};
};

TEST_F(ShimlessRmaServiceTest, ObservePowerCableState) {
  FakePowerCableStateObserver fake_observer;
  shimless_rma_provider_->ObservePowerCableState(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  fake_rmad_client_()->TriggerPowerCableStateObservation(false);
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
}

}  // namespace shimless_rma
}  // namespace ash
