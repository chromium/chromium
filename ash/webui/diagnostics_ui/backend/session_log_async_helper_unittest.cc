// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/session_log_async_helper.h"

#include <memory>

#include "ash/system/diagnostics/log_test_helpers.h"
#include "ash/system/diagnostics/networking_log.h"
#include "ash/system/diagnostics/routine_log.h"
#include "ash/system/diagnostics/telemetry_log.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::diagnostics {

namespace {

constexpr char kFilePathName[] = "session_log.txt";
constexpr char kRoutineLogSubsectionHeader[] = "--- Test Routines ---";
constexpr char kSystemLogSectionHeader[] = "=== System ===";
constexpr char kNetworkingLogSectionHeader[] = "=== Networking ===";
constexpr char kNetworkInfoSectionHeader[] = "--- Network Info ---";
constexpr char kNetworkEventsSectionHeader[] = "--- Network Events ---";
constexpr char kNoRoutinesRun[] =
    "No routines of this type were run in the session.";

class SessionLogAsyncHelperTest : public testing::Test {
 public:
  SessionLogAsyncHelperTest() = default;
  ~SessionLogAsyncHelperTest() override = default;

  void SetUp() override { EXPECT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override { task_environment_.RunUntilIdle(); }

  const base::FilePath GetTempPath() { return temp_dir_.GetPath(); }

 private:
  base::test::TaskEnvironment task_environment_{};
  base::ScopedTempDir temp_dir_;
};

TEST_F(SessionLogAsyncHelperTest, CreateSessionLogFile) {
  auto helper = std::make_unique<SessionLogAsyncHelper>();
  const base::FilePath path = GetTempPath();
  const base::FilePath file_path = path.Append(kFilePathName);
  auto telemetry_log = std::make_unique<TelemetryLog>();
  auto routine_log = std::make_unique<RoutineLog>(path);
  auto networking_log = std::make_unique<NetworkingLog>(path);

  helper->CreateSessionLogOnBlockingPool(
      file_path, telemetry_log.get(), routine_log.get(), networking_log.get());

  EXPECT_TRUE(base::PathExists(file_path));
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &contents));
  std::vector<std::string> lines = GetLogLines(contents);
  EXPECT_EQ(8u, lines.size());
  EXPECT_EQ(kSystemLogSectionHeader, lines[0]);
  EXPECT_EQ(kRoutineLogSubsectionHeader, lines[1]);
  EXPECT_EQ(kNoRoutinesRun, lines[2]);
  EXPECT_EQ(kNetworkingLogSectionHeader, lines[3]);
  EXPECT_EQ(kNetworkInfoSectionHeader, lines[4]);
  EXPECT_EQ(kRoutineLogSubsectionHeader, lines[5]);
  EXPECT_EQ(kNoRoutinesRun, lines[6]);
  EXPECT_EQ(kNetworkEventsSectionHeader, lines[7]);
}

TEST_F(SessionLogAsyncHelperTest, HandlesNullLogPointers) {
  auto helper = std::make_unique<SessionLogAsyncHelper>();
  const base::FilePath file_path = GetTempPath().Append(kFilePathName);
  helper->CreateSessionLogOnBlockingPool(file_path, /*telemetry_log=*/nullptr,
                                         /*routine_log=*/nullptr,
                                         /*networking_log=*/nullptr);

  EXPECT_TRUE(base::PathExists(file_path));
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &contents));
  std::vector<std::string> lines = GetLogLines(contents);
  EXPECT_EQ(6u, lines.size());
  EXPECT_EQ(kSystemLogSectionHeader, lines[0]);
  EXPECT_EQ(kRoutineLogSubsectionHeader, lines[1]);
  EXPECT_EQ(kNoRoutinesRun, lines[2]);
  EXPECT_EQ(kNetworkingLogSectionHeader, lines[3]);
  EXPECT_EQ(kRoutineLogSubsectionHeader, lines[4]);
  EXPECT_EQ(kNoRoutinesRun, lines[5]);
}

}  // namespace
}  // namespace ash::diagnostics
