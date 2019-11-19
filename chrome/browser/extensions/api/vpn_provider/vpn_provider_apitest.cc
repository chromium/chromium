// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chromeos/dbus/shill/fake_shill_third_party_vpn_driver_client.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/vpn_provider/vpn_provider_api.h"
#include "extensions/browser/api/vpn_provider/vpn_service.h"
#include "extensions/browser/api/vpn_provider/vpn_service_factory.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/test/result_catcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::Invoke;

namespace chromeos {

namespace {

namespace api_vpn = extensions::api::vpn_provider;

const char kNetworkProfilePath[] = "/network/test";
const char kTestConfig[] = "testconfig";
const char* kParameterValues[] = {"10.10.10.10",
                                  "24",
                                  "63.145.213.129/32 63.145.212.0/24",
                                  "0.0.0.0/0 63.145.212.128/25",
                                  "8.8.8.8",
                                  "1600",
                                  "10.10.10.255",
                                  "foo:bar"};
const char* kParameterKeys[] = {shill::kAddressParameterThirdPartyVpn,
                                shill::kSubnetPrefixParameterThirdPartyVpn,
                                shill::kExclusionListParameterThirdPartyVpn,
                                shill::kInclusionListParameterThirdPartyVpn,
                                shill::kDnsServersParameterThirdPartyVpn,
                                shill::kMtuParameterThirdPartyVpn,
                                shill::kBroadcastAddressParameterThirdPartyVpn,
                                shill::kDomainSearchParameterThirdPartyVpn};

void DoNothingFailureCallback(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  EXPECT_EQ(true, false);
}

void DoNothingSuccessCallback(const std::string& service_path,
                              const std::string& guid) {}

}  // namespace

// Records the number of calls and their parameters. Always replies successfully
// to calls.
class TestShillThirdPartyVpnDriverClient
    : public FakeShillThirdPartyVpnDriverClient {
 public:
  void SetParameters(
      const std::string& object_path_value,
      const base::DictionaryValue& parameters,
      const ShillClientHelper::StringCallback& callback,
      const ShillClientHelper::ErrorCallback& error_callback) override {
    set_parameters_counter_++;
    parameters_ = parameters.DeepCopy();
    FakeShillThirdPartyVpnDriverClient::SetParameters(
        object_path_value, parameters, callback, error_callback);
  }

  void UpdateConnectionState(
      const std::string& object_path_value,
      const uint32_t connection_state,
      const base::Closure& callback,
      const ShillClientHelper::ErrorCallback& error_callback) override {
    update_connection_state_counter_++;
    connection_state_ = connection_state;
    FakeShillThirdPartyVpnDriverClient::UpdateConnectionState(
        object_path_value, connection_state, callback, error_callback);
  }

  void SendPacket(
      const std::string& object_path_value,
      const std::vector<char>& ip_packet,
      const base::Closure& callback,
      const ShillClientHelper::ErrorCallback& error_callback) override {
    send_packet_counter_++;
    ip_packet_ = ip_packet;
    FakeShillThirdPartyVpnDriverClient::SendPacket(object_path_value, ip_packet,
                                                   callback, error_callback);
  }

  int set_parameters_counter_ = 0;
  base::DictionaryValue* parameters_ = nullptr;
  int update_connection_state_counter_ = 0;
  uint32_t connection_state_;
  int send_packet_counter_ = 0;
  std::vector<char> ip_packet_;
};

class VpnProviderApiTest : public extensions::ExtensionApiTest {
 public:
  VpnProviderApiTest() {}
  ~VpnProviderApiTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    // Destroy the existing client and create a test specific fake client. It
    // will be destroyed in ChromeBrowserMain.
    test_client_ = new TestShillThirdPartyVpnDriverClient();
  }

  void AddNetworkProfileForUser() {
    ShillProfileClient::Get()->GetTestInterface()->AddProfile(
        kNetworkProfilePath,
        chromeos::ProfileHelper::GetUserIdHashFromProfile(profile()));
    content::RunAllPendingInMessageLoop();
  }

  void LoadVpnExtension() {
    extension_ = LoadExtension(test_data_dir_.AppendASCII("vpn_provider"));
    extension_id_ = extension_->id();
    service_ = VpnServiceFactory::GetForBrowserContext(profile());
    content::RunAllPendingInMessageLoop();
  }

  bool RunExtensionTest(const std::string& test_name) {
    GURL url = extension_->GetResourceURL("basic.html?#" + test_name);
    return RunExtensionSubtest("vpn_provider", url.spec());
  }

  std::string GetKey(const std::string& config_name) {
    return service_->GetKey(extension_id_, config_name);
  }

  bool DoesConfigExist(const std::string& config_name) {
    return service_->VerifyConfigExistsForTesting(extension_id_, config_name);
  }

