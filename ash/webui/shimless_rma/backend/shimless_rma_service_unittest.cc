// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/backend/shimless_rma_service.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/system/diagnostics/diagnostics_browser_delegate.h"
#include "ash/system/diagnostics/diagnostics_log_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/shimless_rma/backend/fake_shimless_rma_delegate.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom-shared.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/rmad/fake_rmad_client.h"
#include "chromeos/ash/components/dbus/rmad/rmad_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine.pb.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {
namespace shimless_rma {

namespace {

using chromeos::DBusMethodCallback;

constexpr char kDefaultWifiGuid[] = "WiFi";

class FakeRmadClientForTest : public FakeRmadClient {
 public:
  static void Initialize() {
    // The this pointer is stored in rmad_client.cc:g_instance and deleted when
    // RmadClient::Shutdown() is called in ShimlessRmaServiceTest::TearDown()
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

  void RecordBrowserActionMetric(
      const rmad::RecordBrowserActionMetricRequest request,
      DBusMethodCallback<rmad::RecordBrowserActionMetricReply> callback)
      override {
    rmad::RecordBrowserActionMetricReply response;
    response.set_error(rmad::RmadErrorCode::RMAD_ERROR_OK);
    std::move(callback).Run(response);

    metric_diagnostics = request.diagnostics();
    metric_os_update = request.os_update();
    ++record_browser_action_metric_counter_;
  }

  uint32_t GetRecordBrowserActionMetricCount() {
    return record_browser_action_metric_counter_;
  }

  bool GetMetricOsUpdate() { return metric_os_update; }

  bool GetMetricDiagnostics() { return metric_diagnostics; }

  base::RepeatingCallback<void(const rmad::RmadState& state)>
      check_state_callback;

 private:
  uint32_t record_browser_action_metric_counter_ = 0;
  bool metric_diagnostics = false;
  bool metric_os_update = false;
};

// A fake DiagnosticsBrowserDelegate.
class FakeDiagnosticsBrowserDelegate
    : public diagnostics::DiagnosticsBrowserDelegate {
 public:
  FakeDiagnosticsBrowserDelegate() = default;
  ~FakeDiagnosticsBrowserDelegate() override = default;

  base::FilePath GetActiveUserProfileDir() override { return base::FilePath(); }
};

}  // namespace

// Test class using NoSessionAshTestBase to ensure shell is available for
// tests requiring DiagnosticsLogController singleton.
class ShimlessRmaServiceTest : public NoSessionAshTestBase {
 public:
  ShimlessRmaServiceTest() {}

