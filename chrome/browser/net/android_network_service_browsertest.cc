// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/metrics/histogram_tester.h"
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
    : public AndroidBrowserTest {
 public:
  AndroidChromeNetworkContextCleanupBrowserTest() = default;
  ~AndroidChromeNetworkContextCleanupBrowserTest() override = default;

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

    // Create directories with the default profile and the network context data
    // corresponding to `NetworkContextFilePaths.data_directory`. The test will
    // check that the network service initialization deletes this directory.
    base::FilePath profile_dir =
        user_data_dir().AppendASCII(chrome::kInitialProfile);
    base::FilePath data_directory =
        profile_dir.AppendASCII(chrome::kNetworkDataDirname);
    ASSERT_TRUE(base::CreateDirectory(data_directory));

    // Create a temporary file in the profile directory that should _not_ be
    // removed.
    ASSERT_TRUE(base::CreateTemporaryFileInDir(profile_dir, &profile_file_));
    ASSERT_TRUE(base::PathExists(profile_file_));

    // Create a temporary file in the `data_directory` to check that it is
    // deleted recursively.
    ASSERT_TRUE(
        base::CreateTemporaryFileInDir(data_directory, &network_context_file_));
    ASSERT_TRUE(base::PathExists(network_context_file_));
  }

 protected:
  const base::FilePath& user_data_dir() const {
    return user_data_dir_.GetPath();
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

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

// Check that the network service context initialization code deleted the stale
// storage directory.
IN_PROC_BROWSER_TEST_F(AndroidChromeNetworkContextCleanupBrowserTest,
                       TestStaleCookiesDeleted) {
  // Navigate.
  ASSERT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      embedded_test_server()->GetURL("/android/google.html")));

  // Enable blocking calls to inspect files on disk.
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Wait for the histogram to record successful deletion of the directory.
  // The Safebrowsing context cleanup could not succeed because the stale
  // directory was not created for it. Hence after the waiting is done here the
  // state of the files inside and outside of the stale directory must be
  // deterministic.
  // DeleteResult::kDeleted == 1.
  WaitForSample(histogram_tester_, base::HistogramBase::Sample(1));

  // Expect no delete errors. DeleteResult::kDeleteError == 2.
  histogram_tester_.ExpectBucketCount(kClearHistogramName,
                                      base::HistogramBase::Sample(2), 0);

  EXPECT_FALSE(base::PathExists(network_context_file_));
  EXPECT_TRUE(base::PathExists(profile_file_));
}
