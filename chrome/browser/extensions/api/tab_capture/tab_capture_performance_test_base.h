// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_PERFORMANCE_TEST_BASE_H_
#define CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_PERFORMANCE_TEST_BASE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/trace_event_analyzer.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace extensions {
class Extension;
}

namespace net {
namespace test_server {
struct HttpRequest;
class HttpResponse;
}  // namespace test_server
}  // namespace net

// Base class shared by TabCapturePerformanceTest and
// CastV2StreamingPerformanceTest which includes common set-up and utilities.
// This provides the facility for loading an extension that starts capture,
// loading a test page containing content to be captured, and sending messages
// to engage the extension.
class TabCapturePerformanceTestBase : public InProcessBrowserTest {
 public:
  TabCapturePerformanceTestBase();

  TabCapturePerformanceTestBase(const TabCapturePerformanceTestBase&) = delete;
  TabCapturePerformanceTestBase& operator=(
      const TabCapturePerformanceTestBase&) = delete;

  ~TabCapturePerformanceTestBase() override;

  // SetUp overrides to enable pixel output, configure the embedded test server,
  // allowlist the extension loaded by the tests.
  void SetUp() override;
  void SetUpOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // If true, run a full performance test. If false, all tests should just run a
  // quick test, something appropriate for running in a CQ try run or the
  // waterfall.
  bool is_full_performance_run() const { return is_full_performance_run_; }

  // Returns the currently-loaded extension.
  const extensions::Extension* extension() const { return extension_; }

  // Loads an unpacked extension found at the given path. This may only be
  // called once. This blocks until the extension is ready (but its background
  // page might not have run yet!).
  void LoadExtension(const base::FilePath& unpacked_dir);

  // Navigate the current (only) browser tab to the test page. This will cause a
  // resource request on the embedded test server (see HandleRequest()). This
  // blocks until the load is complete.
  void NavigateToTestPage(const std::string& test_page_html_content);

  // Execute JavaScript in the test page, to send a message to the extension to
  // do something (e.g., start tab capture of the test page), and returns the
  // response value.
  //
  // There is a possible race condition addressed here: The Extensions component
  // uses a specialized "serial delayed load queue" that makes it non-trivial to
  // discover whether a background page has run yet. If the background page has
  // not run yet, there would be nothing listening for the message. To mitigate
  // this problem, a simple retry loop is used.
  base::Value SendMessageToExtension(const std::string& json);

  // Runs the browser for a while, with tracing enabled to collect events
  // matching the given |category_patterns|.
  using TraceAnalyzerUniquePtr = std::unique_ptr<trace_analyzer::TraceAnalyzer>;
  TraceAnalyzerUniquePtr TraceAndObserve(
      const std::string& category_patterns,
      const std::vector<std::string_view>& event_names,
      int required_event_count);

  // Returns the path ".../test/data/extensions/api_test/".
  static base::FilePath GetApiTestDataDir();

  // GzipCompresses the given |input| string, then Base64-encodes and formats to
  // 80-char lines.
  static std::string MakeBase64EncodedGZippedString(const std::string& input);

  // Uses base::RunLoop to run the browser for the given |duration|.
  static void ContinueBrowserFor(base::TimeDelta duration);

  // Queries the |analyzer| for events having the given |event_name| whose phase
  // is classified as BEGIN, INSTANT, or COMPLETE (i.e., omit END events).
  static void QueryTraceEvents(trace_analyzer::TraceAnalyzer* analyzer,
                               std::string_view event_name,
                               trace_analyzer::TraceEventVector* events);

 protected:
  // These are how long the browser is run with trace event recording taking
  // place.
  static constexpr base::TimeDelta kFullRunObservationPeriod =
      base::Seconds(15);
  static constexpr base::TimeDelta kQuickRunObservationPeriod =
      base::Seconds(4);

  // If sending a message to the extension fails, because the extension has not
  // started its message listener yet, how long before the next retry?
  static constexpr base::TimeDelta kSendMessageRetryPeriod =
      base::Milliseconds(250);

  // Note: The hostname must match the pattern found in the Extension's manifest
  // file, or it will not be able to send/receive messaging from the test web
  // page (due to extension permissions).
  static const char kTestWebPageHostname[];
  static const char kTestWebPagePath[];

  // The expected ID of the loaded extension.
  static const char kExtensionId[];

 private:
  // In the spirit of NoBestEffortTasksTests, use a fence to make sure that
  // BEST_EFFORT tasks in the browser process are not required for the success
  // of these tests. In a performance test run, this also removes sources of
  // variance. Do not use the --disable-best-effort-tasks command line switch as
  // that would also preempt BEST_EFFORT tasks in utility processes, and
  // TabCapturePerformanceTest.Performance relies on BEST_EFFORT tasks in
  // utility process for tracing.
  std::optional<base::ThreadPoolInstance::ScopedBestEffortExecutionFence>
      best_effort_fence_;

  bool is_full_performance_run_ = false;

  // Set to the test page that should be served by the next call to
  // HandleRequest().
  std::string test_page_to_serve_;

  // Handles requests from the tab open in the browser. Called by the embedded
  // test server (see SetUpOnMainThread()).
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  raw_ptr<const extensions::Extension, DanglingUntriaged> extension_ = nullptr;

  // Manages the Audio Service feature set, enabled for these performance tests.
  base::test::ScopedFeatureList feature_list_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_PERFORMANCE_TEST_BASE_H_
