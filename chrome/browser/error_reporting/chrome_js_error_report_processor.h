// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ERROR_REPORTING_CHROME_JS_ERROR_REPORT_PROCESSOR_H_
#define CHROME_BROWSER_ERROR_REPORTING_CHROME_JS_ERROR_REPORT_PROCESSOR_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/crash/content/browser/error_reporting/js_error_report_processor.h"

namespace content {
class BrowserContext;
}
namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network
class GURL;
struct JavaScriptErrorReport;

// Chrome's implementation of the JavaScript error reporter.
class ChromeJsErrorReportProcessor : public JsErrorReportProcessor {
 public:
  // Creates a ChromeJsErrorReportProcessor and sets it as the processor that
  // will be returned from JsErrorReportProcessor::Get(). This will only create
  // the processor if appropriate.
  static void Create();

  void SendErrorReport(JavaScriptErrorReport error_report,
                       base::OnceClosure completion_callback,
                       content::BrowserContext* browser_context) override;

 protected:
  // Non-tests should call ChromeJsErrorReportProcessor::Create() instead.
  ChromeJsErrorReportProcessor();
  ~ChromeJsErrorReportProcessor() override;

  // Testing hook -- returns the URL we will send the error reports to. By
  // default, returns the real endpoint.
  virtual std::string GetCrashEndpoint();

  // Determines the version of the OS we are on. Virtual so that tests can
  // override.
  virtual void GetOsVersion(int32_t& os_major_version,
                            int32_t& os_minor_version,
                            int32_t& os_bugfix_version);

 private:
  struct PlatformInfo;

  void OnRequestComplete(std::unique_ptr<network::SimpleURLLoader> url_loader,
                         base::ScopedClosureRunner callback_runner,
                         std::unique_ptr<std::string> response_body);

  base::Optional<JavaScriptErrorReport> CheckConsentAndRedact(
      JavaScriptErrorReport error_report);

  PlatformInfo GetPlatformInfo();

  void SendReport(const GURL& url,
                  const std::string& body,
                  base::ScopedClosureRunner callback_runner,
                  network::SharedURLLoaderFactory* loader_factory);

  void OnConsentCheckCompleted(
      base::ScopedClosureRunner callback_runner,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      base::TimeDelta browser_process_uptime,
      base::Optional<JavaScriptErrorReport> error_report);
};

#endif  // CHROME_BROWSER_ERROR_REPORTING_CHROME_JS_ERROR_REPORT_PROCESSOR_H_
