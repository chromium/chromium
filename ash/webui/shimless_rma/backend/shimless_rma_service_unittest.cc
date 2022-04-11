// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/backend/shimless_rma_service.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/webui/shimless_rma/backend/shimless_rma_delegate.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/rmad/fake_rmad_client.h"
#include "chromeos/dbus/rmad/rmad_client.h"
#include "chromeos/dbus/update_engine/update_engine.pb.h"
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

constexpr char kDefaultWifiGuid[] = "WiFi";

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

// A fake impl of ShimlessRmaDelegate.
class FakeShimlessRmaDelegate : public ShimlessRmaDelegate {
 public:
  FakeShimlessRmaDelegate() = default;

  FakeShimlessRmaDelegate(const FakeShimlessRmaDelegate&) = delete;
  FakeShimlessRmaDelegate& operator=(const FakeShimlessRmaDelegate&) = delete;

  void ExitRmaThenRestartChrome() override {}
  void ShowDiagnosticsDialog() override {}
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
    shimless_rma_provider_ = std::make_unique<ShimlessRmaService>(
        std::make_unique<FakeShimlessRmaDelegate>());

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
      case rmad::RmadState::kWipeSelection:
        state->set_allocated_wipe_selection(new rmad::WipeSelectionState());
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

  void SetupWiFiNetwork(const std::string& guid) {
    network_state_helper().ConfigureService(
        base::StringPrintf(R"({"GUID": "%s", "Type": "wifi", "State": "online",
            "Strength": 50, "AutoConnect": true, "WiFi.HiddenSSID": false,
            "Profile": "user_profile_path",})",
                           guid.c_str()));

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
  SetupWiFiNetwork(kDefaultWifiGuid);
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK,
                       /*can_abort*/ true, /*can_go_back*/ false),
      CreateStateReply(rmad::RmadState::kComponentsRepair, rmad::RMAD_ERROR_OK,
                       /*can_abort*/ false, /*can_go_back*/ true)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state, can_cancel=true, can_go_back=false
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        EXPECT_EQ(can_cancel, true);
        EXPECT_EQ(can_go_back, false);
      }));
  run_loop.RunUntilIdle();

  // Next state, can_cancel=false, can_go_back=true
  shimless_rma_provider_->SetSameOwner(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kSelectComponents);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        EXPECT_EQ(can_cancel, false);
        EXPECT_EQ(can_go_back, true);
      }));
  run_loop.RunUntilIdle();
  // Previous state, can_cancel=true, can_go_back=false
  shimless_rma_provider_->TransitionPreviousState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        EXPECT_EQ(can_cancel, true);
        EXPECT_EQ(can_go_back, false);
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, WelcomeHasNetworkConnection) {
  SetupWiFiNetwork(kDefaultWifiGuid);
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kComponentsRepair,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // With a WiFi network it should redirect to kUpdateOs
  shimless_rma_provider_->BeginFinalization(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kUpdateOs);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, WelcomeHasNoNetworkConnection) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kComponentsRepair,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // TODO(gavindodd): Create a FakeVersionUpdater so no updates available and
  // update progress can be tested.

  // No network should prompt select network page
  shimless_rma_provider_->BeginFinalization(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kConfigureNetwork);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ChooseNetworkHasNetworkConnection) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kComponentsRepair,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // No network should prompt select network page
  shimless_rma_provider_->BeginFinalization(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kConfigureNetwork);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  SetupWiFiNetwork(kDefaultWifiGuid);

  // With a WiFi network it should redirect to kUpdateOs
  shimless_rma_provider_->NetworkSelectionComplete(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kUpdateOs);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ChooseNetworkHasNoNetworkConnection) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kComponentsRepair,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // No network should prompt select network page
  shimless_rma_provider_->BeginFinalization(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kConfigureNetwork);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // With no network it should redirect to next rmad state
  shimless_rma_provider_->NetworkSelectionComplete(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kSelectComponents);
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
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
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
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kUnknown);
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
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->SetSameOwner(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kSelectComponents);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->TransitionPreviousState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
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
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_TRANSITION_FAILED);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, TransitionPreviousStateWithNoPrevStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->TransitionPreviousState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_TRANSITION_FAILED);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, CanCancelRma) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
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
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
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
  const std::vector<rmad::GetStateReply> fake_states = {
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
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetSameOwner(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetSameOwnerFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRestock, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetSameOwner(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetDifferentOwner) {
  const std::vector<rmad::GetStateReply> fake_states = {
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
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetDifferentOwner(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetWipeDevice) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWipeSelection, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kWipeSelection);
        EXPECT_TRUE(state.wipe_selection().wipe_device());
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseWipeDevice);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  const bool expected_wipe_device = true;
  shimless_rma_provider_->SetWipeDevice(
      expected_wipe_device,
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetDifferentOwnerFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRestock, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->SetDifferentOwner(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetManuallyDisableWriteProtect) {
  const std::vector<rmad::GetStateReply> fake_states = {
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
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseWriteProtectDisableMethod);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ChooseManuallyDisableWriteProtect(
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest,
       SetManuallyDisableWriteProtectFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ChooseManuallyDisableWriteProtect(
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetRsuDisableWriteProtect) {
  const std::vector<rmad::GetStateReply> fake_states = {
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
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseWriteProtectDisableMethod);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ChooseRsuDisableWriteProtect(
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetRsuDisableWriteProtectFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ChooseRsuDisableWriteProtect(
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
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
  const std::vector<rmad::GetStateReply> fake_states = {
      write_protect_disable_rsu_state};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kEnterRSUWPDisableCode);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetRsuDisableWriteProtectChallenge(
      base::BindLambdaForTesting([&](const std::string& challenge) {
        EXPECT_EQ(challenge, "rsu write protect challenge code");
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
  const std::vector<rmad::GetStateReply> fake_states = {
      write_protect_disable_rsu_state};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kEnterRSUWPDisableCode);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetRsuDisableWriteProtectHwid(
      base::BindLambdaForTesting([&](const std::string& hwid) {
        EXPECT_EQ(hwid, "rsu write protect hwid");
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
  const std::vector<rmad::GetStateReply> fake_states = {
      write_protect_disable_rsu_state};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kEnterRSUWPDisableCode);
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
  const std::vector<rmad::GetStateReply> fake_states = {
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
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kEnterRSUWPDisableCode);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetRsuDisableWriteProtectCode(
      "test RSU unlock code",
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest,
       SetRsuDisableWriteProtectCodeFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetRsuDisableWriteProtectCode(
      "test RSU unlock code",
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, WriteProtectManuallyDisabled) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWpDisablePhysical,
                       rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kWaitForManualWPDisable);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->WriteProtectManuallyDisabled(
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest,
       WriteProtectManuallyDisabledFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->WriteProtectManuallyDisabled(
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ConfirmManualWpDisableComplete) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWpDisableComplete,
                       rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kWPDisableComplete);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ConfirmManualWpDisableComplete(
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest,
       ConfirmManualWpDisableCompleteFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ConfirmManualWpDisableComplete(
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetWriteProtectDisableCompleteAction) {
  rmad::GetStateReply wp_disable_complete_state = CreateStateReply(
      rmad::RmadState::kWpDisableComplete, rmad::RMAD_ERROR_OK);
  wp_disable_complete_state.mutable_state()
      ->mutable_wp_disable_complete()
      ->set_action(rmad::WriteProtectDisableCompleteState::
                       RMAD_WP_DISABLE_COMPLETE_ASSEMBLE_DEVICE);

  std::vector<rmad::GetStateReply> fake_states = {wp_disable_complete_state};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kWPDisableComplete);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->GetWriteProtectDisableCompleteAction(
      base::BindLambdaForTesting(
          [&](rmad::WriteProtectDisableCompleteState::Action action) {
            EXPECT_EQ(action, rmad::WriteProtectDisableCompleteState::
                                  RMAD_WP_DISABLE_COMPLETE_ASSEMBLE_DEVICE);
          }));
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
  component->set_identifier("Keyboard_1");
  // second component
  component = components_repair_state.mutable_state()
                  ->mutable_components_repair()
                  ->add_components();
  component->set_component(rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD);
  component->set_repair_status(
      rmad::ComponentsRepairState::ComponentRepairStatus::
          RMAD_REPAIR_STATUS_REPLACED);
  component->set_identifier("Touchpad_1");
  const std::vector<rmad::GetStateReply> fake_states = {
      components_repair_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kSelectComponents);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetComponentList(base::BindLambdaForTesting(
      [&](const std::vector<rmad::ComponentsRepairState::ComponentRepairStatus>&
              components) {
        EXPECT_EQ(components.size(), 2UL);
        EXPECT_EQ(components[0].component(),
                  rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD);
        EXPECT_EQ(components[0].repair_status(),
                  rmad::ComponentsRepairState::ComponentRepairStatus::
                      RMAD_REPAIR_STATUS_ORIGINAL);
        EXPECT_EQ(components[0].identifier(), "Keyboard_1");
        EXPECT_EQ(components[1].component(),
                  rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD);
        EXPECT_EQ(components[1].repair_status(),
                  rmad::ComponentsRepairState::ComponentRepairStatus::
                      RMAD_REPAIR_STATUS_REPLACED);
        EXPECT_EQ(components[1].identifier(), "Touchpad_1");
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetComponentListFromWrongStateEmpty) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetComponentList(base::BindLambdaForTesting(
      [&](const std::vector<rmad::ComponentsRepairState::ComponentRepairStatus>&
              components) {
        EXPECT_EQ(components.size(), 0UL);
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
  const std::vector<rmad::GetStateReply> fake_states = {
      components_repair_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kComponentsRepair);
        EXPECT_EQ(state.components_repair().components_size(), 2);
        EXPECT_EQ(state.components_repair().components(0).component(),
                  rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD);
        EXPECT_EQ(state.components_repair().components(0).repair_status(),
                  rmad::ComponentsRepairState::ComponentRepairStatus::
                      RMAD_REPAIR_STATUS_REPLACED);
        EXPECT_EQ(state.components_repair().components(0).identifier(),
                  "Keyboard_1");
        EXPECT_EQ(state.components_repair().components(1).component(),
                  rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD);
        EXPECT_EQ(state.components_repair().components(1).repair_status(),
                  rmad::ComponentsRepairState::ComponentRepairStatus::
                      RMAD_REPAIR_STATUS_ORIGINAL);
        EXPECT_EQ(state.components_repair().components(1).identifier(),
                  "Touchpad_1");
        EXPECT_EQ(state.components_repair().mainboard_rework(), false);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kSelectComponents);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  std::vector<rmad::ComponentsRepairState::ComponentRepairStatus> components(2);
  components[0].set_component(rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD);
  components[0].set_repair_status(
      rmad::ComponentsRepairState::ComponentRepairStatus::
          RMAD_REPAIR_STATUS_REPLACED);
  components[0].set_identifier("Keyboard_1");
  components[1].set_component(rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD);
  components[1].set_repair_status(
      rmad::ComponentsRepairState::ComponentRepairStatus::
          RMAD_REPAIR_STATUS_ORIGINAL);
  components[1].set_identifier("Touchpad_1");

  shimless_rma_provider_->SetComponentList(
      std::move(components),
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetComponentListFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
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
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
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
  const std::vector<rmad::GetStateReply> fake_states = {
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
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kSelectComponents);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReworkMainboard(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}  // namespace shimless_rma

TEST_F(ShimlessRmaServiceTest, ReworkMainboardFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReworkMainboard(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, RoFirmwareUpdateComplete) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kUpdateRoFirmware, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kUpdateRoFirmware);
        EXPECT_EQ(state.update_ro_firmware().choice(),
                  rmad::UpdateRoFirmwareState::RMAD_UPDATE_CHOICE_CONTINUE);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kUpdateRoFirmware);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->RoFirmwareUpdateComplete(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, RoFirmwareUpdateCompleteFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->RoFirmwareUpdateComplete(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ShutdownForRestock) {
  const std::vector<rmad::GetStateReply> fake_states = {
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
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ShutdownForRestock(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ShutdownForRestockFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ShutdownForRestock(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ContinueFinalizationAfterRestock) {
  const std::vector<rmad::GetStateReply> fake_states = {
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
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kRestock);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ContinueFinalizationAfterRestock(
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest,
       ContinueFinalizationAfterRestockFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ContinueFinalizationAfterRestock(
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
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
  const std::vector<rmad::GetStateReply> fake_states = {
      update_device_info_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalSerialNumber(
      base::BindLambdaForTesting([&](const std::string& serial_number) {
        EXPECT_EQ(serial_number, "original serial number");
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalSerialNumberFromWrongStateEmpty) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalSerialNumber(
      base::BindLambdaForTesting([&](const std::string& serial_number) {
        EXPECT_EQ(serial_number, "");
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetRegionList) {
  rmad::GetStateReply update_device_info_state =
      CreateStateReply(rmad::RmadState::kUpdateDeviceInfo, rmad::RMAD_ERROR_OK);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->add_region_list("EMEA");
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->add_region_list("AMER");
  const std::vector<rmad::GetStateReply> fake_states = {
      update_device_info_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetRegionList(
      base::BindLambdaForTesting([&](const std::vector<std::string>& regions) {
        EXPECT_EQ(regions.size(), 2UL);
        EXPECT_EQ(regions[0], "EMEA");
        EXPECT_EQ(regions[1], "AMER");
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetRegionListWrongStateEmpty) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetRegionList(
      base::BindLambdaForTesting([&](const std::vector<std::string>& regions) {
        EXPECT_EQ(regions.size(), 0UL);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetSkuList) {
  rmad::GetStateReply update_device_info_state =
      CreateStateReply(rmad::RmadState::kUpdateDeviceInfo, rmad::RMAD_ERROR_OK);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->add_sku_list(1UL);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->add_sku_list(7UL);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->add_sku_list(2UL);
  const std::vector<rmad::GetStateReply> fake_states = {
      update_device_info_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetSkuList(
      base::BindLambdaForTesting([&](const std::vector<uint64_t>& skus) {
        EXPECT_EQ(skus.size(), 3UL);
        EXPECT_EQ(skus[0], 1UL);
        EXPECT_EQ(skus[1], 7UL);
        EXPECT_EQ(skus[2], 2UL);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetSkuListWrongStateEmpty) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetSkuList(
      base::BindLambdaForTesting([&](const std::vector<uint64_t>& skus) {
        EXPECT_EQ(skus.size(), 0UL);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetWhiteLabelList) {
  rmad::GetStateReply update_device_info_state =
      CreateStateReply(rmad::RmadState::kUpdateDeviceInfo, rmad::RMAD_ERROR_OK);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->add_whitelabel_list("White-label 1");
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->add_whitelabel_list("White-label 5");
  const std::vector<rmad::GetStateReply> fake_states = {
      update_device_info_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetWhiteLabelList(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& whiteLabels) {
        EXPECT_EQ(whiteLabels.size(), 2UL);
        EXPECT_EQ(whiteLabels[0], "White-label 1");
        EXPECT_EQ(whiteLabels[1], "White-label 5");
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetWhiteLabelListWrongStateEmpty) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetWhiteLabelList(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& whiteLabels) {
        EXPECT_EQ(whiteLabels.size(), 0UL);
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
  const std::vector<rmad::GetStateReply> fake_states = {
      update_device_info_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalRegion(
      base::BindLambdaForTesting([&](int32_t region) {
        EXPECT_EQ(region, 3);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalRegionFromWrongStateEmpty) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalRegion(
      base::BindLambdaForTesting([&](int32_t region) {
        EXPECT_EQ(region, 0);
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
  const std::vector<rmad::GetStateReply> fake_states = {
      update_device_info_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalSku(
      base::BindLambdaForTesting([&](int32_t sku) {
        EXPECT_EQ(sku, 4);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalSkuFromWrongStateEmpty) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalSku(
      base::BindLambdaForTesting([&](int32_t sku) {
        EXPECT_EQ(sku, 0);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalWhiteLabel) {
  rmad::GetStateReply update_device_info_state =
      CreateStateReply(rmad::RmadState::kUpdateDeviceInfo, rmad::RMAD_ERROR_OK);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->set_original_whitelabel_index(3);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->set_whitelabel_index(1);
  const std::vector<rmad::GetStateReply> fake_states = {
      update_device_info_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalWhiteLabel(
      base::BindLambdaForTesting([&](int32_t white_label) {
        EXPECT_EQ(white_label, 3);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalWhiteLabelFromWrongStateEmpty) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalWhiteLabel(
      base::BindLambdaForTesting([&](int32_t white_label) {
        EXPECT_EQ(white_label, 0);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalDramPartNumber) {
  rmad::GetStateReply update_device_info_state =
      CreateStateReply(rmad::RmadState::kUpdateDeviceInfo, rmad::RMAD_ERROR_OK);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->set_original_dram_part_number("123-456-789");
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->set_dram_part_number("987-654-321");
  const std::vector<rmad::GetStateReply> fake_states = {
      update_device_info_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalDramPartNumber(
      base::BindLambdaForTesting([&](const std::string& part_number) {
        EXPECT_EQ(part_number, "123-456-789");
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalDramPartNumberFromWrongStateEmpty) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalDramPartNumber(
      base::BindLambdaForTesting([&](const std::string& part_number) {
        EXPECT_EQ(part_number, "");
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetDeviceInformation) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kUpdateDeviceInfo, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kUpdateDeviceInfo);
        EXPECT_EQ(state.update_device_info().serial_number(), "serial number");
        EXPECT_EQ(state.update_device_info().region_index(), 1L);
        EXPECT_EQ(state.update_device_info().sku_index(), 2L);
        EXPECT_EQ(state.update_device_info().whitelabel_index(), 3L);
        EXPECT_EQ(state.update_device_info().dram_part_number(), "123-456-789");
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetDeviceInformation(
      "serial number", 1, 2, 3, "123-456-789",
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetDeviceInformationFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetDeviceInformation(
      "serial number", 1, 2, 3, "123-456-789",
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
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
  const std::vector<rmad::GetStateReply> fake_states = {
      check_calibration_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  EXPECT_EQ(
      check_calibration_state.state().check_calibration().components_size(), 1);

  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kCheckCalibration);
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
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
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
  const std::vector<rmad::GetStateReply> fake_states = {
      setup_calibration_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kSetupCalibration);
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
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
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
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kCheckCalibration, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kCheckCalibration);
        EXPECT_EQ(state.check_calibration().components_size(), 2);
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
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kCheckCalibration);
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
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, StartCalibrationFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) { NOTREACHED(); });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
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
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, RunCalibrationStep) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kSetupCalibration, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kSetupCalibration);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->RunCalibrationStep(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, RunCalibrationStepFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->RunCalibrationStep(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}
TEST_F(ShimlessRmaServiceTest, ContinueCalibration) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRunCalibration, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kRunCalibration);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ContinueCalibration(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ContinueCalibrationFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ContinueCalibration(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, CalibrationComplete) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRunCalibration, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kRunCalibration);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->CalibrationComplete(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, CalibrationCompleteFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->CalibrationComplete(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ProvisioningComplete) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kProvisionDevice, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kProvisionDevice);
        EXPECT_EQ(state.provision_device().choice(),
                  rmad::ProvisionDeviceState::RMAD_PROVISION_CHOICE_CONTINUE);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kProvisionDevice);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ProvisioningComplete(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ProvisioningCompleteFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ProvisioningComplete(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, RetryProvisioning) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kProvisionDevice, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kProvisionDevice);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->RetryProvisioning(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, Finalization) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kFinalize, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kFinalize);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->FinalizationComplete(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, RetryFinalization) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kFinalize, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kFinalize);
        EXPECT_EQ(state.finalize().choice(),
                  rmad::FinalizeState::RMAD_FINALIZE_CHOICE_RETRY);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kFinalize);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->RetryFinalization(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, FinalizationCompleteFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->FinalizationComplete(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, WriteProtectManuallyEnabled) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWpEnablePhysical, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kWaitForManualWPEnable);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->WriteProtectManuallyEnabled(
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, WriteProtectManuallyEnabledFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->WriteProtectManuallyEnabled(
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetLog) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRepairComplete, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  const std::string expected_log = "This is my test log for the RMA process";
  fake_rmad_client_()->SetGetLogReply(expected_log, rmad::RMAD_ERROR_OK);
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kRepairComplete);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetLog(base::BindLambdaForTesting(
      [&](const std::string& log, rmad::RmadErrorCode error) {
        EXPECT_EQ(log, expected_log);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
}

TEST_F(ShimlessRmaServiceTest, GetLogWrongStateEmpty) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetLog(base::BindLambdaForTesting(
      [&](const std::string& log, rmad::RmadErrorCode error) {
        EXPECT_TRUE(log.empty());
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
      }));
  run_loop.RunUntilIdle();
}

TEST_F(ShimlessRmaServiceTest, GetPowerwashRequired) {
  rmad::GetStateReply repair_complete_state =
      CreateStateReply(rmad::RmadState::kRepairComplete, rmad::RMAD_ERROR_OK);
  repair_complete_state.mutable_state()
      ->mutable_repair_complete()
      ->set_powerwash_required(true);
  const std::vector<rmad::GetStateReply> fake_states = {repair_complete_state};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kRepairComplete);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetPowerwashRequired(
      base::BindLambdaForTesting([&](const bool powerwash_required) {
        EXPECT_EQ(powerwash_required, true);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, EndRmaAndShutdown) {
  const std::vector<rmad::GetStateReply> fake_states = {
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
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kRepairComplete);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->EndRma(
      rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_SHUTDOWN,
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, EndRmaAndRebootFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->EndRma(
      rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_SHUTDOWN,
      base::BindLambdaForTesting([&](mojom::State state, bool can_cancel,
                                     bool can_go_back,
                                     rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kChooseDestination);
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

class FakeUpdateRoFirmwareObserver : public mojom::UpdateRoFirmwareObserver {
 public:
  void OnUpdateRoFirmwareStatusChanged(
      rmad::UpdateRoFirmwareStatus status) override {
    observations.push_back(status);
  }

  std::vector<rmad::UpdateRoFirmwareStatus> observations;
  mojo::Receiver<mojom::UpdateRoFirmwareObserver> receiver{this};
};

TEST_F(ShimlessRmaServiceTest, ObserveUpdateRoFirmwareStatus) {
  FakeUpdateRoFirmwareObserver fake_observer;
  shimless_rma_provider_->ObserveRoFirmwareUpdateProgress(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  fake_rmad_client_()->TriggerRoFirmwareUpdateProgressObservation(
      rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_DOWNLOADING);
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
  EXPECT_EQ(fake_observer.observations[0],
            rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_DOWNLOADING);
}

TEST_F(ShimlessRmaServiceTest, ObserveUpdateRoFirmwareStatusAfterSignal) {
  fake_rmad_client_()->TriggerRoFirmwareUpdateProgressObservation(
      rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_DOWNLOADING);
  FakeUpdateRoFirmwareObserver fake_observer;
  shimless_rma_provider_->ObserveRoFirmwareUpdateProgress(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
  EXPECT_EQ(fake_observer.observations[0],
            rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_DOWNLOADING);
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

  mojo::PendingRemote<mojom::CalibrationObserver> GenerateRemote() {
    if (receiver.is_bound())
      receiver.reset();

    mojo::PendingRemote<mojom::CalibrationObserver> remote;
    receiver.Bind(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  std::vector<rmad::CalibrationComponentStatus> component_observations;
  std::vector<rmad::CalibrationOverallStatus> overall_observations;
  mojo::Receiver<mojom::CalibrationObserver> receiver{this};
};

TEST_F(ShimlessRmaServiceTest, ObserveCalibration) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kSetupCalibration, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kRunCalibration, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kSetupCalibration);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  FakeCalibrationObserver fake_observer;
  shimless_rma_provider_->ObserveCalibrationProgress(
      fake_observer.GenerateRemote());
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

  shimless_rma_provider_->RunCalibrationStep(base::BindLambdaForTesting(
      [&](mojom::State state, bool can_cancel, bool can_go_back,
          rmad::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::State::kRunCalibration);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));

  // Simulate returning to the calibration  run page and observing a new
  // calibration.
  shimless_rma_provider_->ObserveCalibrationProgress(
      fake_observer.GenerateRemote());
  fake_rmad_client_()->TriggerCalibrationProgressObservation(
      rmad::RmadComponent::RMAD_COMPONENT_BASE_GYROSCOPE,
      rmad::CalibrationComponentStatus::RMAD_CALIBRATION_COMPLETE, 1.0);
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.component_observations.size(), 2UL);
  EXPECT_EQ(fake_observer.component_observations[1].component(),
            rmad::RmadComponent::RMAD_COMPONENT_BASE_GYROSCOPE);
  EXPECT_EQ(fake_observer.component_observations[1].status(),
            rmad::CalibrationComponentStatus::RMAD_CALIBRATION_COMPLETE);
  EXPECT_EQ(fake_observer.component_observations[1].progress(), 1.0);
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
  struct Observation {
    rmad::ProvisionStatus::Status status;
    float progress;
    rmad::ProvisionStatus::Error error;
  };

  void OnProvisioningUpdated(rmad::ProvisionStatus::Status status,
                             float progress,
                             rmad::ProvisionStatus::Error error) override {
    Observation observation;
    observation.status = status;
    observation.progress = progress;
    observation.error = error;
    observations.push_back(observation);
  }

  std::vector<Observation> observations;
  mojo::Receiver<mojom::ProvisioningObserver> receiver{this};
};

TEST_F(ShimlessRmaServiceTest, ObserveProvisioning) {
  FakeProvisioningObserver fake_observer;
  shimless_rma_provider_->ObserveProvisioningProgress(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;

  const rmad::ProvisionStatus::Status expected_status =
      rmad::ProvisionStatus::RMAD_PROVISION_STATUS_IN_PROGRESS;
  const float expected_progress = 0.75;
  const rmad::ProvisionStatus::Error expected_error =
      rmad::ProvisionStatus::RMAD_PROVISION_ERROR_GENERATE_SECRET;
  fake_rmad_client_()->TriggerProvisioningProgressObservation(
      expected_status, expected_progress, expected_error);
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
  EXPECT_EQ(fake_observer.observations[0].status, expected_status);
  EXPECT_EQ(fake_observer.observations[0].progress, expected_progress);
  EXPECT_EQ(fake_observer.observations[0].error, expected_error);
}

TEST_F(ShimlessRmaServiceTest, ObserveProvisioningAfterSignal) {
  FakeProvisioningObserver fake_observer;
  shimless_rma_provider_->ObserveProvisioningProgress(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;

  const rmad::ProvisionStatus::Status expected_status =
      rmad::ProvisionStatus::RMAD_PROVISION_STATUS_IN_PROGRESS;
  const float expected_progress = 0.75;
  const rmad::ProvisionStatus::Error expected_error =
      rmad::ProvisionStatus::RMAD_PROVISION_ERROR_WP_ENABLED;
  fake_rmad_client_()->TriggerProvisioningProgressObservation(
      expected_status, expected_progress, expected_error);
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
  EXPECT_EQ(fake_observer.observations[0].status, expected_status);
  EXPECT_EQ(fake_observer.observations[0].progress, expected_progress);
  EXPECT_EQ(fake_observer.observations[0].error, expected_error);
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
    rmad::FinalizeStatus::Error error;
  };

  void OnFinalizationUpdated(rmad::FinalizeStatus::Status status,
                             float progress,
                             rmad::FinalizeStatus::Error error) override {
    Observation observation;
    observation.status = status;
    observation.progress = progress;
    observation.error = error;
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

  const rmad::FinalizeStatus::Status expected_status =
      rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_IN_PROGRESS;
  const float expected_progress = 0.5;
  const rmad::FinalizeStatus::Error expected_error =
      rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_CANNOT_ENABLE_HWWP;
  fake_rmad_client_()->TriggerFinalizationProgressObservation(
      expected_status, expected_progress, expected_error);
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
  EXPECT_EQ(fake_observer.observations[0].status, expected_status);
  EXPECT_EQ(fake_observer.observations[0].progress, expected_progress);
  EXPECT_EQ(fake_observer.observations[0].error, expected_error);
}

TEST_F(ShimlessRmaServiceTest, ObserveFinalizationAfterSignal) {
  FakeFinalizationObserver fake_observer;
  shimless_rma_provider_->ObserveFinalizationStatus(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;

  const rmad::FinalizeStatus::Status expected_status =
      rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_FAILED_BLOCKING;
  const float expected_progress = 0.75;
  const rmad::FinalizeStatus::Error expected_error =
      rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_CR50;
  fake_rmad_client_()->TriggerFinalizationProgressObservation(
      expected_status, expected_progress, expected_error);
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
  EXPECT_EQ(fake_observer.observations[0].status, expected_status);
  EXPECT_EQ(fake_observer.observations[0].progress, expected_progress);
  EXPECT_EQ(fake_observer.observations[0].error, expected_error);
}

TEST_F(ShimlessRmaServiceTest, GetWriteProtectManuallyDisabledInstructions) {
  const std::vector<uint8_t> expected_qrcode_data = {
      1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1,
      1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1,
      1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1,
      1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1,
      1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 0, 1,
      1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
      1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1,
      0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
      1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1,
      0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0,
      0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1,
      0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0,
      1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0,
      1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0,
      1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 1,
      1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0,
      1, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1,
      0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0,
      1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1,
      1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1,
      1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1,
      1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 1,
      1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1,
      1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1,
  };

  shimless_rma_provider_->GetWriteProtectManuallyDisabledInstructions(
      base::BindLambdaForTesting(
          [&](const std::string& url, mojom::QrCodePtr qrcode) {
            EXPECT_EQ(url, "g.co/chromebook/");
            EXPECT_EQ(qrcode->size, 25);
            EXPECT_FALSE(qrcode.is_null());
            EXPECT_EQ(qrcode->data.size(), 25UL * 25UL);
            EXPECT_EQ(qrcode->data.size(), expected_qrcode_data.size());
            EXPECT_EQ(qrcode->data, expected_qrcode_data);
          }));
}

class FakeOsUpdateObserver : public mojom::OsUpdateObserver {
 public:
  struct Observation {
    update_engine::Operation operation;
    float progress;
    update_engine::ErrorCode error_code;
  };

  void OnOsUpdateProgressUpdated(update_engine::Operation operation,
                                 float progress,
                                 update_engine::ErrorCode error_code) override {
    Observation observation;
    observation.operation = operation;
    observation.progress = progress;
    observation.error_code = error_code;
    observations.push_back(observation);
  }

  std::vector<Observation> observations;
  mojo::Receiver<mojom::OsUpdateObserver> receiver{this};
};

TEST_F(ShimlessRmaServiceTest, OsUpdateProgress) {
  FakeOsUpdateObserver fake_os_update_observer;
  const update_engine::Operation operation =
      update_engine::Operation::DOWNLOADING;
  const double progress = 50.0;
  const update_engine::ErrorCode error_code =
      update_engine::ErrorCode::kSuccess;

  shimless_rma_provider_->ObserveOsUpdateProgress(
      fake_os_update_observer.receiver.BindNewPipeAndPassRemote());
  shimless_rma_provider_->OsUpdateProgress(operation, progress, error_code);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_EQ(operation, fake_os_update_observer.observations[0].operation);
  EXPECT_EQ(progress, fake_os_update_observer.observations[0].progress);
  EXPECT_EQ(error_code, fake_os_update_observer.observations[0].error_code);
}

}  // namespace shimless_rma
}  // namespace ash
