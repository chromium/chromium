// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/screen_ai_downloader_ash.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/screen_ai/screen_ai_downloader_chromeos.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

class FakeScreenAIDownloader : public screen_ai::ScreenAIDownloaderChromeOS {
 public:
  void SetLastUsageTime() override{};
  void DownloadComponentInternal() override {
    // The passed file path is not used and just indicates that the component
    // exists.
    SetComponentFolder(base::FilePath(FILE_PATH_LITERAL("tmp")));
  }
};

class ScreenAIDownloaderAshTest
    : public testing::Test,
      public testing::WithParamInterface<testing::tuple<
          screen_ai::ScreenAIInstallState::State /*prior_state_*/,
          bool /*start_observing_before_call_*/,
          bool /*request_download_if_needed_*/,
          bool /*fake_download_success_*/>> {
 public:
  ScreenAIDownloaderAshTest() {
    if (start_observing_before_call_) {
      base::RunLoop run_loop;
      // Calling `GetComponentFolder` for the first time sets the observer.
      downloader_ash_.GetComponentFolder(
          /*download_if_needed=*/true,
          base::BindOnce(
              [](base::RunLoop* run_loop,
                 const absl::optional<::base::FilePath>& file_path) {
                run_loop->Quit();
              },
              &run_loop));
      run_loop.Run();

      // Remove downloaded component path and reset state.
      install_state_.ResetForTesting();

      EXPECT_TRUE(downloader_ash_.install_state_observer_.IsObserving());
    }

    if (prior_state_ == screen_ai::ScreenAIInstallState::State::kDownloaded ||
        prior_state_ == screen_ai::ScreenAIInstallState::State::kReady) {
      // The component is already downloaded in these two states.
      install_state_.DownloadComponentInternal();
    }
    install_state_.SetStateForTesting(prior_state_);
  }

  void GetComponentFolder(
      bool download_if_needed,
      ScreenAIDownloaderAsh::GetComponentFolderCallback callback) {
    downloader_ash_.GetComponentFolder(download_if_needed, std::move(callback));
  }

  void MockDownloadCompletion() {
    if (fake_download_success_) {
      install_state_.DownloadComponentInternal();
    } else {
      install_state_.SetState(screen_ai::ScreenAIInstallState::State::kFailed);
    }
  }

 protected:
  // Test params.
  screen_ai::ScreenAIInstallState::State prior_state_ = std::get<0>(GetParam());
  bool start_observing_before_call_ = std::get<1>(GetParam());
  bool request_download_if_needed_ = std::get<2>(GetParam());
  bool fake_download_success_ = std::get<3>(GetParam());

  base::test::SingleThreadTaskEnvironment task_environment_;
  crosapi::ScreenAIDownloaderAsh downloader_ash_;
  FakeScreenAIDownloader install_state_;
};

// Tests if the callback function of `GetComponentFolder` always gets called
// regardless of the prior state in `ScreenAIInstallState`,
// `ScreenAIDownloaderAsh`, and call params. The state is composed of:
//  - Existing state in `ScreenAIInstallState`.
//  - If `ScreenAIDownloaderAsh` is observing `ScreenAIInstallState` before the
//    call.
//  - Call parameter `download_if_needed`.
//  - If fake download should result successful.
TEST_P(ScreenAIDownloaderAshTest, EnsureReplyInAllStates) {
  base::RunLoop run_loop;
  GetComponentFolder(
      /*download_if_needed=*/request_download_if_needed_,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<::base::FilePath>& file_path) {
            run_loop->Quit();
          },
          &run_loop));

  // A `downloading` state will eventually result in a state change to
  // `downloaded` or `failed`.
  if (prior_state_ == screen_ai::ScreenAIInstallState::State::kDownloading) {
    MockDownloadCompletion();
  }

  run_loop.Run();
}
INSTANTIATE_TEST_SUITE_P(
    All,
    ScreenAIDownloaderAshTest,
    testing::Combine(
        testing::Values(screen_ai::ScreenAIInstallState::State::kNotDownloaded,
                        screen_ai::ScreenAIInstallState::State::kDownloading,
                        screen_ai::ScreenAIInstallState::State::kFailed,
                        screen_ai::ScreenAIInstallState::State::kDownloaded,
                        screen_ai::ScreenAIInstallState::State::kReady),
        testing::Bool(),
        testing::Bool(),
        testing::Bool()));

}  // namespace crosapi
