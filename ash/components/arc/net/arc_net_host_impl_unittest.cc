// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/arc_net_host_impl.h"

#include <string>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/fake_cert_manager.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/patchpanel/fake_patchpanel_client.h"
#include "chromeos/ash/components/dbus/patchpanel/patchpanel_client.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  mojo->eap->method = arc::mojom::EapMethod::kTtls;
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

class ArcNetHostImplTest : public testing::Test {
 public:
  ArcNetHostImplTest(const ArcNetHostImplTest&) = delete;
  ArcNetHostImplTest& operator=(const ArcNetHostImplTest&) = delete;

 protected:
  ArcNetHostImplTest()
      : arc_service_manager_(std::make_unique<ArcServiceManager>()),
        context_(std::make_unique<user_prefs::TestBrowserContextWithPrefs>()),
        service_(
            ArcNetHostImpl::GetForBrowserContextForTesting(context_.get())) {
    arc::prefs::RegisterProfilePrefs(pref_service()->registry());
    service()->SetPrefService(pref_service());
  }

  ~ArcNetHostImplTest() override { service_->Shutdown(); }

  void SetUp() override {
    ash::PatchPanelClient::InitializeFake();

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
    auto cert_manager = std::make_unique<FakeCertManager>();
    service_->SetCertManager(std::move(cert_manager));
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    helper_.reset();
    ash::shill_clients::Shutdown();
    scoped_user_manager_.reset();
    ash::LoginState::Shutdown();
    ash::PatchPanelClient::Shutdown();
  }

  ArcNetHostImpl* service() { return service_; }
  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<user_prefs::TestBrowserContextWithPrefs> context_;
  const raw_ptr<ArcNetHostImpl> service_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<ash::NetworkHandlerTestHelper> helper_;
};

TEST_F(ArcNetHostImplTest, SetAlwaysOnVpn_SetPackage) {
  EXPECT_EQ(false, pref_service()->GetBoolean(prefs::kAlwaysOnVpnLockdown));
  EXPECT_EQ("", pref_service()->GetString(prefs::kAlwaysOnVpnPackage));

  const std::string vpn_package = "com.android.vpn";
  const bool lockdown = true;

  service()->SetAlwaysOnVpn(vpn_package, lockdown);

  EXPECT_EQ(lockdown, pref_service()->GetBoolean(prefs::kAlwaysOnVpnLockdown));
  EXPECT_EQ(vpn_package, pref_service()->GetString(prefs::kAlwaysOnVpnPackage));
}

TEST_F(ArcNetHostImplTest, NotifyAndroidWifiMulticastLockChange) {
  int cnt1 = ash::FakePatchPanelClient::Get()
                 ->GetAndroidWifiMulticastLockChangeNotifyCount();
  service()->NotifyAndroidWifiMulticastLockChange(true);
  int cnt2 = ash::FakePatchPanelClient::Get()
                 ->GetAndroidWifiMulticastLockChangeNotifyCount();

  service()->NotifyAndroidWifiMulticastLockChange(false);
  int cnt3 = ash::FakePatchPanelClient::Get()
                 ->GetAndroidWifiMulticastLockChangeNotifyCount();

  EXPECT_EQ(1, cnt2 - cnt1);
  EXPECT_EQ(1, cnt3 - cnt2);
}

TEST_F(ArcNetHostImplTest, CreateNetwork) {
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

TEST_F(ArcNetHostImplTest, ForgetNetwork) {
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

TEST_F(ArcNetHostImplTest, UpdateWifiNetwork) {
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

TEST_F(ArcNetHostImplTest, GetConfiguredNetworks) {
  base::test::TestFuture<const std::string&> create_network_future;
  base::test::TestFuture<arc::mojom::GetNetworksResponseTypePtr>
      get_configured_network_future;

  auto config = CreateWifiConfigWithoutEAP();
  service()->CreateNetwork(std::move(config),
                           create_network_future.GetCallback());
  auto target_guid = create_network_future.Take();
  EXPECT_FALSE(target_guid.empty());

  service()->GetNetworks(mojom::GetNetworksRequestType::CONFIGURED_ONLY,
                         get_configured_network_future.GetCallback());
  auto response = get_configured_network_future.Take();
  EXPECT_EQ(response->status, arc::mojom::NetworkResult::SUCCESS);
  bool found = false;
  for (auto& network : response->networks) {
    if (network->guid == target_guid) {
      found = true;
      VerifyNetworkConfigurationWithoutEAP(std::move(network));
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(ArcNetHostImplTest, StartConnectDisconnect) {
  base::test::TestFuture<const std::string&> create_network_future;
  base::test::TestFuture<arc::mojom::NetworkResult> start_connect_future;
  base::test::TestFuture<arc::mojom::NetworkResult> start_disconnect_future;

  // Test that StartConnect() and StartDisconnect() call will return FAILURE
  // when guid is invalid.
  service()->StartConnect(kInvalidGuid, start_connect_future.GetCallback());
  EXPECT_EQ(start_connect_future.Take(), arc::mojom::NetworkResult::FAILURE);
  service()->StartDisconnect(kInvalidGuid,
                             start_disconnect_future.GetCallback());
  EXPECT_EQ(start_disconnect_future.Take(), arc::mojom::NetworkResult::FAILURE);

  // Create a network for testing
  auto config = CreateWifiConfigWithoutEAP();
  service()->CreateNetwork(std::move(config),
                           create_network_future.GetCallback());
  auto target_guid = create_network_future.Take();
  EXPECT_FALSE(target_guid.empty());

  // Test that the network can be connected successfully.
  service()->StartConnect(target_guid, start_connect_future.GetCallback());
  EXPECT_EQ(start_connect_future.Take(), arc::mojom::NetworkResult::SUCCESS);

  // Test that the network cannot be connected again if already connected.
  service()->StartConnect(target_guid, start_connect_future.GetCallback());
  EXPECT_EQ(start_connect_future.Take(), arc::mojom::NetworkResult::FAILURE);

  // Test that the network can be disconnected successfully.
  service()->StartDisconnect(target_guid,
                             start_disconnect_future.GetCallback());
  EXPECT_EQ(start_disconnect_future.Take(), arc::mojom::NetworkResult::SUCCESS);

  // Test that disconnected network cannot be disconnected again if already
  // disconnected.
  service()->StartDisconnect(target_guid,
                             start_disconnect_future.GetCallback());
  EXPECT_EQ(start_disconnect_future.Take(), arc::mojom::NetworkResult::FAILURE);
}

}  // namespace
}  // namespace arc
