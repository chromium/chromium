// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/sct_reporting_service_factory.h"

#include "base/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ssl/sct_reporting_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
SCTReportingServiceFactory* SCTReportingServiceFactory::GetInstance() {
  return base::Singleton<SCTReportingServiceFactory>::get();
}

// static
SCTReportingService* SCTReportingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<SCTReportingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

SCTReportingServiceFactory::SCTReportingServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "sct_reporting::Factory",
          BrowserContextDependencyManager::GetInstance()) {}

SCTReportingServiceFactory::~SCTReportingServiceFactory() = default;

KeyedService* SCTReportingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  safe_browsing::SafeBrowsingService* safe_browsing_service =
      g_browser_process->safe_browsing_service();
  // In unit tests the safe browsing service can be null, if this happens,
  // return null instead of crashing.
  if (!safe_browsing_service)
    return nullptr;

  return new SCTReportingService(safe_browsing_service,
                                 static_cast<Profile*>(profile));
}

content::BrowserContext* SCTReportingServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

// Force this to be created during BrowserContext creation, since we can't
// lazily create it.
bool SCTReportingServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
