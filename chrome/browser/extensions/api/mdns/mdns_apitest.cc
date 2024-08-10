// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "chrome/browser/extensions/api/mdns/mdns_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/media/router/test/mock_dns_sd_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"

using media_router::DnsSdRegistry;
using ::testing::_;
using ::testing::A;

namespace api = extensions::api;

namespace {

class MDnsAPITest : public extensions::ExtensionApiTest {
 public:
  MDnsAPITest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        "ddchlicdkolnonkihahngkmmmjnjlkkf");
  }

  void SetUpTestDnsSdRegistry() {
    extensions::MDnsAPI* api = extensions::MDnsAPI::Get(profile());
    dns_sd_registry_ = std::make_unique<media_router::MockDnsSdRegistry>(api);
    EXPECT_CALL(*dns_sd_registry_, AddObserver(api)).Times(1);
    api->SetDnsSdRegistryForTesting(dns_sd_registry_.get());
  }

 protected:
  std::unique_ptr<media_router::MockDnsSdRegistry> dns_sd_registry_;
};

}  // namespace

// Test loading extension, registering an MDNS listener and dispatching events.
IN_PROC_BROWSER_TEST_F(MDnsAPITest, RegisterListener) {
  const std::string& service_type = "_googlecast._tcp.local";
  SetUpTestDnsSdRegistry();

  EXPECT_CALL(*dns_sd_registry_, RegisterDnsSdListener(service_type)).Times(1);
  EXPECT_CALL(*dns_sd_registry_, UnregisterDnsSdListener(service_type))
      .Times(1);
  EXPECT_CALL(*dns_sd_registry_,
              RemoveObserver(A<DnsSdRegistry::DnsSdObserver*>()))
      .Times(1);

  EXPECT_TRUE(
      RunExtensionTest("mdns/api", {.extension_url = "register_listener.html"}))
      << message_;

  extensions::ResultCatcher catcher;
  // Dispatch 3 events, one of which should not be sent to the test extension.
  DnsSdRegistry::DnsSdServiceList services;

  media_router::DnsSdService service;
  service.service_name = service_type;
  services.push_back(service);

  dns_sd_registry_->DispatchMDnsEvent(service_type, services);
  dns_sd_registry_->DispatchMDnsEvent("_uninteresting._tcp.local", services);
  dns_sd_registry_->DispatchMDnsEvent(service_type, services);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Test loading extension, registering an MDNS listener and dispatching events.
IN_PROC_BROWSER_TEST_F(MDnsAPITest, ForceDiscovery) {
  const std::string& service_type = "_googlecast._tcp.local";
  SetUpTestDnsSdRegistry();

  EXPECT_CALL(*dns_sd_registry_, RegisterDnsSdListener(service_type)).Times(1);
  EXPECT_CALL(*dns_sd_registry_, UnregisterDnsSdListener(service_type))
      .Times(1);
  EXPECT_CALL(*dns_sd_registry_, ResetAndDiscover()).Times(1);
  EXPECT_CALL(*dns_sd_registry_,
              RemoveObserver(A<DnsSdRegistry::DnsSdObserver*>()))
      .Times(1);

  EXPECT_TRUE(
      RunExtensionTest("mdns/api", {.extension_url = "force_discovery.html"}))
      << message_;

  extensions::ResultCatcher catcher;
  DnsSdRegistry::DnsSdServiceList services;

  media_router::DnsSdService service;
  service.service_name = service_type;
  services.push_back(service);

  dns_sd_registry_->DispatchMDnsEvent(service_type, services);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Test loading extension and registering multiple listeners.
IN_PROC_BROWSER_TEST_F(MDnsAPITest, RegisterMultipleListeners) {
  const std::string& service_type = "_googlecast._tcp.local";
  const std::string& test_service_type = "_testing._tcp.local";
  SetUpTestDnsSdRegistry();

  EXPECT_CALL(*dns_sd_registry_, RegisterDnsSdListener(service_type)).Times(1);
  EXPECT_CALL(*dns_sd_registry_, UnregisterDnsSdListener(service_type))
      .Times(1);
  EXPECT_CALL(*dns_sd_registry_, RegisterDnsSdListener(test_service_type))
      .Times(1);
  EXPECT_CALL(*dns_sd_registry_, UnregisterDnsSdListener(test_service_type))
      .Times(1);
  EXPECT_CALL(*dns_sd_registry_,
              RemoveObserver(A<DnsSdRegistry::DnsSdObserver*>()))
      .Times(1);

  EXPECT_TRUE(RunExtensionTest(
      "mdns/api", {.extension_url = "register_multiple_listeners.html"}))
      << message_;

  extensions::ResultCatcher catcher;
  DnsSdRegistry::DnsSdServiceList services;

  media_router::DnsSdService service;
  service.service_name = service_type;
  services.push_back(service);

  dns_sd_registry_->DispatchMDnsEvent(service_type, services);
  dns_sd_registry_->DispatchMDnsEvent(test_service_type, services);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Test loading extension and registering multiple listeners.
IN_PROC_BROWSER_TEST_F(MDnsAPITest, RegisterTooManyListeners) {
  SetUpTestDnsSdRegistry();

  EXPECT_CALL(*dns_sd_registry_, RegisterDnsSdListener(_)).Times(10);
  EXPECT_CALL(*dns_sd_registry_, UnregisterDnsSdListener(_)).Times(10);
  EXPECT_CALL(*dns_sd_registry_,
              RemoveObserver(A<DnsSdRegistry::DnsSdObserver*>()))
      .Times(1);

  EXPECT_TRUE(RunExtensionTest("mdns/too_many_listeners")) << message_;
}

// Test loading extension and registering multiple listeners.
IN_PROC_BROWSER_TEST_F(MDnsAPITest, MaxServiceInstancesPerEventConst) {
  EXPECT_TRUE(RunExtensionTest(
      "mdns/api", {.extension_url = "get_max_service_instances.html"}));
}
