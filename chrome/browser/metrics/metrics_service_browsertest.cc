// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Tests the MetricsService stat recording to make sure that the numbers are
// what we expect.

#include "components/metrics/metrics_service.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
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
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/persistent_histograms.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_POSIX)
#include <sys/wait.h>
#endif

#if BUILDFLAG(IS_WIN)
#include "sandbox/win/src/sandbox_types.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace {

#if BUILDFLAG(IS_WIN)

void VerifyRendererExitCodeIsSignal(
    const base::HistogramTester& histogram_tester,
    int signal) {
  histogram_tester.ExpectUniqueSample(
      "CrashExitCodes.Renderer", std::abs(static_cast<int32_t>(signal)), 1);
}

#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

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

#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace

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

  MetricsServiceBrowserTest(const MetricsServiceBrowserTest&) = delete;
  MetricsServiceBrowserTest& operator=(const MetricsServiceBrowserTest&) =
      delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable the metrics service for testing (in recording-only mode).
    metrics::EnableMetricsRecordingOnlyForTesting(command_line);
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
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(crashy_url)));
    observer.Wait();

    // The MetricsService listens for the same notification, so the observer
    // might finish waiting before the MetricsService has processed the
    // notification. To avoid racing here, we repeatedly run the message loop.
    content::RunAllPendingInMessageLoop();
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
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP;

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
};

IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserTest, CloseRenderersNormally) {
  base::HistogramTester histogram_tester;
  OpenThreeTabs();

  // Verify that the expected stability metrics were recorded.
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     metrics::StabilityEventType::kPageLoad, 2);
  histogram_tester.ExpectBucketCount(
      "Stability.Counts2", metrics::StabilityEventType::kRendererCrash, 0);
}

// Child crashes fail the process on ASan (see crbug.com/411251,
// crbug.com/368525).
// Note to sheriffs: Do not disable these tests if they starts to flake. If
// either of these tests start to fail then changes likely need to be made
// elsewhere in crash processing, metrics analysis, and dashboards. Please
// consult Stability Team before disabling.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_CrashRenderers DISABLED_CrashRenderers
#define MAYBE_CheckCrashRenderers DISABLED_CheckCrashRenderers
#else
#define MAYBE_CrashRenderers CrashRenderers
#define MAYBE_CheckCrashRenderers CheckCrashRenderers
#endif

IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserTest, MAYBE_CrashRenderers) {
  base::HistogramTester histogram_tester;

  OpenTabsAndNavigateToCrashyUrl(blink::kChromeUICrashURL);

  // Verify that the expected stability metrics were recorded.
  // The three tabs from OpenTabs() and the one tab to open chrome://crash/.
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     metrics::StabilityEventType::kPageLoad, 3);
  histogram_tester.ExpectBucketCount(
      "Stability.Counts2", metrics::StabilityEventType::kRendererCrash, 1);

#if BUILDFLAG(IS_WIN)
  // Consult Stability Team before changing this test as it's recorded to
  // histograms and used for stability measurement.
  VerifyRendererExitCodeIsSignal(histogram_tester, STATUS_ACCESS_VIOLATION);
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  VerifyRendererExitCodeIsSignal(histogram_tester, SIGSEGV);
#endif
}

// Test is disabled on Windows AMR64 because
// TerminateWithHeapCorruption() isn't expected to work there.
// See: https://crbug.com/1054423
#if BUILDFLAG(IS_WIN)
#if defined(ARCH_CPU_ARM64)
#define MAYBE_HeapCorruptionInRenderer DISABLED_HeapCorruptionInRenderer
#else
#define MAYBE_HeapCorruptionInRenderer HeapCorruptionInRenderer
#endif
IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserTest,
                       MAYBE_HeapCorruptionInRenderer) {
  base::HistogramTester histogram_tester;

  OpenTabsAndNavigateToCrashyUrl(blink::kChromeUIHeapCorruptionCrashURL);

  // Verify that the expected stability metrics were recorded.
  // The three tabs from OpenTabs() and the one tab to open chrome://crash/.
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     metrics::StabilityEventType::kPageLoad, 3);
  histogram_tester.ExpectBucketCount(
      "Stability.Counts2", metrics::StabilityEventType::kRendererCrash, 1);

  histogram_tester.ExpectUniqueSample(
      "CrashExitCodes.Renderer",
      std::abs(static_cast<int32_t>(STATUS_HEAP_CORRUPTION)), 1);
  LOG(INFO) << histogram_tester.GetAllHistogramsRecorded();
}
#endif  // BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserTest, MAYBE_CheckCrashRenderers) {
  base::HistogramTester histogram_tester;

  OpenTabsAndNavigateToCrashyUrl(blink::kChromeUICheckCrashURL);

  // Verify that the expected stability metrics were recorded.
  // The three tabs from OpenTabs() and the one tab to open
  // chrome://checkcrash/.
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     metrics::StabilityEventType::kPageLoad, 3);
  histogram_tester.ExpectBucketCount(
      "Stability.Counts2", metrics::StabilityEventType::kRendererCrash, 1);

