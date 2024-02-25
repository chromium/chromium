// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ERROR_REPORTING_CHROME_JS_ERROR_REPORT_PROCESSOR_H_
#define CHROME_BROWSER_ERROR_REPORTING_CHROME_JS_ERROR_REPORT_PROCESSOR_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/crash/content/browser/error_reporting/js_error_report_processor.h"

namespace base {
class Process;
}
namespace content {
class BrowserContext;
}
namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network
class GURL;
struct JavaScriptErrorReport;
namespace variations {
struct ExperimentListInfo;
}

// Chrome's implementation of the JavaScript error reporter.
class ChromeJsErrorReportProcessor : public JsErrorReportProcessor {
 public:
  // Creates a ChromeJsErrorReportProcessor and sets it as the processor that
  // will be returned from JsErrorReportProcessor::Get(). This will only create
  // the processor if appropriate.
  static void Create();

  // JsErrorReportProcessor:
  void SendErrorReport(JavaScriptErrorReport error_report,
                       base::OnceClosure completion_callback,
                       content::BrowserContext* browser_context) override;

  void set_clock_for_testing(base::Clock* clock) { clock_ = clock; }

  // Access to the recent_error_reports map allows tests to confirm we are not
  // growing this map without bound.
  const base::flat_map<std::string, base::Time>&
  get_recent_error_reports_for_testing() const {
    return recent_error_reports_;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Force the error report processor to use the less-commonly-used temp file
  // solution for communicating with crash_reporter. This is normally only used
  // on old kernels without memfd_create, so we don't get good unit test
  // coverage unless we force it.
  void set_force_non_memfd_for_test() { force_non_memfd_for_test_ = true; }

  // Set the length of time we want for the crash_reporter (or the
  // mock_crash_reporter) to finish.
  void set_maximium_wait_for_crash_reporter_for_test(base::TimeDelta max_wait) {
    maximium_wait_for_crash_reporter_ = max_wait;
  }
#endif

 protected:
  // Non-tests should call ChromeJsErrorReportProcessor::Create() instead.
  ChromeJsErrorReportProcessor();
  ~ChromeJsErrorReportProcessor() override;

  // Wrapper around variations::GetExperimentListInfo(). Separate virtual
  // wrapper to allow dependency injection.
  virtual variations::ExperimentListInfo GetExperimentListInfo() const;

#if BUILDFLAG(IS_CHROMEOS)
  // Returns the first element(s) of the crash_reporter argv. By default, this
  // is just the command name (so {"/sbin/crash_reporter"}). Virtual so that
  // tests can override and can provide additional arguments to the test binary
  // if needed.
  virtual std::vector<std::string> GetCrashReporterArgvStart();
#else
  // Determines the version of the OS we are on. Virtual so that tests can
  // override. On Chrome OS, this information is added by the crash_reporter.
  virtual std::string GetOsVersion();

  // Testing hook -- returns the URL we will send the error reports to. By
  // default, returns the real endpoint.
  virtual std::string GetCrashEndpoint();

  // Testing hook -- returns the URL we will send the error reports to if
  // JavaScriptErrorReport::send_to_production_servers is false. By
  // default, returns the real staging endpoint.
  virtual std::string GetCrashEndpointStaging();

  // Update the uploads.log file with a record of this error report. This
  // ensures that the error appears on chrome://crashes and is listed in the
  // feedback reports.
  virtual void UpdateReportDatabase(std::string remote_report_id,
                                    base::Time report_time);
#endif  // !BUILDFLAG(IS_CHROMEOS)

 private:
  struct PlatformInfo;
  using ParameterMap = std::map<std::string, std::string>;

  std::optional<JavaScriptErrorReport> CheckConsentAndRedact(
      JavaScriptErrorReport error_report);

  PlatformInfo GetPlatformInfo();

  void SendReport(const GURL& url,
                  const std::string& body,
                  base::ScopedClosureRunner callback_runner,
                  base::Time report_time,
                  network::SharedURLLoaderFactory* loader_factory);

  void OnConsentCheckCompleted(
      base::ScopedClosureRunner callback_runner,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      base::TimeDelta browser_process_uptime,
      base::Time report_time,
      std::optional<JavaScriptErrorReport> error_report);

  // To avoid spamming the error collection system, do not send duplicate
  // error reports more than once per hour. Otherwise, if we get an error
  // each time the user types another character in a search box (for
  // instance), we would get flooded.
  // This function both determines if we should send the error report and also
  // updates the map to indicate that we did send it. It assumes we will send
  // the report if it sets |should_send| to true.
  void CheckAndUpdateRecentErrorReports(
      const JavaScriptErrorReport& error_report,
      bool* should_send);

  void SendReport(
      ParameterMap params,
      std::optional<std::string> stack_trace,
      bool send_to_production_servers,
      base::ScopedClosureRunner callback_runner,
      base::Time report_time,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory);

  // Add parameters indicating the current field trial experiments.
  void AddExperimentIds(ParameterMap& params);

#if BUILDFLAG(IS_CHROMEOS)
  // Write the parameters (and the stack_trace, if present) into a string
  // suitable for passing the crash_reporter. Returns the string.
  //
  // Format is the same key:length:value format used by Crashpad and Breakpad
  // when talking to crash_reporter. Example:
  // value1:5:abcdevalue2:10:hellothere
  static std::string ParamsToCrashReporterString(
      const ParameterMap& params,
      const std::optional<std::string>& stack_trace);

  void SendReportViaCrashReporter(ParameterMap params,
                                  std::optional<std::string> stack_trace,
                                  base::ScopedClosureRunner callback_runner);
  void WaitForCrashReporter(base::Process process,
                            base::Time process_creation_time,
                            base::ScopedClosureRunner file_cleanup,
                            base::ScopedClosureRunner external_callback_runner);

  bool force_non_memfd_for_test_ = false;

  // If crash_reporter isn't finished after this long, kill it and clean up
  // anyways.
  base::TimeDelta maximium_wait_for_crash_reporter_;
#else
  // Turn the parameter key/value pairs into a list of parameters suitable for
  // being the query part of a URL. Does URL escaping and such.
  static std::string BuildPostRequestQueryString(const ParameterMap& params);

  void OnRequestComplete(std::unique_ptr<network::SimpleURLLoader> url_loader,
                         base::ScopedClosureRunner callback_runner,
                         base::Time report_time,
                         std::unique_ptr<std::string> response_body);

#endif

  // For JavaScript error reports, a mapping of message+product+line+column to
  // the last time we sent an error message for that
  // message+product+line+column.
  base::flat_map<std::string, base::Time> recent_error_reports_;

  // To avoid recent_error_reports_ growing without bound, we clean it out every
  // once in while. This is the last time we cleaned it out.
  base::Time last_recent_error_reports_cleaning_;

  // Clock for dependency injection. Not owned.
  raw_ptr<base::Clock> clock_;
};

#endif  // CHROME_BROWSER_ERROR_REPORTING_CHROME_JS_ERROR_REPORT_PROCESSOR_H_
