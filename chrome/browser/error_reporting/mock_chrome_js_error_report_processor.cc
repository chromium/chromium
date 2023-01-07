// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/error_reporting/mock_chrome_js_error_report_processor.h"

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"
#include "components/crash/content/browser/error_reporting/mock_crash_endpoint.h"
#include "components/variations/variations_crash_keys.h"

const char MockChromeJsErrorReportProcessor::
    kDefaultExperimentListStringPreEscaping[] =
        "6598898b-ac59b6dc,";  // variations::GetExperimentListInfo() always
                               // leaves a trailing comma.

const char MockChromeJsErrorReportProcessor::kDefaultExperimentListString[] =
    "6598898b-ac59b6dc%2C";  // The URL escaping turns the comma into %2C in the
                             // query string.

// Tricium gets confused by the #if's and thinks we should change this to
// "= default".
// NOLINTNEXTLINE
MockChromeJsErrorReportProcessor::MockChromeJsErrorReportProcessor() {
#if BUILDFLAG(IS_CHROMEOS)
  // Shorten timeout or tests will time out.
  set_maximium_wait_for_crash_reporter_for_test(base::Seconds(5));
#endif
}

MockChromeJsErrorReportProcessor::~MockChromeJsErrorReportProcessor() = default;

void MockChromeJsErrorReportProcessor::SendErrorReport(
    JavaScriptErrorReport error_report,
    base::OnceClosure completion_callback,
    content::BrowserContext* browser_context) {
  ++send_count_;
  ChromeJsErrorReportProcessor::SendErrorReport(
      std::move(error_report), std::move(completion_callback), browser_context);
}

void MockChromeJsErrorReportProcessor::SetAsDefault() {
  LOG(INFO) << "MockChromeJsErrorReportProcessor installed as error processor";
  JsErrorReportProcessor::SetDefault(this);
}

// static
void MockChromeJsErrorReportProcessor::SetDefaultTo(
    scoped_refptr<JsErrorReportProcessor> new_default) {
  LOG(INFO) << "MockChromeJsErrorReportProcessor uninstalled";
  JsErrorReportProcessor::SetDefault(new_default);
}

void MockChromeJsErrorReportProcessor::SetCrashEndpoint(
    std::string crash_endpoint) {
  crash_endpoint_ = crash_endpoint;
}

void MockChromeJsErrorReportProcessor::SetCrashEndpointStaging(
    std::string crash_endpoint) {
  crash_endpoint_staging_ = crash_endpoint;
}

variations::ExperimentListInfo
MockChromeJsErrorReportProcessor::GetExperimentListInfo() const {
  if (use_real_experiment_list_) {
    return ChromeJsErrorReportProcessor::GetExperimentListInfo();
  }
  variations::ExperimentListInfo result;
  result.num_experiments = 1;
  result.experiment_list = kDefaultExperimentListStringPreEscaping;
  return result;
}

#if BUILDFLAG(IS_CHROMEOS)
std::vector<std::string>
MockChromeJsErrorReportProcessor::GetCrashReporterArgvStart() {
  // Redirect uploads to our a simple upload shim which will then send them to
  // the MockCrashEndpoint. This simulates the Chrome OS crash_reporter and
  // crash_sender in a way that allows most tests to run without changes.
  base::FilePath mock_crash_reporter_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &mock_crash_reporter_path));
  mock_crash_reporter_path =
      mock_crash_reporter_path.Append("mock_chromeos_crash_reporter");
  return {mock_crash_reporter_path.value(),
          base::StrCat({"--upload_to=", crash_endpoint_})};
}
#else
std::string MockChromeJsErrorReportProcessor::GetOsVersion() {
  return "7.20.1";
}

std::string MockChromeJsErrorReportProcessor::GetCrashEndpoint() {
  return crash_endpoint_;
}

std::string MockChromeJsErrorReportProcessor::GetCrashEndpointStaging() {
  return crash_endpoint_staging_;
}

void MockChromeJsErrorReportProcessor::UpdateReportDatabase(
    std::string remote_report_id,
    base::Time report_time) {
  if (update_report_database_) {
    ChromeJsErrorReportProcessor::UpdateReportDatabase(
        std::move(remote_report_id), report_time);
  }
}
#endif  //  !BUILDFLAG(IS_CHROMEOS)

ScopedMockChromeJsErrorReportProcessor::ScopedMockChromeJsErrorReportProcessor(
    const MockCrashEndpoint& endpoint)
    : processor_(base::MakeRefCounted<MockChromeJsErrorReportProcessor>()),
      previous_(JsErrorReportProcessor::Get()) {
  processor_->SetCrashEndpoint(endpoint.GetCrashEndpointURL());
  processor_->SetCrashEndpointStaging(endpoint.GetCrashEndpointURL());
  processor_->SetAsDefault();
}

ScopedMockChromeJsErrorReportProcessor::
    ~ScopedMockChromeJsErrorReportProcessor() {
  DCHECK(processor_ == JsErrorReportProcessor::Get())
      << "processor_ is no longer the default processor.";

  MockChromeJsErrorReportProcessor::SetDefaultTo(previous_);
}
