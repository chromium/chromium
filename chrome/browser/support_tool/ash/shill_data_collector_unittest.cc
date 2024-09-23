// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ash/shill_data_collector.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ContainerEq;
using ::testing::IsSupersetOf;

constexpr char kNetworkDevices[] = "network_devices";
constexpr char kNetworkServices[] = "network_services";

constexpr char kDeviceStart[] = R"("/device/wifi1")";
constexpr char kServiceStart[] = R"("/service/wifi1")";

struct TestData {
  // Name of the data in the log source and the file that the logs will be
  // exported into.
  std::string data_source_name;
  // The logs that will be collected.
  std::string test_logs;
  // The version of the logs that has PII sensitive data redacted.
  std::string test_logs_pii_redacted;
};

const TestData kTestData[] = {
    {/*data_source_name=*/kNetworkDevices,
     /*test_logs=*/R"("/device/wifi1": {
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
      })",
     /*test_logs_pii_redacted=*/R"data("/device/wifi1": {
         "Address": "23456789abcd",
         "DBus.Object": "/device/wifi1",
         "DBus.Service": "org.freedesktop.ModemManager1",
         "IPConfigs": {
            "ipconfig_v4_path": {
               "Address": "(IPv4: 1)",
               "Gateway": "(IPv4: 2)",
               "Method": "ipv4",
               "Prefixlen": 1,
               "WebProxyAutoDiscoveryUrl": "(URL: 1)"
            },
            "ipconfig_v6_path": {
               "Address": "(IPv6: 1)",
               "Method": "ipv6"
            }
         },
         "Name": "*** MASKED ***",
         "Type": "wifi"
      })data"},
    {/*data_source_name=*/kNetworkServices,
     /*test_logs=*/R"("/service/wifi1": {
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
      })",
     /*test_logs_pii_redacted=*/R"("/service/wifi1": {
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
      })"},
};

// The PII sensitive data that the test data contains.
const PIIMap kPIIInTestData = {
    {redaction::PIIType::kIPAddress,
     {"100.0.0.1", "100.0.0.2", "0:0:0:0:100:0:0:1"}},
    {redaction::PIIType::kURL, {"http://wpad.com/wpad.dat"}},
    {redaction::PIIType::kSSID,
     {"\"7769666931\"\n", "stub_wifi_device1", "wifi1"}},
    {redaction::PIIType::kMACAddress, {"0123456789ab", "23456789abcd"}}};

// Types of all PII data contained in the test data
const std::set<redaction::PIIType> kAllPIITypesInData = {
    redaction::PIIType::kIPAddress, redaction::PIIType::kURL,
    redaction::PIIType::kSSID, redaction::PIIType::kMACAddress};

class ShillDataCollectorTest : public ::testing::Test {
 public:
  ShillDataCollectorTest() {
    // Set up task runner and container for RedactionTool. We will use these
    // when creating the SystemLogSourceDataCollector instance for testing.
    task_runner_for_redaction_tool_ =
        base::ThreadPool::CreateSequencedTaskRunner({});
    redaction_tool_container_ =
        base::MakeRefCounted<redaction::RedactionToolContainer>(
            task_runner_for_redaction_tool_, nullptr);
  }

  ShillDataCollectorTest(const ShillDataCollectorTest&) = delete;
  ShillDataCollectorTest& operator=(const ShillDataCollectorTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    // Creates default entries in shill logs
    ash::shill_clients::InitializeFakes();
  }

  void TearDown() override {
    ash::shill_clients::Shutdown();
    if (!temp_dir_.IsValid())
      return;
    EXPECT_TRUE(temp_dir_.Delete());
  }

  // Traverses the files in `directory` and returns the contents of the files in
  // a map.
  std::map<base::FilePath, std::string> ReadFileContentsToMap(
      base::FilePath directory) {
    std::map<base::FilePath, std::string> contents;
    base::FileEnumerator file_enumerator(directory, /*recursive=*/false,
                                         base::FileEnumerator::FILES);
    for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
         path = file_enumerator.Next()) {
      std::string logs;
      EXPECT_TRUE(base::ReadFileToString(path, &logs));
      contents[path] = logs;
    }
    return contents;
  }

 protected:
  base::FilePath GetTempDirForOutput() { return temp_dir_.GetPath(); }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool_;
  scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container_;
};