  bool IsConfigConnected() {
    return service_->VerifyConfigIsConnectedForTesting(extension_id_);
  }

  std::string GetSingleServicePath() {
    std::string service_path = service_->GetSingleServicepathForTesting();
    EXPECT_FALSE(service_path.empty());
    return service_path;
  }

  bool CreateConfigForTest(const std::string& name) {
    scoped_refptr<extensions::VpnProviderCreateConfigFunction> create(
        new extensions::VpnProviderCreateConfigFunction());

    create->set_extension(GetSingleLoadedExtension());
    return extensions::api_test_utils::RunFunction(
        create.get(), "[\"" + name + "\"]", profile());
  }

  bool DestroyConfigForTest(const std::string& name) {
    scoped_refptr<extensions::VpnProviderDestroyConfigFunction> destroy(
        new extensions::VpnProviderDestroyConfigFunction());

    destroy->set_extension(GetSingleLoadedExtension());
    return extensions::api_test_utils::RunFunction(
        destroy.get(), "[\"" + name + "\"]", profile());
  }

  void TriggerInternalRemove() {
    NetworkHandler::Get()->network_configuration_handler()->RemoveConfiguration(
        GetSingleServicePath(), base::DoNothing(),
        base::Bind(DoNothingFailureCallback));
  }

