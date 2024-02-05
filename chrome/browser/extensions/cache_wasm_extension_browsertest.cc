// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser_commands.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/switches.h"

namespace chrome {

class WasmExtensionCachingBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  WasmExtensionCachingBrowserTest() = default;
  ~WasmExtensionCachingBrowserTest() override = default;

  const base::FilePath& GetExtensionDir() {
    if (!tmp_dir_.IsValid()) {
      CHECK(tmp_dir_.CreateUniqueTempDir());
    }
    return tmp_dir_.GetPath();
  }

  // Fetch the |histogram|'s |bucket| in every renderer process until reaching,
  // but not exceeding, |expected_count|.
  template <typename T>
  void CheckHistogramCount(base::StringPiece histogram,
                           T bucket,
                           int expected_count) {
    while (true) {
      content::FetchHistogramsFromChildProcesses();
      metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

      int count = histogram_tester_.GetBucketCount(histogram, bucket);
      CHECK_LE(count, expected_count);
      if (count == expected_count) {
        return;
      }

      base::PlatformThread::Sleep(base::Milliseconds(5));
    }
  }

 private:
  // Flag `allow-natives-syntax` is needed to be able to call runtime functions
  // (runtime functions have the % prefix).
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                    "--allow-natives-syntax");
    command_line->AppendSwitchASCII("wasm-caching-threshold", "1");
    ExtensionBrowserTest::SetUpCommandLine(command_line);
  }

  base::ScopedTempDir tmp_dir_;
  base::HistogramTester histogram_tester_;
};

// The enum values need to match "WasmCodeCaching" in
// tools/metrics/histograms/metadata/v8/enums.xml.
enum WasmCodeCaching {
  kMiss = 0,
  kHit = 1,
  kInvalidCacheEntry = 2,
  kNoCacheHandler = 3,

  kMaxValue = kNoCacheHandler
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
      chrome.tabs.onCreated.addListener(() => {
        // Run all functions until there are no unoptimized functions left.
        WebAssembly.instantiateStreaming(fetch("large.wasm")).then(result => {
          let has_unoptimized = true;
          while (has_unoptimized) {
            has_unoptimized = false;
            for (export_name in result.instance.exports) {
              const func = result.instance.exports[export_name];
              func(1, 2, 4);
              has_unoptimized ||= %IsLiftoffFunction(func);
            }
          }
        });
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

  // After loading the extension, no WebAssembly have been executed.
  extensions::ChromeTestExtensionLoader loader(browser()->profile());
  loader.LoadExtension(GetExtensionDir());
  CheckHistogramCount("V8.WasmCodeCaching", WasmCodeCaching::kMiss, 0);
  CheckHistogramCount("V8.WasmCodeCaching", WasmCodeCaching::kHit, 0);

  // Compile and execute webassembly code for the first time. This should be a
  // cache miss.
  NewTab(browser());
  CheckHistogramCount("V8.WasmCodeCaching", WasmCodeCaching::kMiss, 1);
  CheckHistogramCount("V8.WasmCodeCaching", WasmCodeCaching::kHit, 0);

  // Repeat: This should be a cache hit this time.
  NewTab(browser());
  CheckHistogramCount("V8.WasmCodeCaching", WasmCodeCaching::kMiss, 1);
  CheckHistogramCount("V8.WasmCodeCaching", WasmCodeCaching::kHit, 1);
}

}  // namespace chrome
