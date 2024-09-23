// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_install_state.h"

#include "base/files/file_path.h"
#include "base/scoped_observation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestScreenAIInstallState : public screen_ai::ScreenAIInstallState {
 public:
  TestScreenAIInstallState() = default;

  TestScreenAIInstallState(const TestScreenAIInstallState&) = delete;
  TestScreenAIInstallState& operator=(const TestScreenAIInstallState&) = delete;

  ~TestScreenAIInstallState() override = default;

  void SetLastUsageTime() override {}

  void DownloadComponentInternal() override {
    // The passed file path is not used and just indicates that the component
    // exists.
    ScreenAIInstallState::GetInstance()->SetComponentFolder(
        base::FilePath(FILE_PATH_LITERAL("tmp")));
  }
};

}  // namespace

namespace screen_ai {

class ScreenAIInstallStateTest : public testing::Test,
                                 ScreenAIInstallState::Observer {
 public:
  ScreenAIInstallStateTest() = default;

  void StartObservation() {
    component_downloaded_observer_.Observe(ScreenAIInstallState::GetInstance());
  }

  void DownloadComponent() { test_install_state_.DownloadComponentInternal(); }

  void StateChanged(ScreenAIInstallState::State state) override {
    if (state == ScreenAIInstallState::State::kDownloaded) {
      component_downloaded_received_ = true;
    }
  }

  bool ComponentDownloadedReceived() { return component_downloaded_received_; }

 private:
  TestScreenAIInstallState test_install_state_;

  base::ScopedObservation<screen_ai::ScreenAIInstallState,
                          ScreenAIInstallState::Observer>
      component_downloaded_observer_{this};

  bool component_downloaded_received_ = false;
};

TEST_F(ScreenAIInstallStateTest, AddingObserverTriggersDownload) {
  StartObservation();
  EXPECT_TRUE(ComponentDownloadedReceived());
}

TEST_F(ScreenAIInstallStateTest, DownloadedBeforeObservation) {
  DownloadComponent();
  StartObservation();
  EXPECT_TRUE(ComponentDownloadedReceived());
}

TEST_F(ScreenAIInstallStateTest, ObservationAfterFailure) {
  ScreenAIInstallState::GetInstance()->SetStateForTesting(
      screen_ai::ScreenAIInstallState::State::kDownloadFailed);
  StartObservation();
  EXPECT_TRUE(ComponentDownloadedReceived());
}

}  // namespace screen_ai
