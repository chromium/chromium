// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/arc_wifi_host_impl.h"

#include <string>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/fake_cert_manager.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace arc {
namespace {
constexpr char kBSSID[] = "bssid";
constexpr char kHexSSID[] = "123456";
constexpr char kIPAddress1[] = "192.168.2.1";
constexpr char kGateway[] = "192.168.0.1";
constexpr char kNameServer0[] = "1.1.1.1";
constexpr char kNameServer1[] = "2.2.2.2";
constexpr char kSearchDomain0[] = "example.com";
constexpr char kSearchDomain1[] = "chromium.org";
constexpr char kAllowedBSSID0[] = "00:11:22:33:44:55";
constexpr char kAllowedBSSID1[] = "66:77:88:99:AA:BB";
constexpr char kSecurityEAP[] = "WPA-EAP";
constexpr char kSecurityNone[] = "None";
constexpr char kPassphrase[] = "passphrase";
constexpr char kInvalidGuid[] = "nonexist_guid";
constexpr uint8_t kPrefixLen = 16;

arc::mojom::WifiConfigurationPtr CreateWifiConfigWithoutEAP() {
  auto mojo = arc::mojom::WifiConfiguration::New();
  mojo->hexssid = kHexSSID;
  mojo->security = kSecurityNone;
  mojo->bssid_allowlist = {kAllowedBSSID0, kAllowedBSSID1};
  mojo->dns_servers = {kNameServer0, kNameServer1};
  mojo->domains = {kSearchDomain0, kSearchDomain1};
  mojo->metered_override = arc::mojom::MeteredOverride::kMetered;

  mojo->static_ipv4_config = arc::mojom::StaticIpv4Configuration::New();
  mojo->static_ipv4_config->ipv4_addr = kIPAddress1;
  mojo->static_ipv4_config->gateway_ipv4_addr = kGateway;
  mojo->static_ipv4_config->prefix_length = kPrefixLen;

  auto configured = arc::mojom::ConfiguredNetworkDetails::New();
  configured->bssid = kBSSID;
  configured->autoconnect = true;
  mojo->details =
      arc::mojom::NetworkDetails::NewConfigured(std::move(configured));
  return mojo;
}

arc::mojom::WifiConfigurationPtr CreateWifiConfigWithEAP() {
  auto mojo = CreateWifiConfigWithoutEAP();
  mojo->security = kSecurityEAP;
  mojo->details->get_configured()->passphrase = kPassphrase;

  mojo->eap = arc::mojom::EapCredentials::New();
  mojo->eap->method = arc::mojom::EapMethod::kTls;
  mojo->eap->phase2_method = arc::mojom::EapPhase2Method::kMschapv2;
  return mojo;
}

void VerifyNetworkConfigurationWithoutEAP(
    arc::mojom::NetworkConfigurationPtr mojo) {
  EXPECT_EQ(mojo->type, arc::mojom::NetworkType::WIFI);
  EXPECT_FALSE(!mojo->wifi);
  EXPECT_EQ(mojo->wifi->hex_ssid, kHexSSID);
  EXPECT_EQ(mojo->wifi->security, arc::mojom::SecurityType::NONE);
}

class ArcWifiHostImplTest : public testing::Test {
 public:
  ArcWifiHostImplTest(const ArcWifiHostImplTest&) = delete;
  ArcWifiHostImplTest& operator=(const ArcWifiHostImplTest&) = delete;

 protected:
  ArcWifiHostImplTest() {}

  ~ArcWifiHostImplTest() override = default;

  void SetUp() override {
    // Set up UserManager to fake the login state.
    ash::LoginState::Initialize();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<user_manager::FakeUserManager>());

    // Required for initializingFakeShillManagerClient.
    ash::shill_clients::InitializeFakes();
    ash::ShillManagerClient::Get()
        ->GetTestInterface()
        ->SetupDefaultEnvironment();

    helper_ = std::make_unique<ash::NetworkHandlerTestHelper>();
    helper_->AddDefaultProfiles();
    helper_->ResetDevicesAndServices();

    ash::NetworkHandler::Get()
        ->managed_network_configuration_handler()
        ->SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY,
                    /*userhash=*/std::string(),
                    /*network_configs_onc=*/base::Value::List(),
                    /*global_network_config=*/base::Value::Dict());

    bridge_service_ = std::make_unique<ArcBridgeService>();
    service_ =
        std::make_unique<ArcWifiHostImpl>(nullptr, bridge_service_.get());
    auto cert_manager = std::make_unique<FakeCertManager>();
    service_->SetCertManager(std::move(cert_manager));
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    service_->Shutdown();
    helper_.reset();
    scoped_user_manager_.reset();
    ash::shill_clients::Shutdown();
    ash::LoginState::Shutdown();
  }

  ArcWifiHostImpl* service() { return service_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ArcBridgeService> bridge_service_;
  std::unique_ptr<ArcWifiHostImpl> service_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<ash::NetworkHandlerTestHelper> helper_;
};

