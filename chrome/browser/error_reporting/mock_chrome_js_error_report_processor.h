// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ERROR_REPORTING_MOCK_CHROME_JS_ERROR_REPORT_PROCESSOR_H_
#define CHROME_BROWSER_ERROR_REPORTING_MOCK_CHROME_JS_ERROR_REPORT_PROCESSOR_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_path_override.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/error_reporting/chrome_js_error_report_processor.h"

class MockCrashEndpoint;

class MockChromeJsErrorReportProcessor : public ChromeJsErrorReportProcessor {
 public:
  MockChromeJsErrorReportProcessor();

  // JsErrorReportProcessor:
  void SendErrorReport(JavaScriptErrorReport error_report,
                       base::OnceClosure completion_callback,
                       content::BrowserContext* browser_context) override;

  int send_count() const { return send_count_; }

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

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  // By default, a MockChromeJsErrorReportProcessor will suppress the updating
  // of the crash database (a.k.a. uploads.log) to avoid contaminating the real
  // database with test uploads. Set |update_report_database| to true to have
  // ChromeJsErrorReportProcessor::UpdateReportDatabase called like it normally
  // would be.
  void set_update_report_database(bool update_report_database) {
    update_report_database_ = update_report_database;
  }
#endif

 protected:
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  std::vector<std::string> GetCrashReporterArgvStart() override;
#else
  // Always returns "7.20.1" (arbitrary).
  std::string GetOsVersion() override;
  std::string GetCrashEndpoint() override;
  std::string GetCrashEndpointStaging() override;
  void UpdateReportDatabase(std::string remote_report_id,
                            base::Time report_time) override;
#endif

 private:
  ~MockChromeJsErrorReportProcessor() override;
  // Number of times SendErrorReport has been called.
  int send_count_ = 0;
  std::string crash_endpoint_;
  std::string crash_endpoint_staging_;
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  bool update_report_database_ = false;
#endif
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

  MockChromeJsErrorReportProcessor& processor() const { return *processor_; }

 private:
  scoped_refptr<MockChromeJsErrorReportProcessor> processor_;
  scoped_refptr<JsErrorReportProcessor> previous_;
};

#endif  // CHROME_BROWSER_ERROR_REPORTING_MOCK_CHROME_JS_ERROR_REPORT_PROCESSOR_H_
