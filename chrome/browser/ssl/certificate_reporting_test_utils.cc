// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/certificate_reporting_test_utils.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/cert_report_helper.h"
#include "chrome/browser/ssl/certificate_error_report.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/variations/variations_associated_data.h"
#include "net/url_request/report_sender.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#endif

namespace certificate_reporting_test_utils {

// This is a test implementation of the interface that blocking pages use to
// send certificate reports. It checks that the blocking page calls or does not
// call the report method when a report should or should not be sent,
// respectively.
class MockSSLCertReporter : public SSLCertReporter {
 public:
  MockSSLCertReporter(
      const base::Callback<
          void(const std::string&,
               const chrome_browser_ssl::CertLoggerRequest_ChromeChannel)>&
          report_sent_callback,
      ExpectReport expect_report)
      : report_sent_callback_(report_sent_callback),
        expect_report_(expect_report),
        reported_(false) {}

  ~MockSSLCertReporter() override {
    if (expect_report_ == CERT_REPORT_EXPECTED) {
      EXPECT_TRUE(reported_);
    } else {
      EXPECT_FALSE(reported_);
    }
  }

  // SSLCertReporter implementation.
  void ReportInvalidCertificateChain(
      const std::string& serialized_report) override {
    reported_ = true;
    CertificateErrorReport report;
    EXPECT_TRUE(report.InitializeFromString(serialized_report));
    report_sent_callback_.Run(report.hostname(), report.chrome_channel());
  }

 private:
  const base::Callback<void(
      const std::string&,
      const chrome_browser_ssl::CertLoggerRequest_ChromeChannel)>
      report_sent_callback_;
  const ExpectReport expect_report_;
  bool reported_;

  DISALLOW_COPY_AND_ASSIGN(MockSSLCertReporter);
};

SSLCertReporterCallback::SSLCertReporterCallback(base::RunLoop* run_loop)
    : run_loop_(run_loop),
      chrome_channel_(
          chrome_browser_ssl::CertLoggerRequest::CHROME_CHANNEL_NONE) {}

SSLCertReporterCallback::~SSLCertReporterCallback() {}

void SSLCertReporterCallback::ReportSent(
    const std::string& hostname,
    const chrome_browser_ssl::CertLoggerRequest::ChromeChannel chrome_channel) {
  latest_hostname_reported_ = hostname;
  chrome_channel_ = chrome_channel;
  run_loop_->Quit();
}

const std::string& SSLCertReporterCallback::GetLatestHostnameReported() const {
  return latest_hostname_reported_;
}

chrome_browser_ssl::CertLoggerRequest::ChromeChannel
SSLCertReporterCallback::GetLatestChromeChannelReported() const {
  return chrome_channel_;
}

#if !defined(OS_ANDROID)
void SetCertReportingOptIn(Browser* browser, OptIn opt_in) {
  safe_browsing::SetExtendedReportingPref(browser->profile()->GetPrefs(),
                                          opt_in == EXTENDED_REPORTING_OPT_IN);
}
#endif

std::unique_ptr<SSLCertReporter> CreateMockSSLCertReporter(
    const base::Callback<
        void(const std::string&,
             const chrome_browser_ssl::CertLoggerRequest_ChromeChannel)>&
        report_sent_callback,
    ExpectReport expect_report) {
  return std::unique_ptr<SSLCertReporter>(
      new MockSSLCertReporter(report_sent_callback, expect_report));
}

ExpectReport GetReportExpectedFromFinch() {
  const std::string group_name = base::FieldTrialList::FindFullName(
      CertReportHelper::kFinchExperimentName);

  if (group_name == CertReportHelper::kFinchGroupShowPossiblySend) {
    const std::string param = variations::GetVariationParamValue(
        CertReportHelper::kFinchExperimentName,
        CertReportHelper::kFinchParamName);
    double sendingThreshold;
    if (!base::StringToDouble(param, &sendingThreshold))
      return CERT_REPORT_NOT_EXPECTED;

    if (sendingThreshold == 1.0)
      return certificate_reporting_test_utils::CERT_REPORT_EXPECTED;
  }
  return certificate_reporting_test_utils::CERT_REPORT_NOT_EXPECTED;
}

}  // namespace certificate_reporting_test_utils
