// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests the MetricsService stat recording to make sure that the numbers are
// what we expect.

#include "components/metrics/metrics_service.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/memory.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/persistent_histograms.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"
#include "services/service_manager/embedder/switches.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if defined(OS_POSIX)
#include <sys/wait.h>
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_types.h"
#endif

#if defined(OS_MACOSX) || defined(OS_LINUX)
namespace {

// Check CrashExitCodes.Renderer histogram for a single bucket entry and then
// verify that the bucket entry contains a signal and the signal is |signal|.
void VerifyRendererExitCodeIsSignal(
    const base::HistogramTester& histogram_tester,
    int signal) {
  const auto buckets =
      histogram_tester.GetAllSamples("CrashExitCodes.Renderer");
  ASSERT_EQ(1UL, buckets.size());
  EXPECT_EQ(1, buckets[0].count);
  int32_t exit_code = buckets[0].min;
  EXPECT_TRUE(WIFSIGNALED(exit_code));
  EXPECT_EQ(signal, WTERMSIG(exit_code));
}

}  // namespace
#endif  // OS_MACOSX || OS_LINUX

// This test class verifies that metrics reporting works correctly for various
// renderer behaviors such as page loads, recording crashed tabs, and browser
// starts. It also verifies that if a renderer process crashes, the correct exit
// code is recorded.
//
// TODO(isherman): We should also verify that
// metrics::prefs::kStabilityExitedCleanly is set correctly after each of these
// tests, but this preference isn't set until the browser exits... it's not
// clear to me how to test that.
class MetricsServiceBrowserTest : public InProcessBrowserTest {
 public:
  MetricsServiceBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable the metrics service for testing (in recording-only mode).
    command_line->AppendSwitch(metrics::switches::kMetricsRecordingOnly);
  }

  void SetUp() override {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &metrics_consent_);
    InProcessBrowserTest::SetUp();
  }

  // Open three tabs then navigate to |crashy_url| and wait for the renderer to
  // crash.
  void OpenTabsAndNavigateToCrashyUrl(const std::string& crashy_url) {
    // Opens three tabs.
    OpenThreeTabs();

    // Kill the process for one of the tabs by navigating to |crashy_url|.
    content::RenderProcessHostWatcher observer(
        browser()->tab_strip_model()->GetActiveWebContents(),
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    // Opens one tab.
    ui_test_utils::NavigateToURL(browser(), GURL(crashy_url));
    observer.Wait();

    // The MetricsService listens for the same notification, so the |observer|
    // might finish waiting before the MetricsService has a chance to process
    // the notification.  To avoid racing here, we repeatedly run the message
    // loop until the MetricsService catches up.  This should happen "real soon
    // now", since the notification is posted to all observers essentially
    // simultaneously... so busy waiting here shouldn't be too bad.
    const PrefService* prefs = g_browser_process->local_state();
    while (!prefs->GetInteger(metrics::prefs::kStabilityRendererCrashCount)) {
      content::RunAllPendingInMessageLoop();
    }
  }

  // Open a couple of tabs of random content.
  //
  // Calling this method causes three page load events:
  // 1. title2.html
  // 2. iframe.html
  // 3. title1.html (iframed by iframe.html)
  void OpenThreeTabs() {
    const int kBrowserTestFlags =
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION;

    base::FilePath test_directory;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_directory));

    base::FilePath page1_path = test_directory.AppendASCII("title2.html");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), net::FilePathToFileURL(page1_path),
        WindowOpenDisposition::NEW_FOREGROUND_TAB, kBrowserTestFlags);

    base::FilePath page2_path = test_directory.AppendASCII("iframe.html");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), net::FilePathToFileURL(page2_path),
        WindowOpenDisposition::NEW_FOREGROUND_TAB, kBrowserTestFlags);
  }

 private:
  bool metrics_consent_ = true;

  DISALLOW_COPY_AND_ASSIGN(MetricsServiceBrowserTest);
};

IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserTest, CloseRenderersNormally) {
  OpenThreeTabs();

  // Verify that the expected stability metrics were recorded.
  const PrefService* prefs = g_browser_process->local_state();
  EXPECT_EQ(1, prefs->GetInteger(metrics::prefs::kStabilityLaunchCount));
  EXPECT_EQ(3, prefs->GetInteger(metrics::prefs::kStabilityPageLoadCount));
  EXPECT_EQ(0, prefs->GetInteger(metrics::prefs::kStabilityRendererCrashCount));
}

// Flaky on Linux. See http://crbug.com/131094
// Child crashes fail the process on ASan (see crbug.com/411251,
// crbug.com/368525).
// Flaky timeouts on Win7 Tests (dbg)(1); see https://crbug.com/985255.
#if defined(OS_LINUX) || defined(ADDRESS_SANITIZER) || \
    (defined(OS_WIN) && !defined(NDEBUG))
#define MAYBE_CrashRenderers DISABLED_CrashRenderers
#define MAYBE_CheckCrashRenderers DISABLED_CheckCrashRenderers
#else
#define MAYBE_CrashRenderers CrashRenderers
#define MAYBE_CheckCrashRenderers CheckCrashRenderers
#endif

IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserTest, MAYBE_CrashRenderers) {
  base::HistogramTester histogram_tester;

  OpenTabsAndNavigateToCrashyUrl(content::kChromeUICrashURL);

  // Verify that the expected stability metrics were recorded.
  const PrefService* prefs = g_browser_process->local_state();
  EXPECT_EQ(1, prefs->GetInteger(metrics::prefs::kStabilityLaunchCount));
  // The three tabs from OpenTabs() and the one tab to open chrome://crash/.
  EXPECT_EQ(4, prefs->GetInteger(metrics::prefs::kStabilityPageLoadCount));
  EXPECT_EQ(1, prefs->GetInteger(metrics::prefs::kStabilityRendererCrashCount));

#if defined(OS_WIN)
  // Consult Stability Team before changing this test as it's recorded to
  // histograms and used for stability measurement.
  histogram_tester.ExpectUniqueSample(
      "CrashExitCodes.Renderer",
      std::abs(static_cast<int32_t>(STATUS_ACCESS_VIOLATION)), 1);
#elif defined(OS_MACOSX) || defined(OS_LINUX)
  VerifyRendererExitCodeIsSignal(histogram_tester, SIGSEGV);
#endif
  histogram_tester.ExpectUniqueSample("Tabs.SadTab.CrashCreated", 1, 1);
}

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserTest, HeapCorruptionInRenderer) {
  base::HistogramTester histogram_tester;

  OpenTabsAndNavigateToCrashyUrl(content::kChromeUIHeapCorruptionCrashURL);

  // Verify that the expected stability metrics were recorded.
  const PrefService* prefs = g_browser_process->local_state();
  EXPECT_EQ(1, prefs->GetInteger(metrics::prefs::kStabilityLaunchCount));
  // The three tabs from OpenTabs() and the one tab to open chrome://crash/.
  EXPECT_EQ(4, prefs->GetInteger(metrics::prefs::kStabilityPageLoadCount));
  EXPECT_EQ(1, prefs->GetInteger(metrics::prefs::kStabilityRendererCrashCount));

  histogram_tester.ExpectUniqueSample(
      "CrashExitCodes.Renderer",
      std::abs(static_cast<int32_t>(STATUS_HEAP_CORRUPTION)), 1);
  histogram_tester.ExpectUniqueSample("Tabs.SadTab.CrashCreated", 1, 1);
  LOG(INFO) << histogram_tester.GetAllHistogramsRecorded();
}
#endif  // OS_WIN

IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserTest, MAYBE_CheckCrashRenderers) {
  base::HistogramTester histogram_tester;

  OpenTabsAndNavigateToCrashyUrl(content::kChromeUICheckCrashURL);

  // Verify that the expected stability metrics were recorded.
  const PrefService* prefs = g_browser_process->local_state();
  EXPECT_EQ(1, prefs->GetInteger(metrics::prefs::kStabilityLaunchCount));
  // The three tabs from OpenTabs() and the one tab to open
  // chrome://checkcrash/.
  EXPECT_EQ(4, prefs->GetInteger(metrics::prefs::kStabilityPageLoadCount));
  EXPECT_EQ(1, prefs->GetInteger(metrics::prefs::kStabilityRendererCrashCount));

#if defined(OS_WIN)
  // Consult Stability Team before changing this test as it's recorded to
  // histograms and used for stability measurement.
  histogram_tester.ExpectUniqueSample(
      "CrashExitCodes.Renderer",
      std::abs(static_cast<int32_t>(STATUS_BREAKPOINT)), 1);
#elif defined(OS_MACOSX) || defined(OS_LINUX)
  VerifyRendererExitCodeIsSignal(histogram_tester, SIGTRAP);
#endif
  histogram_tester.ExpectUniqueSample("Tabs.SadTab.CrashCreated", 1, 1);
}

// OOM code only works on Windows.
#if defined(OS_WIN) && !defined(ADDRESS_SANITIZER)
IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserTest, OOMRenderers) {
  // Disable stack traces during this test since DbgHelp is unreliable in
  // low-memory conditions (see crbug.com/692564).
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      service_manager::switches::kDisableInProcessStackTraces);

  base::HistogramTester histogram_tester;

  OpenTabsAndNavigateToCrashyUrl(content::kChromeUIMemoryExhaustURL);

  // Verify that the expected stability metrics were recorded.
  const PrefService* prefs = g_browser_process->local_state();
  EXPECT_EQ(1, prefs->GetInteger(metrics::prefs::kStabilityLaunchCount));
  // The three tabs from OpenTabs() and the one tab to open
  // chrome://memory-exhaust/.
  EXPECT_EQ(4, prefs->GetInteger(metrics::prefs::kStabilityPageLoadCount));
  EXPECT_EQ(1, prefs->GetInteger(metrics::prefs::kStabilityRendererCrashCount));

// On 64-bit, the Job object should terminate the renderer on an OOM. However,
// if the system is low on memory already, then the allocator might just return
// a normal OOM before hitting the Job limit.
// Note: Exit codes are recorded after being passed through std::abs see
// MapCrashExitCodeForHistogram.
#if defined(ARCH_CPU_64_BITS)
  const base::Bucket expected_possible_exit_codes[] = {
      base::Bucket(
          std::abs(static_cast<int32_t>(sandbox::SBOX_FATAL_MEMORY_EXCEEDED)),
          1),
      base::Bucket(std::abs(static_cast<int32_t>(base::win::kOomExceptionCode)),
                   1)};
#else
  const base::Bucket expected_possible_exit_codes[] = {base::Bucket(
      std::abs(static_cast<int32_t>(base::win::kOomExceptionCode)), 1)};
#endif

  EXPECT_THAT(histogram_tester.GetAllSamples("CrashExitCodes.Renderer"),
              ::testing::IsSubsetOf(expected_possible_exit_codes));

  histogram_tester.ExpectUniqueSample("Tabs.SadTab.OomCreated", 1, 1);
}
#endif  // OS_WIN && !ADDRESS_SANITIZER

// Base class for testing if browser-metrics files get removed or not.
// The code under tests is run before any actual test methods so the test
// conditions must be created during SetUp in order to affect said code.
class MetricsServiceBrowserFilesTest : public InProcessBrowserTest {
  using super = InProcessBrowserTest;

 public:
  MetricsServiceBrowserFilesTest() {}

