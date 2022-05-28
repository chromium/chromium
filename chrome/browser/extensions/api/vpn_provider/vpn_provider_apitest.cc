// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_provider_api.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service_factory.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service_interface.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chromeos/dbus/shill/fake_shill_third_party_vpn_driver_client.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/pepper_vpn_provider_resource_host_proxy.h"
#include "content/public/browser/vpn_service_proxy.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/test/result_catcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/vpn_service_ash.h"

namespace chromeos {

namespace {

namespace api_vpn = extensions::api::vpn_provider;

const char kNetworkProfilePath[] = "/network/test";
const char kTestConfig[] = "testconfig";
const char kPacket[] = "feebdaed";
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

void DoNothingFailureCallback(const std::string& error_name) {
  FAIL();
}

void DoNothingSuccessCallback(const std::string& service_path,
                              const std::string& guid) {}

crosapi::VpnServiceAsh* GetVpnServiceAsh() {
  return crosapi::CrosapiManager::Get()->crosapi_ash()->vpn_service_ash();
}

}  // namespace

// Records the number of calls and their parameters. Always replies successfully
// to calls.
class TestShillThirdPartyVpnDriverClient
    : public FakeShillThirdPartyVpnDriverClient {
 public:
  void SetParameters(const std::string& object_path_value,
                     const base::Value& parameters,
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
  base::Value parameters_;
  int update_connection_state_counter_ = 0;
  uint32_t connection_state_;
  int send_packet_counter_ = 0;
  std::vector<char> ip_packet_;
};

class VpnProviderApiTest : public extensions::ExtensionApiTest {
 public:
  VpnProviderApiTest() = default;
  ~VpnProviderApiTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    // Destroy the existing client and create a test specific fake client. It
    // will be destroyed in ChromeBrowserMain.
    test_client_ = new TestShillThirdPartyVpnDriverClient();
  }

  void AddNetworkProfileForUser() {
    ShillProfileClient::Get()->GetTestInterface()->AddProfile(
        kNetworkProfilePath,
        ash::ProfileHelper::GetUserIdHashFromProfile(profile()));
    content::RunAllPendingInMessageLoop();
  }

  void LoadVpnExtension() {
    extension_ = LoadExtension(test_data_dir_.AppendASCII("vpn_provider"));
    extension_id_ = extension_->id();
    service_ = VpnServiceFactory::GetForBrowserContext(profile());
    content::RunAllPendingInMessageLoop();
  }

  bool RunTest(const std::string& test_name) {
    GURL url = extension_->GetResourceURL("basic.html?#" + test_name);
    return RunExtensionTest("vpn_provider", {.page_url = url.spec().c_str()});
  }

  std::string GetKey(const std::string& configuration_name) const {
    return crosapi::VpnServiceForExtensionAsh::GetKey(extension_id_,
                                                      configuration_name);
  }

  bool DoesConfigExist(const std::string& configuration_name) const {
    const auto& mapping = GetVpnServiceAsh()->extension_id_to_service_;
    if (!base::Contains(mapping, extension_id_)) {
      return false;
    }
    return base::Contains(mapping.at(extension_id_)->key_to_configuration_map_,
                          GetKey(configuration_name));
  }

  bool IsConfigConnected() const {
    const auto& mapping = GetVpnServiceAsh()->extension_id_to_service_;
    if (!base::Contains(mapping, extension_id_)) {
      return false;
    }
    return mapping.at(extension_id_)->OwnsActiveConfiguration();
  }

  std::string GetSingleServicePath() {
    auto* vpn_service_ash = GetVpnServiceAsh();
    std::vector<std::string> service_paths;
    for (const auto& [extension_id, service] :
         vpn_service_ash->extension_id_to_service_) {
      const auto& service_path_map =
          service->service_path_to_configuration_map_;
      if (service_path_map.empty()) {
        continue;
      }
      DCHECK_EQ(service_path_map.size(), 1U);
      service_paths.push_back(service_path_map.begin()->first);
    }
    EXPECT_EQ(service_paths.size(), 1U);
    return service_paths[0];
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
        GetSingleServicePath(), /*remove_confirmer=*/absl::nullopt,
        base::DoNothing(), base::BindOnce(DoNothingFailureCallback));
  }

