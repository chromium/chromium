// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/backend/version_updater.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine.pb.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/network_certificate_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-shared.h"
#include "components/onc/onc_constants.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace shimless_rma {

namespace {
class VersionUpdaterTest : public testing::Test {
 public:
  VersionUpdaterTest() {
    fake_update_engine_client_ = UpdateEngineClient::InitializeFakeForTest();
    cros_network_config_test_helper_ =
        std::make_unique<network_config::CrosNetworkConfigTestHelper>(false);
    InitializeManagedNetworkConfigurationHandler();
    cros_network_config_test_helper().Initialize(
        managed_network_configuration_handler_.get());
    NetworkHandler::Initialize();
    version_updater_ = std::make_unique<VersionUpdater>();
    base::RunLoop().RunUntilIdle();
  }

  ~VersionUpdaterTest() override {
    base::RunLoop().RunUntilIdle();
    version_updater_.reset();
    NetworkHandler::Shutdown();
    cros_network_config_test_helper_.reset();
    managed_network_configuration_handler_.reset();
    network_configuration_handler_.reset();
    network_profile_handler_.reset();
    ui_proxy_config_service_.reset();
    UpdateEngineClient::Shutdown();
  }

 protected:
  void SetupWiFiNetwork() {
    network_state_helper().ConfigureService(
        R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "online",
            "Strength": 50, "AutoConnect": true, "WiFi.HiddenSSID":
            false})");

    base::RunLoop().RunUntilIdle();
  }

  void SetupMeteredNetwork() {
    network_state_helper().ConfigureService(
        R"({"GUID": "cellular_guid", "Type": "cellular", "Technology": "LTE",
            "State": "online"})");

    base::RunLoop().RunUntilIdle();
  }

  void SetCallback() {
    version_updater().SetOsUpdateStatusCallback(
        base::BindRepeating(&VersionUpdaterTest::OnOsUpdateStatusCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  VersionUpdater& version_updater() { return *version_updater_; }
  network_config::CrosNetworkConfigTestHelper&
  cros_network_config_test_helper() {
    return *cros_network_config_test_helper_;
  }

  NetworkStateTestHelper& network_state_helper() {
    return cros_network_config_test_helper_->network_state_helper();
  }

  void InitializeManagedNetworkConfigurationHandler() {
    network_profile_handler_ = NetworkProfileHandler::InitializeForTesting();
    network_configuration_handler_ =
        NetworkConfigurationHandler::InitializeForTest(
            network_state_helper().network_state_handler(),
            cros_network_config_test_helper().network_device_handler());

    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
    ::onc::RegisterProfilePrefs(user_prefs_.registry());
    ::onc::RegisterPrefs(local_state_.registry());

    ui_proxy_config_service_ = std::make_unique<UIProxyConfigService>(
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
        /*network_configs_onc=*/base::Value::List(),
        /*global_network_config=*/base::Value::Dict());

    // Wait until the |managed_network_configuration_handler_| is initialized
    // and set up.
    base::RunLoop().RunUntilIdle();
  }

  void OnOsUpdateStatusCallback(update_engine::Operation operation,
                                double progress,
                                bool rollback,
                                bool powerwash,
                                const std::string& version,
                                int64_t update_size,
                                update_engine::ErrorCode error_code) {
    callback_count_++;
    error_code_ = error_code;
  }

  uint32_t callback_count_ = 0;
  FakeUpdateEngineClient& fake_update_engine_client() {
    return *fake_update_engine_client_;
  }

  update_engine::ErrorCode error_code() { return error_code_; }

 private:
  std::unique_ptr<VersionUpdater> version_updater_;
  std::unique_ptr<network_config::CrosNetworkConfigTestHelper>
      cros_network_config_test_helper_;

  std::unique_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<UIProxyConfigService> ui_proxy_config_service_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;
  raw_ptr<FakeUpdateEngineClient, DanglingUntriaged> fake_update_engine_client_;
  update_engine::ErrorCode error_code_;

  base::test::TaskEnvironment task_environment_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<VersionUpdaterTest> weak_ptr_factory_{this};
};

TEST_F(VersionUpdaterTest, IsIdleWhenUpdateEngineIdle) {
  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::IDLE);
  fake_update_engine_client().set_default_status(status);
  EXPECT_TRUE(version_updater().IsUpdateEngineIdle());
}

TEST_F(VersionUpdaterTest, IsNotIdleWhenUpdateEngineNotIdle) {
  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::DOWNLOADING);
  fake_update_engine_client().set_default_status(status);
  EXPECT_FALSE(version_updater().IsUpdateEngineIdle());
}

TEST_F(VersionUpdaterTest, WithoutNetworkUpdateOsFails) {
  SetCallback();
  EXPECT_FALSE(version_updater().UpdateOs());
}

TEST_F(VersionUpdaterTest, WithNetworkUpdateOsOk) {
  SetCallback();
  SetupWiFiNetwork();
  EXPECT_TRUE(version_updater().UpdateOs());
}

TEST_F(VersionUpdaterTest, WithMeteredNetworkUpdateOsFails) {
  SetCallback();
  SetupMeteredNetwork();
  EXPECT_FALSE(version_updater().UpdateOs());
}

TEST_F(VersionUpdaterTest, CallbackFiresWhenUpdateEngineStatusChanges) {
  if (!features::IsShimlessRMAOsUpdateEnabled()) {
    return;
  }

  SetCallback();
  SetupWiFiNetwork();
  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  fake_update_engine_client().NotifyObserversThatStatusChanged(status);
  EXPECT_EQ(1u, callback_count_);
}

TEST_F(VersionUpdaterTest, UpdateStatusChangedGetError) {
  SetCallback();
  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::REPORTING_ERROR_EVENT);
  status.set_last_attempt_error(
      static_cast<int32_t>(update_engine::ErrorCode::kError));
  version_updater().UpdateStatusChangedForTesting(status);
  EXPECT_EQ(1u, callback_count_);
  EXPECT_EQ(update_engine::ErrorCode::kError, error_code());
}

}  // namespace
}  // namespace shimless_rma
}  // namespace ash
