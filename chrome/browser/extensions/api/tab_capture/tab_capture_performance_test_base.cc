// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_capture/tab_capture_performance_test_base.h"

#include <stdint.h>

#include <cmath>

#include "base/base64.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/service_manager/sandbox/features.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/gl/gl_switches.h"

namespace {
constexpr base::StringPiece kFullPerformanceRunSwitch = "full-performance-run";
}  // namespace

TabCapturePerformanceTestBase::TabCapturePerformanceTestBase() = default;

TabCapturePerformanceTestBase::~TabCapturePerformanceTestBase() = default;

void TabCapturePerformanceTestBase::SetUp() {
  // Because screen capture is involved, require pixel output.
  EnablePixelOutput();

  feature_list_.InitWithFeatures(
      {
          service_manager::features::kAudioServiceSandbox,
          features::kAudioServiceLaunchOnStartup,
          features::kAudioServiceOutOfProcess,
      },
      {});

  InProcessBrowserTest::SetUp();
}

void TabCapturePerformanceTestBase::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();

  best_effort_fence_.emplace();

  host_resolver()->AddRule("*", "127.0.0.1");
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &TabCapturePerformanceTestBase::HandleRequest, base::Unretained(this)));
  const bool did_start = embedded_test_server()->Start();
  CHECK(did_start);
}

void TabCapturePerformanceTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  is_full_performance_run_ = command_line->HasSwitch(kFullPerformanceRunSwitch);

  // Note: The naming "kUseGpuInTests" is very misleading. It actually means
  // "don't use a software OpenGL implementation." Subclasses will either call
  // UseSoftwareCompositing() to use Chrome's software compositor, or else they
  // won't (which means use the default hardware-accelerated compositor).
  command_line->AppendSwitch(switches::kUseGpuInTests);

  command_line->AppendSwitchASCII(extensions::switches::kWhitelistedExtensionID,
                                  kExtensionId);

  InProcessBrowserTest::SetUpCommandLine(command_line);
}

void TabCapturePerformanceTestBase::LoadExtension(
    const base::FilePath& unpacked_dir) {
  CHECK(!extension_);

  LOG(INFO) << "Loading extension...";
  auto* const extension_registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  extensions::TestExtensionRegistryObserver registry_observer(
      extension_registry);
  auto* const extension_service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();
  extensions::UnpackedInstaller::Create(extension_service)->Load(unpacked_dir);
  extension_ = registry_observer.WaitForExtensionReady();
  CHECK(extension_);
  CHECK_EQ(kExtensionId, extension_->id());
}

void TabCapturePerformanceTestBase::NavigateToTestPage(
    const std::string& test_page_html_content) {
  LOG(INFO) << "Navigating to test page...";
  test_page_to_serve_ = test_page_html_content;
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(kTestWebPageHostname, kTestWebPagePath));
}

base::Value TabCapturePerformanceTestBase::SendMessageToExtension(
    const std::string& json) {
  CHECK(extension_);

  const std::string javascript = base::StringPrintf(
      "new Promise((resolve, reject) => {\n"
      "  chrome.runtime.sendMessage(\n"
      "      '%s',\n"
      "      %s,\n"
      "      response => {\n"
      "        if (!response) {\n"
      "          reject(chrome.runtime.lastError.message);\n"
      "        } else {\n"
      "          resolve(response);\n"
      "        }\n"
      "      });\n"
      "})",
      extension_->id().c_str(), json.c_str());
  LOG(INFO) << "Sending message to extension: " << json;
  auto* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  for (;;) {
    const auto result = content::EvalJs(web_contents, javascript);
    if (result.error.empty()) {
      return result.value.Clone();
    }
    LOG(INFO) << "Race condition: Waiting for extension to come up, before "
                 "'sendMessage' retry...";
    ContinueBrowserFor(kSendMessageRetryPeriod);
  }
  NOTREACHED();
  return base::Value();
}

