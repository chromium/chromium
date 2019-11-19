// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/bad_clock_blocking_page.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/interstitials/chrome_metrics_helper.h"
#include "chrome/browser/interstitials/enterprise_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/cert_report_helper.h"
#include "chrome/browser/ssl/chrome_ssl_blocking_page.h"
#include "chrome/browser/ssl/ssl_error_controller_client.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"
#include "components/security_interstitials/core/bad_clock_ui.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"

using content::InterstitialPageDelegate;
using content::NavigationController;
using content::NavigationEntry;

namespace {

const char kBadClockMetricsName[] = "bad_clock";

std::unique_ptr<ChromeMetricsHelper> CreateBadClockMetricsHelper(
    content::WebContents* web_contents,
    const GURL& request_url) {
  // Set up the metrics helper for the BadClockUI.
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix = kBadClockMetricsName;
  std::unique_ptr<ChromeMetricsHelper> metrics_helper =
      std::make_unique<ChromeMetricsHelper>(web_contents, request_url,
                                            reporting_info);
  metrics_helper.get()->StartRecordingCaptivePortalMetrics(false);
  return metrics_helper;
}

}  // namespace

// static
const InterstitialPageDelegate::TypeID BadClockBlockingPage::kTypeForTesting =
    &BadClockBlockingPage::kTypeForTesting;

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
// Creating an interstitial without showing (e.g. from chrome://interstitials)
// it leaks memory, so don't create it here.
BadClockBlockingPage::BadClockBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    const base::Time& time_triggered,
    ssl_errors::ClockState clock_state,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter)
    : SSLBlockingPageBase(
          web_contents,
          CertificateErrorReport::INTERSTITIAL_CLOCK,
          ssl_info,
          request_url,
          std::move(ssl_cert_reporter),
          false /* overridable */,
          time_triggered,
          std::make_unique<SSLErrorControllerClient>(
              web_contents,
              ssl_info,
              cert_error,
              request_url,
              CreateBadClockMetricsHelper(web_contents, request_url))),
      ssl_info_(ssl_info),
      bad_clock_ui_(new security_interstitials::BadClockUI(request_url,
                                                           cert_error,
                                                           ssl_info,
                                                           time_triggered,
                                                           clock_state,
                                                           controller())) {
  ChromeSSLBlockingPage::DoChromeSpecificSetup(this);
}

BadClockBlockingPage::~BadClockBlockingPage() = default;

bool BadClockBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

InterstitialPageDelegate::TypeID BadClockBlockingPage::GetTypeForTesting() {
  return BadClockBlockingPage::kTypeForTesting;
}

void BadClockBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) {
  bad_clock_ui_->PopulateStringsForHTML(load_time_data);
  cert_report_helper()->PopulateExtendedReportingOption(load_time_data);
}

void BadClockBlockingPage::OverrideEntry(NavigationEntry* entry) {
  entry->GetSSL() = content::SSLStatus(ssl_info_);
}

// This handles the commands sent from the interstitial JavaScript.
void BadClockBlockingPage::CommandReceived(const std::string& command) {
  if (command == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }

  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval);

  // Let the CertReportHelper handle commands first, This allows it to get set
  // up to send reports, so that the report is populated properly if
  // BadClockErrorUI's command handling triggers a report to be sent.
  cert_report_helper()->HandleReportingCommands(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd),
      controller()->GetPrefService());
  bad_clock_ui_->HandleCommand(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd));
}
