// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_blocking_page_base.h"

#include "chrome/browser/ssl/cert_report_helper.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"

SSLBlockingPageBase::SSLBlockingPageBase(
    content::WebContents* web_contents,
    CertificateErrorReport::InterstitialReason interstitial_reason,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    bool overridable,
    const base::Time& time_triggered,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : security_interstitials::SecurityInterstitialPage(
          web_contents,
          request_url,
          std::move(controller_client)),
      cert_report_helper_(
          std::make_unique<CertReportHelper>(std::move(ssl_cert_reporter),
                                             web_contents,
                                             request_url,
                                             ssl_info,
                                             interstitial_reason,
                                             overridable,
                                             time_triggered,
                                             controller()->metrics_helper())) {}

SSLBlockingPageBase::~SSLBlockingPageBase() = default;

void SSLBlockingPageBase::OnInterstitialClosing() {
  UpdateMetricsAfterSecurityInterstitial();
  cert_report_helper_->FinishCertCollection();
}

void SSLBlockingPageBase::OverrideRendererPrefs(
    blink::mojom::RendererPreferences* prefs) {
  if (renderer_pref_callback_)
    renderer_pref_callback_.Run(web_contents(), prefs);
}

void SSLBlockingPageBase::SetSSLCertReporterForTesting(
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter) {
  cert_report_helper_->SetSSLCertReporterForTesting(
      std::move(ssl_cert_reporter));
}