#if BUILDFLAG(IS_WIN)
  // Consult Stability Team before changing this test as it's recorded to
  // histograms and used for stability measurement.
  VerifyRendererExitCodeIsSignal(histogram_tester, STATUS_BREAKPOINT);
#elif BUILDFLAG(IS_MAC)
  VerifyRendererExitCodeIsSignal(histogram_tester, SIGTRAP);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#if defined(OFFICIAL_BUILD)
  VerifyRendererExitCodeIsSignal(histogram_tester, SIGILL);
#else
  VerifyRendererExitCodeIsSignal(histogram_tester, SIGSEGV);
#endif  // defined(OFFICIAL_BUILD)
#endif
}

#if BUILDFLAG(ENABLE_RUST_CRASH)
IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserTest, CrashRenderersInRust) {
  base::HistogramTester histogram_tester;

  OpenTabsAndNavigateToCrashyUrl(blink::kChromeUICrashRustURL);

  // Verify that the expected stability metrics were recorded.
  // The three tabs from OpenTabs() and the one tab to open
  // chrome://crash/rust/.
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     metrics::StabilityEventType::kPageLoad, 3);
  histogram_tester.ExpectBucketCount(
      "Stability.Counts2", metrics::StabilityEventType::kRendererCrash, 1);
}
#endif  // BUILDFLAG(ENABLE_RUST_CRASH)

// OOM code only works on Windows.
#if BUILDFLAG(IS_WIN) && !defined(ADDRESS_SANITIZER)
IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserTest, OOMRenderers) {
  // Disable stack traces during this test since DbgHelp is unreliable in
  // low-memory conditions (see crbug.com/692564).
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableInProcessStackTraces);

  base::HistogramTester histogram_tester;

  OpenTabsAndNavigateToCrashyUrl(blink::kChromeUIMemoryExhaustURL);

  // Verify that the expected stability metrics were recorded.
  // The three tabs from OpenTabs() and the one tab to open
  // chrome://memory-exhaust/.
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     metrics::StabilityEventType::kPageLoad, 3);
  histogram_tester.ExpectBucketCount(
      "Stability.Counts2", metrics::StabilityEventType::kRendererCrash, 1);

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
}
#endif  // BUILDFLAG(IS_WIN) && !defined(ADDRESS_SANITIZER)

// Base class for testing if browser-metrics files get removed or not.
// The code under tests is run before any actual test methods so the test
// conditions must be created during SetUp in order to affect said code.
class MetricsServiceBrowserFilesTest : public InProcessBrowserTest {
  using super = InProcessBrowserTest;

 public:
  MetricsServiceBrowserFilesTest() {}

  MetricsServiceBrowserFilesTest(const MetricsServiceBrowserFilesTest&) =
      delete;
  MetricsServiceBrowserFilesTest& operator=(
      const MetricsServiceBrowserFilesTest&) = delete;

  bool SetUpUserDataDirectory() override {
    if (!super::SetUpUserDataDirectory()) {
      return false;
    }

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
      state->SetValue(metrics::prefs::kMetricsDefaultOptIn,
                      base::Value(metrics::EnableMetricsDefault::OPT_OUT), 0);
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
    CHECK(upload_file.WriteAtCurrentPosAndCheck(
        base::byte_span_from_cstring("foobar")));

    return true;
  }

