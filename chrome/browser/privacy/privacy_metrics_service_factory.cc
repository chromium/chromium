// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy/privacy_metrics_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/privacy/privacy_metrics_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"

PrivacyMetricsServiceFactory* PrivacyMetricsServiceFactory::GetInstance() {
  return base::Singleton<PrivacyMetricsServiceFactory>::get();
}

PrivacyMetricsService* PrivacyMetricsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PrivacyMetricsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrivacyMetricsServiceFactory::PrivacyMetricsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PrivacyMetricsService",
          BrowserContextDependencyManager::GetInstance()) {
  // No service dependencies other than prefs, which are always created.
}

KeyedService* PrivacyMetricsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // No metrics recorded for OTR profiles.
  if (context->IsOffTheRecord())
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(context);
  return new PrivacyMetricsService(profile->GetPrefs());
}