TEST_F(ShillDataCollectorTest, CollectAndExportUnmaskedData) {
  // Initialize ShillDataCollector for testing.
  ShillDataCollector data_collector;

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
  // Get the types of all PII data detected
  std::set<redaction::PIIType> detected_pii_types;
  base::ranges::transform(
      detected_pii, std::inserter(detected_pii_types, detected_pii_types.end()),
      &PIIMap::value_type::first);
  // If set A is a subset of set B, then A unioned with B equals B
  EXPECT_THAT(detected_pii_types, IsSupersetOf(kAllPIITypesInData));

  // For each PII type, checks if every PII string in the fake entries has
  // been detected.
  for (const auto& pii_type : kAllPIITypesInData) {
    EXPECT_THAT(detected_pii[pii_type],
                IsSupersetOf(kPIIInTestData.at(pii_type)));
  }

  // Check PII removal and data export.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_export_data;
  base::FilePath output_dir = GetTempDirForOutput();
  // Export collected data to a directory and keep all PII.
  data_collector.ExportCollectedDataWithPII(
      /*pii_types_to_keep=*/kAllPIITypesInData, output_dir,
      task_runner_for_redaction_tool_, redaction_tool_container_,
      test_future_export_data.GetCallback());
  // Check if ExportCollectedDataWithPII call returned an error.
  error = test_future_export_data.Get();
  EXPECT_EQ(error, std::nullopt);
  // Read the output file.
  std::string shill_logs;
  EXPECT_TRUE(base::ReadFileToString(
      output_dir.Append(FILE_PATH_LITERAL("shill_properties.txt")),
      &shill_logs));
  // Extracts the fake entries
  std::map<std::string, std::string> result_contents = {
      {kNetworkDevices, shill_logs.substr(shill_logs.find(kDeviceStart),
                                          kTestData[0].test_logs.length())},
      {kNetworkServices, shill_logs.substr(shill_logs.find(kServiceStart),
                                           kTestData[1].test_logs.length())}};
  std::map<std::string, std::string> expected_contents;
  for (const auto& data : kTestData)
    expected_contents[data.data_source_name] = data.test_logs;
  EXPECT_THAT(result_contents, ContainerEq(expected_contents));
}

TEST_F(ShillDataCollectorTest, CollectAndExportMaskedData) {
  // Initialize SystemLogSourceDataCollector for testing.
  ShillDataCollector data_collector;

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
  // Get the types of all PII data detected
  std::set<redaction::PIIType> detected_pii_types;
  base::ranges::transform(
      detected_pii, std::inserter(detected_pii_types, detected_pii_types.end()),
      &PIIMap::value_type::first);
  // If set A is a subset of set B, then A unioned with B equals B
  EXPECT_THAT(detected_pii_types, IsSupersetOf(kAllPIITypesInData));

  // For each PII type, checks if every PII string in the fake entries has
  // been detected.
  for (const auto& pii_type : kAllPIITypesInData) {
    EXPECT_THAT(detected_pii[pii_type],
                IsSupersetOf(kPIIInTestData.at(pii_type)));
  }

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
  std::string shill_logs;
  EXPECT_TRUE(base::ReadFileToString(
      output_dir.Append(FILE_PATH_LITERAL("shill_properties.txt")),
      &shill_logs));
  // Extracts the fake entries
  std::map<std::string, std::string> result_contents = {
      {kNetworkDevices,
       shill_logs.substr(shill_logs.find(kDeviceStart),
                         kTestData[0].test_logs_pii_redacted.length())},
      {kNetworkServices,
       shill_logs.substr(shill_logs.find(kServiceStart),
                         kTestData[1].test_logs_pii_redacted.length())}};
  std::map<std::string, std::string> expected_contents;
  for (const auto& data : kTestData)
    expected_contents[data.data_source_name] = data.test_logs_pii_redacted;
  EXPECT_THAT(result_contents, ContainerEq(expected_contents));
}
