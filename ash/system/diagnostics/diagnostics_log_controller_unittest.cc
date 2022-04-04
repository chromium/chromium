// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/diagnostics_log_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {

namespace {

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

 private:
  base::test::ScopedFeatureList feature_list_;
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

}  // namespace diagnostics
}  // namespace ash
