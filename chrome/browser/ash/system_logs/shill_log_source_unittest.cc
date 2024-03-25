// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/shill_log_source.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_logs {

namespace {
constexpr char kNetworkDevices[] = "network_devices";
constexpr char kNetworkServices[] = "network_services";
}  // namespace

class ShillLogSourceTest : public ::testing::Test {
 public:
  ShillLogSourceTest() {}
  ~ShillLogSourceTest() override = default;
  ShillLogSourceTest(const ShillLogSourceTest&) = delete;
  ShillLogSourceTest*& operator=(const ShillLogSourceTest&) = delete;

  void SetUp() override { ash::shill_clients::InitializeFakes(); }
  void TearDown() override { ash::shill_clients::Shutdown(); }

  std::unique_ptr<SystemLogsResponse> Fetch(bool scrub) {
    std::unique_ptr<SystemLogsResponse> result;
    base::RunLoop run_loop;
    source_ = std::make_unique<ShillLogSource>(scrub);
    source_->Fetch(base::BindOnce(
        [](std::unique_ptr<SystemLogsResponse>* result,
           base::OnceClosure quit_closure,
           std::unique_ptr<SystemLogsResponse> response) {
          *result = std::move(response);
          std::move(quit_closure).Run();
        },
        &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ShillLogSource> source_;
};

constexpr char kNotScrubbedDeviceStart[] = R"("/device/wifi1")";
constexpr char kNotScrubbedDeviceExpected[] = R"("/device/wifi1": {
      "Address": "23456789abcd",
      "DBus.Object": "/device/wifi1",
      "DBus.Service": "org.freedesktop.ModemManager1",
      "IPConfigs": {
         "ipconfig_v4_path": {
            "Address": "100.0.0.1",
            "Gateway": "100.0.0.2",
            "Method": "ipv4",
            "Prefixlen": 1,
            "WebProxyAutoDiscoveryUrl": "http://wpad.com/wpad.dat"
         },
         "ipconfig_v6_path": {
            "Address": "0:0:0:0:100:0:0:1",
            "Method": "ipv6"
         }
      },
      "Name": "stub_wifi_device1",
      "Type": "wifi"
   })";

constexpr char kNotScrubbedServiceStart[] = R"("/service/wifi1")";
constexpr char kNotScrubbedServiceExpected[] = R"("/service/wifi1": {
      "Connectable": true,
      "Device": "/device/wifi1",
      "GUID": "wifi1_guid",
      "Mode": "managed",
      "Name": "wifi1",
      "Profile": "/profile/default",
      "SSID": "wifi1",
      "SecurityClass": "wep",
      "State": "online",
      "Type": "wifi",
      "Visible": true,
      "WiFi.HexSSID": "7769666931"
   })";

TEST_F(ShillLogSourceTest, NotScrubbed) {
  std::unique_ptr<SystemLogsResponse> response = Fetch(/*scrub=*/false);
  ASSERT_TRUE(response);

  const auto devices_iter = response->find(kNetworkDevices);
  EXPECT_NE(devices_iter, response->end());
  // Look for the fake wifi device and then compare the strings to easily
  // identify any differences if the fake implementation changes.
  size_t idx = devices_iter->second.find(kNotScrubbedDeviceStart);
  EXPECT_NE(idx, std::string::npos);
  EXPECT_EQ(
      devices_iter->second.substr(idx, strlen(kNotScrubbedDeviceExpected)),
      std::string(kNotScrubbedDeviceExpected));

  const auto services_iter = response->find(kNetworkServices);
  EXPECT_NE(services_iter, response->end());
  // Look for a fake wifi service and then compare the strings to easily
  // identify any differences if the fake implementation changes.
  idx = services_iter->second.find(kNotScrubbedServiceStart);
  EXPECT_NE(idx, std::string::npos);
  EXPECT_EQ(
      services_iter->second.substr(idx, strlen(kNotScrubbedServiceExpected)),
      std::string(kNotScrubbedServiceExpected));
}

constexpr char kScrubbedDeviceStart[] = R"("/device/wifi1")";
constexpr char kScrubbedDeviceExpected[] = R"("/device/wifi1": {
      "Address": "*** MASKED ***",
      "DBus.Object": "/device/wifi1",
      "DBus.Service": "org.freedesktop.ModemManager1",
      "IPConfigs": {
         "ipconfig_v4_path": {
            "Address": "100.0.0.1",
            "Gateway": "100.0.0.2",
            "Method": "ipv4",
            "Prefixlen": 1,
            "WebProxyAutoDiscoveryUrl": "http://wpad.com/wpad.dat"
         },
         "ipconfig_v6_path": {
            "Address": "0:0:0:0:100:0:0:1",
            "Method": "ipv6"
         }
      },
      "Name": "*** MASKED ***",
      "Type": "wifi"
   })";

constexpr char kScrubbedServiceStart[] = R"("/service/wifi1")";
constexpr char kScrubbedServiceExpected[] = R"("/service/wifi1": {
      "Connectable": true,
      "Device": "/device/wifi1",
      "GUID": "wifi1_guid",
      "Mode": "managed",
      "Name": "service_wifi1",
      "Profile": "/profile/default",
      "SSID": "*** MASKED ***",
      "SecurityClass": "wep",
      "State": "online",
      "Type": "wifi",
      "Visible": true,
      "WiFi.HexSSID": "*** MASKED ***"
   })";

TEST_F(ShillLogSourceTest, Scrubbed) {
  std::unique_ptr<SystemLogsResponse> response = Fetch(/*scrub=*/true);
  ASSERT_TRUE(response);

  const auto devices_iter = response->find(kNetworkDevices);
  EXPECT_NE(devices_iter, response->end());
  // Look for the fake wifi device and then compare the strings to easily
  // identify any differences if the fake implementation changes.
  size_t idx = devices_iter->second.find(kScrubbedDeviceStart);
  EXPECT_NE(idx, std::string::npos);
  EXPECT_EQ(devices_iter->second.substr(idx, strlen(kScrubbedDeviceExpected)),
            std::string(kScrubbedDeviceExpected));

  const auto services_iter = response->find(kNetworkServices);
  EXPECT_NE(services_iter, response->end());
  // Look for a fake wifi service and then compare the strings to easily
  // identify any differences if the fake implementation changes.
  idx = services_iter->second.find(kScrubbedServiceStart);
  EXPECT_NE(idx, std::string::npos);
  EXPECT_EQ(services_iter->second.substr(idx, strlen(kScrubbedServiceExpected)),
            std::string(kScrubbedServiceExpected));
}

}  // namespace system_logs
