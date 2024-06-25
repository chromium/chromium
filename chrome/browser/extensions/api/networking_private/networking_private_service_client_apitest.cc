// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "components/wifi/fake_wifi_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"
#include "extensions/browser/api/networking_private/networking_private_event_router.h"
#include "extensions/browser/api/networking_private/networking_private_event_router_factory.h"
#include "extensions/browser/api/networking_private/networking_private_service_client.h"
#include "extensions/common/switches.h"
#include "testing/gmock/include/gmock/gmock.h"

// This tests the Windows / Mac implementation of the networkingPrivate API
// (NetworkingPrivateServiceClient). Note, only a subset of the
// networkingPrivate API is implemented in NetworkingPrivateServiceClient, so
// this uses its own set of test expectations to reflect that. The expectations
// should be kept similar to the ChromeOS (primary) implementation as much as
// possible. See also crbug.com/460119.

using testing::_;
using testing::Return;

using extensions::NetworkingPrivateDelegate;
using extensions::NetworkingPrivateDelegateFactory;
using extensions::NetworkingPrivateEventRouter;
using extensions::NetworkingPrivateEventRouterFactory;
using extensions::NetworkingPrivateServiceClient;

namespace {

class NetworkingPrivateServiceClientApiTest
    : public extensions::ExtensionApiTest {
 public:
  NetworkingPrivateServiceClientApiTest() = default;

  NetworkingPrivateServiceClientApiTest(
      const NetworkingPrivateServiceClientApiTest&) = delete;
  NetworkingPrivateServiceClientApiTest& operator=(
      const NetworkingPrivateServiceClientApiTest&) = delete;

  bool RunNetworkingSubtest(const std::string& subtest) {
    const std::string extension_url = "main.html?" + subtest;
    return RunExtensionTest("networking_private/service_client",
                            {.extension_url = extension_url.c_str()},
                            {.load_as_component = true});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    // Allowlist the extension ID of the test extension.
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        "epcifkihnkjgphfkloaaleeakhpmgdmn");
  }

  static std::unique_ptr<KeyedService> CreateNetworkingPrivateServiceClient(
      content::BrowserContext* context) {
    std::unique_ptr<wifi::FakeWiFiService> wifi_service(
        new wifi::FakeWiFiService());
    return std::unique_ptr<KeyedService>(
        new NetworkingPrivateServiceClient(std::move(wifi_service)));
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    content::RunAllPendingInMessageLoop();
    NetworkingPrivateDelegateFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&CreateNetworkingPrivateServiceClient));
  }

  void TearDownOnMainThread() override {
    content::RunAllPendingInMessageLoop();
    extensions::ExtensionApiTest::TearDownOnMainThread();
  }
};

// Place each subtest into a separate browser test so that the stub networking
// library state is reset for each subtest run. This way they won't affect each
// other.

IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest, StartConnect) {
  EXPECT_TRUE(RunNetworkingSubtest("startConnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest, StartDisconnect) {
  EXPECT_TRUE(RunNetworkingSubtest("startDisconnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest,
                       StartConnectNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startConnectNonexistent")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest,
                       StartDisconnectNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startDisconnectNonexistent")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest,
                       StartGetPropertiesNonexistent) {
  EXPECT_TRUE(RunNetworkingSubtest("startGetPropertiesNonexistent"))
      << message_;
}

// TODO(stevenjb/mef): Implement |limit| to fix this, crbug.com/371442.
IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest,
                       DISABLED_GetNetworks) {
  EXPECT_TRUE(RunNetworkingSubtest("getNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest,
                       GetVisibleNetworks) {
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest,
                       GetVisibleNetworksWifi) {
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworksWifi")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest,
                       RequestNetworkScan) {
  EXPECT_TRUE(RunNetworkingSubtest("requestNetworkScan")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest, GetProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("getProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest, GetState) {
  EXPECT_TRUE(RunNetworkingSubtest("getState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest,
                       GetStateNonExistent) {
  EXPECT_TRUE(RunNetworkingSubtest("getStateNonExistent")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest, SetProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("setProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest, CreateNetwork) {
  EXPECT_TRUE(RunNetworkingSubtest("createNetwork")) << message_;
}

// TODO(stevenjb/mef): Fix this, crbug.com/371442.
IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest,
                       DISABLED_GetManagedProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("getManagedProperties")) << message_;
}

// TODO(b/349276078): This test is flaky.
IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest,
                       DISABLED_OnNetworksChangedEventConnect) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworksChangedEventConnect"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest,
                       OnNetworksChangedEventDisconnect) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworksChangedEventDisconnect"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateServiceClientApiTest,
                       OnNetworkListChangedEvent) {
  EXPECT_TRUE(RunNetworkingSubtest("onNetworkListChangedEvent")) << message_;
}

}  // namespace
