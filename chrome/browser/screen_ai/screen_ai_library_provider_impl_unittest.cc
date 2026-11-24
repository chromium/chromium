// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_library_provider_impl.h"

#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace screen_ai {

namespace {

// An install state that starts a download when it is asked to, and leaves it
// to the test to finish it.
class TestScreenAIInstallState : public ScreenAIInstallState {
 public:
  using ScreenAIInstallState::HasObservers;

  // ScreenAIInstallState:
  void SetLastUsageTime() override { ++set_last_usage_time_count_; }
  void DownloadComponentInternal() override {
    ++download_count_;
    SetState(State::kDownloading);
  }

  int set_last_usage_time_count() const { return set_last_usage_time_count_; }
  int download_count() const { return download_count_; }

 private:
  int set_last_usage_time_count_ = 0;
  int download_count_ = 0;
};

}  // namespace

class ScreenAILibraryProviderImplTest : public testing::Test {
 public:
  ScreenAILibraryProviderImplTest() {
    ScreenAIInstallState::SetDeviceCompatibleForTesting(true);
  }

 protected:
  // Asks for the library and adds the answer to `answers_`.
  base::CallbackListSubscription EnsureLibraryAvailable() {
    return library_provider_.EnsureLibraryAvailable(base::BindLambdaForTesting(
        [this](bool available) { answers_.push_back(available); }));
  }

  void FinishDownload() {
    // The path is not used and just indicates that the library exists.
    install_state_.SetComponentFolder(base::FilePath(FILE_PATH_LITERAL("tmp")));
  }

  void FailDownload() {
    install_state_.SetState(ScreenAIInstallState::State::kDownloadFailed);
  }

  // The provider posts a task for a request that is made while it answers.
  base::test::SingleThreadTaskEnvironment task_environment_;
  // Replaces the install state singleton while the test runs.
  TestScreenAIInstallState install_state_;
  ScreenAILibraryProviderImpl library_provider_;
  std::vector<bool> answers_;
};

TEST_F(ScreenAILibraryProviderImplTest, AnswersWhenLibraryIsDownloaded) {
  base::CallbackListSubscription subscription = EnsureLibraryAvailable();
  EXPECT_EQ(install_state_.download_count(), 1);
  EXPECT_TRUE(answers_.empty());
  EXPECT_TRUE(library_provider_.GetBinaryPath().empty());

  FinishDownload();
  EXPECT_EQ(answers_, std::vector<bool>{true});
  EXPECT_FALSE(library_provider_.GetBinaryPath().empty());

  // The install state is only observed while a request is pending.
  EXPECT_FALSE(install_state_.HasObservers());
}

TEST_F(ScreenAILibraryProviderImplTest,
       AnswersAtOnceWhenLibraryIsAlreadyDownloaded) {
  FinishDownload();

  base::CallbackListSubscription subscription = EnsureLibraryAvailable();
  EXPECT_EQ(answers_, std::vector<bool>{true});
  EXPECT_EQ(install_state_.download_count(), 0);

  // The install state is not observed for this. Observing it would record that
  // the library is used, which `RecordUsage()` is for.
  EXPECT_FALSE(install_state_.HasObservers());
  EXPECT_EQ(install_state_.set_last_usage_time_count(), 0);
}

TEST_F(ScreenAILibraryProviderImplTest, AnswersWhenDownloadFails) {
  base::CallbackListSubscription subscription = EnsureLibraryAvailable();
  FailDownload();

  EXPECT_EQ(answers_, std::vector<bool>{false});
  EXPECT_FALSE(install_state_.HasObservers());
}

TEST_F(ScreenAILibraryProviderImplTest, AnswersAtOnceWhenDownloadCannotStart) {
  // Normally the install state starts a download and the answer comes when it
  // ends. For a device that cannot run the library, the install state fails
  // the download while the provider is still asking for it.
  ScreenAIInstallState::SetDeviceCompatibleForTesting(false);

  base::CallbackListSubscription subscription = EnsureLibraryAvailable();
  EXPECT_EQ(answers_, std::vector<bool>{false});
  EXPECT_EQ(install_state_.download_count(), 0);
  EXPECT_FALSE(install_state_.HasObservers());
}

