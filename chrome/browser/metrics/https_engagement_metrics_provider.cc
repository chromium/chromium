// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/https_engagement_metrics_provider.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_service.h"
#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"

HttpsEngagementMetricsProvider::HttpsEngagementMetricsProvider() {}

HttpsEngagementMetricsProvider::~HttpsEngagementMetricsProvider() {}

void HttpsEngagementMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return;

  // Do not try to create profile here if it does not exist, because this method
  // can be called during browser shutdown.
  Profile* profile = profile_manager->GetLastUsedProfileIfLoaded();
  if (!profile)
    return;

  HttpsEngagementService* engagement_service =
      HttpsEngagementServiceFactory::GetForBrowserContext(profile);
  if (!engagement_service)
    return;
  engagement_service->StoreMetricsAndClear();
}