  bool HasService(const std::string& service_path) const {
    std::string profile_path;
    base::Value properties =
        ShillProfileClient::Get()->GetTestInterface()->GetService(
            service_path, &profile_path);
    return properties.is_dict();
  }

  void SendPlatformError(const std::string& extension_id,
                         const std::string& configuration_name,
                         const std::string& error_message) {
    const auto& mapping = GetVpnServiceAsh()->extension_id_to_service_;
    DCHECK(base::Contains(mapping, extension_id));
    auto* service = mapping.at(extension_id).get();
    service->DispatchOnPlatformMessageEvent(
        configuration_name, api_vpn::PLATFORM_MESSAGE_ERROR, error_message);
  }

 protected:
  TestShillThirdPartyVpnDriverClient* test_client_ = nullptr;  // Unowned
  extensions::api::VpnServiceInterface* service_ = nullptr;
  std::string extension_id_;
  std::string service_path_;
  const extensions::Extension* extension_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, ComboSuite) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_TRUE(RunTest("comboSuite"));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CreateConfigWithoutNetworkProfile) {
  LoadVpnExtension();
  EXPECT_TRUE(RunTest("createConfigWithoutNetworkProfile"));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CreateConfig) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_TRUE(RunTest("createConfigSuccess"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));
  EXPECT_TRUE(HasService(GetSingleServicePath()));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, DestroyConfig) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_TRUE(CreateConfigForTest(kTestConfig));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));
  const std::string service_path = GetSingleServicePath();
  EXPECT_TRUE(HasService(service_path));

  EXPECT_TRUE(RunTest("destroyConfigSuccess"));
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
  EXPECT_FALSE(HasService(service_path));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, DestroyConnectedConfig) {
  LoadVpnExtension();
  AddNetworkProfileForUser();

  EXPECT_TRUE(CreateConfigForTest(kTestConfig));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));
  const std::string service_path = GetSingleServicePath();
  EXPECT_TRUE(HasService(service_path));
  EXPECT_FALSE(IsConfigConnected());

  const std::string object_path = shill::kObjectPathBase + GetKey(kTestConfig);
  test_client_->OnPlatformMessage(object_path,
                                  api_vpn::PLATFORM_MESSAGE_CONNECTED);
  EXPECT_TRUE(IsConfigConnected());

  EXPECT_TRUE(RunTest("destroyConnectedConfigSetup"));

  extensions::ResultCatcher catcher;

  EXPECT_TRUE(DestroyConfigForTest(kTestConfig));
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
  EXPECT_FALSE(HasService(service_path));

  ASSERT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, VpnSuccess) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_TRUE(RunTest("createConfigConnectAndDisconnect"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));
  EXPECT_TRUE(HasService(GetSingleServicePath()));
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
  for (size_t i = 0; i < std::size(kParameterValues); ++i) {
    const std::string* value =
        test_client_->parameters_.FindStringKey(kParameterKeys[i]);
    ASSERT_TRUE(value);
    EXPECT_EQ(kParameterValues[i], *value);
  }
  std::vector<char> packet(std::begin(kPacket), std::prev(std::end(kPacket)));
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
  EXPECT_TRUE(RunTest("configInternalRemove"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));

  extensions::ResultCatcher catcher;
  TriggerInternalRemove();
  ASSERT_TRUE(catcher.GetNextResult());
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CheckEvents) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_TRUE(RunTest("expectEvents"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));

  extensions::ResultCatcher catcher;
  SendPlatformError(extension_id_, kTestConfig, "error_message");
  service_->SendShowAddDialogToExtension(extension_id_);
  service_->SendShowConfigureDialogToExtension(extension_id_, kTestConfig);
  EXPECT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, ConfigPersistence) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_FALSE(DoesConfigExist(kTestConfig));

  base::Value::Dict properties;
  properties.Set(shill::kTypeProperty, shill::kTypeVPN);
  properties.Set(shill::kNameProperty, kTestConfig);
  properties.Set(shill::kProviderHostProperty, extension_id_);
  properties.Set(shill::kObjectPathSuffixProperty, GetKey(kTestConfig));
  properties.Set(shill::kProviderTypeProperty, shill::kProviderThirdPartyVpn);
  properties.Set(shill::kProfileProperty, kNetworkProfilePath);

  NetworkHandler::Get()
      ->network_configuration_handler()
      ->CreateShillConfiguration(base::Value(std::move(properties)),
                                 base::BindOnce(DoNothingSuccessCallback),
                                 base::BindOnce(DoNothingFailureCallback));
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(DoesConfigExist(kTestConfig));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CreateUninstall) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_TRUE(RunTest("createConfigSuccess"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));

  const std::string service_path = GetSingleServicePath();
  EXPECT_TRUE(HasService(service_path));

  UninstallExtension(extension_id_);
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
  EXPECT_FALSE(HasService(service_path));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CreateDisable) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_TRUE(RunTest("createConfigSuccess"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));

  const std::string service_path = GetSingleServicePath();
  EXPECT_TRUE(HasService(service_path));

  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  extension_service->DisableExtension(
      extension_id_, extensions::disable_reason::DISABLE_USER_ACTION);
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
  EXPECT_FALSE(HasService(service_path));
}

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, CreateBlocklist) {
  LoadVpnExtension();
  AddNetworkProfileForUser();
  EXPECT_TRUE(RunTest("createConfigSuccess"));
  EXPECT_TRUE(DoesConfigExist(kTestConfig));

  const std::string service_path = GetSingleServicePath();
  EXPECT_TRUE(HasService(service_path));

  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  extension_service->BlocklistExtensionForTest(extension_id_);
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(DoesConfigExist(kTestConfig));
  EXPECT_FALSE(HasService(service_path));
}

