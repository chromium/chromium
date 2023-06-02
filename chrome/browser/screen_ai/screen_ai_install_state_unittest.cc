// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_install_state.h"

#include "base/files/file_path.h"
#include "base/scoped_observation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace screen_ai {

class ScreenAIInstallStateTest : public testing::Test,
                                 ScreenAIInstallState::Observer {
 public:
  ScreenAIInstallStateTest() {
    ScreenAIInstallState::GetInstance()->ResetForTesting();
  }

  void StartObservation() {
    component_downloaded_observer_.Observe(ScreenAIInstallState::GetInstance());
  }

  void MakeComponentDownloaded() {
    // The passed file path is not used and just indicates that the component
    // exists.
    ScreenAIInstallState::GetInstance()->SetComponentFolder(
        base::FilePath(FILE_PATH_LITERAL("tmp")));
  }

  void StateChanged(ScreenAIInstallState::State state) override {
    if (state == ScreenAIInstallState::State::kDownloaded) {
      component_downloaded_received_ = true;
    }
  }

  bool ComponentDownloadedReceived() { return component_downloaded_received_; }

 private:
  base::ScopedObservation<screen_ai::ScreenAIInstallState,
                          ScreenAIInstallState::Observer>
      component_downloaded_observer_{this};

  bool component_downloaded_received_ = false;
};

TEST_F(ScreenAIInstallStateTest, NeverDownloaded) {
  StartObservation();
  EXPECT_FALSE(ComponentDownloadedReceived());
}

TEST_F(ScreenAIInstallStateTest, DownloadedBeforeObservation) {
  MakeComponentDownloaded();
  StartObservation();
  EXPECT_TRUE(ComponentDownloadedReceived());
}

TEST_F(ScreenAIInstallStateTest, DownloadedAfterObservation) {
  StartObservation();
  MakeComponentDownloaded();
  EXPECT_TRUE(ComponentDownloadedReceived());
}

}  // namespace screen_ai
