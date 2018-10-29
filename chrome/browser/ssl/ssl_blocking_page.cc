// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_blocking_page.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/chrome_metrics_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ssl/cert_report_helper.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate_factory.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "chrome/browser/ssl/ssl_error_controller_client.h"
#include "chrome/common/chrome_switches.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "components/security_interstitials/core/superfish_error_ui.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "net/base/net_errors.h"

using base::TimeTicks;
using content::InterstitialPage;
using content::InterstitialPageDelegate;
using content::NavigationEntry;
using security_interstitials::SSLErrorUI;

namespace {

std::unique_ptr<ChromeMetricsHelper> CreateSslProblemMetricsHelper(
    content::WebContents* web_contents,
    int cert_error,
    const GURL& request_url,
    bool overridable,
    bool is_superfish) {
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  if (is_superfish) {
    reporting_info.metric_prefix = "superfish";
  } else {
    reporting_info.metric_prefix =
        overridable ? "ssl_overridable" : "ssl_nonoverridable";
  }
  return std::make_unique<ChromeMetricsHelper>(web_contents, request_url,
                                               reporting_info);
}

}  // namespace

// static
const InterstitialPageDelegate::TypeID SSLBlockingPage::kTypeForTesting =
    &SSLBlockingPage::kTypeForTesting;

// static
SSLBlockingPage* SSLBlockingPage::Create(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    const base::Time& time_triggered,
    const GURL& support_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    bool is_superfish,
    const base::Callback<void(content::CertificateRequestResultType)>&
        callback) {
  bool overridable = IsOverridable(options_mask);
  std::unique_ptr<ChromeMetricsHelper> metrics_helper(
      CreateSslProblemMetricsHelper(web_contents, cert_error, request_url,
                                    overridable, is_superfish));
  metrics_helper.get()->StartRecordingCaptivePortalMetrics(overridable);

  ChromeSSLHostStateDelegate* state =
      ChromeSSLHostStateDelegateFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  state->DidDisplayErrorPage(cert_error);
  bool is_recurrent_error = state->HasSeenRecurrentErrors(cert_error);
  if (overridable) {
    UMA_HISTOGRAM_BOOLEAN("interstitial.ssl_overridable.is_recurrent_error",
                          is_recurrent_error);
    if (cert_error == net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED) {
      UMA_HISTOGRAM_BOOLEAN(
          "interstitial.ssl_overridable.is_recurrent_error.ct_error",
          is_recurrent_error);
    }
  } else {
    UMA_HISTOGRAM_BOOLEAN("interstitial.ssl_nonoverridable.is_recurrent_error",
                          is_recurrent_error);
    if (cert_error == net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED) {
      UMA_HISTOGRAM_BOOLEAN(
          "interstitial.ssl_nonoverridable.is_recurrent_error.ct_error",
          is_recurrent_error);
    }
  }

  if (cert_error == net::ERR_CERT_SYMANTEC_LEGACY) {
    GURL symantec_support_url(kSymantecSupportUrl);
    return new SSLBlockingPage(
        web_contents, cert_error, ssl_info, request_url, options_mask,
        time_triggered, std::move(symantec_support_url),
        std::move(ssl_cert_reporter), overridable, std::move(metrics_helper),
        is_superfish, callback);
  }

  return new SSLBlockingPage(web_contents, cert_error, ssl_info, request_url,
                             options_mask, time_triggered, support_url,
                             std::move(ssl_cert_reporter), overridable,
                             std::move(metrics_helper), is_superfish, callback);
}

bool SSLBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

InterstitialPageDelegate::TypeID SSLBlockingPage::GetTypeForTesting() const {
  return SSLBlockingPage::kTypeForTesting;
}

SSLBlockingPage::~SSLBlockingPage() {
  if (!callback_.is_null()) {
    // The page is closed without the user having chosen what to do, default to
    // deny.
    NotifyDenyCertificate();
  }
}

void SSLBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) {
  ssl_error_ui_->PopulateStringsForHTML(load_time_data);
  cert_report_helper()->PopulateExtendedReportingOption(load_time_data);
}

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
SSLBlockingPage::SSLBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    const base::Time& time_triggered,
    const GURL& support_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    bool overridable,
    std::unique_ptr<ChromeMetricsHelper> metrics_helper,
    bool is_superfish,
    const base::Callback<void(content::CertificateRequestResultType)>& callback)
    : SSLBlockingPageBase(web_contents,
                          cert_error,
                          is_superfish
                              ? CertificateErrorReport::INTERSTITIAL_SUPERFISH
                              : CertificateErrorReport::INTERSTITIAL_SSL,
                          ssl_info,
                          request_url,
                          std::move(ssl_cert_reporter),
                          overridable,
                          time_triggered,
                          std::make_unique<SSLErrorControllerClient>(
                              web_contents,
                              ssl_info,
                              cert_error,
                              request_url,
                              std::move(metrics_helper))),
      callback_(callback),
      ssl_info_(ssl_info),
      overridable_(overridable),
      expired_but_previously_allowed_(
          (options_mask & SSLErrorUI::EXPIRED_BUT_PREVIOUSLY_ALLOWED) != 0),
      ssl_error_ui_(
          is_superfish
              ? std::make_unique<security_interstitials::SuperfishErrorUI>(
                    request_url,
                    cert_error,
                    ssl_info,
                    options_mask,
                    time_triggered,
                    controller())
              : std::make_unique<SSLErrorUI>(request_url,
                                             cert_error,
                                             ssl_info,
                                             options_mask,
                                             time_triggered,
                                             support_url,
                                             controller())) {
  // Creating an interstitial without showing (e.g. from chrome://interstitials)
  // it leaks memory, so don't create it here.
}

void SSLBlockingPage::OverrideEntry(NavigationEntry* entry) {
  entry->GetSSL() = content::SSLStatus(ssl_info_);
}

// This handles the commands sent from the interstitial JavaScript.
void SSLBlockingPage::CommandReceived(const std::string& command) {
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
  // SSLErrorUI's command handling triggers a report to be sent.
  cert_report_helper()->HandleReportingCommands(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd),
      controller()->GetPrefService());
  ssl_error_ui_->HandleCommand(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd));
}

void SSLBlockingPage::OverrideRendererPrefs(
      content::RendererPreferences* prefs) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  renderer_preferences_util::UpdateFromSystemSettings(prefs, profile);
}

void SSLBlockingPage::OnProceed() {
  OnInterstitialClosing();

  // Accepting the certificate resumes the loading of the page.
  DCHECK(!callback_.is_null());
  callback_.Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE);
  callback_.Reset();
}

void SSLBlockingPage::OnDontProceed() {
  OnInterstitialClosing();
  NotifyDenyCertificate();
}

void SSLBlockingPage::NotifyDenyCertificate() {
  // It's possible that callback_ may not exist if the user clicks "Proceed"
  // followed by pressing the back button before the interstitial is hidden.
  // In that case the certificate will still be treated as allowed.
  if (callback_.is_null())
    return;

  callback_.Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
  callback_.Reset();
}

// static
bool SSLBlockingPage::IsOverridable(int options_mask) {
  const bool is_overridable =
      (options_mask & SSLErrorUI::SOFT_OVERRIDE_ENABLED) &&
      !(options_mask & SSLErrorUI::STRICT_ENFORCEMENT) &&
      !(options_mask & SSLErrorUI::HARD_OVERRIDE_DISABLED);
  return is_overridable;
}