  ~ShimlessRmaServiceTest() override {}

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kShimlessRMAOsUpdate}, {});
    chromeos::PowerManagerClient::InitializeFake();
    // VersionUpdater depends on UpdateEngineClient.
    UpdateEngineClient::InitializeFake();

    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<user_manager::FakeUserManager>());

    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    NoSessionAshTestBase::SetUp();
    diagnostics::DiagnosticsLogController::Initialize(
        std::make_unique<FakeDiagnosticsBrowserDelegate>());

    SetupFakeNetwork();
    FakeRmadClientForTest::Initialize();
    rmad_client_ = RmadClient::Get();
    // ShimlessRmaService has to be created after RmadClient or there will be a
    // null ptr dereference in the service constructor.
    shimless_rma_provider_ = std::make_unique<ShimlessRmaService>(
        std::make_unique<FakeShimlessRmaDelegate>());

    version_updater_ = shimless_rma_provider_->GetVersionUpdaterForTesting();

    // Wait until |cros_network_config_test_helper_| has initialized.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    // ShimlessRmaService has to be shutdown before RmadClient or there will be
    // a null ptr dereference in the service destructor.
    shimless_rma_provider_.reset();
    RmadClient::Shutdown();
    NetworkHandler::Shutdown();
    cros_network_config_test_helper_.reset();

    scoped_user_manager_.reset();
    UpdateEngineClient::Shutdown();

    task_environment()->RunUntilIdle();
    NoSessionAshTestBase::TearDown();
  }

  void SetupFakeNetwork() {
    cros_network_config_test_helper_ =
        std::make_unique<network_config::CrosNetworkConfigTestHelper>(false);
    network_configuration_handler_ =
        NetworkConfigurationHandler::InitializeForTest(
            network_state_helper().network_state_handler(),
            cros_network_config_test_helper().network_device_handler());
    managed_network_configuration_handler_ =
        ManagedNetworkConfigurationHandler::InitializeForTesting(
            /*network_state_handler=*/nullptr,
            /*network_profile_handler=*/nullptr,
            /*network_device_handler=*/nullptr,
            network_configuration_handler_.get(),
            /*ui_proxy_config_service=*/nullptr);
    cros_network_config_test_helper().Initialize(
        managed_network_configuration_handler_.get());

    NetworkHandler::Initialize();
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
    return google::protobuf::down_cast<FakeRmadClientForTest*>(
        rmad_client_.get());
  }

  void SetupWiFiNetwork(const std::string& guid) {
    network_state_helper().ConfigureService(
        base::StringPrintf(R"({"GUID": "%s", "Type": "wifi", "State": "online",
            "Strength": 50, "AutoConnect": true, "WiFi.HiddenSSID": false,
            "Profile": "user_profile_path",})",
                           guid.c_str()));

    base::RunLoop().RunUntilIdle();
  }

  void GetCurrentlyConfiguredWifiNetworks(
      NetworkStateHandler::NetworkStateList* list) {
    network_state_handler()->GetNetworkListByType(
        NetworkTypePattern::WiFi(), /*configured_only=*/true,
        /*visible_only=*/true, /*no_limit=*/0, list);
  }

  void ResetFeatures() { scoped_feature_list_.Reset(); }

 protected:
  network_config::CrosNetworkConfigTestHelper&
  cros_network_config_test_helper() {
    return *cros_network_config_test_helper_;
  }

  NetworkStateTestHelper& network_state_helper() {
    return cros_network_config_test_helper_->network_state_helper();
  }

  NetworkStateHandler* network_state_handler() {
    return network_state_helper().network_state_handler();
  }

  TechnologyStateController* technology_state_controller() {
    return network_state_helper().technology_state_controller();
  }

  std::unique_ptr<ShimlessRmaService> shimless_rma_provider_;
  raw_ptr<RmadClient, DanglingUntriaged> rmad_client_ =
      nullptr;  // Unowned convenience pointer.
  raw_ptr<VersionUpdater, DanglingUntriaged> version_updater_ = nullptr;

 private:
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  std::unique_ptr<network_config::CrosNetworkConfigTestHelper>
      cros_network_config_test_helper_;
  std::unique_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  base::test::ScopedFeatureList scoped_feature_list_;
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

  // Initialize current state, can_exit=true, can_go_back=false
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        EXPECT_EQ(state_result_ptr->can_exit, true);
        EXPECT_EQ(state_result_ptr->can_go_back, false);
      }));
  run_loop.RunUntilIdle();

  // Next state, can_exit=false, can_go_back=true
  shimless_rma_provider_->SetSameOwner(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kSelectComponents);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        EXPECT_EQ(state_result_ptr->can_exit, false);
        EXPECT_EQ(state_result_ptr->can_go_back, true);
      }));
  run_loop.RunUntilIdle();
  // Previous state, can_exit=true, can_go_back=false
  shimless_rma_provider_->TransitionPreviousState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        EXPECT_EQ(state_result_ptr->can_exit, true);
        EXPECT_EQ(state_result_ptr->can_go_back, false);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // With a WiFi network it should redirect to kUpdateOs if the update flag is
  // on.
  shimless_rma_provider_->BeginFinalization(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kUpdateOs);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, WelcomePageSkipsOsUpdateIfFlagIsOff) {
  ResetFeatures();

  SetupWiFiNetwork(kDefaultWifiGuid);
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kComponentsRepair,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // With a WiFi network it should redirect to kComponentsRepair if the Os
  // Update flag is off.
  shimless_rma_provider_->BeginFinalization(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kSelectComponents);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // TODO(gavindodd): Create a FakeVersionUpdater so no updates available and
  // update progress can be tested.

  // No network should prompt select network page
  shimless_rma_provider_->BeginFinalization(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kConfigureNetwork);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // No network should prompt select network page
  shimless_rma_provider_->BeginFinalization(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kConfigureNetwork);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  SetupWiFiNetwork(kDefaultWifiGuid);

  // With a WiFi network it should redirect to kUpdateOs
  shimless_rma_provider_->NetworkSelectionComplete(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kUpdateOs);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Make sure the network and OS update pages are skipped when the
// `ShimlessRMAOsUpdate` feature flag is disabled.
TEST_F(ShimlessRmaServiceTest, NetworkPageOsUpdatePageSkipped) {
  ResetFeatures();

  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kComponentsRepair,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // Even without a network connection, the network page will be skipped.
  shimless_rma_provider_->BeginFinalization(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kSelectComponents);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
}

TEST_F(ShimlessRmaServiceTest, ChooseNetworkHasNoNetworkConnection) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kComponentsRepair,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // No network should prompt select network page
  shimless_rma_provider_->BeginFinalization(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kConfigureNetwork);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // With no network it should redirect to next rmad state
  shimless_rma_provider_->NetworkSelectionComplete(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kSelectComponents);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, KeepExistingNetworksWithSavedList) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->SetAbortable(true);

  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // Sets it to `kConfigureNetwork` state.
  shimless_rma_provider_->BeginFinalization(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kConfigureNetwork);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // Simulate 2 saved networks existing before the start of RMA.
  SetupWiFiNetwork("WiFi 1");
  SetupWiFiNetwork("WiFi 2");
  NetworkStateHandler::NetworkStateList configured_networks;
  GetCurrentlyConfiguredWifiNetworks(&configured_networks);
  EXPECT_EQ(2u, configured_networks.size());

  // Snapshot the saved networks before connecting to a network during RMA.
  shimless_rma_provider_->TrackConfiguredNetworks();
  run_loop.RunUntilIdle();

  // End RMA and expect no networks to be removed from the saved networks.
  shimless_rma_provider_->AbortRma(
      base::BindLambdaForTesting([&](rmad::RmadErrorCode error) {
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  GetCurrentlyConfiguredWifiNetworks(&configured_networks);
  EXPECT_EQ(2u, configured_networks.size());
}

TEST_F(ShimlessRmaServiceTest, KeepExistingNetworksWithoutSavedList) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->SetAbortable(true);

  // Simulate 2 saved networks existing before the start of RMA.
  SetupWiFiNetwork("WiFi 1");
  SetupWiFiNetwork("WiFi 2");
  NetworkStateHandler::NetworkStateList configured_networks;
  GetCurrentlyConfiguredWifiNetworks(&configured_networks);
  EXPECT_EQ(2u, configured_networks.size());

  // Skip saving networks and end RMA expecting no networks to be removed from
  // the saved networks.
  base::RunLoop run_loop;
  shimless_rma_provider_->AbortRma(
      base::BindLambdaForTesting([&](rmad::RmadErrorCode error) {
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  GetCurrentlyConfiguredWifiNetworks(&configured_networks);
  EXPECT_EQ(2u, configured_networks.size());
}

TEST_F(ShimlessRmaServiceTest, DropNewNetworks) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->SetAbortable(true);

  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // Sets it to `kConfigureNetwork` state.
  shimless_rma_provider_->BeginFinalization(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kConfigureNetwork);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // Simulate a saved network existing before the start of RMA.
  const std::string saved_network_guid = "WiFi 1";
  SetupWiFiNetwork(saved_network_guid);
  NetworkStateHandler::NetworkStateList configured_networks;
  GetCurrentlyConfiguredWifiNetworks(&configured_networks);
  EXPECT_EQ(1u, configured_networks.size());

  // Snapshot the saved networks before connecting to a network during RMA.
  shimless_rma_provider_->TrackConfiguredNetworks();
  run_loop.RunUntilIdle();

  // Simulate connecting to a new network on the RMA `kConfigureNetwork` page.
  SetupWiFiNetwork("WiFi 2");
  GetCurrentlyConfiguredWifiNetworks(&configured_networks);
  EXPECT_EQ(2u, configured_networks.size());

  shimless_rma_provider_->AbortRma(
      base::BindLambdaForTesting([&](rmad::RmadErrorCode error) {
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  GetCurrentlyConfiguredWifiNetworks(&configured_networks);
  EXPECT_EQ(1u, configured_networks.size());
  EXPECT_EQ(saved_network_guid, configured_networks[0]->guid());
}

TEST_F(ShimlessRmaServiceTest, TransitBackFromNetworkState) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state.
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // No network should direct user to the NetworkPage.
  shimless_rma_provider_->BeginFinalization(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kConfigureNetwork);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));

  run_loop.RunUntilIdle();

  // Transit back to previous state should go to mojom::State::kWelcomeScreen.
  shimless_rma_provider_->TransitionPreviousState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, TransitBackFromOsUpdateState) {
  SetupWiFiNetwork(kDefaultWifiGuid);
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state.
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // User already connects to network and will go to os update page.
  shimless_rma_provider_->BeginFinalization(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kUpdateOs);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // Transit back to previous state should go to mojom::State::kWelcomeScreen.
  shimless_rma_provider_->TransitionPreviousState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        run_loop.Quit();
      }));

  run_loop.Run();
}

