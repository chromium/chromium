// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CERTIFICATE_REPORTING_TEST_UTILS_H_
#define CHROME_BROWSER_SSL_CERTIFICATE_REPORTING_TEST_UTILS_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ssl/certificate_error_reporter.h"
#include "components/security_interstitials/content/cert_logger.pb.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"

#if !BUILDFLAG(IS_ANDROID)
class Browser;
#endif

namespace base {
class RunLoop;
}

namespace certificate_reporting_test_utils {

enum OptIn { EXTENDED_REPORTING_OPT_IN, EXTENDED_REPORTING_DO_NOT_OPT_IN };

enum ExpectReport { CERT_REPORT_NOT_EXPECTED, CERT_REPORT_EXPECTED };

// This class is used to keep track of the latest hostname for sent reports. An
// SSLCertReporter is destroyed when the interstitial it's associated with is
// dismissed, so the tests can't check whether SSLCertReporter sent a report.
// For that reason, SSLCertReporter calls ReportSent() method on this class when
// it sends a report. SSLCertReporterCallback outlives both the SSLCertReporter
// and the interstitial so that the tests can query the latest hostname for
// which a report was sent.
class SSLCertReporterCallback {
 public:
  explicit SSLCertReporterCallback(base::RunLoop* run_loop);

  SSLCertReporterCallback(const SSLCertReporterCallback&) = delete;
  SSLCertReporterCallback& operator=(const SSLCertReporterCallback&) = delete;

  ~SSLCertReporterCallback();

  void ReportSent(const std::string& hostname,
                  const chrome_browser_ssl::CertLoggerRequest::ChromeChannel
                      chrome_channel);

  const std::string& GetLatestHostnameReported() const;
  chrome_browser_ssl::CertLoggerRequest::ChromeChannel
  GetLatestChromeChannelReported() const;

 private:
  raw_ptr<base::RunLoop> run_loop_;
  std::string latest_hostname_reported_;
  chrome_browser_ssl::CertLoggerRequest::ChromeChannel chrome_channel_;
};

#if !BUILDFLAG(IS_ANDROID)
// Sets the browser preference to enable or disable extended reporting.
void SetCertReportingOptIn(Browser* browser, OptIn opt_in);
#endif

// Creates a mock SSLCertReporter and return a pointer to it, which will
// be owned by the caller. The mock SSLCertReporter will call
// |report_sent_callback| when a report is sent. It also checks that a
// report is sent or not sent according to |expect_report|.
std::unique_ptr<SSLCertReporter> CreateMockSSLCertReporter(
    base::RepeatingCallback<
        void(const std::string&,
             const chrome_browser_ssl::CertLoggerRequest_ChromeChannel)>
        report_sent_callback,
    ExpectReport expect_report);

// Returns whether a report should be expected (due to the Finch config)
// if the user opts in.
ExpectReport GetReportExpectedFromFinch();

}  // namespace certificate_reporting_test_utils

#endif  // CHROME_BROWSER_SSL_CERTIFICATE_REPORTING_TEST_UTILS_H_