 protected:
  TestShillThirdPartyVpnDriverClient* test_client_ = nullptr;  // Unowned
  VpnService* service_ = nullptr;
  std::string extension_id_;
  std::string service_path_;
  const extensions::Extension* extension_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, ComboSuite) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_TRUE(RunExtensionTest("comboSuite"));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CreateConfigWithoutNetworkProfile) {
  LoadVpnExtension();
  EXPECT_TRUE(RunExtensionTest("createConfigWithoutNetworkProfile"));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CreateConfig) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_TRUE(RunExtensionTest("createConfigSuccess"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));
  const std::string service_path = GetSingleServicePath();
  std::string profile_path;
  base::DictionaryValue properties;
  EXPECT_TRUE(ShillProfileClient::Get()->GetTestInterface()->GetService(
      service_path, &profile_path, &properties));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, DestroyConfig) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_TRUE(CreateConfigForTest(kTestConfig));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));
  const std::string service_path = GetSingleServicePath();
  std::string profile_path;
  base::DictionaryValue properties;
  EXPECT_TRUE(ShillProfileClient::Get()->GetTestInterface()->GetService(
      service_path, &profile_path, &properties));

  EXPECT_TRUE(RunExtensionTest("destroyConfigSuccess"));
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
  EXPECT_FALSE(ShillProfileClient::Get()->GetTestInterface()->GetService(
      service_path, &profile_path, &properties));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, DestroyConnectedConfig) {
  LoadVpnExtension();
  AddNetworkProfileForUser();

  EXPECT_TRUE(CreateConfigForTest(kTestConfig));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));
  const std::string service_path = GetSingleServicePath();
  std::string profile_path;
  base::DictionaryValue properties;
  EXPECT_TRUE(ShillProfileClient::Get()->GetTestInterface()->GetService(
      service_path, &profile_path, &properties));
  EXPECT_FALSE(IsConfigConnected());

  const std::string object_path = shill::kObjectPathBase + GetKey(kTestConfig);
  test_client_->OnPlatformMessage(object_path,
                                  api_vpn::PLATFORM_MESSAGE_CONNECTED);
  EXPECT_TRUE(IsConfigConnected());

  EXPECT_TRUE(RunExtensionTest("destroyConnectedConfigSetup"));

  EXPECT_TRUE(DestroyConfigForTest(kTestConfig));
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
  EXPECT_FALSE(ShillProfileClient::Get()->GetTestInterface()->GetService(
      service_path, &profile_path, &properties));

  extensions::ResultCatcher catcher;
  ASSERT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, VpnSuccess) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_TRUE(RunExtensionTest("createConfigConnectAndDisconnect"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));
  const std::string service_path = GetSingleServicePath();
  std::string profile_path;
  base::DictionaryValue properties;
  EXPECT_TRUE(ShillProfileClient::Get()->GetTestInterface()->GetService(
      service_path, &profile_path, &properties));
  EXPECT_FALSE(IsConfigConnected());

  const std::string object_path = shill::kObjectPathBase + GetKey(kTestConfig);

  extensions::ResultCatcher catcher;
  EXPECT_EQ(0, test_client_->set_parameters_counter_);
  EXPECT_EQ(0, test_client_->update_connection_state_counter_);
  EXPECT_EQ(0, test_client_->send_packet_counter_);
  test_client_->OnPlatformMessage(object_path,
                                  api_vpn::PLATFORM_MESSAGE_CONNECTED);
  EXPECT_TRUE(IsConfigConnected());
  ASSERT_TRUE(catcher.GetNextResult());
  EXPECT_EQ(1, test_client_->set_parameters_counter_);
  EXPECT_EQ(1, test_client_->update_connection_state_counter_);
  EXPECT_EQ(1, test_client_->send_packet_counter_);
  EXPECT_EQ(api_vpn::VPN_CONNECTION_STATE_CONNECTED,
            test_client_->update_connection_state_counter_);
  for (size_t i = 0; i < base::size(kParameterValues); ++i) {
    std::string value;
    EXPECT_TRUE(
        test_client_->parameters_->GetString(kParameterKeys[i], &value));
    EXPECT_EQ(kParameterValues[i], value);
  }
  const char kPacket[] = "feebdaed";
  std::vector<char> packet(&kPacket[0], &kPacket[8]);
  EXPECT_EQ(packet, test_client_->ip_packet_);

  packet.assign(test_client_->ip_packet_.rbegin(),
                test_client_->ip_packet_.rend());
  test_client_->OnPacketReceived(object_path, packet);
  ASSERT_TRUE(catcher.GetNextResult());

  test_client_->OnPlatformMessage(object_path,
                                  api_vpn::PLATFORM_MESSAGE_DISCONNECTED);
  ASSERT_TRUE(catcher.GetNextResult());
  EXPECT_FALSE(IsConfigConnected());
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, ConfigInternalRemove) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_TRUE(RunExtensionTest("configInternalRemove"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));

  extensions::ResultCatcher catcher;
  TriggerInternalRemove();
  ASSERT_TRUE(catcher.GetNextResult());
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CheckEvents) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_TRUE(RunExtensionTest("expectEvents"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));

  extensions::ResultCatcher catcher;
  service_->SendPlatformError(extension_id_, kTestConfig, "error_message");
  service_->SendShowAddDialogToExtension(extension_id_);
  service_->SendShowConfigureDialogToExtension(extension_id_, kTestConfig);
  EXPECT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, ConfigPersistence) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_FALSE(DoesConfigExist(kTestConfig));

  base::DictionaryValue properties;
  properties.SetKey(shill::kTypeProperty, base::Value(shill::kTypeVPN));
  properties.SetKey(shill::kNameProperty, base::Value(kTestConfig));
  properties.SetKey(shill::kProviderHostProperty, base::Value(extension_id_));
  properties.SetKey(shill::kObjectPathSuffixProperty,
                    base::Value(GetKey(kTestConfig)));
  properties.SetKey(shill::kProviderTypeProperty,
                    base::Value(shill::kProviderThirdPartyVpn));
  properties.SetKey(shill::kProfileProperty, base::Value(kNetworkProfilePath));
  NetworkHandler::Get()
      ->network_configuration_handler()
      ->CreateShillConfiguration(properties,
                                 base::Bind(DoNothingSuccessCallback),
                                 base::Bind(DoNothingFailureCallback));
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(DoesConfigExist(kTestConfig));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CreateUninstall) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_TRUE(RunExtensionTest("createConfigSuccess"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));

  const std::string service_path = GetSingleServicePath();
  std::string profile_path;
  base::DictionaryValue properties;
  EXPECT_TRUE(ShillProfileClient::Get()->GetTestInterface()->GetService(
      service_path, &profile_path, &properties));

  UninstallExtension(extension_id_);
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
  EXPECT_FALSE(ShillProfileClient::Get()->GetTestInterface()->GetService(
      service_path, &profile_path, &properties));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CreateDisable) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_TRUE(RunExtensionTest("createConfigSuccess"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));

  const std::string service_path = GetSingleServicePath();
  std::string profile_path;
  base::DictionaryValue properties;
  EXPECT_TRUE(ShillProfileClient::Get()->GetTestInterface()->GetService(
      service_path, &profile_path, &properties));

  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  extension_service->DisableExtension(
      extension_id_, extensions::disable_reason::DISABLE_USER_ACTION);
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
  EXPECT_FALSE(ShillProfileClient::Get()->GetTestInterface()->GetService(
      service_path, &profile_path, &properties));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CreateBlacklist) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_TRUE(RunExtensionTest("createConfigSuccess"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));

  const std::string service_path = GetSingleServicePath();
  std::string profile_path;
  base::DictionaryValue properties;
  EXPECT_TRUE(ShillProfileClient::Get()->GetTestInterface()->GetService(
      service_path, &profile_path, &properties));

  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  extension_service->BlacklistExtensionForTest(extension_id_);
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
  EXPECT_FALSE(ShillProfileClient::Get()->GetTestInterface()->GetService(
      service_path, &profile_path, &properties));
}

}  // namespace chromeos