class FakePepperVpnProviderResourceHostProxy
    : public content::PepperVpnProviderResourceHostProxy {
 public:
  FakePepperVpnProviderResourceHostProxy(
      base::test::TestFuture<bool>* unbind,
      base::test::TestFuture<std::vector<char>>* data)
      : unbind_(unbind), data_(data) {}

  void SendOnUnbind() override { unbind_->SetValue(true); }

  void SendOnPacketReceived(const std::vector<char>& data) override {
    data_->SetValue(data);
  }

 private:
  raw_ptr<base::test::TestFuture<bool>> unbind_;
  raw_ptr<base::test::TestFuture<std::vector<char>>> data_;
};

IN_PROC_BROWSER_TEST_F(VpnProviderApiTest, PepperProxy) {
  LoadVpnExtension();
  AddNetworkProfileForUser();

  base::test::TestFuture<bool> unbind;
  base::test::TestFuture<std::vector<char>> data;
  // This class will be used as a receiver for mojo::SelfOwnedReceiver.
  // Therefore it's unsafe to keep these TestFuture-s as members (especially
  // |unbind|).
  auto pepper_proxy =
      std::make_unique<FakePepperVpnProviderResourceHostProxy>(&unbind, &data);

  extensions::ResultCatcher catcher;

  // Create config and imitate the platform sending a
  // PLATFORM_MESSAGE_CONNECTED.
  EXPECT_TRUE(RunTest("createConfigConnectForBind"));
  ASSERT_TRUE(catcher.GetNextResult());
  const std::string object_path = shill::kObjectPathBase + GetKey(kTestConfig);
  test_client_->OnPlatformMessage(object_path,
                                  api_vpn::PLATFORM_MESSAGE_CONNECTED);
  ASSERT_TRUE(catcher.GetNextResult());

  // Synchronously bind the fake pepper proxy.
  base::RunLoop run_loop;
  service_->GetVpnServiceProxy()->Bind(
      extension_id_, {}, kTestConfig, run_loop.QuitClosure(), base::DoNothing(),
      std::move(pepper_proxy));
  run_loop.Run();

  // Assert that packets are routed through the proxy.
  test_client_->OnPacketReceived(
      object_path, std::vector<char>{std::begin(kPacket), std::end(kPacket)});
  ASSERT_TRUE(data.Wait());

  // Assert that pepper proxy receives an OnUnbind event on
  // PLATFORM_MESSAGE_DISCONNECTED.
  test_client_->OnPlatformMessage(object_path,
                                  api_vpn::PLATFORM_MESSAGE_DISCONNECTED);
  ASSERT_TRUE(catcher.GetNextResult());
  ASSERT_TRUE(unbind.Wait());
}

}  // namespace chromeos
