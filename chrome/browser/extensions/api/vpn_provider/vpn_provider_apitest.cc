// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_provider_api.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/vpn_provider.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_third_party_vpn_driver_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/shill_property_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/common/extension.h"
#include "extensions/test/result_catcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

namespace api_vpn = extensions::api::vpn_provider;

const char kTestConfig[] = "testconfig";
const char kPacket[] = "feebdaed";

const char kNetworkProfilePath[] = "/network/test";
constexpr std::array kParameterValues = {"10.10.10.10",
                                         "24",
                                         "63.145.213.129/32 63.145.212.0/24",
                                         "0.0.0.0/0 63.145.212.128/25",
                                         "8.8.8.8",
                                         "1600",
                                         "10.10.10.255",
                                         "foo:bar"};
constexpr std::array kParameterKeys = {
    shill::kAddressParameterThirdPartyVpn,
    shill::kSubnetPrefixParameterThirdPartyVpn,
    shill::kExclusionListParameterThirdPartyVpn,
    shill::kInclusionListParameterThirdPartyVpn,
    shill::kDnsServersParameterThirdPartyVpn,
    shill::kMtuParameterThirdPartyVpn,
    shill::kBroadcastAddressParameterThirdPartyVpn,
    shill::kDomainSearchParameterThirdPartyVpn};

void DoNothingFailureCallback(const std::string& error_name) {
  FAIL();
}

void DoNothingSuccessCallback(const std::string& service_path,
                              const std::string& guid) {}


}  // namespace