TEST_F(ScreenAILibraryProviderImplTest, DownloadsAgainAfterFailure) {
  base::CallbackListSubscription subscription = EnsureLibraryAvailable();
  FailDownload();
  ASSERT_EQ(answers_, std::vector<bool>{false});

  // A new request starts a new download. It is not answered with the earlier
  // failure.
  subscription = EnsureLibraryAvailable();
  EXPECT_EQ(install_state_.download_count(), 2);
  EXPECT_EQ(answers_, std::vector<bool>{false});

  FinishDownload();
  EXPECT_EQ(answers_, (std::vector<bool>{false, true}));
}

TEST_F(ScreenAILibraryProviderImplTest,
       RequestMadeWhileAnsweringWaitsForNewDownload) {
  // The first of two requests asks again as soon as it is told that the
  // download failed.
  base::CallbackListSubscription new_subscription;
  base::CallbackListSubscription subscription1 =
      library_provider_.EnsureLibraryAvailable(
          base::BindLambdaForTesting([&](bool available) {
            answers_.push_back(available);
            new_subscription = EnsureLibraryAvailable();
          }));
  base::CallbackListSubscription subscription2 = EnsureLibraryAvailable();
  ASSERT_EQ(install_state_.download_count(), 1);

  // Both requests are told about the failure. The new request is not: it was
  // made after that download.
  FailDownload();
  EXPECT_EQ(answers_, (std::vector<bool>{false, false}));

  // The install state was telling its observers about the failure when the new
  // request was made, so the new download starts after that.
  EXPECT_EQ(install_state_.download_count(), 1);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return install_state_.download_count() == 2; }));
  EXPECT_TRUE(install_state_.HasObservers());
  EXPECT_EQ(answers_, (std::vector<bool>{false, false}));

  FinishDownload();
  EXPECT_EQ(answers_, (std::vector<bool>{false, false, true}));
  EXPECT_FALSE(install_state_.HasObservers());
}

TEST_F(ScreenAILibraryProviderImplTest, AnswersAllRequestsOfOneDownload) {
  base::CallbackListSubscription subscription1 = EnsureLibraryAvailable();
  base::CallbackListSubscription subscription2 = EnsureLibraryAvailable();
  EXPECT_EQ(install_state_.download_count(), 1);

  FinishDownload();
  EXPECT_EQ(answers_, (std::vector<bool>{true, true}));
}

TEST_F(ScreenAILibraryProviderImplTest, DoesNotAnswerCanceledRequest) {
  base::CallbackListSubscription subscription1 = EnsureLibraryAvailable();
  base::CallbackListSubscription subscription2 = EnsureLibraryAvailable();

  // Another request still waits, so the install state is still observed.
  subscription1 = {};
  EXPECT_TRUE(install_state_.HasObservers());

  FinishDownload();
  EXPECT_EQ(answers_, std::vector<bool>{true});
}

TEST_F(ScreenAILibraryProviderImplTest, StopsObservingWhenNobodyWaits) {
  base::CallbackListSubscription subscription = EnsureLibraryAvailable();
  ASSERT_TRUE(install_state_.HasObservers());

  // The download continues, but the provider does not wait for it anymore.
  subscription = {};
  EXPECT_FALSE(install_state_.HasObservers());
  EXPECT_EQ(install_state_.get_state(),
            ScreenAIInstallState::State::kDownloading);

  FinishDownload();
  EXPECT_TRUE(answers_.empty());
}

TEST_F(ScreenAILibraryProviderImplTest, AnswersWhenInstallStateIsReset) {
  base::CallbackListSubscription subscription = EnsureLibraryAvailable();
  ASSERT_TRUE(install_state_.HasObservers());

  // The install state tells its observers that it goes away, and forgets them.
  install_state_.ResetForTesting();
  ScreenAIInstallState::SetDeviceCompatibleForTesting(true);
  EXPECT_EQ(answers_, std::vector<bool>{false});
  EXPECT_FALSE(install_state_.HasObservers());

  // The next request makes the provider observe the install state again.
  subscription = EnsureLibraryAvailable();
  EXPECT_TRUE(install_state_.HasObservers());
  EXPECT_EQ(install_state_.download_count(), 2);

  FinishDownload();
  EXPECT_EQ(answers_, (std::vector<bool>{false, true}));
}

TEST_F(ScreenAILibraryProviderImplTest, RecordsUsage) {
  library_provider_.RecordUsage();
  EXPECT_EQ(install_state_.set_last_usage_time_count(), 1);
}

}  // namespace screen_ai
