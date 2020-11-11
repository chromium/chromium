// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ERROR_REPORTING_MOCK_CHROME_JS_ERROR_REPORT_PROCESSOR_H_
#define CHROME_BROWSER_ERROR_REPORTING_MOCK_CHROME_JS_ERROR_REPORT_PROCESSOR_H_

#include <stdint.h>

#include <string>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/error_reporting/chrome_js_error_report_processor.h"

class MockCrashEndpoint;

class MockChromeJsErrorReportProcessor : public ChromeJsErrorReportProcessor {
 public:
  MockChromeJsErrorReportProcessor();

  // Controls what is returned from GetCrashEndpoint() override.
  void SetCrashEndpoint(std::string crash_endpoint);
  // Controls what is returned from GetCrashEndpointStaging() override.
  void SetCrashEndpointStaging(std::string crash_endpoint);

  // Allow tests to manipulate the result of JsErrorReportProcessor::Get().
  // Calling this will cause JsErrorReportProcessor::Get() to return this
  // object....
  void SetAsDefault();
  // ...and calling SetDefaultTo() will cause JsErrorReportProcessor::Get() to
  // return the given (other) JsErrorReportProcessor.
  static void SetDefaultTo(scoped_refptr<JsErrorReportProcessor> new_default);

 protected:
  std::string GetCrashEndpoint() override;
  std::string GetCrashEndpointStaging() override;

  // Always returns 7.20.1 (arbitrary).
  void GetOsVersion(int32_t& os_major_version,
                    int32_t& os_minor_version,
                    int32_t& os_bugfix_version) override;

 private:
  ~MockChromeJsErrorReportProcessor() override;
  std::string crash_endpoint_;
  std::string crash_endpoint_staging_;
};

// Wrapper for MockChromeJsErrorReportProcessor. Will automatically create, set
// up, and register a MockChromeJsErrorReportProcessor in the constructor and
// then unregister it in the destructor.
class ScopedMockChromeJsErrorReportProcessor {
 public:
  // Creates a MockChromeJsErrorReportProcessor, sets its crash endpoint to
  // the provided MockCrashEndpoint, and then registers the
  // MockChromeJsErrorReportProcessor as the processor returned from
  // JsErrorReportProcessor::Get().
  explicit ScopedMockChromeJsErrorReportProcessor(
      const MockCrashEndpoint& endpoint);

  // Removes the MockChromeJsErrorReportProcessor created in the constructor
  // from JsErrorReportProcessor::Get(). After this,
  // JsErrorReportProcessor::Get() will return nullptr.
  ~ScopedMockChromeJsErrorReportProcessor();

 private:
  scoped_refptr<MockChromeJsErrorReportProcessor> processor_;
  scoped_refptr<JsErrorReportProcessor> previous_;
};

#endif  // CHROME_BROWSER_ERROR_REPORTING_MOCK_CHROME_JS_ERROR_REPORT_PROCESSOR_H_