  void SetUp() override {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &metrics_consent_);
    metrics::EnabledStateProvider::SetIgnoreForceFieldTrialsForTesting(true);
    super::SetUp();
  }

  // Finds any non-pma files that were created as part of the test. PMA files
  // may be created as part of the browser setup and cannot be deleted while
  // open on all operating systems.
  std::vector<base::FilePath> FindNonPMAFiles() {
    base::ScopedAllowBlockingForTesting allow_blocking;

    std::vector<base::FilePath> files;
    if (!base::PathExists(upload_dir_)) {
      return files;
    }

    base::FileEnumerator file_iter(upload_dir_, true,
                                   base::FileEnumerator::FILES);
    while (!file_iter.Next().empty()) {
      base::FilePath name = file_iter.GetInfo().GetName();
      if (name.Extension() != FILE_PATH_LITERAL(".pma")) {
        files.push_back(std::move(name));
      }
    }
    return files;
  }

  bool HasNonPMAFiles() {
    return !FindNonPMAFiles().empty();
  }

  base::FilePath& upload_dir() { return upload_dir_; }
  void set_metrics_consent(bool enabled) { metrics_consent_ = enabled; }

 private:
  bool metrics_consent_ = true;
  base::FilePath upload_dir_;
};

// Specific class for testing when metrics upload is fully enabled.
class MetricsServiceBrowserDoUploadTest
    : public MetricsServiceBrowserFilesTest {
 public:
  MetricsServiceBrowserDoUploadTest() {}

  MetricsServiceBrowserDoUploadTest(const MetricsServiceBrowserDoUploadTest&) =
      delete;
  MetricsServiceBrowserDoUploadTest& operator=(
      const MetricsServiceBrowserDoUploadTest&) = delete;

  void SetUp() override {
    set_metrics_consent(true);
    feature_list_.InitAndEnableFeature(
        metrics::internal::kMetricsReportingFeature);
    MetricsServiceBrowserFilesTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
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

  MetricsServiceBrowserNoUploadTest(const MetricsServiceBrowserNoUploadTest&) =
      delete;
  MetricsServiceBrowserNoUploadTest& operator=(
      const MetricsServiceBrowserNoUploadTest&) = delete;

  void SetUp() override {
    set_metrics_consent(false);
    feature_list_.InitAndEnableFeature(
        metrics::internal::kMetricsReportingFeature);
    MetricsServiceBrowserFilesTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserNoUploadTest, FilesRemoved) {
  // SetUp() has removed consent and made metrics "sampled-in" (enabled).
  auto non_pma_files = FindNonPMAFiles();
  for (const auto& file : non_pma_files) {
    LOG(INFO) << "Found non-PMA file:" << file;
  }
  EXPECT_TRUE(non_pma_files.empty());
}

// Specific class for testing when metrics upload is disabled by sampling.
class MetricsServiceBrowserSampledOutTest
    : public MetricsServiceBrowserFilesTest {
 public:
  MetricsServiceBrowserSampledOutTest() {}

  MetricsServiceBrowserSampledOutTest(
      const MetricsServiceBrowserSampledOutTest&) = delete;
  MetricsServiceBrowserSampledOutTest& operator=(
      const MetricsServiceBrowserSampledOutTest&) = delete;

  void SetUp() override {
    set_metrics_consent(true);
    feature_list_.InitAndDisableFeature(
        metrics::internal::kMetricsReportingFeature);
    MetricsServiceBrowserFilesTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserSampledOutTest, FilesRemoved) {
  // SetUp() has provided consent and made metrics "sampled-out" (disabled).
  auto non_pma_files = FindNonPMAFiles();
  for (const auto& file : non_pma_files) {
    LOG(INFO) << "Found non-PMA file:" << file;
  }
  EXPECT_TRUE(non_pma_files.empty());
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserTest, EntropyTransfer) {
  // While creating, the EntropyState should have been transferred from the
  // Ash init params to the Entropy values.
  auto* init_params = chromeos::BrowserParamsProxy::Get();
  metrics::MetricsService* metrics_service =
      g_browser_process->GetMetricsServicesManager()->GetMetricsService();
  // Due to version skew it could be that the used version of Ash does not
  // support this yet.
  if (init_params->EntropySource()) {
    EXPECT_EQ(metrics_service->GetLowEntropySource(),
              init_params->EntropySource()->low_entropy);
    EXPECT_NE(init_params->EntropySource()->low_entropy, -1);
    EXPECT_EQ(metrics_service->GetOldLowEntropySource(),
              init_params->EntropySource()->old_low_entropy);
    EXPECT_EQ(metrics_service->GetPseudoLowEntropySource(),
              init_params->EntropySource()->pseudo_low_entropy);
  } else {
    LOG(WARNING) << "MetricsReportingLacrosBrowserTest.EntropyTransfer "
                 << "- Ash version does not support entropy transfer yet";
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
