// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <string_view>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/switches.h"

class WasmExtensionCachingBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  WasmExtensionCachingBrowserTest() = default;
  ~WasmExtensionCachingBrowserTest() override = default;

  static constexpr std::string_view kHistogram = "V8.WasmCodeCaching";

  // The enum values need to match "WasmCodeCaching" in
  // tools/metrics/histograms/metadata/v8/enums.xml.
  enum WasmCodeCaching {
    kMiss = 0,
    kHit = 1,
    kInvalidCacheEntry = 2,
    kNoCacheHandler = 3,

    kMaxValue = kNoCacheHandler
  };

  static constexpr std::string_view kWasmCodeCachingBucketNames[] = {
      "kMiss", "kHit", "kInvalidCacheEntry", "kNoCacheHandler"};

  const base::FilePath& GetExtensionDir() {
    if (!tmp_dir_.IsValid()) {
      CHECK(tmp_dir_.CreateUniqueTempDir());
    }
    return tmp_dir_.GetPath();
  }

  int GetBucketCount(WasmCodeCaching bucket) const {
    return histogram_tester_.GetBucketCount(kHistogram, bucket);
  }

  int FetchAndAccumulateHistogram() {
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    // Log and sum up the samples. The logging will help diagnose failures or
    // flakes.
    auto buckets = histogram_tester_.GetAllSamples(kHistogram);
    int total_samples = 0;
    LOG(INFO) << "Histogram '" << kHistogram << "': ";
    for (auto& bucket : buckets) {
      LOG(INFO) << kWasmCodeCachingBucketNames[bucket.min] << ": "
                << bucket.count << "\t";
      total_samples += bucket.count;
    }
    return total_samples;
  }

  // Fetch the `bucket` from the `histogram` in every renderer process until
  // reaching, but not exceeding, `expected_samples`.
  void WaitForHistogramSamples(std::string_view histogram,
                               int expected_samples) {
    // We sleep for an increasing amount of time for the background task to
    // finish.
    int millis_sleep = 100;
    while (true) {
      LOG(INFO) << "Fetching histograms, waiting for " << expected_samples
                << " samples...";

      int total_samples = FetchAndAccumulateHistogram();
      if (total_samples == expected_samples) {
        return;
      }
      CHECK_LT(total_samples, expected_samples);

      base::PlatformThread::Sleep(base::Milliseconds(millis_sleep));
      // Increase sleep time, but never sleep more than 5 seconds.
      millis_sleep = std::min(5000, millis_sleep * 2);
    }
  }

 private:
  // JS flags:
  // --allow-natives-syntax:          Enables the use of (internal) runtime
  //                                  functions like `%IsLiftoffFunction`.
  // --wasm-caching-threshold=1:      Trigger caching as soon as any TurboFan
  //                                  code is available.
  // --wasm-caching-hard-threshold=1: Trigger caching immediately, not after a
  //                                  delay.
  // --wasm-tiering-budget=1:         Trigger tier-up earlier.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                    "--allow-natives-syntax"
                                    " --wasm-caching-threshold=1"
                                    " --wasm-caching-hard-threshold=1"
                                    " --wasm-tiering-budget=1");
    ExtensionBrowserTest::SetUpCommandLine(command_line);
  }

  base::ScopedTempDir tmp_dir_;
  base::HistogramTester histogram_tester_;
};

// The `large.wasm` file is very large: 2.5MB. To avoid increasing the git
// repository, we prefer borrowing it from web_tests.
base::FilePath LargeWasmPath() {
  base::FilePath root_path;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_path));
  return root_path.Append(FILE_PATH_LITERAL(
      "third_party/blink/web_tests/http/tests/wasm/resources/large.wasm"));
}

// Test that we cache streaming compiled/instantiated Wasm modules in
// extensions. We have to wait until caching of the module happens and the
// histogram is populated.
IN_PROC_BROWSER_TEST_F(WasmExtensionCachingBrowserTest, CacheWasmExtensions) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  CHECK(base::CopyFile(LargeWasmPath(),
                       GetExtensionDir().AppendASCII("large.wasm")));

  base::WriteFile(GetExtensionDir().AppendASCII("service_worker_background.js"),
                  R"(
      function execute_wasm(instance) {
        let has_unoptimized = false;
        for (export_name in instance.exports) {
          const func = instance.exports[export_name];
          func(1, 2, 4);
          has_unoptimized ||= %IsLiftoffFunction(func);
        }
        if (has_unoptimized) {
          setTimeout(() => execute_wasm(instance), 1);
        }
      }
      chrome.tabs.onCreated.addListener(() => {
        // Run all functions until there are no unoptimized functions left.
        WebAssembly.instantiateStreaming(fetch("large.wasm")).then(
          ({instance}) => execute_wasm(instance));
      });
    )");

  base::WriteFile(GetExtensionDir().AppendASCII("manifest.json"), R"({
    "name": "foo",
    "description": "foo",
    "version": "0.1",
    "manifest_version": 2,
    "content_security_policy": "script-src 'self' 'unsafe-eval'; object-src 'self'",
    "background": {
      "service_worker": "service_worker_background.js"
    }
  })");

  // After loading the extension, no WebAssembly has been executed.
  extensions::ChromeTestExtensionLoader loader(browser()->profile());
  LOG(INFO) << "Loading extension, expecting zero misses / hits";
  loader.LoadExtension(GetExtensionDir());
  WaitForHistogramSamples(kHistogram, 0);
  CHECK_EQ(0, GetBucketCount(kMiss));
  CHECK_EQ(0, GetBucketCount(kHit));

  // Open up to 10 tabs, with some waiting in between. We should eventually see
  // a cache hit.
  // Typically (on a reasonably fast machine) the first tab creates the cache
  // entry and the second tab consumes it. Very slow machines might require a
  // few more loads.
  for (int num_tabs = 1; num_tabs <= 10; ++num_tabs) {
    LOG(INFO) << "Opening new tab #" << num_tabs;
    chrome::NewTab(browser());
    // Wait until we got a total of `num_tabs` many samples.
    WaitForHistogramSamples(kHistogram, num_tabs);
    // If there was a hit, we are happy (and done).
    if (GetBucketCount(kHit)) {
      return;
    }
    CHECK_EQ(num_tabs, GetBucketCount(kMiss));

    // Before opening the next tab, wait for some (increasing) time.
    // The total maximum waiting time is 1 + 2 + ... + 10 = 55 seconds.
    base::PlatformThread::Sleep(base::Seconds(num_tabs));
  }

  FAIL() << "Failure: No cache hits";
}