// User has seen the NetworkPage and has selected a network. Clicking back
// button from the next page will direct user back to NetworkPage.
TEST_F(ShimlessRmaServiceTest, SelectNetworkAndGoBack) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kComponentsRepair,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // No network should direct user to the NetworkPage
  shimless_rma_provider_->BeginFinalization(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kConfigureNetwork);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // Simulate connecting to a new network on the NetworkPage.
  SetupWiFiNetwork("WiFi 1");
  NetworkStateHandler::NetworkStateList configured_networks;
  GetCurrentlyConfiguredWifiNetworks(&configured_networks);
  EXPECT_EQ(1u, configured_networks.size());

  // Simulate that there is no OS update available.
  version_updater_->DisableUpdateOnceForTesting();

  // Complete network selection. User should go to the SelectComponentPage
  shimless_rma_provider_->NetworkSelectionComplete(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kSelectComponents);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // Transition to previous page. User should be back to the NetworkPage.
  shimless_rma_provider_->TransitionPreviousState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kConfigureNetwork);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// User has seen the NetworkPage but does not select a network. Clicking back
// button from the next page will direct user back to NetworkPage.
TEST_F(ShimlessRmaServiceTest, DoNotSelectNetworkAndGoBack) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kComponentsRepair,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state.
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // No network should direct user to the NetworkPage.
  shimless_rma_provider_->BeginFinalization(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kConfigureNetwork);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // Complete network selection without selecting any network.
  shimless_rma_provider_->NetworkSelectionComplete(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kSelectComponents);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // Transition to previous page. User should be back to the NetworkPage.
  shimless_rma_provider_->TransitionPreviousState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kConfigureNetwork);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// When Network is already connected and user did not see the NetworkPage.