  bool SetUpUserDataDirectory() override {
    if (!super::SetUpUserDataDirectory())
      return false;

    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath user_dir;
    CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_dir));

    // Create a local-state file with what we want the browser to use. This
    // has to be done here because there is no hook between when the browser
    // is initialized and the metrics-client acts on the pref values. The
    // "Local State" directory is hard-coded because the FILE_LOCAL_STATE
    // path is not yet defined at this point.
    {
      base::test::TaskEnvironment task_env;
      auto state = base::MakeRefCounted<JsonPrefStore>(
          user_dir.Append(FILE_PATH_LITERAL("Local State")));
      state->SetValue(
          metrics::prefs::kMetricsDefaultOptIn,
          std::make_unique<base::Value>(metrics::EnableMetricsDefault::OPT_OUT),
          0);
    }

    // Create the upload dir. Note that ASSERT macros won't fail in SetUp,
    // hence the use of CHECK.
    upload_dir_ = user_dir.AppendASCII(kBrowserMetricsName);
    CHECK(!base::PathExists(upload_dir_));
    CHECK(base::CreateDirectory(upload_dir_));

    // Create a file inside the upload dir that can be watched to see if an
    // attempt was made to delete everything.
    base::File upload_file(
        upload_dir().AppendASCII("foo.bar"),
        base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    CHECK_EQ(6, upload_file.WriteAtCurrentPos("foobar", 6));

    return true;
  }

  void SetUp() override {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &metrics_consent_);
    super::SetUp();
  }

  // Check for the existence of any non-pma files that were created as part
  // of the test. PMA files may be created as part of the browser setup and
  // cannot be deleted while open on all operating systems.
  bool HasNonPMAFiles() {
    base::ScopedAllowBlockingForTesting allow_blocking;

    if (!base::PathExists(upload_dir_))
      return false;

    base::FileEnumerator file_iter(upload_dir_, true,
                                   base::FileEnumerator::FILES);
    while (!file_iter.Next().empty()) {
      if (file_iter.GetInfo().GetName().Extension() !=
          FILE_PATH_LITERAL(".pma")) {
        return true;
      }
    }
    return false;
  }

  base::FilePath& upload_dir() { return upload_dir_; }
  void set_metrics_consent(bool enabled) { metrics_consent_ = enabled; }

 private:
  bool metrics_consent_ = true;
  base::FilePath upload_dir_;

  DISALLOW_COPY_AND_ASSIGN(MetricsServiceBrowserFilesTest);
};

// Specific class for testing when metrics upload is fully enabled.
class MetricsServiceBrowserDoUploadTest
    : public MetricsServiceBrowserFilesTest {
 public:
  MetricsServiceBrowserDoUploadTest() {}

  void SetUp() override {
    set_metrics_consent(true);
    feature_list_.InitAndEnableFeature(
        metrics::internal::kMetricsReportingFeature);
    MetricsServiceBrowserFilesTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(MetricsServiceBrowserDoUploadTest);
};

IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserDoUploadTest, FilesRemain) {
  // SetUp() has provided consent and made metrics "sampled-in" (enabled).
  EXPECT_TRUE(HasNonPMAFiles());
}

// Specific class for testing when metrics upload is explicitly disabled.
class MetricsServiceBrowserNoUploadTest
    : public MetricsServiceBrowserFilesTest {
 public:
  MetricsServiceBrowserNoUploadTest() {}

  void SetUp() override {
    set_metrics_consent(false);
    feature_list_.InitAndEnableFeature(
        metrics::internal::kMetricsReportingFeature);
    MetricsServiceBrowserFilesTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(MetricsServiceBrowserNoUploadTest);
};

IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserNoUploadTest, FilesRemoved) {
  // SetUp() has removed consent and made metrics "sampled-in" (enabled).
  EXPECT_FALSE(HasNonPMAFiles());
}

// Specific class for testing when metrics upload is disabled by sampling.
class MetricsServiceBrowserSampledOutTest
    : public MetricsServiceBrowserFilesTest {
 public:
  MetricsServiceBrowserSampledOutTest() {}

  void SetUp() override {
    set_metrics_consent(true);
    feature_list_.InitAndDisableFeature(
        metrics::internal::kMetricsReportingFeature);
    MetricsServiceBrowserFilesTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(MetricsServiceBrowserSampledOutTest);
};

IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserSampledOutTest, FilesRemoved) {
  // SetUp() has provided consent and made metrics "sampled-out" (disabled).
  EXPECT_FALSE(HasNonPMAFiles());
}