TEST_F(ArcWifiHostImplTest, ToggleWifiEnabledState) {
  base::test::TestFuture<bool> future;
  // Enable WiFi and get WiFi state.
  service()->SetWifiEnabledState(true, future.GetCallback());
  ASSERT_TRUE(future.Take());
  service()->GetWifiEnabledState(future.GetCallback());
  ASSERT_TRUE(future.Take());

  // Disable WiFi and get WiFi state.
  service()->SetWifiEnabledState(false, future.GetCallback());
  ASSERT_TRUE(future.Take());
  service()->GetWifiEnabledState(future.GetCallback());
  ASSERT_FALSE(future.Take());
}

TEST_F(ArcWifiHostImplTest, CreateNetwork) {
  base::test::TestFuture<const std::string&> future;

  // Test that network can be created with empty EAP credentials.
  auto config = CreateWifiConfigWithoutEAP();
  service()->CreateNetwork(std::move(config), future.GetCallback());
  ASSERT_FALSE(future.Take().empty());

  // Test that network can be created with EAP credentials.
  auto config_eap = CreateWifiConfigWithEAP();
  service()->CreateNetwork(std::move(config_eap), future.GetCallback());
  ASSERT_FALSE(future.Take().empty());

  // Test that network cannot be created with invalid configuration (no SSID).
  auto config_invalid = CreateWifiConfigWithoutEAP();
  config_invalid->hexssid.reset();
  service()->CreateNetwork(std::move(config_invalid), future.GetCallback());
  ASSERT_TRUE(future.Take().empty());
}

TEST_F(ArcWifiHostImplTest, ForgetNetwork) {
  base::test::TestFuture<const std::string&> create_network_future;
  base::test::TestFuture<arc::mojom::NetworkResult> forget_network_future;

  // Test that ForgetNetwork() call will return FAILURE when guid is invalid.
  service()->ForgetNetwork(kInvalidGuid, forget_network_future.GetCallback());
  EXPECT_EQ(forget_network_future.Take(), arc::mojom::NetworkResult::FAILURE);

  // Test that ForgetNetwork() call will return SUCCESS when guid is valid.
  auto config = CreateWifiConfigWithoutEAP();
  service()->CreateNetwork(std::move(config),
                           create_network_future.GetCallback());
  auto guid = create_network_future.Take();
  EXPECT_FALSE(guid.empty());

  service()->ForgetNetwork(guid, forget_network_future.GetCallback());
  EXPECT_EQ(forget_network_future.Take(), arc::mojom::NetworkResult::SUCCESS);
}

TEST_F(ArcWifiHostImplTest, UpdateWifiNetwork) {
  base::test::TestFuture<const std::string&> create_network_future;
  base::test::TestFuture<arc::mojom::NetworkResult> update_network_future;

  // Test that UpdateWifiNetwork() call will return FAILURE when guid is
  // invalid.
  auto config = CreateWifiConfigWithoutEAP();
  service()->UpdateWifiNetwork(kInvalidGuid, std::move(config),
                               update_network_future.GetCallback());
  EXPECT_EQ(update_network_future.Take(), arc::mojom::NetworkResult::FAILURE);

  // Test that UpdateWifiNetwork() call will return SUCCESS when guid is valid.
  auto create_network_config = CreateWifiConfigWithoutEAP();
  service()->CreateNetwork(std::move(create_network_config),
                           create_network_future.GetCallback());
  auto guid = create_network_future.Take();
  EXPECT_FALSE(guid.empty());

  auto updated_config = CreateWifiConfigWithoutEAP();
  updated_config->bssid_allowlist = {kAllowedBSSID0};
  service()->UpdateWifiNetwork(guid, std::move(updated_config),
                               update_network_future.GetCallback());
  EXPECT_EQ(update_network_future.Take(), arc::mojom::NetworkResult::SUCCESS);
}

TEST_F(ArcWifiHostImplTest, GetConfiguredWifiServices) {
  base::test::TestFuture<const std::string&> create_network_future;
  base::test::TestFuture<std::vector<arc::mojom::NetworkConfigurationPtr>>
      get_configured_network_future;

  auto config = CreateWifiConfigWithoutEAP();
  service()->CreateNetwork(std::move(config),
                           create_network_future.GetCallback());
  auto target_guid = create_network_future.Take();
  EXPECT_FALSE(target_guid.empty());

  service()->GetConfiguredWifiServices(
      get_configured_network_future.GetCallback());
  auto response = get_configured_network_future.Take();
  bool found = false;
  for (auto& network : response) {
    if (network->guid == target_guid) {
      found = true;
      VerifyNetworkConfigurationWithoutEAP(std::move(network));
    }
  }
  EXPECT_TRUE(found);
}

}  // namespace
}  // namespace arc