// Clicking back button from the next page will skip NetworkPage and direct
// user back to the WelcomePage.
TEST_F(ShimlessRmaServiceTest, WithNetworkConnectedAndGoBack) {
  SetupWiFiNetwork(kDefaultWifiGuid);
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kComponentsRepair,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Initialize current state.
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // Simulate that there is no OS update available.
  version_updater_->DisableUpdateOnceForTesting();

  // Since network is already connected. User will skip the NetworkPage
  // and go to SelectComponentPage directly.
  shimless_rma_provider_->BeginFinalization(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kSelectComponents);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // Transition to previous page. User should be back to the WelcomePage
  // directly.
  shimless_rma_provider_->TransitionPreviousState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Navigating to the SelectNetwork page should enable WiFi if it's currently
// disabled.
TEST_F(ShimlessRmaServiceTest, SelectNetworkTurnsOnWiFi) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kComponentsRepair,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;

  // Simulate disabling WiFi.
  technology_state_controller()->SetTechnologiesEnabled(
      NetworkTypePattern::WiFi(), /*enabled=*/false,
      network_handler::ErrorCallback());
  EXPECT_FALSE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  // Initialize current state.
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // No network should direct user to the NetworkPage.
  shimless_rma_provider_->BeginFinalization(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kConfigureNetwork);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  // After transitioning to the select network page, WiFi should be enabled.
  EXPECT_TRUE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));
}

