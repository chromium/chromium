// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/error_reporting/mock_chrome_js_error_report_processor.h"

#include "base/check.h"
#include "components/crash/content/browser/error_reporting/mock_crash_endpoint.h"

MockChromeJsErrorReportProcessor::MockChromeJsErrorReportProcessor() = default;
MockChromeJsErrorReportProcessor::~MockChromeJsErrorReportProcessor() = default;

void MockChromeJsErrorReportProcessor::SetAsDefault() {
  JsErrorReportProcessor::SetDefault(this);
}

// static
void MockChromeJsErrorReportProcessor::SetDefaultTo(
    scoped_refptr<JsErrorReportProcessor> new_default) {
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

ScopedMockChromeJsErrorReportProcessor::ScopedMockChromeJsErrorReportProcessor(
    const MockCrashEndpoint& endpoint)
    : processor_(base::MakeRefCounted<MockChromeJsErrorReportProcessor>()),
      previous_(JsErrorReportProcessor::Get()) {
  processor_->SetCrashEndpoint(endpoint.GetCrashEndpointURL());
  processor_->SetAsDefault();
}

ScopedMockChromeJsErrorReportProcessor::
    ~ScopedMockChromeJsErrorReportProcessor() {
  DCHECK(processor_ == JsErrorReportProcessor::Get())
      << "processor_ is no longer the default processor.";

  MockChromeJsErrorReportProcessor::SetDefaultTo(previous_);
}
