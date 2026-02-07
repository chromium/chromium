// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ash/network_health_data_collector.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/ash/system_logs/network_health_source.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using ::testing::ContainerEq;
using ::testing::StrEq;

namespace {

constexpr char kFakeNetworkHealthSnaphot[] =
    "Name: test_ethernet\n"
    "GUID: ethernet_guid\n"
    "Type: kEthernet\n"
    "State: kOnline\n"
    "Portal State: kOnline\n"
    "MAC Address: aa:aa:aa:aa:aa:aa\n"
    "IPV4 Address: 255.255.155.2\n"
    "IPV6 Addresses: ::0101:ffff:c0a8:640a\n"
    "\n"
    "Name: test_wifi\n"
    "GUID: wifi_guid\n"
    "Type: kWiFi\n"
    "State: kIdle\n"
    "Portal State: kOnline\n"
    "Signal Strength: 80\n"
    "Signal Strength (Average): 81.25555419921875\n"
    "Signal Strength (Deviation): 3.1622776985168457\n"
    "Signal Strength (Samples): [80,80,75,75,82,82,75,75,80,80,85,85,85]\n"
    "MAC Address: aa:bb:cc:dd:ee:ff\n"
    "IPV4 Address: N/A\n"
    "IPV6 Addresses: ::ffff:cb0c:10ea\n"
    "\n";

constexpr char kRedactedNetworkHealthSnapshot[] =
    "Name: ethernet_0\n"
    "GUID: ethernet_0\n"
    "Type: kEthernet\n"
    "State: kOnline\n"
    "Portal State: kOnline\n"
    "MAC Address: (MAC OUI=aa:aa:aa IFACE=1)\n"
    "IPV4 Address: (IPv4: 1)\n"
    "IPV6 Addresses: (IPv6: 1)\n"
    "\n"
    "Name: wifi_none_1\n"
    "GUID: wifi_none_1\n"
    "Type: kWiFi\n"
    "State: kIdle\n"
    "Portal State: kOnline\n"
    "Signal Strength: 80\n"
    "Signal Strength (Average): 81.25555419921875\n"
    "Signal Strength (Deviation): 3.1622776985168457\n"
    "Signal Strength (Samples): [80,80,75,75,82,82,75,75,80,80,85,85,85]\n"
    "MAC Address: (MAC OUI=aa:bb:cc IFACE=2)\n"
    "IPV4 Address: N/A\n"
    "IPV6 Addresses: (IPv6: 2)\n"
    "\n";

const PIIMap kExpectedPIIMap = {
    {redaction::PIIType::kIPAddress,
     {"255.255.155.2", "::0101:ffff:c0a8:640a", "::ffff:cb0c:10ea"}},
    {redaction::PIIType::kMACAddress,
     {"aa:aa:aa:aa:aa:aa", "aa:bb:cc:dd:ee:ff"}},
    {redaction::PIIType::kStableIdentifier,
     {"test_ethernet", "test_wifi", "ethernet_guid", "wifi_guid"}}};

class TestLogSource : public system_logs::SystemLogsSource {
 public:
  TestLogSource() : system_logs::SystemLogsSource("Test Log Source") {}

  TestLogSource(const TestLogSource&) = delete;
  TestLogSource& operator=(const TestLogSource&) = delete;

  ~TestLogSource() override = default;

  // SystemLogsSource override.
  void Fetch(system_logs::SysLogsSourceCallback callback) override {
    std::unique_ptr<system_logs::SystemLogsResponse> response =
        std::make_unique<system_logs::SystemLogsResponse>();
    // Add test data to the response.
    response->emplace(system_logs::kNetworkHealthSnapshotEntry,
                      kFakeNetworkHealthSnaphot);
    std::move(callback).Run(std::move(response));
  }
};

}  // namespace

class NetworkHealthDataCollectorTest : public ::testing::Test {
 public:
  NetworkHealthDataCollectorTest() {
    // Set up task runner and container for RedactionTool. We will use when
    // calling CollectDataAndDetectPII() and ExportCollectedDataWithPII()
    // functions on NetworkHealthDataCollector for testing.
    task_runner_for_redaction_tool_ =
        base::ThreadPool::CreateSequencedTaskRunner({});
    redaction_tool_container_ =
        base::MakeRefCounted<redaction::RedactionToolContainer>(
            task_runner_for_redaction_tool_, nullptr);
  }

