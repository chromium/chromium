// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/telemetry_log.h"

#include "ash/system/diagnostics/log_test_helpers.h"
#include "ash/webui/diagnostics_ui/mojom/system_data_provider.mojom.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {
namespace {

mojom::SystemInfoPtr CreateSystemInfoPtr(const std::string& board_name,
                                         const std::string& marketing_name,
                                         const std::string& cpu_model,
                                         uint32_t total_memory_kib,
                                         uint16_t cpu_threads_count,
                                         uint32_t cpu_max_clock_speed_khz,
                                         bool has_battery,
                                         const std::string& milestone_version,
                                         const std::string& full_version) {
  auto version_info = mojom::VersionInfo::New(milestone_version, full_version);
  auto device_capabilities = mojom::DeviceCapabilities::New(has_battery);

  auto system_info = mojom::SystemInfo::New(
      board_name, marketing_name, cpu_model, total_memory_kib,
      cpu_threads_count, cpu_max_clock_speed_khz, std::move(version_info),
      std::move(device_capabilities));
  return system_info;
}

}  // namespace

class TelemetryLogTest : public testing::Test {
 public:
  TelemetryLogTest() = default;

  ~TelemetryLogTest() override = default;
};

TEST_F(TelemetryLogTest, DetailedLogContents) {
  const std::string expected_board_name = "board_name";
  const std::string expected_marketing_name = "marketing_name";
  const std::string expected_cpu_model = "cpu_model";
  const uint32_t expected_total_memory_kib = 1234;
  const uint16_t expected_cpu_threads_count = 5678;
  const uint32_t expected_cpu_max_clock_speed_khz = 91011;
  const bool expected_has_battery = true;
  const std::string expected_milestone_version = "M99";
  const std::string expected_full_version = "M99.1234.5.6";

  mojom::SystemInfoPtr test_info = CreateSystemInfoPtr(
      expected_board_name, expected_marketing_name, expected_cpu_model,
      expected_total_memory_kib, expected_cpu_threads_count,
      expected_cpu_max_clock_speed_khz, expected_has_battery,
      expected_milestone_version, expected_full_version);

  TelemetryLog log;

  log.UpdateSystemInfo(test_info.Clone());

  const std::string log_as_string = log.GetContents();
  const std::vector<std::string> log_lines = GetLogLines(log_as_string);

  const std::string expected_snapshot_time_prefix = "Snapshot Time: ";
  // Expect one title line and 9 content lines.
  EXPECT_EQ(10u, log_lines.size());
  EXPECT_GT(log_lines[1].size(), expected_snapshot_time_prefix.size());
  EXPECT_TRUE(base::StartsWith(log_lines[1], expected_snapshot_time_prefix));
  EXPECT_EQ("Board Name: " + expected_board_name, log_lines[2]);
  EXPECT_EQ("Marketing Name: " + expected_marketing_name, log_lines[3]);
  EXPECT_EQ("CpuModel Name: " + expected_cpu_model, log_lines[4]);
  EXPECT_EQ(
      "Total Memory (kib): " + base::NumberToString(expected_total_memory_kib),
      log_lines[5]);
  EXPECT_EQ(
      "Thread Count:  " + base::NumberToString(expected_cpu_threads_count),
      log_lines[6]);
  EXPECT_EQ("Cpu Max Clock Speed (kHz):  " +
                base::NumberToString(expected_cpu_max_clock_speed_khz),
            log_lines[7]);
  EXPECT_EQ("Version: " + expected_full_version, log_lines[8]);
  EXPECT_EQ("Has Battery: true", log_lines[9]);
}

TEST_F(TelemetryLogTest, ChangeContents) {
  const std::string expected_board_name = "board_name";
  const std::string expected_marketing_name = "marketing_name";
  const std::string expected_cpu_model = "cpu_model";
  const uint32_t expected_total_memory_kib = 1234;
  const uint16_t expected_cpu_threads_count = 5678;
  const uint32_t expected_cpu_max_clock_speed_khz = 91011;
  const bool expected_has_battery = true;
  const std::string expected_milestone_version = "M99";
  const std::string expected_full_version = "M99.1234.5.6";

  mojom::SystemInfoPtr test_info = CreateSystemInfoPtr(
      expected_board_name, expected_marketing_name, expected_cpu_model,
      expected_total_memory_kib, expected_cpu_threads_count,
      expected_cpu_max_clock_speed_khz, expected_has_battery,
      expected_milestone_version, expected_full_version);

  TelemetryLog log;

  log.UpdateSystemInfo(test_info.Clone());

  test_info->board_name = "new board_name";

  log.UpdateSystemInfo(test_info.Clone());

  const std::string log_as_string = log.GetContents();
  const std::vector<std::string> log_lines = GetLogLines(log_as_string);
}

TEST_F(TelemetryLogTest, CpuUsageUint8) {
  const std::string expected_board_name = "board_name";
  const std::string expected_marketing_name = "marketing_name";
  const std::string expected_cpu_model = "cpu_model";
  const uint32_t expected_total_memory_kib = 1234;
  const uint16_t expected_cpu_threads_count = 5678;
  const uint32_t expected_cpu_max_clock_speed_khz = 91011;
  const bool expected_has_battery = true;
  const std::string expected_milestone_version = "M99";
  const std::string expected_full_version = "M99.1234.5.6";
  const uint8_t percent_usage_user = 10;
  const uint8_t percent_usage_system = 20;
  const uint8_t percent_usage_free = 80;
  const uint16_t average_cpu_temp_celsius = 31;
  const uint32_t scaling_current_frequency_khz = 500;

  mojom::SystemInfoPtr test_info = CreateSystemInfoPtr(
      expected_board_name, expected_marketing_name, expected_cpu_model,
      expected_total_memory_kib, expected_cpu_threads_count,
      expected_cpu_max_clock_speed_khz, expected_has_battery,
      expected_milestone_version, expected_full_version);

  mojom::CpuUsagePtr cpu_usage = mojom::CpuUsage::New(
      percent_usage_user, percent_usage_system, percent_usage_free,
      average_cpu_temp_celsius, scaling_current_frequency_khz);

  TelemetryLog log;

  log.UpdateSystemInfo(test_info.Clone());
  log.UpdateCpuUsage(cpu_usage.Clone());

  const std::string log_as_string = log.GetContents();
  const std::vector<std::string> log_lines = GetLogLines(log_as_string);

  EXPECT_EQ("Usage User (%): " + base::NumberToString(percent_usage_user),
            log_lines[11]);
  EXPECT_EQ("Usage System (%): " + base::NumberToString(percent_usage_system),
            log_lines[12]);
  EXPECT_EQ("Usage Free (%): " + base::NumberToString(percent_usage_free),
            log_lines[13]);
}

}  // namespace diagnostics
}  // namespace ash
