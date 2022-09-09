// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/certificate_reporting_metrics_provider.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_factory.h"

CertificateReportingMetricsProvider::CertificateReportingMetricsProvider() {}

CertificateReportingMetricsProvider::~CertificateReportingMetricsProvider() {}

void CertificateReportingMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* unused) {
  // When ProvideCurrentSessionData is called, this class is being asked to
  // provide metrics to the metrics service. It doesn't provide any metrics
  // though, instead it tells CertificateReportingService to upload any pending
  // reports.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return;

  // Do not try to create profile here if it does not exist, because this method
  // can be called during browser shutdown.
  Profile* profile = profile_manager->GetLastUsedProfileIfLoaded();
  if (!profile)
    return;

  CertificateReportingService* service =
      CertificateReportingServiceFactory::GetForBrowserContext(profile);
  if (service)
    service->SendPending();
}
