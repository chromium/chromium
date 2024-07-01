// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/screen_ai_downloader_ash.h"

#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/screen_ai/screen_ai_downloader_chromeos.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

class FakeScreenAIDownloader : public screen_ai::ScreenAIDownloaderChromeOS {
 public:
  void SetLastUsageTime() override {}
  void DownloadComponentInternal() override {
    // The passed file path is not used and just indicates that the component
    // exists.
    SetComponentFolder(base::FilePath(FILE_PATH_LITERAL("tmp")));
  }
};

class ScreenAIDownloaderAshTest : public testing::Test {
 public:
  ScreenAIDownloaderAshTest() = default;

  void GetComponentFolder(
      bool download_if_needed,
      ScreenAIDownloaderAsh::GetComponentFolderCallback callback) {
    downloader_remote_->GetComponentFolder(
        /*download_if_needed=*/true, std::move(callback));
    FlushForTesting();
  }

  void FlushForTesting() { downloader_ash_.receivers_.FlushForTesting(); }

  bool HasPendingCallbacks() {
    return !downloader_ash_.pending_download_callback_map_.empty();
  }

  bool HasReceivers() { return !downloader_ash_.receivers_.empty(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  FakeScreenAIDownloader install_state_;
  crosapi::ScreenAIDownloaderAsh downloader_ash_;
  mojo::Remote<crosapi::mojom::ScreenAIDownloader> downloader_remote_;
};

TEST_F(ScreenAIDownloaderAshTest, EnsurePendingCallbackDestruction) {
  downloader_ash_.Bind(downloader_remote_.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(HasReceivers());

  // `kDownloading` will create a pending callback.
  install_state_.SetStateForTesting(
      screen_ai::ScreenAIInstallState::State::kDownloading);
  GetComponentFolder(
      /*download_if_needed=*/true,
      base::BindOnce([](const std::optional<::base::FilePath>& file_path) {
        // do nothing
      }));
  EXPECT_TRUE(HasPendingCallbacks());

  // Destroying the remote will trigger a disconnect handler set in the
  // receiver, which will clear out any pending callbacks.
  downloader_remote_.reset();
  FlushForTesting();
  EXPECT_FALSE(HasPendingCallbacks());
  EXPECT_FALSE(HasReceivers());
}

class ScreenAIDownloaderAshReplyTest
    : public ScreenAIDownloaderAshTest,
      public testing::WithParamInterface<testing::tuple<
          screen_ai::ScreenAIInstallState::State /*prior_state_*/,
          bool /*start_observing_before_call_*/,
          bool /*request_download_if_needed_*/,
          bool /*fake_download_success_*/>> {
 public:
  ScreenAIDownloaderAshReplyTest() {
    downloader_ash_.Bind(downloader_remote_.BindNewPipeAndPassReceiver());
    if (start_observing_before_call_) {
      base::test::TestFuture<const std::optional<::base::FilePath>&> future;
      // Calling `GetComponentFolder` for the first time sets the observer.
      GetComponentFolder(
          /*download_if_needed=*/true, future.GetCallback());
      EXPECT_TRUE(future.Wait());

      // Remove downloaded component path and reset state.
      install_state_.ResetForTesting();

      EXPECT_TRUE(downloader_ash_.install_state_observer_.IsObserving());
    }

    if (prior_state_ == screen_ai::ScreenAIInstallState::State::kDownloaded) {
      // The component is already downloaded.
      install_state_.DownloadComponentInternal();
    }
    install_state_.SetStateForTesting(prior_state_);
  }

  void MockDownloadCompletion() {
    if (fake_download_success_) {
      install_state_.DownloadComponentInternal();
    } else {
      install_state_.SetState(
          screen_ai::ScreenAIInstallState::State::kDownloadFailed);
    }
  }

 protected:
  // Test params.
  screen_ai::ScreenAIInstallState::State prior_state_ = std::get<0>(GetParam());
  bool start_observing_before_call_ = std::get<1>(GetParam());
  bool request_download_if_needed_ = std::get<2>(GetParam());
  bool fake_download_success_ = std::get<3>(GetParam());
};

// Tests if the callback function of `GetComponentFolder` always gets called
// regardless of the prior state in `ScreenAIInstallState`,
// `ScreenAIDownloaderAsh`, and call params. The state is composed of:
//  - Existing state in `ScreenAIInstallState`.
//  - If `ScreenAIDownloaderAsh` is observing `ScreenAIInstallState` before the
//    call.
//  - Call parameter `download_if_needed`.
//  - If fake download should result successful.
TEST_P(ScreenAIDownloaderAshReplyTest, EnsureReplyInAllStates) {
  base::test::TestFuture<const std::optional<::base::FilePath>&> future;
  GetComponentFolder(
      /*download_if_needed=*/request_download_if_needed_, future.GetCallback());

  // A `downloading` state will eventually result in a state change to
  // `downloaded` or `failed`.
  if (prior_state_ == screen_ai::ScreenAIInstallState::State::kDownloading) {
    MockDownloadCompletion();
  }

  EXPECT_TRUE(future.Wait());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ScreenAIDownloaderAshReplyTest,
    testing::Combine(
        testing::Values(screen_ai::ScreenAIInstallState::State::kNotDownloaded,
                        screen_ai::ScreenAIInstallState::State::kDownloading,
                        screen_ai::ScreenAIInstallState::State::kDownloadFailed,
                        screen_ai::ScreenAIInstallState::State::kDownloaded),
        testing::Bool(),
        testing::Bool(),
        testing::Bool()));

}  // namespace crosapi
