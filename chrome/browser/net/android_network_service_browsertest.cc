// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_file_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

class AndroidChromeNetworkContextCleanupBrowserTest
    : public AndroidBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  AndroidChromeNetworkContextCleanupBrowserTest() = default;
  ~AndroidChromeNetworkContextCleanupBrowserTest() override = default;

 protected:
  const base::FilePath& user_data_dir() const {
    return user_data_dir_.GetPath();
  }

  // Helper method to perform navigation. This makes sure the various network
  // contexts are initialized when it is time to check for test expectations.
  void Navigate() {
    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(),
        embedded_test_server()->GetURL("/android/google.html")));
  }

  // Whether to create the `NetworkContextFilePaths.data_directory` before
  // initializing the network service.
  bool create_data_directory_;

  // Whether to create the `data_direcory` for the SafeBrowsing network context
  // or for the regular one. The tests support creating only one.
  bool data_directory_is_for_safebrowsing_;

  // Whether to write protect the `data_directory`. Used to check errors on
  // deletion.
  bool write_protect_data_directory_;

  // The `data_directory` prepared by the test.
  base::FilePath data_directory_;

  // A file in the stale network context directory. Should be wiped by
  // initialization.
  base::FilePath network_context_file_;

  // A file in the profile directory that should remain present after
  // initialization of the network service.
  base::FilePath profile_file_;

  // The histogram tester snapshots the state at construction time to avoid race
  // conditions. The network context initialization (being tested) starts
  // running during AndroidBrowserTest::SetUp().
  base::HistogramTester histogram_tester_;

 private:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    PlatformBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Create a new directory that serves as a user data dir. Enlist it as a
    // command line option. This way the SetUp() does not override it.
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::IsDirectoryEmpty(user_data_dir()));
    command_line->AppendSwitchPath(switches::kUserDataDir, user_data_dir());

    // Initialize the parameters.
    create_data_directory_ = std::get<0>(GetParam());
    write_protect_data_directory_ = std::get<1>(GetParam());
    data_directory_is_for_safebrowsing_ = std::get<2>(GetParam());

    if (data_directory_is_for_safebrowsing_) {
      data_directory_ = user_data_dir()
                            .AppendASCII(chrome::kInitialProfile)
                            .AppendASCII("Safe Browsing Network");
    } else {
      data_directory_ = user_data_dir()
                            .AppendASCII(chrome::kInitialProfile)
                            .AppendASCII(chrome::kNetworkDataDirname);
    }

    // Create the profile directory.
    base::FilePath profile_dir =
        user_data_dir().AppendASCII(chrome::kInitialProfile);
    ASSERT_TRUE(base::CreateDirectory(profile_dir));

    // Create a temporary file in the profile directory that should _not_ be
    // removed.
    ASSERT_TRUE(base::CreateTemporaryFileInDir(profile_dir, &profile_file_));
    ASSERT_TRUE(base::PathExists(profile_file_));

    if (create_data_directory_) {
      // Create a temporary file in the `data_directory` to check that it is
      // deleted recursively.
      ASSERT_TRUE(base::CreateDirectory(data_directory_));
      ASSERT_TRUE(base::CreateTemporaryFileInDir(data_directory_,
                                                 &network_context_file_));
      ASSERT_TRUE(base::PathExists(network_context_file_));
      if (write_protect_data_directory_) {
        ASSERT_TRUE(base::MakeFileUnwritable(data_directory_));
      }
    }
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  base::ScopedTempDir user_data_dir_;
};

constexpr char kClearHistogramName[] =
    "NetworkService.ClearStaleDataDirectoryResult";

bool HasSample(const base::HistogramTester& histogram_tester,
               base::HistogramBase::Sample sample) {
  if (histogram_tester.GetBucketCount(kClearHistogramName, sample) > 0)
    return true;
  return false;
}