TabCapturePerformanceTestBase::TraceAnalyzerUniquePtr
TabCapturePerformanceTestBase::TraceAndObserve(
    const std::string& category_patterns,
    const std::vector<base::StringPiece>& event_names,
    int required_event_count) {
  const base::TimeDelta observation_period = is_full_performance_run_
                                                 ? kFullRunObservationPeriod
                                                 : kQuickRunObservationPeriod;

  LOG(INFO) << "Starting tracing...";
  {
    // Wait until all child processes have ACK'ed that they are now tracing.
    base::trace_event::TraceConfig trace_config(
        category_patterns, base::trace_event::RECORD_CONTINUOUSLY);
    base::RunLoop run_loop;
    const bool did_begin_tracing = tracing::BeginTracingWithTraceConfig(
        trace_config, run_loop.QuitClosure());
    CHECK(did_begin_tracing);
    run_loop.Run();
  }

  LOG(INFO) << "Running browser for " << observation_period.InSecondsF()
            << " sec...";
  ContinueBrowserFor(observation_period);

  LOG(INFO) << "Observation period has completed. Ending tracing...";
  std::string json_events;
  const bool success = tracing::EndTracing(&json_events);
  CHECK(success);

  std::unique_ptr<trace_analyzer::TraceAnalyzer> result(
      trace_analyzer::TraceAnalyzer::Create(json_events));
  result->AssociateAsyncBeginEndEvents();
  bool have_enough_events = true;
  for (const auto& event_name : event_names) {
    trace_analyzer::TraceEventVector events;
    QueryTraceEvents(result.get(), event_name, &events);
    LOG(INFO) << "Collected " << events.size() << " events ("
              << required_event_count << " required) for: " << event_name;
    if (static_cast<int>(events.size()) < required_event_count) {
      have_enough_events = false;
    }
  }
  LOG_IF(WARNING, !have_enough_events) << "Insufficient data collected.";

  VLOG_IF(2, result) << "Dump of trace events (trace_events.json.gz.b64):\n"
                     << MakeBase64EncodedGZippedString(json_events);
  return result;
}

// static
base::FilePath TabCapturePerformanceTestBase::GetApiTestDataDir() {
  base::FilePath dir;
  const bool success = base::PathService::Get(chrome::DIR_TEST_DATA, &dir);
  CHECK(success);
  return dir.AppendASCII("extensions").AppendASCII("api_test");
}

// static
std::string TabCapturePerformanceTestBase::MakeBase64EncodedGZippedString(
    const std::string& input) {
  std::string gzipped_input;
  compression::GzipCompress(input, &gzipped_input);
  std::string result;
  base::Base64Encode(gzipped_input, &result);

  // Break up the string with newlines to make it easier to handle in the
  // console logs.
  constexpr size_t kMaxLineWidth = 80;
  std::string formatted_result;
  formatted_result.reserve(result.size() + 1 + (result.size() / kMaxLineWidth));
  for (std::string::size_type src_pos = 0; src_pos < result.size();
       src_pos += kMaxLineWidth) {
    formatted_result.append(result, src_pos, kMaxLineWidth);
    formatted_result.append(1, '\n');
  }
  return formatted_result;
}

// static
void TabCapturePerformanceTestBase::ContinueBrowserFor(
    base::TimeDelta duration) {
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), duration);
  run_loop.Run();
}

// static
void TabCapturePerformanceTestBase::QueryTraceEvents(
    trace_analyzer::TraceAnalyzer* analyzer,
    base::StringPiece event_name,
    trace_analyzer::TraceEventVector* events) {
  const trace_analyzer::Query kQuery =
      trace_analyzer::Query::EventNameIs(event_name.as_string()) &&
      (trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_BEGIN) ||
       trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_ASYNC_BEGIN) ||
       trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_FLOW_BEGIN) ||
       trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_INSTANT) ||
       trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_COMPLETE));
  analyzer->FindEvents(kQuery, events);
}

std::unique_ptr<net::test_server::HttpResponse>
TabCapturePerformanceTestBase::HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  const GURL& url = request.GetURL();
  if (url.path() == kTestWebPagePath) {
    response->set_content(test_page_to_serve_);
  } else {
    response->set_code(net::HTTP_NOT_FOUND);
  }
  VLOG(1) << __func__ << ": request url=" << url.spec()
          << ", response=" << response->code();
  return response;
}

// static
constexpr base::TimeDelta
    TabCapturePerformanceTestBase::kFullRunObservationPeriod;

// static
constexpr base::TimeDelta
    TabCapturePerformanceTestBase::kQuickRunObservationPeriod;

// static
constexpr base::TimeDelta
    TabCapturePerformanceTestBase::kSendMessageRetryPeriod;

// static
const char TabCapturePerformanceTestBase::kTestWebPageHostname[] =
    "in-process-perf-test.chromium.org";

// static
const char TabCapturePerformanceTestBase::kTestWebPagePath[] =
    "/test_page.html";

// static
const char TabCapturePerformanceTestBase::kExtensionId[] =
    "ddchlicdkolnonkihahngkmmmjnjlkkf";
