// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ash/network_routes_data_collector.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrEq;

namespace {

// We have one fake routes output for both IPv4 and IPv6 because we can set the
// routes that will be returned by FakeDebugDaemonClient once but
// NetworkRoutesDataCollector will call it twice: one for IPv4 and other for
// IPv6.
const std::vector<std::string> fake_routes = {
    "[ ip -4/-6 rule list ]\n"
    "0: from all lookup local\n"
    "9: from all lookup main\n"
    "10: from all fwmark 0x3ea0000/0xffff0000 lookup 1002\n"
    "10: from all oif eth0 lookup 1002\n"
    "10: from 255.255.155.2/23 lookup 1002\n"
    "10: from all iif eth0 lookup 1002\n"
    "32765: from all lookup 1002\n"
    "32766: from all lookup main\n"
    "32767: from all lookup default\n",
    "[ ip -4/-6 route show table all ]\n"
    "default via 255.255.155.255 dev eth0 table 1002 metric 10\n"
    "100.115.92.0/30 dev arcbr0 proto kernel scope link src 100.115.92.1\n"
    "100.115.92.4/30 dev arc_eth0 proto kernel scope link src "
    "100.115.92.5\n"
    "100.115.92.12/30 dev arc_wlan0 proto kernel scope link src "
    "100.115.92.13\n"
    "100.115.92.128/30 via 100.115.92.129 dev arc_ns0\n"
    "172.11.5.5/23 dev eth0 proto kernel scope link src 255.255.155.2\n"
    "broadcast 100.115.92.0 dev arcbr0 table local proto kernel scope link "
    "src 100.115.92.1\n"
    "local 100.115.92.1 dev arcbr0 table local proto kernel scope host src "
    "100.115.92.1\n"
    "broadcast 100.115.92.3 dev arcbr0 table local proto kernel scope link "
    "src 100.115.92.1\n"
    "broadcast 100.115.92.4 dev arc_eth0 table local proto kernel scope "
    "link src 100.115.92.5",
    "[ ip -4/-6 route show table main ]\n"
    "100.115.92.12/30 dev arc_wlan0 proto kernel scope link src 100.115.92.13\n"
    "100.115.92.128/30 via 100.115.92.129 dev arc_ns0\n"
    "100.115.92.140/30 dev arc_ns9 proto kernel scope link src 100.115.92.141\n"
    "172.11.5.5/23 dev eth0 proto kernel scope link src 255.255.155.2\n",
    "[ ip -4/-6 route show table 1002 ]\n"
    "default via 255.255.155.255 dev eth0 metric 10\n"};

// We will use `fake_routes` for both IPv4 and IPv6 outputs. That's why in the
// `routes_output_redacted` we have a repetition of the same string twice, one
// for IPv4 and other for IPv6 output.
const char redacted_routes_output[] =
    "[ ip -4/-6 rule list ]\n"
    "0: from all lookup local\n"
    "9: from all lookup main\n"
    "10: from all fwmark 0x3ea0000/0xffff0000 lookup 1002\n"
    "10: from all oif eth0 lookup 1002\n"
    "10: from (IPv4: 1)/23 lookup 1002\n"
    "10: from all iif eth0 lookup 1002\n"
    "32765: from all lookup 1002\n"
    "32766: from all lookup main\n"
    "32767: from all lookup default\n"
    "\n"
    "[ ip -4/-6 route show table all ]\n"
    "default via (IPv4: 2) dev eth0 table 1002 metric 10\n"
    "100.115.92.0/30 dev arcbr0 proto kernel scope link src 100.115.92.1\n"
    "100.115.92.4/30 dev arc_eth0 proto kernel scope link src "
    "100.115.92.5\n"
    "100.115.92.12/30 dev arc_wlan0 proto kernel scope link src "
    "100.115.92.13\n"
    "100.115.92.128/30 via 100.115.92.129 dev arc_ns0\n"
    "(IPv4: 3)/23 dev eth0 proto kernel scope link src (IPv4: 1)\n"
    "broadcast 100.115.92.0 dev arcbr0 table local proto kernel scope link "
    "src 100.115.92.1\n"
    "local 100.115.92.1 dev arcbr0 table local proto kernel scope host src "
    "100.115.92.1\n"
    "broadcast 100.115.92.3 dev arcbr0 table local proto kernel scope link "
    "src 100.115.92.1\n"
    "broadcast 100.115.92.4 dev arc_eth0 table local proto kernel scope "
    "link src 100.115.92.5\n"
    "[ ip -4/-6 route show table main ]\n"
    "100.115.92.12/30 dev arc_wlan0 proto kernel scope link src 100.115.92.13\n"
    "100.115.92.128/30 via 100.115.92.129 dev arc_ns0\n"
    "100.115.92.140/30 dev arc_ns9 proto kernel scope link src 100.115.92.141\n"
    "(IPv4: 3)/23 dev eth0 proto kernel scope link src (IPv4: 1)\n"
    "\n"
    "[ ip -4/-6 route show table 1002 ]\n"
    "default via (IPv4: 2) dev eth0 metric 10\n"
    "\n"
    "[ ip -4/-6 rule list ]\n"
    "0: from all lookup local\n"
    "9: from all lookup main\n"
    "10: from all fwmark 0x3ea0000/0xffff0000 lookup 1002\n"
    "10: from all oif eth0 lookup 1002\n"
    "10: from (IPv4: 1)/23 lookup 1002\n"
    "10: from all iif eth0 lookup 1002\n"
    "32765: from all lookup 1002\n"
    "32766: from all lookup main\n"
    "32767: from all lookup default\n"
    "\n"
    "[ ip -4/-6 route show table all ]\n"
    "default via (IPv4: 2) dev eth0 table 1002 metric 10\n"
    "100.115.92.0/30 dev arcbr0 proto kernel scope link src 100.115.92.1\n"
    "100.115.92.4/30 dev arc_eth0 proto kernel scope link src "
    "100.115.92.5\n"
    "100.115.92.12/30 dev arc_wlan0 proto kernel scope link src "
    "100.115.92.13\n"
    "100.115.92.128/30 via 100.115.92.129 dev arc_ns0\n"
    "(IPv4: 3)/23 dev eth0 proto kernel scope link src (IPv4: 1)\n"
    "broadcast 100.115.92.0 dev arcbr0 table local proto kernel scope link "
    "src 100.115.92.1\n"
    "local 100.115.92.1 dev arcbr0 table local proto kernel scope host src "
    "100.115.92.1\n"
    "broadcast 100.115.92.3 dev arcbr0 table local proto kernel scope link "
    "src 100.115.92.1\n"
    "broadcast 100.115.92.4 dev arc_eth0 table local proto kernel scope "
    "link src 100.115.92.5\n"
    "[ ip -4/-6 route show table main ]\n"
    "100.115.92.12/30 dev arc_wlan0 proto kernel scope link src 100.115.92.13\n"
    "100.115.92.128/30 via 100.115.92.129 dev arc_ns0\n"
    "100.115.92.140/30 dev arc_ns9 proto kernel scope link src 100.115.92.141\n"
    "(IPv4: 3)/23 dev eth0 proto kernel scope link src (IPv4: 1)\n"
    "\n"
    "[ ip -4/-6 route show table 1002 ]\n"
    "default via (IPv4: 2) dev eth0 metric 10\n";
}  // namespace