  NetworkHealthDataCollectorTest(const NetworkHealthDataCollectorTest&) =
      delete;
  NetworkHealthDataCollectorTest& operator=(
      const NetworkHealthDataCollectorTest&) = delete;

  void SetUp() override {
    // Allow blocking for file operations.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    SetupDefaultShillState();
    base::RunLoop().RunUntilIdle();
    ash::DebugDaemonClient::InitializeFake();
  }

  void TearDown() override {
    // Allow blocking for file operations.
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!temp_dir_.IsValid())
      return;
    EXPECT_TRUE(temp_dir_.Delete());
    ash::DebugDaemonClient::Shutdown();
  }

 protected:
  base::FilePath GetTempDirForOutput() { return temp_dir_.GetPath(); }

  void SetupDefaultShillState() {
    // Make sure any observer calls complete before clearing devices and
    // services.
    base::RunLoop().RunUntilIdle();
    auto* device_test = ash::ShillDeviceClient::Get()->GetTestInterface();
    device_test->ClearDevices();
    device_test->AddDevice("/device/eth0", shill::kTypeEthernet,
                           "stub_eth_device");
    device_test->AddDevice("/device/wlan0", shill::kTypeWifi,
                           "stub_wifi_device");

    auto* service_test = ash::ShillServiceClient::Get()->GetTestInterface();
    service_test->ClearServices();
    const bool add_to_visible = true;

    // Ethernet
    service_test->AddService("/service/0", "ethernet_guid", "test_ethernet",
                             shill::kTypeEthernet, shill::kStateOnline,
                             add_to_visible);

    // WiFi
    service_test->AddService("/service/1", "wifi_guid", "test_wifi",
                             shill::kTypeWifi, shill::kStateIdle,
                             add_to_visible);
    service_test->SetServiceProperty("/service/1",
                                     shill::kSecurityClassProperty,
                                     base::Value(shill::kSecurityClassNone));

    base::RunLoop().RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_;
  ash::NetworkHandlerTestHelper network_handler_test_helper_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool_;
  scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container_;
  ash::system::ScopedFakeStatisticsProvider statistics_provider_;
};

TEST_F(NetworkHealthDataCollectorTest, CollectAndExportData) {
  // Initialize NetworkHealthDataCollector for testing.
  NetworkHealthDataCollector data_collector;
  data_collector.SetLogSourceForTesting(std::make_unique<TestLogSource>());

  // Test data collection and PII detection.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_collect_data;
  data_collector.CollectDataAndDetectPII(test_future_collect_data.GetCallback(),
                                         task_runner_for_redaction_tool_,
                                         redaction_tool_container_);
  // Check if CollectDataAndDetectPII call returned an error.
  std::optional<SupportToolError> error = test_future_collect_data.Get();
  EXPECT_EQ(error, std::nullopt);

  PIIMap detected_pii = data_collector.GetDetectedPII();
  EXPECT_THAT(detected_pii, ContainerEq(kExpectedPIIMap));

  // Check PII removal and data export.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_export_data;
  base::FilePath output_dir = GetTempDirForOutput();
  // Export collected data to a directory and remove all PII from it.
  data_collector.ExportCollectedDataWithPII(
      /*pii_types_to_keep=*/{}, output_dir, task_runner_for_redaction_tool_,
      redaction_tool_container_, test_future_export_data.GetCallback());
  // Check if ExportCollectedDataWithPII call returned an error.
  error = test_future_export_data.Get();
  EXPECT_EQ(error, std::nullopt);
  // Read the output file.
  std::string output_file_contents;
  {
    // Allow blocking for file operations.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(
        output_dir
            .Append(FILE_PATH_LITERAL(system_logs::kNetworkHealthSnapshotEntry))
            .AddExtension(FILE_PATH_LITERAL(".log")),
        &output_file_contents));
  }

  EXPECT_THAT(output_file_contents, StrEq(kRedactedNetworkHealthSnapshot));
}