// Records the number of calls and their parameters. Always replies successfully
// to calls.
class TestShillThirdPartyVpnDriverClient
    : public ash::FakeShillThirdPartyVpnDriverClient {
 public:
  void SetParameters(const std::string& object_path_value,
                     const base::DictValue& parameters,
                     StringCallback callback,
                     ErrorCallback error_callback) override {
    set_parameters_counter_++;
    parameters_ = parameters.Clone();
    FakeShillThirdPartyVpnDriverClient::SetParameters(
        object_path_value, parameters, std::move(callback),
        std::move(error_callback));
  }

  void UpdateConnectionState(const std::string& object_path_value,
                             const uint32_t connection_state,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override {
    update_connection_state_counter_++;
    connection_state_ = connection_state;
    FakeShillThirdPartyVpnDriverClient::UpdateConnectionState(
        object_path_value, connection_state, std::move(callback),
        std::move(error_callback));
  }

  void SendPacket(const std::string& object_path_value,
                  const std::vector<char>& ip_packet,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override {
    send_packet_counter_++;
    ip_packet_ = ip_packet;
    FakeShillThirdPartyVpnDriverClient::SendPacket(object_path_value, ip_packet,
                                                   std::move(callback),
                                                   std::move(error_callback));
  }

  int set_parameters_counter_ = 0;
  base::DictValue parameters_;
  int update_connection_state_counter_ = 0;
  uint32_t connection_state_;
  int send_packet_counter_ = 0;
  std::vector<char> ip_packet_;
};

class VpnProviderApiTestBase : public extensions::ExtensionApiTest {
 public:
  // extensions::ExtensionApiTest
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    LoadVpnExtension();
  }

  bool RunTest(const std::string& test_name) {
    DCHECK(extension_);
    const std::string extension_url = "basic.html?#" + test_name;
    return RunExtensionTest("vpn_provider",
                            {.extension_url = extension_url.c_str()});
  }

  const std::string& extension_id() const {
    DCHECK(extension_id_);
    return *extension_id_;
  }

  chromeos::VpnService* service() {
    return static_cast<chromeos::VpnService*>(
        chromeos::VpnServiceFactory::GetForBrowserContext(profile()));
  }

  virtual void OnPlatformMessage(const std::string& configuration_name,
                                 api_vpn::PlatformMessage) = 0;
  virtual void OnPacketReceived(const std::string& configuration_name,
                                const std::vector<char>& data) = 0;

 protected:
  void LoadVpnExtension() {
    DCHECK(!extension_);
    extension_ = LoadExtension(test_data_dir_.AppendASCII("vpn_provider"));
    extension_id_ = extension_->id();
  }

  raw_ptr<const extensions::Extension, DanglingUntriaged> extension_ = nullptr;
  std::optional<std::string> extension_id_;
};

class VpnProviderApiTest : public VpnProviderApiTestBase {
 public:
  // VpnProviderApiTestBase:
  void SetUpInProcessBrowserTestFixture() override {
    VpnProviderApiTestBase::SetUpInProcessBrowserTestFixture();
    // Destroy the existing client and create a test specific fake client. It
    // will be destroyed in ChromeBrowserMain.
    test_client_ = new TestShillThirdPartyVpnDriverClient();
  }
  void SetUpOnMainThread() override {
    VpnProviderApiTestBase::SetUpOnMainThread();
    AddNetworkProfileForUser();
  }
  void OnPlatformMessage(const std::string& configuration_name,
                         api_vpn::PlatformMessage message) override {
    test_client_->OnPlatformMessage(
        shill::kObjectPathBase + GetKey(configuration_name),
        std::to_underlying(message));
  }
  void OnPacketReceived(const std::string& configuration_name,
                        const std::vector<char>& data) override {
    test_client_->OnPacketReceived(
        shill::kObjectPathBase + GetKey(configuration_name), data);
  }

  std::string GetKey(const std::string& configuration_name) const {
    return VpnService::GetKeyForTesting(extension_id(), configuration_name);
  }

  bool DoesConfigExist(const std::string& configuration_name) {
    return service()->LookupConfiguration(extension_id(), configuration_name) !=
           nullptr;
  }

  bool IsConfigConnected() {
    return service()->GetActiveConfigurationForExtension(extension_id());
  }

  std::string GetSingleServicePath() {
    EXPECT_EQ(service()->service_path_to_configuration_map_.size(), 1);
    return service()->service_path_to_configuration_map_.begin()->first;
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
    ash::NetworkHandler::Get()
        ->network_configuration_handler()
        ->RemoveConfiguration(
            GetSingleServicePath(), /*remove_confirmer=*/std::nullopt,
            base::DoNothing(), base::BindOnce(DoNothingFailureCallback));
  }

  bool HasService(const std::string& service_path) const {
    std::string profile_path;
    std::optional<base::DictValue> properties =
        ash::ShillProfileClient::Get()->GetTestInterface()->GetService(
            service_path, &profile_path);
    return properties.has_value();
  }

  void SendPlatformError(const std::string& extension_id,
                         const std::string& configuration_name) {
    service()->SendOnPlatformMessageToExtension(
        extension_id, configuration_name,
        std::to_underlying(api_vpn::PlatformMessage::kError));
  }

  void ClearNetworkProfiles() {
    ash::ShillProfileClient::Get()->GetTestInterface()->ClearProfiles();
    // ShillProfileClient doesn't notify NetworkProfileHandler that profiles got
    // cleared, therefore we have to call ShillManagerClient explicitly.
    ash::ShillManagerClient::Get()->GetTestInterface()->ClearProfiles();
  }

 protected:
  void AddNetworkProfileForUser() {
    ash::ShillProfileClient::Get()->GetTestInterface()->AddProfile(
        kNetworkProfilePath,
        ash::ProfileHelper::GetUserIdHashFromProfile(profile()));
    content::RunAllPendingInMessageLoop();
  }

  raw_ptr<TestShillThirdPartyVpnDriverClient, DanglingUntriaged> test_client_ =
      nullptr;  // Unowned
};

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CreateConfigWithoutNetworkProfile) {
  ClearNetworkProfiles();
  EXPECT_TRUE(RunTest("createConfigWithoutNetworkProfile"));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CreateConfig) {
  EXPECT_TRUE(RunTest("createConfigSuccess"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));
  EXPECT_TRUE(HasService(GetSingleServicePath()));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, DestroyConfig) {
  EXPECT_TRUE(CreateConfigForTest(kTestConfig));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));
  const std::string service_path = GetSingleServicePath();
  EXPECT_TRUE(HasService(service_path));

  EXPECT_TRUE(RunTest("destroyConfigSuccess"));
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
  EXPECT_FALSE(HasService(service_path));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, DestroyConnectedConfig) {
  EXPECT_TRUE(CreateConfigForTest(kTestConfig));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));
  const std::string service_path = GetSingleServicePath();
  EXPECT_TRUE(HasService(service_path));
  EXPECT_FALSE(IsConfigConnected());

  OnPlatformMessage(kTestConfig, api_vpn::PlatformMessage::kConnected);
  EXPECT_TRUE(IsConfigConnected());

  EXPECT_TRUE(RunTest("destroyConnectedConfigSetup"));

  extensions::ResultCatcher catcher;

  EXPECT_TRUE(DestroyConfigForTest(kTestConfig));
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
  EXPECT_FALSE(HasService(service_path));

  ASSERT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, ConfigInternalRemove) {
  EXPECT_TRUE(RunTest("configInternalRemove"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));

  extensions::ResultCatcher catcher;
  TriggerInternalRemove();
  ASSERT_TRUE(catcher.GetNextResult());
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CheckEvents) {
  EXPECT_TRUE(RunTest("expectEvents"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));

  extensions::ResultCatcher catcher;
  SendPlatformError(extension_id(), kTestConfig);
  service()->SendShowAddDialogToExtension(extension_id());
  service()->SendShowConfigureDialogToExtension(extension_id(), kTestConfig);
  EXPECT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, ConfigPersistence) {
  EXPECT_FALSE(DoesConfigExist(kTestConfig));

  base::DictValue properties;
  properties.Set(shill::kTypeProperty, shill::kTypeVPN);
  properties.Set(shill::kNameProperty, kTestConfig);
  properties.Set(shill::kProviderHostProperty, extension_id());
  properties.Set(shill::kObjectPathSuffixProperty, GetKey(kTestConfig));
  properties.Set(shill::kProviderTypeProperty, shill::kProviderThirdPartyVpn);
  properties.Set(shill::kProfileProperty, kNetworkProfilePath);

  ash::NetworkHandler::Get()
      ->network_configuration_handler()
      ->CreateShillConfiguration(std::move(properties),
                                 base::BindOnce(DoNothingSuccessCallback),
                                 base::BindOnce(DoNothingFailureCallback));
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(DoesConfigExist(kTestConfig));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CreateUninstall) {
  EXPECT_TRUE(RunTest("createConfigSuccess"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));

  const std::string service_path = GetSingleServicePath();
  EXPECT_TRUE(HasService(service_path));

  UninstallExtension(extension_id());
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
  EXPECT_FALSE(HasService(service_path));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CreateDisable) {
  EXPECT_TRUE(RunTest("createConfigSuccess"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));

  const std::string service_path = GetSingleServicePath();
  EXPECT_TRUE(HasService(service_path));

  extensions::ExtensionRegistrar::Get(profile())->DisableExtension(
      extension_id(), {extensions::disable_reason::DISABLE_USER_ACTION});
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
  EXPECT_FALSE(HasService(service_path));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CreateBlocklist) {
  EXPECT_TRUE(RunTest("createConfigSuccess"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));

  const std::string service_path = GetSingleServicePath();
  EXPECT_TRUE(HasService(service_path));

  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  extension_service->BlocklistExtensionForTest(extension_id());
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
  EXPECT_FALSE(HasService(service_path));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, ComboSuite) {
  EXPECT_TRUE(RunTest("comboSuite"));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, VpnSuccess) {
  EXPECT_TRUE(RunTest("createConfigConnectAndDisconnect"));

  EXPECT_TRUE(DoesConfigExist(kTestConfig));
  EXPECT_TRUE(HasService(GetSingleServicePath()));
  EXPECT_FALSE(IsConfigConnected());
  EXPECT_EQ(0, test_client_->set_parameters_counter_);
  EXPECT_EQ(0, test_client_->update_connection_state_counter_);
  EXPECT_EQ(0, test_client_->send_packet_counter_);

  extensions::ResultCatcher catcher;
  OnPlatformMessage(kTestConfig, api_vpn::PlatformMessage::kConnected);
  ASSERT_TRUE(catcher.GetNextResult());

  EXPECT_TRUE(IsConfigConnected());
  EXPECT_EQ(1, test_client_->set_parameters_counter_);
  EXPECT_EQ(1, test_client_->update_connection_state_counter_);
  EXPECT_EQ(1, test_client_->send_packet_counter_);
  EXPECT_EQ(std::to_underlying(api_vpn::VpnConnectionState::kConnected),
            test_client_->update_connection_state_counter_);
  for (size_t i = 0; i < std::size(kParameterValues); ++i) {
    const std::string* value =
        test_client_->parameters_.FindString(kParameterKeys[i]);
    ASSERT_TRUE(value);
    EXPECT_EQ(kParameterValues[i], *value);
  }
  std::vector<char> received_packet(std::begin(kPacket),
                                    std::prev(std::end(kPacket)));
  EXPECT_EQ(received_packet, test_client_->ip_packet_);

  std::vector<char> packet(++std::rbegin(kPacket), std::rend(kPacket));
  OnPacketReceived(kTestConfig, packet);
  ASSERT_TRUE(catcher.GetNextResult());

  OnPlatformMessage(kTestConfig, api_vpn::PlatformMessage::kDisconnected);
  ASSERT_TRUE(catcher.GetNextResult());

  EXPECT_FALSE(IsConfigConnected());
}

}  // namespace chromeos