class NetworkRoutesDataCollectorTest : public ::testing::Test {
 public:
  NetworkRoutesDataCollectorTest() {
    // Set up task runner and container for RedactionTool. We will use when
    // calling CollectDataAndDetectPII() and ExportCollectedDataWithPII()
    // functions on NetworkRoutesDataCollector for testing.
    task_runner_for_redaction_tool_ =
        base::ThreadPool::CreateSequencedTaskRunner({});
    redaction_tool_container_ =
        base::MakeRefCounted<redaction::RedactionToolContainer>(
            task_runner_for_redaction_tool_, nullptr);
  }

  NetworkRoutesDataCollectorTest(const NetworkRoutesDataCollectorTest&) =
      delete;
  NetworkRoutesDataCollectorTest& operator=(
      const NetworkRoutesDataCollectorTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ash::DebugDaemonClient::InitializeFake();
    static_cast<ash::FakeDebugDaemonClient*>(ash::DebugDaemonClient::Get())
        ->SetRoutesForTesting(fake_routes);
  }

  void TearDown() override {
    if (!temp_dir_.IsValid())
      return;
    EXPECT_TRUE(temp_dir_.Delete());

    ash::DebugDaemonClient::Shutdown();
  }

 protected:
  base::FilePath GetTempDirForOutput() { return temp_dir_.GetPath(); }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool_;
  scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container_;
};

TEST_F(NetworkRoutesDataCollectorTest, CollectAndExportData) {
  // Initialize NetworkRoutesDataCollector for testing.
  NetworkRoutesDataCollector data_collector;

  // Test data collection and PII detection.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_collect_data;
  data_collector.CollectDataAndDetectPII(test_future_collect_data.GetCallback(),
                                         task_runner_for_redaction_tool_,
                                         redaction_tool_container_);
  // Check if CollectDataAndDetectPII call returned an error.
  std::optional<SupportToolError> error = test_future_collect_data.Get();
  EXPECT_EQ(error, std::nullopt);

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
  ASSERT_TRUE(base::ReadFileToString(
      output_dir.Append(FILE_PATH_LITERAL("network_routes"))
          .AddExtension(FILE_PATH_LITERAL(".txt")),
      &output_file_contents));

  EXPECT_THAT(output_file_contents, StrEq(redacted_routes_output));
}
