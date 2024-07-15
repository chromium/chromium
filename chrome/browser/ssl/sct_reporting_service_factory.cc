// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/sct_reporting_service_factory.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ssl/sct_reporting_service.h"

// static
SCTReportingServiceFactory* SCTReportingServiceFactory::GetInstance() {
  static base::NoDestructor<SCTReportingServiceFactory> instance;
  return instance.get();
}

// static
SCTReportingService* SCTReportingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<SCTReportingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

SCTReportingServiceFactory::SCTReportingServiceFactory()
    : ProfileKeyedServiceFactory(
          "sct_reporting::Factory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

SCTReportingServiceFactory::~SCTReportingServiceFactory() = default;

std::unique_ptr<KeyedService>
SCTReportingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  safe_browsing::SafeBrowsingService* safe_browsing_service =
      g_browser_process->safe_browsing_service();
  // In unit tests the safe browsing service can be null, if this happens,
  // return null instead of crashing.
  if (!safe_browsing_service)
    return nullptr;

  return std::make_unique<SCTReportingService>(safe_browsing_service,
                                               static_cast<Profile*>(profile));
}

// Force this to be created during BrowserContext creation, since we can't
// lazily create it.
bool SCTReportingServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
