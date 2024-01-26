// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/wifi_network_configuration/wifi_network_configuration_handler.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/nearby_sharing/wifi_credentials_attachment.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/services/network_config/cros_network_config.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const uint64_t kTestId = 0;
const char kTestSsid[] = "test_SSID";
const char kTestPassword[] = "T35t_P@ssw0rd";
const char kTestNetworkGuid[] = "test_guid";
const char kTestErrorMessage[] = "no_errors";
const WifiCredentialsAttachment::SecurityType kTestSecurityType =
    sharing::mojom::WifiCredentialsMetadata::SecurityType::kWpaPsk;

class FakeCrosNetworkConfig : public ash::network_config::CrosNetworkConfig {
 public:
  explicit FakeCrosNetworkConfig(
      ash::NetworkStateTestHelper* network_state_test_helper)
      : CrosNetworkConfig(
            network_state_test_helper->network_state_handler(),
            network_state_test_helper->network_device_handler(),
            /*cellular_inhibitor=*/nullptr,
            /*cellular_esim_profile_handler=*/nullptr,
            /*network_configuration_handler=*/nullptr,
            /*network_connection_handler=*/nullptr,
            /*network_certificate_handler=*/nullptr,
            /*network_profile_handler=*/nullptr,
            network_state_test_helper->technology_state_controller()) {}

  ~FakeCrosNetworkConfig() override = default;

  void ConfigureNetwork(
      chromeos::network_config::mojom::ConfigPropertiesPtr properties,
      bool shared,
      chromeos::network_config::mojom::CrosNetworkConfig::
          ConfigureNetworkCallback callback) override {
    ++num_configure_network_calls_;
    last_properties_ = std::move(properties);
    last_shared_ = shared;
    std::move(callback).Run(guid_, error_message_);
  }

  void SetOutput(const std::optional<std::string>& network_guid,
                 const std::string& error_message) {
    guid_ = network_guid;
    error_message_ = error_message;
  }

  size_t num_configure_network_calls() const {
    return num_configure_network_calls_;
  }
  const chromeos::network_config::mojom::ConfigPropertiesPtr& last_properties()
      const {
    return last_properties_;
  }
  bool last_shared() const { return last_shared_; }

 private:
  size_t num_configure_network_calls_ = 0;
  chromeos::network_config::mojom::ConfigPropertiesPtr last_properties_;
  bool last_shared_;
  std::optional<std::string> guid_ = "not set";
  std::string error_message_ = "not set";
};

}  // namespace

TEST(WifiNetworkConfigurationHandlerTest, Success) {
  base::test::TaskEnvironment task_environment;
  ash::system::ScopedFakeStatisticsProvider statistics_provider_;
  ash::NetworkStateTestHelper network_state_test_helper{
      /*use_default_devices_and_services=*/true};
  FakeCrosNetworkConfig fake_cros_network_config{&network_state_test_helper};
  fake_cros_network_config.SetOutput(kTestNetworkGuid, kTestErrorMessage);
  ash::network_config::OverrideInProcessInstanceForTesting(
      &fake_cros_network_config);

  WifiNetworkConfigurationHandler handler;
  WifiCredentialsAttachment attachment(kTestId, kTestSecurityType, kTestSsid);
  attachment.set_wifi_password(kTestPassword);

  base::RunLoop run_loop;
  handler.ConfigureWifiNetwork(
      attachment,
      base::BindLambdaForTesting(
          [&run_loop](const std::optional<std::string>& network_guid,
                      const std::string& error_message) {
            EXPECT_EQ(kTestNetworkGuid, network_guid);
            EXPECT_EQ(kTestErrorMessage, error_message);
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_FALSE(fake_cros_network_config.last_shared());
  EXPECT_TRUE(fake_cros_network_config.last_properties()->auto_connect->value);
  EXPECT_EQ(kTestPassword, fake_cros_network_config.last_properties()
                               ->type_config->get_wifi()
                               ->passphrase);
  EXPECT_EQ(kTestSsid, fake_cros_network_config.last_properties()
                           ->type_config->get_wifi()
                           ->ssid);
  EXPECT_EQ(chromeos::network_config::mojom::HiddenSsidMode::kDisabled,
            fake_cros_network_config.last_properties()
                ->type_config->get_wifi()
                ->hidden_ssid);
  EXPECT_EQ(1u, fake_cros_network_config.num_configure_network_calls());
}

TEST(WifiNetworkConfigurationHandlerTest, Failure) {
  base::test::TaskEnvironment task_environment;
  ash::system::ScopedFakeStatisticsProvider statistics_provider_;
  ash::NetworkStateTestHelper network_state_test_helper{
      /*use_default_devices_and_services=*/true};
  FakeCrosNetworkConfig fake_cros_network_config{&network_state_test_helper};

  fake_cros_network_config.SetOutput(/*network_guid=*/std::nullopt,
                                     kTestErrorMessage);
  ash::network_config::OverrideInProcessInstanceForTesting(
      &fake_cros_network_config);

  WifiNetworkConfigurationHandler handler;
  WifiCredentialsAttachment attachment(kTestId, kTestSecurityType, kTestSsid);
  attachment.set_wifi_password(kTestPassword);

  base::RunLoop run_loop;
  handler.ConfigureWifiNetwork(
      attachment,
      base::BindLambdaForTesting(
          [&run_loop](const std::optional<std::string>& network_guid,
                      const std::string& error_message) {
            EXPECT_FALSE(network_guid);
            EXPECT_EQ(kTestErrorMessage, error_message);
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_EQ(1u, fake_cros_network_config.num_configure_network_calls());
}
