// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/error_reporting/mock_chrome_js_error_report_processor.h"

#include "base/check.h"
#include "base/logging.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"
#include "components/crash/content/browser/error_reporting/mock_crash_endpoint.h"

MockChromeJsErrorReportProcessor::MockChromeJsErrorReportProcessor() = default;

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

std::string MockChromeJsErrorReportProcessor::GetCrashEndpoint() {
  return crash_endpoint_;
}

std::string MockChromeJsErrorReportProcessor::GetCrashEndpointStaging() {
  return crash_endpoint_staging_;
}

void MockChromeJsErrorReportProcessor::GetOsVersion(
    int32_t& os_major_version,
    int32_t& os_minor_version,
    int32_t& os_bugfix_version) {
  os_major_version = 7;
  os_minor_version = 20;
  os_bugfix_version = 1;
}

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
void MockChromeJsErrorReportProcessor::UpdateReportDatabase(
    std::string remote_report_id,
    base::Time report_time) {
  if (update_report_database_) {
    ChromeJsErrorReportProcessor::UpdateReportDatabase(
        std::move(remote_report_id), report_time);
  }
}
#endif  //  !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)

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
