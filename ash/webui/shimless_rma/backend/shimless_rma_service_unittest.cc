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
  ShimlessRmaServiceTest() = default;

  ~ShimlessRmaServiceTest() override { base::RunLoop().RunUntilIdle(); }

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

  FakeRmadClientForTest* fake_rmad_client_() {
    return google::protobuf::down_cast<FakeRmadClientForTest*>(rmad_client_);
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

TEST_F(ShimlessRmaServiceTest, SetSameOwner) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kDeviceDestination);
        EXPECT_EQ(state.device_destination().destination(),
                  rmad::DeviceDestinationState::RMAD_DESTINATION_SAME);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetSameOwner(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetSameOwnerFromWrongStateFails) {
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

  shimless_rma_provider_->SetSameOwner(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kRequestInvalid);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetDifferentOwner) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kDeviceDestination);
        EXPECT_EQ(state.device_destination().destination(),
                  rmad::DeviceDestinationState::RMAD_DESTINATION_DIFFERENT);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseDestination);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetDifferentOwner(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetDifferentOwnerFromWrongStateFails) {
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
  shimless_rma_provider_->SetDifferentOwner(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kRequestInvalid);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetManuallyDisableWriteProtect) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWpDisableMethod, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
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
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseWriteProtectDisableMethod);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ChooseManuallyDisableWriteProtect(
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, mojom::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
            EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest,
       SetManuallyDisableWriteProtectFromWrongStateFails) {
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

  shimless_rma_provider_->ChooseManuallyDisableWriteProtect(
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, mojom::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
            EXPECT_EQ(error, mojom::RmadErrorCode::kRequestInvalid);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetRsuDisableWriteProtect) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWpDisableMethod, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kWpDisableMethod);
        EXPECT_EQ(state.wp_disable_method().disable_method(),
                  rmad::WriteProtectDisableMethodState::RMAD_WP_DISABLE_RSU);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseWriteProtectDisableMethod);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ChooseRsuDisableWriteProtect(
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, mojom::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
            EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetRsuDisableWriteProtectFromWrongStateFails) {
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

  shimless_rma_provider_->ChooseRsuDisableWriteProtect(
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, mojom::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
            EXPECT_EQ(error, mojom::RmadErrorCode::kRequestInvalid);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetRsuDisableWriteProtectCode) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWpDisableRsu, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kWpDisableRsu);
        EXPECT_EQ(state.wp_disable_rsu().unlock_code(), "test RSU unlock code");
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kEnterRSUWPDisableCode);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetRsuDisableWriteProtectCode(
      "test RSU unlock code",
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, mojom::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
            EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest,
       SetRsuDisableWriteProtectCodeFromWrongStateFails) {
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

  shimless_rma_provider_->SetRsuDisableWriteProtectCode(
      "test RSU unlock code",
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, mojom::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
            EXPECT_EQ(error, mojom::RmadErrorCode::kRequestInvalid);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetComponentList) {
  rmad::GetStateReply components_repair_state =
      CreateStateReply(rmad::RmadState::kComponentsRepair, rmad::RMAD_ERROR_OK);
  // first component
  rmad::ComponentRepairState* component =
      components_repair_state.mutable_state()
          ->mutable_components_repair()
          ->add_components();
  component->set_name(rmad::ComponentRepairState::RMAD_COMPONENT_KEYBOARD);
  component->set_repair_state(rmad::ComponentRepairState::RMAD_REPAIR_ORIGINAL);
  // second component
  component = components_repair_state.mutable_state()
                  ->mutable_components_repair()
                  ->add_components();
  component->set_name(rmad::ComponentRepairState::RMAD_COMPONENT_TRACKPAD);
  component->set_repair_state(rmad::ComponentRepairState::RMAD_REPAIR_REPLACED);
  std::vector<rmad::GetStateReply> fake_states = {
      components_repair_state,
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kSelectComponents);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetComponentList(base::BindLambdaForTesting(
      [&](std::vector<mojom::ComponentPtr> components) {
        EXPECT_EQ(2UL, components.size());
        EXPECT_EQ(components[0]->component, mojom::ComponentType::kKeyboard);
        EXPECT_EQ(components[0]->state, mojom::ComponentRepairState::kOriginal);
        EXPECT_EQ(components[1]->component, mojom::ComponentType::kTrackpad);
        EXPECT_EQ(components[1]->state, mojom::ComponentRepairState::kReplaced);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetComponentListFromWrongStateEmpty) {
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

  shimless_rma_provider_->GetComponentList(base::BindLambdaForTesting(
      [&](std::vector<mojom::ComponentPtr> components) {
        EXPECT_EQ(0UL, components.size());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetComponentList) {
  rmad::GetStateReply components_repair_state =
      CreateStateReply(rmad::RmadState::kComponentsRepair, rmad::RMAD_ERROR_OK);
  // first component
  rmad::ComponentRepairState* component =
      components_repair_state.mutable_state()
          ->mutable_components_repair()
          ->add_components();
  component->set_name(rmad::ComponentRepairState::RMAD_COMPONENT_TRACKPAD);
  component->set_repair_state(rmad::ComponentRepairState::RMAD_REPAIR_ORIGINAL);
  // second component
  component = components_repair_state.mutable_state()
                  ->mutable_components_repair()
                  ->add_components();
  component->set_name(rmad::ComponentRepairState::RMAD_COMPONENT_KEYBOARD);
  component->set_repair_state(rmad::ComponentRepairState::RMAD_REPAIR_ORIGINAL);
  std::vector<rmad::GetStateReply> fake_states = {
      components_repair_state,
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kComponentsRepair);
        EXPECT_EQ(2, state.components_repair().components_size());
        EXPECT_EQ(state.components_repair().components(0).name(),
                  rmad::ComponentRepairState::RMAD_COMPONENT_KEYBOARD);
        EXPECT_EQ(state.components_repair().components(0).repair_state(),
                  rmad::ComponentRepairState::RMAD_REPAIR_REPLACED);
        EXPECT_EQ(state.components_repair().components(1).name(),
                  rmad::ComponentRepairState::RMAD_COMPONENT_TRACKPAD);
        EXPECT_EQ(state.components_repair().components(1).repair_state(),
                  rmad::ComponentRepairState::RMAD_REPAIR_ORIGINAL);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kSelectComponents);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();

  std::vector<mojom::ComponentPtr> components;
  components.push_back(mojom::Component::New(
      mojom::ComponentType::kKeyboard, mojom::ComponentRepairState::kReplaced));
  components.push_back(mojom::Component::New(
      mojom::ComponentType::kTrackpad, mojom::ComponentRepairState::kOriginal));

  shimless_rma_provider_->SetComponentList(
      std::move(components),
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, mojom::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
            EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetComponentListFromWrongStateFails) {
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

  std::vector<mojom::ComponentPtr> components;
  components.push_back(mojom::Component::New(
      mojom::ComponentType::kKeyboard, mojom::ComponentRepairState::kReplaced));

  shimless_rma_provider_->SetComponentList(
      std::move(components),
      base::BindLambdaForTesting(
          [&](mojom::RmaState state, mojom::RmadErrorCode error) {
            EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
            EXPECT_EQ(error, mojom::RmadErrorCode::kRequestInvalid);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReworkMainboard) {
  rmad::GetStateReply components_repair_state =
      CreateStateReply(rmad::RmadState::kComponentsRepair, rmad::RMAD_ERROR_OK);
  // first component
  rmad::ComponentRepairState* component =
      components_repair_state.mutable_state()
          ->mutable_components_repair()
          ->add_components();
  component->set_name(rmad::ComponentRepairState::RMAD_COMPONENT_TRACKPAD);
  component->set_repair_state(rmad::ComponentRepairState::RMAD_REPAIR_ORIGINAL);
  // second component
  component = components_repair_state.mutable_state()
                  ->mutable_components_repair()
                  ->add_components();
  component->set_name(rmad::ComponentRepairState::RMAD_COMPONENT_KEYBOARD);
  component->set_repair_state(rmad::ComponentRepairState::RMAD_REPAIR_ORIGINAL);
  std::vector<rmad::GetStateReply> fake_states = {
      components_repair_state,
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kComponentsRepair);
        EXPECT_EQ(1, state.components_repair().components_size());
        EXPECT_EQ(state.components_repair().components(0).name(),
                  rmad::ComponentRepairState::RMAD_COMPONENT_MAINBOARD_REWORK);
        EXPECT_EQ(state.components_repair().components(0).repair_state(),
                  rmad::ComponentRepairState::RMAD_REPAIR_REPLACED);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kSelectComponents);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReworkMainboard(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
        run_loop.Quit();
      }));
  run_loop.Run();
}  // namespace shimless_rma

TEST_F(ShimlessRmaServiceTest, ReworkMainboardFromWrongStateFails) {
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

  shimless_rma_provider_->ReworkMainboard(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kRequestInvalid);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReimageRequiredTrue) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kUpdateRoFirmware, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
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
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
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
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
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
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageSkipped(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReimageSkippedWhenRequiredFails) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kUpdateRoFirmware, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageSkipped(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, mojom::RmadErrorCode::kRequestInvalid);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReimageSkippedFromWrongStateFails) {
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
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
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
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageFromDownload(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReimageFromDownloadFromWrongStateFails) {
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

  shimless_rma_provider_->ReimageFromDownload(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kRequestInvalid);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReimageFromUsb) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kUpdateRoFirmware, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
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
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kChooseFirmwareReimageMethod);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReimageFromUsb(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ReimageFromUsbFromWrongStateFails) {
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

  shimless_rma_provider_->ReimageFromUsb(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kRequestInvalid);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, FinalizeAndReboot) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kFinalize, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kFinalize);
        EXPECT_EQ(state.finalize().shutdown(),
                  rmad::FinalizeState::RMAD_FINALIZE_REBOOT);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRepairComplete);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->FinalizeAndReboot(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, FinalizeAndRebootFromWrongStateFails) {
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

  shimless_rma_provider_->FinalizeAndReboot(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kRequestInvalid);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, FinalizeAndShutdown) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kFinalize, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kFinalize);
        EXPECT_EQ(state.finalize().shutdown(),
                  rmad::FinalizeState::RMAD_FINALIZE_SHUTDOWN);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRepairComplete);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->FinalizeAndShutdown(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, FinalizeAndShutdownFromWrongStateFails) {
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

  shimless_rma_provider_->FinalizeAndShutdown(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kRequestInvalid);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, CutoffBattery) {
  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kFinalize, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->check_state_callback =
      base::BindRepeating([](const rmad::RmadState& state) {
        EXPECT_EQ(state.state_case(), rmad::RmadState::kFinalize);
        EXPECT_EQ(state.finalize().shutdown(),
                  rmad::FinalizeState::RMAD_FINALIZE_BATERY_CUTOFF);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kRepairComplete);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->CutoffBattery(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kOk);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, CutoffBatteryFromWrongStateFails) {
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

  shimless_rma_provider_->CutoffBattery(base::BindLambdaForTesting(
      [&](mojom::RmaState state, mojom::RmadErrorCode error) {
        EXPECT_EQ(state, mojom::RmaState::kWelcomeScreen);
        EXPECT_EQ(error, mojom::RmadErrorCode::kRequestInvalid);
        run_loop.Quit();
      }));
  run_loop.Run();
}

class FakeErrorObserver : public mojom::ErrorObserver {
 public:
  void OnError(mojom::RmadErrorCode error) override {
    observations.push_back(error);
  }

  std::vector<mojom::RmadErrorCode> observations;
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
  void OnCalibrationUpdated(mojom::CalibrationComponent component,
                            float progress) override {
    observations.push_back(
        std::pair<mojom::CalibrationComponent, float>(component, progress));
  }

  std::vector<std::pair<mojom::CalibrationComponent, float>> observations;
  mojo::Receiver<mojom::CalibrationObserver> receiver{this};
};

TEST_F(ShimlessRmaServiceTest, ObserveCalibration) {
  FakeCalibrationObserver fake_observer;
  shimless_rma_provider_->ObserveCalibrationProgress(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  fake_rmad_client_()->TriggerCalibrationProgressObservation(
      rmad::CalibrateComponentsState::RMAD_CALIBRATION_COMPONENT_ACCELEROMETER,
      0.25);
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer.observations.size(), 1UL);
}

class FakeProvisioningObserver : public mojom::ProvisioningObserver {
 public:
  void OnProvisioningUpdated(mojom::ProvisioningStep step,
                             float progress) override {
    observations.push_back(
        std::pair<mojom::ProvisioningStep, float>(step, progress));
  }

  std::vector<std::pair<mojom::ProvisioningStep, float>> observations;
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
