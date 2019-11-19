// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/networking_cast_private/chrome_networking_cast_private_delegate.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "extensions/browser/api/networking_private/networking_cast_private_delegate.h"
#include "extensions/common/switches.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#endif

namespace extensions {

namespace {

class TestNetworkingCastPrivateDelegate
    : public ChromeNetworkingCastPrivateDelegate {
 public:
  TestNetworkingCastPrivateDelegate() {}
  ~TestNetworkingCastPrivateDelegate() override {}

  void VerifyDestination(std::unique_ptr<Credentials> credentials,
                         const VerifiedCallback& success_callback,
                         const FailureCallback& failure_callback) override {
    AssertCredentials(*credentials);
    success_callback.Run(true);
  }

  void VerifyAndEncryptData(const std::string& data,
                            std::unique_ptr<Credentials> credentials,
                            const DataCallback& success_callback,
                            const FailureCallback& failure_callback) override {
    AssertCredentials(*credentials);
    success_callback.Run("encrypted_data");
  }

 private:
  void AssertCredentials(const Credentials& credentials) {
    ASSERT_EQ("certificate", credentials.certificate());
    ASSERT_EQ("ica1,ica2,ica3",
              base::JoinString(credentials.intermediate_certificates(), ","));
    ASSERT_EQ("cHVibGljX2tleQ==", credentials.public_key());
    ASSERT_EQ("00:01:02:03:04:05", credentials.device_bssid());
    ASSERT_EQ("c2lnbmVkX2RhdGE=", credentials.signed_data());
    ASSERT_EQ(
        "Device 0123,device_serial,00:01:02:03:04:05,cHVibGljX2tleQ==,nonce",
        credentials.unsigned_data());
  }

  DISALLOW_COPY_AND_ASSIGN(TestNetworkingCastPrivateDelegate);
};

}  // namespace

class NetworkingCastPrivateApiTest : public ExtensionApiTest {
 public:
  NetworkingCastPrivateApiTest() = default;
  ~NetworkingCastPrivateApiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Whitelist the extension ID of the test extension.
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "epcifkihnkjgphfkloaaleeakhpmgdmn");
  }

  void SetUp() override {
    networking_cast_private_delegate_factory_ = base::Bind(
        &NetworkingCastPrivateApiTest::CreateNetworkingCastPrivateDelegate,
        base::Unretained(this));
    ChromeNetworkingCastPrivateDelegate::SetFactoryCallbackForTest(
        &networking_cast_private_delegate_factory_);

    ExtensionApiTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

#if defined(OS_CHROMEOS)
    chromeos::DBusThreadManager* dbus_manager =
        chromeos::DBusThreadManager::Get();
    chromeos::ShillDeviceClient::TestInterface* device_test =
        dbus_manager->GetShillDeviceClient()->GetTestInterface();
    device_test->ClearDevices();
    device_test->AddDevice("/device/stub_wifi_device1", shill::kTypeWifi,
                           "stub_wifi_device");
    device_test->SetTDLSState(shill::kTDLSConnectedState);

    chromeos::ShillServiceClient::TestInterface* service_test =
        dbus_manager->GetShillServiceClient()->GetTestInterface();
    service_test->ClearServices();
    service_test->AddService("stub_wifi", "stub_wifi_guid", "wifi",
                             shill::kTypeWifi, shill::kStateOnline,
                             true /* add_to_visible */);
#endif  // defined(OS_CHROMEOS)
  }

  void TearDown() override {
    ExtensionApiTest::TearDown();
    ChromeNetworkingCastPrivateDelegate::SetFactoryCallbackForTest(nullptr);
  }

  bool TdlsSupported() {
#if defined(OS_CHROMEOS)
    return true;
#else
    return false;
#endif
  }

 private:
  std::unique_ptr<ChromeNetworkingCastPrivateDelegate>
  CreateNetworkingCastPrivateDelegate() {
    return std::make_unique<TestNetworkingCastPrivateDelegate>();
  }

  ChromeNetworkingCastPrivateDelegate::FactoryCallback
      networking_cast_private_delegate_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingCastPrivateApiTest);
};

IN_PROC_BROWSER_TEST_F(NetworkingCastPrivateApiTest, Basic) {
  const std::string arg =
      base::StringPrintf("{\"tdlsSupported\": %d}", TdlsSupported());
  EXPECT_TRUE(RunPlatformAppTestWithArg("networking_cast_private", arg.c_str()))
      << message_;
}

}  // namespace extensions
