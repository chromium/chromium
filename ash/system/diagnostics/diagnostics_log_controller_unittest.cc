// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/diagnostics_log_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {

namespace {

const char kLogFileContents[] = "Diagnostics Log";
const char kTestSessionLogFileName[] = "test_session_log.txt";

// Fake delegate used to set the expected user directory path.
class FakeDiagnosticsBrowserDelegate : public DiagnosticsBrowserDelegate {
 public:
  FakeDiagnosticsBrowserDelegate() = default;
  ~FakeDiagnosticsBrowserDelegate() override = default;

  base::FilePath GetActiveUserProfileDir() override { return base::FilePath(); }
};

}  // namespace

class DiagnosticsLogControllerTest : public NoSessionAshTestBase {
 public:
  DiagnosticsLogControllerTest() = default;
  DiagnosticsLogControllerTest(DiagnosticsLogControllerTest&) = delete;
  DiagnosticsLogControllerTest& operator=(DiagnosticsLogControllerTest&) =
      delete;
  ~DiagnosticsLogControllerTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        ash::features::kEnableLogControllerForDiagnosticsApp);

    NoSessionAshTestBase::SetUp();
  }

 protected:
  base::FilePath GetSessionLogPath() {
    EXPECT_TRUE(save_dir_.CreateUniqueTempDir());
    return save_dir_.GetPath().Append(kTestSessionLogFileName);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir save_dir_;
};

TEST_F(DiagnosticsLogControllerTest,
       ShellProvidesControllerWhenFeatureEnabled) {
  EXPECT_NO_FATAL_FAILURE(DiagnosticsLogController::Get());
  EXPECT_NE(nullptr, DiagnosticsLogController::Get());
}

TEST_F(DiagnosticsLogControllerTest, IsInitializedAfterDelegateProvided) {
  EXPECT_NE(nullptr, DiagnosticsLogController::Get());
  EXPECT_FALSE(DiagnosticsLogController::IsInitialized());
  DiagnosticsLogController::Initialize(
      std::make_unique<FakeDiagnosticsBrowserDelegate>());
  EXPECT_TRUE(DiagnosticsLogController::IsInitialized());
}

TEST_F(DiagnosticsLogControllerTest, GenerateSessionLogOnBlockingPoolFile) {
  const base::FilePath save_file_path = GetSessionLogPath();
  EXPECT_TRUE(DiagnosticsLogController::Get()->GenerateSessionLogOnBlockingPool(
      save_file_path));
  EXPECT_TRUE(base::PathExists(save_file_path));

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(save_file_path, &contents));
  EXPECT_EQ(kLogFileContents, contents);
}

}  // namespace diagnostics
}  // namespace ash
