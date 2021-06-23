// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/backend/shimless_rma_service.h"

#include <map>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/rmad/fake_rmad_client.h"
#include "chromeos/dbus/rmad/rmad_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace shimless_rma {

class ShimlessRmaServiceTest : public testing::Test {
 public:
  ShimlessRmaServiceTest() = default;

  ~ShimlessRmaServiceTest() override { base::RunLoop().RunUntilIdle(); }

  void SetUp() override {
    shimless_rma_provider_ = std::make_unique<ShimlessRmaService>();
    chromeos::RmadClient::InitializeFake();
    rmad_client_ = chromeos::RmadClient::Get();
  }

  void TearDown() override { chromeos::RmadClient::Shutdown(); }

  rmad::RmadState* CreateState(rmad::RmadState::StateCase state_case) {
    rmad::RmadState* state = new rmad::RmadState();
    switch (state_case) {
      case rmad::RmadState::kWelcome:
        state->set_allocated_welcome(new rmad::WelcomeState());
        break;
      case rmad::RmadState::kSelectNetwork:
        state->set_allocated_select_network(new rmad::SelectNetworkState());
        break;
      case rmad::RmadState::kUpdateChrome:
        state->set_allocated_update_chrome(new rmad::UpdateChromeState());
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
      case rmad::RmadState::kCalibrateComponents:
        state->set_allocated_calibrate_components(
            new rmad::CalibrateComponentsState());
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

  chromeos::FakeRmadClient* fake_rmad_client_() {
    return google::protobuf::down_cast<chromeos::FakeRmadClient*>(rmad_client_);
  }

 protected:
  std::unique_ptr<ShimlessRmaService> shimless_rma_provider_;
  chromeos::RmadClient* rmad_client_ = nullptr;  // Unowned convenience pointer.

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ShimlessRmaServiceTest, GetCurrentState) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK));
  fake_states.push_back(
      CreateStateReply(rmad::RmadState::kSelectNetwork, rmad::RMAD_ERROR_OK));
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetCurrentStateNoRma) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kUnknown);
        EXPECT_EQ(error, mojom::RmadErrorCode::kRmaNotRequired);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetNextState) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK));
  fake_states.push_back(
      CreateStateReply(rmad::RmadState::kSelectNetwork, rmad::RMAD_ERROR_OK));
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->GetNextState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kConfigureNetwork);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetNextStateWithoutCurrentStateInvalid) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK));
  fake_states.push_back(
      CreateStateReply(rmad::RmadState::kSelectNetwork, rmad::RMAD_ERROR_OK));
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetNextState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kRequestInvalid);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetNextStateWithNoNextStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->GetNextState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kTransitionFailed);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetPrevState) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK));
  fake_states.push_back(
      CreateStateReply(rmad::RmadState::kSelectNetwork, rmad::RMAD_ERROR_OK));
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->GetNextState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kConfigureNetwork);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->GetPrevState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetPrevStateWithoutCurrentStateFails) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK));
  fake_states.push_back(
      CreateStateReply(rmad::RmadState::kSelectNetwork, rmad::RMAD_ERROR_OK));
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetPrevState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kTransitionFailed);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetPrevStateWithNoPrevStateFails) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->GetPrevState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kTransitionFailed);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, CanCancelRma) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->SetAbortable(true);
  base::RunLoop run_loop;
  shimless_rma_provider_->AbortRma(
      base::BindLambdaForTesting([&](mojom::RmadErrorCode error) {
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, CannotCancelRma) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->SetAbortable(false);
  base::RunLoop run_loop;
  shimless_rma_provider_->AbortRma(
      base::BindLambdaForTesting([&](mojom::RmadErrorCode error) {
        EXPECT_EQ(error, mojom::RmadErrorCode::kCannotCancelRma);
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace shimless_rma
}  // namespace ash
