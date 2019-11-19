// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_ssl_blocking_page.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/chrome_metrics_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate_factory.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ssl/ssl_error_controller_client.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"

#if defined(OS_WIN)
#include "base/enterprise_util.h"
#elif defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#endif

namespace {

std::unique_ptr<ChromeMetricsHelper> CreateSSLProblemMetricsHelper(
    content::WebContents* web_contents,
    int cert_error,
    const GURL& request_url,
    bool overridable) {
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix =
      overridable ? "ssl_overridable" : "ssl_nonoverridable";
  return std::make_unique<ChromeMetricsHelper>(web_contents, request_url,
                                               reporting_info);
}

}  // namespace

// static
SSLBlockingPage* ChromeSSLBlockingPage::Create(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    const base::Time& time_triggered,
    const GURL& support_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter) {
  bool overridable = SSLBlockingPage::IsOverridable(options_mask);
  std::unique_ptr<ChromeMetricsHelper> metrics_helper(
      CreateSSLProblemMetricsHelper(web_contents, cert_error, request_url,
                                    overridable));
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

  auto controller_client = std::make_unique<SSLErrorControllerClient>(
      web_contents, ssl_info, cert_error, request_url,
      std::move(metrics_helper));

  std::unique_ptr<SSLBlockingPage> page;

  if (cert_error == net::ERR_CERT_SYMANTEC_LEGACY) {
    GURL symantec_support_url(chrome::kSymantecSupportUrl);
    page = std::make_unique<SSLBlockingPage>(
        web_contents, cert_error, ssl_info, request_url, options_mask,
        time_triggered, std::move(symantec_support_url),
        std::move(ssl_cert_reporter), overridable,
        std::move(controller_client));
  } else {
    page = std::make_unique<SSLBlockingPage>(
        web_contents, cert_error, ssl_info, request_url, options_mask,
        time_triggered, support_url, std::move(ssl_cert_reporter), overridable,
        std::move(controller_client));
  }

  DoChromeSpecificSetup(page.get());
  return page.release();
}

// static
void ChromeSSLBlockingPage::DoChromeSpecificSetup(SSLBlockingPageBase* page) {
  page->set_renderer_pref_callback(
      base::BindRepeating([](content::WebContents* web_contents,
                             blink::mojom::RendererPreferences* prefs) {
        Profile* profile =
            Profile::FromBrowserContext(web_contents->GetBrowserContext());
        renderer_preferences_util::UpdateFromSystemSettings(prefs, profile);
      }));

  page->cert_report_helper()->set_client_details_callback(
      base::BindRepeating([](CertificateErrorReport* report) {
        report->AddChromeChannel(chrome::GetChannel());

#if defined(OS_WIN)
        report->SetIsEnterpriseManaged(base::IsMachineExternallyManaged());
#elif defined(OS_CHROMEOS)
        report->SetIsEnterpriseManaged(g_browser_process->platform_part()
                                           ->browser_policy_connector_chromeos()
                                           ->IsEnterpriseManaged());
#endif

        // TODO(estade): this one is probably necessary for all clients, and
        // should be enforced (e.g. via a pure virtual method) rather than
        // optional.
        report->AddNetworkTimeInfo(g_browser_process->network_time_tracker());
      }));
}