void QuitLoopIfHasSample(const base::HistogramTester& histogram_tester,
                         base::HistogramBase::Sample expected_sample,
                         base::RunLoop& run_loop,
                         const char* histogram_name_ignored,
                         uint64_t name_hash_ignored,
                         base::HistogramBase::Sample arrived_sample_ignored) {
  if (HasSample(histogram_tester, expected_sample))
    run_loop.Quit();
}

void WaitForSample(const base::HistogramTester& histogram_tester,
                   base::HistogramBase::Sample sample) {
  if (HasSample(histogram_tester, sample))
    return;

  base::RunLoop run_loop;
  base::StatisticsRecorder::ScopedHistogramSampleObserver observer(
      kClearHistogramName,
      base::BindRepeating(&QuitLoopIfHasSample, std::ref(histogram_tester),
                          sample, std::ref(run_loop)));

  run_loop.Run();
}

}  // namespace

// Instantiate two tests: for the main network service context directory, and
// for the SafeBrowsing one. Both of them check that the directory is deleted.
INSTANTIATE_TEST_SUITE_P(
    WithDataDirectory,
    AndroidChromeNetworkContextCleanupBrowserTest,
    testing::Combine(
        /* create_data_directory_= */ testing::Values(true),
        /* write_protect_data_directory_= */ testing::Values(false),
        /* data_directory_is_for_safebrowsing_= */ testing::Bool()));

// Check that when the directories are already cleaned up, the histogram with
// the corresponding bucket is recorded.
INSTANTIATE_TEST_SUITE_P(
    WithoutDataDirectory,
    AndroidChromeNetworkContextCleanupBrowserTest,
    testing::Combine(
        /* create_data_directory_= */ testing::Values(false),
        /* write_protect_data_directory_= */ testing::Values(false),
        /* data_directory_is_for_safebrowsing_= */ testing::Values(false)));

// Check that failure to delete a network context data directory is recorded in
// UMA.
INSTANTIATE_TEST_SUITE_P(
    WriteProtectedDataDirectory,
    AndroidChromeNetworkContextCleanupBrowserTest,
    testing::Combine(
        /* create_data_directory_= */ testing::Values(true),
        /* write_protect_data_directory_= */ testing::Values(true),
        /* data_directory_is_for_safebrowsing_= */ testing::Values(false)));

IN_PROC_BROWSER_TEST_P(AndroidChromeNetworkContextCleanupBrowserTest,
                       NavigateAndCheck) {
  Navigate();

  // Enable blocking calls to inspect files on disk.
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Wait for the histogram to be recorded. Two network contexts are not created
  // simultaneously by the test, hence after the histogram is recorded the
  // contents of the tested files on disk are _not_ going to change.
  if (create_data_directory_ && !write_protect_data_directory_) {
    // DeleteResult::kDeleted == 1.
    WaitForSample(histogram_tester_, base::HistogramBase::Sample(1));
  } else if (!create_data_directory_) {
    // DeleteResult::kNotFound == 0.
    WaitForSample(histogram_tester_, base::HistogramBase::Sample(0));
  } else if (write_protect_data_directory_) {
    // Wait for delete error. DeleteResult::kDeleteError == 2.
    WaitForSample(histogram_tester_, base::HistogramBase::Sample(2));
  }

  // Expect delete errors only when the `data_directory` is write protected.
  int delete_sample_count = write_protect_data_directory_ ? 1 : 0;
  histogram_tester_.ExpectBucketCount(
      kClearHistogramName, base::HistogramBase::Sample(2), delete_sample_count);

  // Expect that the file outside all of the network context directories remains
  // present.
  EXPECT_TRUE(base::PathExists(profile_file_));

  if (create_data_directory_) {
    if (write_protect_data_directory_) {
      EXPECT_TRUE(base::PathExists(network_context_file_));

      // Allow the test to clean up after itself.
      EXPECT_EQ(0, HANDLE_EINTR(chmod(data_directory_.value().c_str(),
                                      S_IWUSR | S_IWGRP | S_IWOTH)));
    } else {
      EXPECT_FALSE(base::PathExists(network_context_file_));
    }
  }
}
