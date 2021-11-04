// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/backend/shimless_rma_service.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/rmad/fake_rmad_client.h"
#include "chromeos/dbus/rmad/rmad_client.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
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
  ShimlessRmaServiceTest() {}

  ~ShimlessRmaServiceTest() override {}

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    cros_network_config_test_helper_ =
        std::make_unique<network_config::CrosNetworkConfigTestHelper>(false);
    cros_network_config_test_helper().Initialize(nullptr);
    NetworkHandler::Initialize();

    FakeRmadClientForTest::Initialize();
    rmad_client_ = chromeos::RmadClient::Get();
    // ShimlessRmaService has to be created after RmadClient or there will be a
    // null ptr dereference in the service constructor.
    shimless_rma_provider_ = std::make_unique<ShimlessRmaService>();

    // Wait until |cros_network_config_test_helper_| has initialized.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    // ShimlessRmaService has to be shutdown before RmadClient or there will be
    // a null ptr dereference in the service destructor.
    shimless_rma_provider_.reset();
    chromeos::RmadClient::Shutdown();
    NetworkHandler::Shutdown();
    cros_network_config_test_helper_.reset();
    chromeos::DBusThreadManager::Shutdown();
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
      case rmad::RmadState::kRepairComplete:
        state->set_allocated_repair_complete(new rmad::RepairCompleteState());
        break;
      default:
        assert(false);
    }
    EXPECT_EQ(state->state_case(), state_case);
    return state;
  }

  rmad::GetStateReply CreateStateReply(rmad::RmadState::StateCase state,
                                       rmad::RmadErrorCode error,
                                       bool can_abort = false,
                                       bool can_go_back = false) {
    rmad::GetStateReply reply;
    reply.set_allocated_state(CreateState(state));
    reply.set_can_abort(can_abort);
    reply.set_can_go_back(can_go_back);
    reply.set_error(error);
    return reply;
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
  network_config::CrosNetworkConfigTestHelper&
  cros_network_config_test_helper() {
    return *cros_network_config_test_helper_;
  }

  chromeos::NetworkStateTestHelper& network_state_helper() {
    return cros_network_config_test_helper_->network_state_helper();
  }

  std::unique_ptr<ShimlessRmaService> shimless_rma_provider_;
  chromeos::RmadClient* rmad_client_ = nullptr;  // Unowned convenience pointer.

 private:
  std::unique_ptr<network_config::CrosNetworkConfigTestHelper>
      cros_network_config_test_helper_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ShimlessRmaServiceTest, AbortAndGoBackStatePassedCorrectly) {
  SetupWiFiNetwork();
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK,
                       /*can_abort*/ true, /*can_go_back*/ false),
      CreateStateReply(rmad::RmadState::kComponentsRepair, rmad::RMAD_ERROR_OK,
                       /*can_abort*/ false, /*can_go_back*/ true)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state, can_cancel=true, can_go_back=false
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        EXPECT_EQ(can_cancel, true);
        EXPECT_EQ(can_go_back, false);
      }));
  run_loop.RunUntilIdle();

  // Next state, can_cancel=false, can_go_back=true
  shimless_rma_provider_->SetSameOwner(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kSelectComponents);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        EXPECT_EQ(can_cancel, false);
        EXPECT_EQ(can_go_back, true);
      }));
  run_loop.RunUntilIdle();
  // Previous state, can_cancel=true, can_go_back=false
  shimless_rma_provider_->TransitionPreviousState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        EXPECT_EQ(can_cancel, true);
        EXPECT_EQ(can_go_back, false);
        run_loop.Quit();
      }));

  run_loop.Run();
}

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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // With a WiFi network it should redirect to kUpdateOs
  shimless_rma_provider_->BeginFinalization(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // TODO(gavindodd): Create a FakeVersionUpdater so no updates available and
  // update progress can be tested.

  // No network should prompt select network page
  shimless_rma_provider_->BeginFinalization(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kConfigureNetwork);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ChooseNetworkHasNetworkConnection) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kComponentsRepair,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // No network should prompt select network page
  shimless_rma_provider_->BeginFinalization(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kConfigureNetwork);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  SetupWiFiNetwork();

  // With a WiFi network it should redirect to kUpdateOs
  shimless_rma_provider_->NetworkSelectionComplete(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kUpdateOs);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ChooseNetworkHasNoNetworkConnection) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kComponentsRepair,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // No network should prompt select network page
  shimless_rma_provider_->BeginFinalization(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kConfigureNetwork);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // With no network it should redirect to next rmad state
  shimless_rma_provider_->NetworkSelectionComplete(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kSelectComponents);
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kUnknown);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_RMA_NOT_REQUIRED);
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->SetSameOwner(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kSelectComponents);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->TransitionPreviousState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->TransitionPreviousState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetSameOwner(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetSameOwner(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetDifferentOwner(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->SetDifferentOwner(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseWriteProtectDisableMethod);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ChooseManuallyDisableWriteProtect(
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ChooseManuallyDisableWriteProtect(
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseWriteProtectDisableMethod);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ChooseRsuDisableWriteProtect(
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ChooseRsuDisableWriteProtect(
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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

TEST_F(ShimlessRmaServiceTest, GetRsuDisableWriteProtectHwid) {
  rmad::GetStateReply write_protect_disable_rsu_state =
      CreateStateReply(rmad::RmadState::kWpDisableRsu, rmad::RMAD_ERROR_OK);
  write_protect_disable_rsu_state.mutable_state()
      ->mutable_wp_disable_rsu()
      ->set_hwid("rsu write protect hwid");
  std::vector<rmad::GetStateReply> fake_states = {
      write_protect_disable_rsu_state};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kEnterRSUWPDisableCode);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetRsuDisableWriteProtectHwid(
      base::BindLambdaForTesting([&](const std::string& hwid) {
        EXPECT_EQ("rsu write protect hwid", hwid);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetRsuDisableWriteProtectChallengeQrCode) {
  rmad::GetStateReply write_protect_disable_rsu_state =
      CreateStateReply(rmad::RmadState::kWpDisableRsu, rmad::RMAD_ERROR_OK);
  write_protect_disable_rsu_state.mutable_state()
      ->mutable_wp_disable_rsu()
      ->set_challenge_url("https://challenge/url");
  std::vector<rmad::GetStateReply> fake_states = {
      write_protect_disable_rsu_state};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kEnterRSUWPDisableCode);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  std::vector<uint8_t> expected_qrcode_data = {
      1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1,
      1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1,
      1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 1,
      1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1,
      1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1,
      1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
      1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1,
      0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
      1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1,
      1, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0,
      0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1,
      1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1,
      1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
      1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0,
      1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1,
      1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0, 1, 1, 0, 1,
      1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0,
      1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1,
      1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1,
      1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1,
      1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1,
      1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1,
      1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1,
  };
  shimless_rma_provider_->GetRsuDisableWriteProtectChallengeQrCode(
      base::BindLambdaForTesting([&](mojom::QrCodePtr qrcode) {
        EXPECT_FALSE(qrcode.is_null());
        EXPECT_EQ(qrcode->size, 25);
        EXPECT_EQ(qrcode->data.size(), 25UL * 25UL);
        EXPECT_EQ(qrcode->data.size(), expected_qrcode_data.size());
        EXPECT_EQ(qrcode->data, expected_qrcode_data);
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kEnterRSUWPDisableCode);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetRsuDisableWriteProtectCode(
      "test RSU unlock code",
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetRsuDisableWriteProtectCode(
      "test RSU unlock code",
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, WriteProtectManuallyDisabled) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWpDisablePhysical,
                       rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWaitForManualWPDisable);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->WriteProtectManuallyDisabled(
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest,
       WriteProtectManuallyDisabledFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->WriteProtectManuallyDisabled(
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ConfirmManualWpDisableComplete) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWpDisableComplete,
                       rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWPDisableComplete);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ConfirmManualWpDisableComplete(
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest,
       ConfirmManualWpDisableCompleteFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ConfirmManualWpDisableComplete(
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
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
          ->add_components();
  component->set_component(rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD);
  component->set_repair_status(
      rmad::ComponentsRepairState::ComponentRepairStatus::
          RMAD_REPAIR_STATUS_ORIGINAL);
  // second component
  component = components_repair_state.mutable_state()
                  ->mutable_components_repair()
                  ->add_components();
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
          ->add_components();
  component->set_component(rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD);
  component->set_repair_status(
      rmad::ComponentsRepairState::ComponentRepairStatus::
          RMAD_REPAIR_STATUS_ORIGINAL);
  // second component
  component = components_repair_state.mutable_state()
                  ->mutable_components_repair()
                  ->add_components();
  component->set_component(rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD);
  component->set_repair_status(
      rmad::ComponentsRepairState::ComponentRepairStatus::
          RMAD_REPAIR_STATUS_ORIGINAL);
  // This is not the expected state from rmad, but allows us to confirm the flag
  // is cleared by SetComponentList
  components_repair_state.mutable_state()
      ->mutable_components_repair()
      ->set_mainboard_rework(true);
  std::vector<rmad::GetStateReply> fake_states = {
      components_repair_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kComponentsRepair);
        EXPECT_EQ(2, state.components_repair().components_size());
        EXPECT_EQ(state.components_repair().components(0).component(),
                  rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD);
        EXPECT_EQ(state.components_repair().components(0).repair_status(),
                  rmad::ComponentsRepairState::ComponentRepairStatus::
                      RMAD_REPAIR_STATUS_REPLACED);
        EXPECT_EQ(state.components_repair().components(1).component(),
                  rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD);
        EXPECT_EQ(state.components_repair().components(1).repair_status(),
                  rmad::ComponentsRepairState::ComponentRepairStatus::
                      RMAD_REPAIR_STATUS_ORIGINAL);
        EXPECT_EQ(state.components_repair().mainboard_rework(), false);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
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
          ->add_components();
  component->set_component(rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD);
  component->set_repair_status(
      rmad::ComponentsRepairState::ComponentRepairStatus::
          RMAD_REPAIR_STATUS_ORIGINAL);
  // second component
  component = components_repair_state.mutable_state()
                  ->mutable_components_repair()
                  ->add_components();
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
        EXPECT_EQ(state.state_case(), rmad::RmadState::kComponentsRepair);
        EXPECT_EQ(state.components_repair().mainboard_rework(), true);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kSelectComponents);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReworkMainboard(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReworkMainboard(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageSkipped(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageSkipped(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageFromDownload(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageFromDownload(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageFromUsb(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageFromUsb(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ShutdownForRestock) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRestock, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kRestock);
        EXPECT_EQ(state.restock().choice(),
                  rmad::RestockState::RMAD_RESTOCK_SHUTDOWN_AND_RESTOCK);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ShutdownForRestock(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ShutdownForRestockFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ShutdownForRestock(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ContinueFinalizationAfterRestock) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRestock, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kRestock);
        EXPECT_EQ(state.restock().choice(),
                  rmad::RestockState::RMAD_RESTOCK_CONTINUE_RMA);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ContinueFinalizationAfterRestock(
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest,
       ContinueFinalizationAfterRestockFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ContinueFinalizationAfterRestock(
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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

TEST_F(ShimlessRmaServiceTest, GetOriginalSerialNumberFromWrongStateEmpty) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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

TEST_F(ShimlessRmaServiceTest, GetOriginalRegionFromWrongStateEmpty) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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

TEST_F(ShimlessRmaServiceTest, GetOriginalSkuFromWrongStateEmpty) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kUpdateDeviceInformation);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetDeviceInformation(
      "serial number", 1, 2,
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
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
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetDeviceInformation(
      "serial number", 1, 2,
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetCalibrationComponentList) {
  rmad::GetStateReply check_calibration_state =
      CreateStateReply(rmad::RmadState::kCheckCalibration, rmad::RMAD_ERROR_OK);
  rmad::CalibrationComponentStatus* component =
      check_calibration_state.mutable_state()
          ->mutable_check_calibration()
          ->add_components();
  component->set_component(
      rmad::RmadComponent::RMAD_COMPONENT_BASE_ACCELEROMETER);
  component->set_status(
      rmad::CalibrationComponentStatus::RMAD_CALIBRATION_IN_PROGRESS);
  component->set_progress(0.5);
  std::vector<rmad::GetStateReply> fake_states = {
      check_calibration_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  EXPECT_EQ(
      check_calibration_state.state().check_calibration().components_size(), 1);

  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kCheckCalibration);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetCalibrationComponentList(
      base::BindLambdaForTesting(
          [&](const std::vector<rmad::CalibrationComponentStatus>& components) {
            EXPECT_EQ(components.size(), 1u);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetCalibrationComponentListWrongStateEmpty) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetCalibrationComponentList(
      base::BindLambdaForTesting(
          [&](const std::vector<rmad::CalibrationComponentStatus>& components) {
            EXPECT_EQ(components.size(), 0u);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetCalibrationSetupInstructions) {
  rmad::GetStateReply setup_calibration_state =
      CreateStateReply(rmad::RmadState::kSetupCalibration, rmad::RMAD_ERROR_OK);
  setup_calibration_state.mutable_state()
      ->mutable_setup_calibration()
      ->set_instruction(
          rmad::CalibrationSetupInstruction::
              RMAD_CALIBRATION_INSTRUCTION_PLACE_BASE_ON_FLAT_SURFACE);
  std::vector<rmad::GetStateReply> fake_states = {
      setup_calibration_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kSetupCalibration);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetCalibrationSetupInstructions(
      base::BindLambdaForTesting(
          [&](rmad::CalibrationSetupInstruction instruction) {
            EXPECT_EQ(
                instruction,
                rmad::CalibrationSetupInstruction::
                    RMAD_CALIBRATION_INSTRUCTION_PLACE_BASE_ON_FLAT_SURFACE);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest,
       GetCalibrationSetupInstructionsWrongStateNoInstructions) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetCalibrationSetupInstructions(
      base::BindLambdaForTesting(
          [&](rmad::CalibrationSetupInstruction instruction) {
            EXPECT_EQ(instruction, rmad::CalibrationSetupInstruction::
                                       RMAD_CALIBRATION_INSTRUCTION_UNKNOWN);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, StartCalibration) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kCheckCalibration, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kCheckCalibration);
        EXPECT_EQ(2, state.check_calibration().components_size());
        EXPECT_EQ(state.check_calibration().components(0).component(),
                  rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD);
        EXPECT_EQ(state.check_calibration().components(0).status(),
                  rmad::CalibrationComponentStatus::RMAD_CALIBRATION_WAITING);
        // Progress passed to rmad should always be zero.
        EXPECT_EQ(state.check_calibration().components(0).progress(), 0.0);
        EXPECT_EQ(state.check_calibration().components(1).component(),
                  rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD);
        EXPECT_EQ(state.check_calibration().components(1).status(),
                  rmad::CalibrationComponentStatus::RMAD_CALIBRATION_SKIP);
        // Progress passed to rmad should always be zero.
        EXPECT_EQ(state.check_calibration().components(1).progress(), 0.0);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kCheckCalibration);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  std::vector<rmad::CalibrationComponentStatus> components(2);
  components[0].set_component(rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD);
  components[0].set_status(
      rmad::CalibrationComponentStatus::RMAD_CALIBRATION_WAITING);
  components[0].set_progress(0.25);
  components[1].set_component(rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD);
  components[1].set_status(
      rmad::CalibrationComponentStatus::RMAD_CALIBRATION_SKIP);
  components[1].set_progress(0.5);

  shimless_rma_provider_->StartCalibration(
      std::move(components),
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, StartCalibrationFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) { NOTREACHED(); });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  std::vector<rmad::CalibrationComponentStatus> components(2);
  components[0].set_component(rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD);
  components[0].set_status(
      rmad::CalibrationComponentStatus::RMAD_CALIBRATION_WAITING);
  components[1].set_component(rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD);
  components[1].set_status(
      rmad::CalibrationComponentStatus::RMAD_CALIBRATION_SKIP);

  shimless_rma_provider_->StartCalibration(
      std::move(components),
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, RunCalibrationStep) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kSetupCalibration, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kSetupCalibration);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->RunCalibrationStep(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, RunCalibrationStepFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->RunCalibrationStep(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}
TEST_F(ShimlessRmaServiceTest, ContinueCalibration) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRunCalibration, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRunCalibration);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ContinueCalibration(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ContinueCalibrationFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ContinueCalibration(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, CalibrationComplete) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRunCalibration, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRunCalibration);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->CalibrationComplete(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, CalibrationCompleteFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->CalibrationComplete(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ProvisioningComplete) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kProvisionDevice, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kProvisionDevice);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ProvisioningComplete(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ProvisioningCompleteFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ProvisioningComplete(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, kFinalize) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kFinalize, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kFinalize);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->FinalizationComplete(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, FinalizationCompleteFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->FinalizationComplete(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, WriteProtectManuallyEnabled) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWpEnablePhysical, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWaitForManualWPEnable);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->WriteProtectManuallyEnabled(
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, WriteProtectManuallyEnabledFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->WriteProtectManuallyEnabled(
      base::BindLambdaForTesting([&](mojom::RmaState state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetLog) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRepairComplete, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRepairComplete);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetLog(base::BindLambdaForTesting(
      [&](const std::string& log) { EXPECT_FALSE(log.empty()); }));
  run_loop.RunUntilIdle();
}

TEST_F(ShimlessRmaServiceTest, GetLogWrongStateEmpty) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetLog(base::BindLambdaForTesting(
      [&](const std::string& log) { EXPECT_TRUE(log.empty()); }));
  run_loop.RunUntilIdle();
}

TEST_F(ShimlessRmaServiceTest, EndRmaAndReboot) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRepairComplete, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kRepairComplete);
        EXPECT_EQ(state.repair_complete().shutdown(),
                  rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_REBOOT);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRepairComplete);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->EndRmaAndReboot(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, EndRmaAndRebootFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->EndRmaAndReboot(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, EndRmaAndShutdown) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRepairComplete, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kRepairComplete);
        EXPECT_EQ(state.repair_complete().shutdown(),
                  rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_SHUTDOWN);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRepairComplete);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->EndRmaAndShutdown(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, EndRmaAndShutdownFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->EndRmaAndShutdown(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, EndRmaAndCutoffBattery) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRepairComplete, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kRepairComplete);
        EXPECT_EQ(
            state.repair_complete().shutdown(),
            rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_BATTERY_CUTOFF);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRepairComplete);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->EndRmaAndCutoffBattery(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, EndRmaAndCutoffBatteryFromWrongStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->EndRmaAndCutoffBattery(base::BindLambdaForTesting(
      [&](mojom::RmaState state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
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
  base::RunLoop run_loop;
  fake_rmad_client_()->TriggerErrorObservation(
      rmad::RmadErrorCode::RMAD_ERROR_REIMAGING_UNKNOWN_FAILURE);
  run_loop.RunUntilIdle();
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
      const rmad::CalibrationComponentStatus& component_status) override {
    component_observations.push_back(component_status);
  }

  void OnCalibrationStepComplete(
      rmad::CalibrationOverallStatus status) override {
    overall_observations.push_back(status);
  }

  std::vector<rmad::CalibrationComponentStatus> component_observations;
  std::vector<rmad::CalibrationOverallStatus> overall_observations;
  mojo::Receiver<mojom::CalibrationObserver> receiver{this};
};

TEST_F(ShimlessRmaServiceTest, ObserveCalibration) {
  FakeCalibrationObserver fake_observer;
  shimless_rma_provider_->ObserveCalibrationProgress(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  fake_rmad_client_()->TriggerCalibrationProgressObservation(
      rmad::RmadComponent::RMAD_COMPONENT_BASE_ACCELEROMETER,
      rmad::CalibrationComponentStatus::RMAD_CALIBRATION_IN_PROGRESS, 0.25);
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.component_observations.size(), 1UL);
  EXPECT_EQ(fake_observer.component_observations[0].component(),
            rmad::RmadComponent::RMAD_COMPONENT_BASE_ACCELEROMETER);
  EXPECT_EQ(fake_observer.component_observations[0].status(),
            rmad::CalibrationComponentStatus::RMAD_CALIBRATION_IN_PROGRESS);
  EXPECT_EQ(fake_observer.component_observations[0].progress(), 0.25);
}

TEST_F(ShimlessRmaServiceTest, ObserveCalibrationAfterSignal) {
  fake_rmad_client_()->TriggerCalibrationProgressObservation(
      rmad::RmadComponent::RMAD_COMPONENT_BASE_ACCELEROMETER,
      rmad::CalibrationComponentStatus::RMAD_CALIBRATION_IN_PROGRESS, 0.25);
  FakeCalibrationObserver fake_observer;
  shimless_rma_provider_->ObserveCalibrationProgress(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.component_observations.size(), 1UL);
  EXPECT_EQ(fake_observer.component_observations[0].component(),
            rmad::RmadComponent::RMAD_COMPONENT_BASE_ACCELEROMETER);
  EXPECT_EQ(fake_observer.component_observations[0].status(),
            rmad::CalibrationComponentStatus::RMAD_CALIBRATION_IN_PROGRESS);
  EXPECT_EQ(fake_observer.component_observations[0].progress(), 0.25);
}

TEST_F(ShimlessRmaServiceTest, ObserveOverallCalibration) {
  FakeCalibrationObserver fake_observer;
  shimless_rma_provider_->ObserveCalibrationProgress(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  fake_rmad_client_()->TriggerCalibrationOverallProgressObservation(
      rmad::CalibrationOverallStatus::
          RMAD_CALIBRATION_OVERALL_CURRENT_ROUND_COMPLETE);
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.overall_observations.size(), 1UL);
  EXPECT_EQ(fake_observer.overall_observations[0],
            rmad::CalibrationOverallStatus::
                RMAD_CALIBRATION_OVERALL_CURRENT_ROUND_COMPLETE);
}

TEST_F(ShimlessRmaServiceTest, ObserveOverallCalibrationAfterSignal) {
  fake_rmad_client_()->TriggerCalibrationOverallProgressObservation(
      rmad::CalibrationOverallStatus::
          RMAD_CALIBRATION_OVERALL_CURRENT_ROUND_COMPLETE);
  FakeCalibrationObserver fake_observer;
  shimless_rma_provider_->ObserveCalibrationProgress(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.overall_observations.size(), 1UL);
  EXPECT_EQ(fake_observer.overall_observations[0],
            rmad::CalibrationOverallStatus::
                RMAD_CALIBRATION_OVERALL_CURRENT_ROUND_COMPLETE);
}

class FakeProvisioningObserver : public mojom::ProvisioningObserver {
 public:
  void OnProvisioningUpdated(rmad::ProvisionStatus::Status step,
                             float progress) override {
    observations.push_back(
        std::pair<rmad::ProvisionStatus_Status, float>(step, progress));
  }

  std::vector<std::pair<rmad::ProvisionStatus_Status, float>> observations;
  mojo::Receiver<mojom::ProvisioningObserver> receiver{this};
};

TEST_F(ShimlessRmaServiceTest, ObserveProvisioning) {
  FakeProvisioningObserver fake_observer;
  shimless_rma_provider_->ObserveProvisioningProgress(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  fake_rmad_client_()->TriggerProvisioningProgressObservation(
      rmad::ProvisionStatus::RMAD_PROVISION_STATUS_IN_PROGRESS, 0.75);
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
}

TEST_F(ShimlessRmaServiceTest, ObserveProvisioningAfterSignal) {
  fake_rmad_client_()->TriggerProvisioningProgressObservation(
      rmad::ProvisionStatus::RMAD_PROVISION_STATUS_IN_PROGRESS, 0.75);
  FakeProvisioningObserver fake_observer;
  shimless_rma_provider_->ObserveProvisioningProgress(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
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

TEST_F(ShimlessRmaServiceTest, ObserveHardwareWriteProtectionStateAfterSignal) {
  fake_rmad_client_()->TriggerHardwareWriteProtectionStateObservation(false);
  FakeHardwareWriteProtectionStateObserver fake_observer;
  shimless_rma_provider_->ObserveHardwareWriteProtectionState(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
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

TEST_F(ShimlessRmaServiceTest, ObservePowerCableStateAfterSignal) {
  FakePowerCableStateObserver fake_observer;
  fake_rmad_client_()->TriggerPowerCableStateObservation(false);
  shimless_rma_provider_->ObservePowerCableState(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
}

class FakeHardwareVerificationStatusObserver
    : public mojom::HardwareVerificationStatusObserver {
 public:
  struct Observation {
    bool is_compliant;
    std::string error_message;
  };

  void OnHardwareVerificationResult(bool is_compliant,
                                    const std::string& error_message) override {
    Observation observation;
    observation.is_compliant = is_compliant;
    observation.error_message = error_message;
    observations.push_back(observation);
  }

  std::vector<Observation> observations;
  mojo::Receiver<mojom::HardwareVerificationStatusObserver> receiver{this};
};

TEST_F(ShimlessRmaServiceTest, ObserveHardwareVerification) {
  FakeHardwareVerificationStatusObserver fake_observer;
  shimless_rma_provider_->ObserveHardwareVerificationStatus(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  fake_rmad_client_()->TriggerHardwareVerificationResultObservation(true, "ok");
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
  EXPECT_EQ(fake_observer.observations[0].is_compliant, true);
  EXPECT_EQ(fake_observer.observations[0].error_message, "ok");
}

TEST_F(ShimlessRmaServiceTest, ObserveHardwareVerificationAfterSignal) {
  fake_rmad_client_()->TriggerHardwareVerificationResultObservation(true,
                                                                    "also ok");
  FakeHardwareVerificationStatusObserver fake_observer;
  shimless_rma_provider_->ObserveHardwareVerificationStatus(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
  EXPECT_EQ(fake_observer.observations[0].is_compliant, true);
  EXPECT_EQ(fake_observer.observations[0].error_message, "also ok");
}

class FakeFinalizationObserver : public mojom::FinalizationObserver {
 public:
  struct Observation {
    rmad::FinalizeStatus::Status status;
    float progress;
  };

  void OnFinalizationUpdated(rmad::FinalizeStatus::Status status,
                             float progress) override {
    Observation observation;
    observation.status = status;
    observation.progress = progress;
    observations.push_back(observation);
  }

  std::vector<Observation> observations;
  mojo::Receiver<mojom::FinalizationObserver> receiver{this};
};

TEST_F(ShimlessRmaServiceTest, ObserveFinalization) {
  FakeFinalizationObserver fake_observer;
  shimless_rma_provider_->ObserveFinalizationStatus(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  fake_rmad_client_()->TriggerFinalizationProgressObservation(
      rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_IN_PROGRESS, 0.5);
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
  EXPECT_EQ(fake_observer.observations[0].status,
            rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_IN_PROGRESS);
  EXPECT_EQ(fake_observer.observations[0].progress, 0.5f);
}

TEST_F(ShimlessRmaServiceTest, ObserveFinalizationAfterSignal) {
  fake_rmad_client_()->TriggerFinalizationProgressObservation(
      rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_FAILED_BLOCKING, 0.75);
  FakeFinalizationObserver fake_observer;
  shimless_rma_provider_->ObserveFinalizationStatus(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
  EXPECT_EQ(fake_observer.observations[0].status,
            rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_FAILED_BLOCKING);
  EXPECT_EQ(fake_observer.observations[0].progress, 0.75f);
}

}  // namespace shimless_rma
}  // namespace ash