TEST_F(ShimlessRmaServiceTest, GetCurrentState) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(CreateStateReply(rmad::RmadState::kDeviceDestination,
                                         rmad::RMAD_ERROR_OK));
  fake_states.push_back(CreateStateReply(rmad::RmadState::kComponentsRepair,
                                         rmad::RMAD_ERROR_OK));
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetCurrentStateNoRma) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kUnknown);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_RMA_NOT_REQUIRED);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->SetSameOwner(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kSelectComponents);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->TransitionPreviousState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->TransitionPreviousState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_TRANSITION_FAILED);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, TransitionPreviousStateWithNoPrevStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->TransitionPreviousState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_TRANSITION_FAILED);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, CanExitRma) {
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

TEST_F(ShimlessRmaServiceTest, AbortRmaRequestsFullReboot) {
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

  EXPECT_EQ(
      1, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
}

TEST_F(ShimlessRmaServiceTest,
       CriticalErrorExitToLoginDoesntRequestFullReboot) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->SetAbortable(true);
  shimless_rma_provider_->SetCriticalErrorOccurredForTest(true);
  base::RunLoop run_loop;
  shimless_rma_provider_->CriticalErrorExitToLogin(
      base::BindLambdaForTesting([&](rmad::RmadErrorCode error) {
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(
      0, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
}

TEST_F(ShimlessRmaServiceTest,
       ShutDownAfterHardwareErrorInFinalizationRequestsShutdown) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kFinalize, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->SetAbortable(true);
  base::RunLoop run_loop;

  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kFinalize);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ShutDownAfterHardwareError();
  run_loop.RunUntilIdle();

  EXPECT_EQ(
      1, chromeos::FakePowerManagerClient::Get()->num_request_shutdown_calls());
}

TEST_F(ShimlessRmaServiceTest,
       ShutDownAfterHardwareErrorInProvisioningRequestsShutdown) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kProvisionDevice, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->SetAbortable(true);
  base::RunLoop run_loop;

  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kProvisionDevice);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ShutDownAfterHardwareError();
  run_loop.RunUntilIdle();

  EXPECT_EQ(
      1, chromeos::FakePowerManagerClient::Get()->num_request_shutdown_calls());
}

TEST_F(ShimlessRmaServiceTest, CriticalErrorRebootRequestsFullReboot) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  fake_rmad_client_()->SetAbortable(true);
  shimless_rma_provider_->SetCriticalErrorOccurredForTest(true);
  base::RunLoop run_loop;
  shimless_rma_provider_->CriticalErrorReboot(
      base::BindLambdaForTesting([&](rmad::RmadErrorCode error) {
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(
      1, chromeos::FakePowerManagerClient::Get()->num_request_restart_calls());
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetSameOwner(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kRestock);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetSameOwnerFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRestock, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kRestock);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetSameOwner(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kRestock);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetDifferentOwner(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kRestock);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseWipeDevice);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  const bool expected_wipe_device = true;
  shimless_rma_provider_->SetWipeDevice(
      expected_wipe_device,
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetDifferentOwnerFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRestock, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kRestock);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  shimless_rma_provider_->SetDifferentOwner(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kRestock);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kChooseWriteProtectDisableMethod);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetManuallyDisableWriteProtect(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetManuallyDisableWriteProtect(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kChooseWriteProtectDisableMethod);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetRsuDisableWriteProtect(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetRsuDisableWriteProtectFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetRsuDisableWriteProtect(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kEnterRSUWPDisableCode);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kEnterRSUWPDisableCode);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  const std::string challenge_url = "https://challenge/url";
  rmad::GetStateReply write_protect_disable_rsu_state =
      CreateStateReply(rmad::RmadState::kWpDisableRsu, rmad::RMAD_ERROR_OK);
  write_protect_disable_rsu_state.mutable_state()
      ->mutable_wp_disable_rsu()
      ->set_challenge_url(challenge_url);
  const std::vector<rmad::GetStateReply> fake_states = {
      write_protect_disable_rsu_state};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kEnterRSUWPDisableCode);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
  std::vector<uint8_t> expected_qrcode_data = {
      104, 116, 116, 112, 115, 58,  47, 47,  99,  104, 97,
      108, 108, 101, 110, 103, 101, 47, 117, 114, 108};
  shimless_rma_provider_->GetRsuDisableWriteProtectChallengeQrCode(
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& qr_code_data) {
        EXPECT_EQ(qr_code_data.size(), expected_qrcode_data.size());
        EXPECT_EQ(qr_code_data, expected_qrcode_data);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kEnterRSUWPDisableCode);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetRsuDisableWriteProtectCode(
      "test RSU unlock code",
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetRsuDisableWriteProtectCode(
      "test RSU unlock code",
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kWaitForManualWPDisable);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->WriteProtectManuallyDisabled(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->WriteProtectManuallyDisabled(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWPDisableComplete);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ConfirmManualWpDisableComplete(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ConfirmManualWpDisableComplete(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWPDisableComplete);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kSelectComponents);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kSelectComponents);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetComponentListFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  std::vector<rmad::ComponentsRepairState::ComponentRepairStatus> components(1);
  components[0].set_component(rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD);
  components[0].set_repair_status(
      rmad::ComponentsRepairState::ComponentRepairStatus::
          RMAD_REPAIR_STATUS_REPLACED);

  shimless_rma_provider_->SetComponentList(
      std::move(components),
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kSelectComponents);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReworkMainboard(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}  // namespace shimless_rma

TEST_F(ShimlessRmaServiceTest, ReworkMainboardFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ReworkMainboard(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kUpdateRoFirmware);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->RoFirmwareUpdateComplete(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, RoFirmwareUpdateCompleteFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->RoFirmwareUpdateComplete(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kRestock);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ShutdownForRestock(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ShutdownForRestockFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ShutdownForRestock(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kRestock);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ContinueFinalizationAfterRestock(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ContinueFinalizationAfterRestock(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetSkuList(
      base::BindLambdaForTesting([&](const std::vector<uint64_t>& skus) {
        EXPECT_EQ(skus.size(), 0UL);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetCustomLabelList) {
  rmad::GetStateReply update_device_info_state =
      CreateStateReply(rmad::RmadState::kUpdateDeviceInfo, rmad::RMAD_ERROR_OK);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->add_custom_label_list("Custom-label 1");
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->add_custom_label_list("Custom-label 5");
  const std::vector<rmad::GetStateReply> fake_states = {
      update_device_info_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetCustomLabelList(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& custom_labels) {
        EXPECT_EQ(custom_labels.size(), 2UL);
        EXPECT_EQ(custom_labels[0], "Custom-label 1");
        EXPECT_EQ(custom_labels[1], "Custom-label 5");
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetCustomLabelListWrongStateEmpty) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetCustomLabelList(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& custom_labels) {
        EXPECT_EQ(custom_labels.size(), 0UL);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetSkuDescriptionList) {
  rmad::GetStateReply update_device_info_state =
      CreateStateReply(rmad::RmadState::kUpdateDeviceInfo, rmad::RMAD_ERROR_OK);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->add_sku_description_list("SKU 1");
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->add_sku_description_list("SKU 2");
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->add_sku_description_list("SKU 3");
  const std::vector<rmad::GetStateReply> fake_states = {
      update_device_info_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetSkuDescriptionList(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& sku_descriptions) {
        EXPECT_EQ(sku_descriptions.size(), 3UL);
        EXPECT_EQ(sku_descriptions[0], "SKU 1");
        EXPECT_EQ(sku_descriptions[1], "SKU 2");
        EXPECT_EQ(sku_descriptions[2], "SKU 3");
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetSkuDescriptionListWrongStateEmpty) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetSkuDescriptionList(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& sku_descriptions) {
        EXPECT_EQ(sku_descriptions.size(), 0UL);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalSku(
      base::BindLambdaForTesting([&](int32_t sku) {
        EXPECT_EQ(sku, 0);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalCustomLabel) {
  rmad::GetStateReply update_device_info_state =
      CreateStateReply(rmad::RmadState::kUpdateDeviceInfo, rmad::RMAD_ERROR_OK);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->set_original_custom_label_index(3);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->set_custom_label_index(1);
  const std::vector<rmad::GetStateReply> fake_states = {
      update_device_info_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalCustomLabel(
      base::BindLambdaForTesting([&](int32_t custom_label) {
        EXPECT_EQ(custom_label, 3);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalCustomLabelFromWrongStateEmpty) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalCustomLabel(
      base::BindLambdaForTesting([&](int32_t custom_label) {
        EXPECT_EQ(custom_label, 0);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalDramPartNumber(
      base::BindLambdaForTesting([&](const std::string& part_number) {
        EXPECT_EQ(part_number, "123-456-789");
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalFeatureLevel) {
  rmad::GetStateReply update_device_info_state =
      CreateStateReply(rmad::RmadState::kUpdateDeviceInfo, rmad::RMAD_ERROR_OK);
  update_device_info_state.mutable_state()
      ->mutable_update_device_info()
      ->set_original_feature_level(
          rmad::UpdateDeviceInfoState_FeatureLevel::
              UpdateDeviceInfoState_FeatureLevel_RMAD_FEATURE_LEVEL_0);
  const std::vector<rmad::GetStateReply> fake_states = {
      update_device_info_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalFeatureLevel(base::BindLambdaForTesting(
      [&](rmad::UpdateDeviceInfoState::FeatureLevel feature_level) {
        EXPECT_EQ(feature_level,
                  rmad::UpdateDeviceInfoState::RMAD_FEATURE_LEVEL_0);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalFeatureLevelFromWrongStateEmpty) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetOriginalFeatureLevel(base::BindLambdaForTesting(
      [&](rmad::UpdateDeviceInfoState::FeatureLevel feature_level) {
        EXPECT_EQ(feature_level,
                  rmad::UpdateDeviceInfoState::RMAD_FEATURE_LEVEL_UNSUPPORTED);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, GetOriginalDramPartNumberFromWrongStateEmpty) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
        EXPECT_EQ(state.update_device_info().custom_label_index(), 3L);
        EXPECT_EQ(state.update_device_info().dram_part_number(), "123-456-789");
        EXPECT_EQ(state.update_device_info().is_chassis_branded(), true);
        EXPECT_EQ(state.update_device_info().hw_compliance_version(), 22u);
      });
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kUpdateDeviceInformation);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetDeviceInformation(
      "serial number", 1, 2, 3, "123-456-789", true, 22u,
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, SetDeviceInformationFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SetDeviceInformation(
      "serial number", 1, 2, 3, "123-456-789", false, 0,
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kCheckCalibration);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kSetupCalibration);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kCheckCalibration);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kSetupCalibration);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->RunCalibrationStep(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, RunCalibrationStepFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->RunCalibrationStep(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kRunCalibration);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ContinueCalibration(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ContinueCalibrationFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ContinueCalibration(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kRunCalibration);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->CalibrationComplete(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, CalibrationCompleteFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->CalibrationComplete(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kProvisionDevice);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ProvisioningComplete(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, ProvisioningCompleteFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->ProvisioningComplete(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kProvisionDevice);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->RetryProvisioning(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kFinalize);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->FinalizationComplete(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kFinalize);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->RetryFinalization(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, FinalizationCompleteFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->FinalizationComplete(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state,
                  mojom::State::kWaitForManualWPEnable);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->WriteProtectManuallyEnabled(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, WriteProtectManuallyEnabledFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->WriteProtectManuallyEnabled(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kRepairComplete);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->GetLog(base::BindLambdaForTesting(
      [&](const std::string& log, rmad::RmadErrorCode error) {
        EXPECT_EQ(log, expected_log);
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();
}

TEST_F(ShimlessRmaServiceTest, SaveLog) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRepairComplete, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  const std::unique_ptr<base::FilePath> expected_save_path =
      std::make_unique<base::FilePath>(
          FILE_PATH_LITERAL("log/save/path/for/testing"));

  EXPECT_TRUE(fake_rmad_client_()->GetDiagnosticsLogsText().empty());
  fake_rmad_client_()->SetSaveLogReply(expected_save_path->value(),
                                       rmad::RMAD_ERROR_OK);
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kRepairComplete);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->SaveLog(base::BindLambdaForTesting(
      [&](const base::FilePath& save_path, rmad::RmadErrorCode error) {
        EXPECT_EQ(save_path, *expected_save_path.get());
        EXPECT_EQ(error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_FALSE(fake_rmad_client_()->GetDiagnosticsLogsText().empty());
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kRepairComplete);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kRepairComplete);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->EndRma(
      rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_SHUTDOWN,
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, EndRmaAndRebootFromWrongStateFails) {
  const std::vector<rmad::GetStateReply> fake_states = {CreateStateReply(
      rmad::RmadState::kDeviceDestination, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  shimless_rma_provider_->EndRma(
      rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_SHUTDOWN,
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kChooseDestination);
        EXPECT_EQ(state_result_ptr->error,
                  rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShimlessRmaServiceTest, LaunchDiagnosticsSendsMetric) {
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kRepairComplete, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));
  base::RunLoop run_loop;
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kRepairComplete);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop.RunUntilIdle();

  fake_rmad_client_()->SetRecordBrowserActionMetricReply(rmad::RMAD_ERROR_OK);

  EXPECT_EQ(0u, fake_rmad_client_()->GetRecordBrowserActionMetricCount());

  shimless_rma_provider_->LaunchDiagnostics();
  run_loop.RunUntilIdle();

  EXPECT_EQ(1u, fake_rmad_client_()->GetRecordBrowserActionMetricCount());
  EXPECT_TRUE(fake_rmad_client_()->GetMetricDiagnostics());
  EXPECT_FALSE(fake_rmad_client_()->GetMetricOsUpdate());
}

TEST_F(ShimlessRmaServiceTest, OsUpdateSendsMetric) {
  // Connect to wifi so that we can skip the network page.
  SetupWiFiNetwork(kDefaultWifiGuid);

  // Since OS Update is only a mojo state, and not an rmad state, we have
  // to start at the landing page, and skip two pages to get to OS Update.
  const std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK)};
  fake_rmad_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop_1;

  // Initialize current state
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kWelcomeScreen);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
      }));
  run_loop_1.RunUntilIdle();

  // Move to the next page. This should be the OS Update page.
  shimless_rma_provider_->BeginFinalization(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kUpdateOs);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
        run_loop_1.Quit();
      }));
  run_loop_1.Run();

  // Now, actually test the OS Update page.
  base::RunLoop run_loop_2;

  fake_rmad_client_()->SetRecordBrowserActionMetricReply(rmad::RMAD_ERROR_OK);

  EXPECT_EQ(0u, fake_rmad_client_()->GetRecordBrowserActionMetricCount());

  shimless_rma_provider_->UpdateOs(
      base::BindLambdaForTesting([&](bool update_started) {
        EXPECT_TRUE(update_started);
        run_loop_2.Quit();
      }));
  run_loop_2.Run();

  EXPECT_EQ(1u, fake_rmad_client_()->GetRecordBrowserActionMetricCount());
  EXPECT_TRUE(fake_rmad_client_()->GetMetricOsUpdate());
  EXPECT_FALSE(fake_rmad_client_()->GetMetricDiagnostics());
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
    if (receiver.is_bound()) {
      receiver.reset();
    }

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
  shimless_rma_provider_->GetCurrentState(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kSetupCalibration);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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

  shimless_rma_provider_->RunCalibrationStep(
      base::BindLambdaForTesting([&](mojom::StateResultPtr state_result_ptr) {
        EXPECT_EQ(state_result_ptr->state, mojom::State::kRunCalibration);
        EXPECT_EQ(state_result_ptr->error, rmad::RmadErrorCode::RMAD_ERROR_OK);
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

class FakeExternalDiskStateObserver : public mojom::ExternalDiskStateObserver {
 public:
  void OnExternalDiskStateChanged(bool detected) override {
    observations.push_back(detected);
  }

  std::vector<bool> observations;
  mojo::Receiver<mojom::ExternalDiskStateObserver> receiver{this};
};

TEST_F(ShimlessRmaServiceTest, ObserveExternalDiskState) {
  FakeExternalDiskStateObserver fake_observer1;
  FakeExternalDiskStateObserver fake_observer2;

  // Shimless is expected to support multiple ExternalDiskState observers.
  shimless_rma_provider_->ObserveExternalDiskState(
      fake_observer1.receiver.BindNewPipeAndPassRemote());
  shimless_rma_provider_->ObserveExternalDiskState(
      fake_observer2.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  fake_rmad_client_()->TriggerExternalDiskStateObservation(true);
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer1.observations.size(), 1UL);
  EXPECT_EQ(fake_observer1.observations[0], true);
  EXPECT_EQ(fake_observer2.observations.size(), 1UL);
  EXPECT_EQ(fake_observer2.observations[0], true);
}

TEST_F(ShimlessRmaServiceTest, ObserveExternalDiskStateAfterSignal) {
  FakeExternalDiskStateObserver fake_observer1;
  FakeExternalDiskStateObserver fake_observer2;
  fake_rmad_client_()->TriggerExternalDiskStateObservation(true);

  // Shimless is expected to support multiple ExternalDiskState observers.
  shimless_rma_provider_->ObserveExternalDiskState(
      fake_observer1.receiver.BindNewPipeAndPassRemote());
  shimless_rma_provider_->ObserveExternalDiskState(
      fake_observer2.receiver.BindNewPipeAndPassRemote());
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_EQ(fake_observer1.observations.size(), 2UL);
  EXPECT_EQ(fake_observer1.observations[0], true);
  EXPECT_EQ(fake_observer1.observations[1], true);
  EXPECT_EQ(fake_observer2.observations.size(), 1UL);
  EXPECT_EQ(fake_observer2.observations[0], true);
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
