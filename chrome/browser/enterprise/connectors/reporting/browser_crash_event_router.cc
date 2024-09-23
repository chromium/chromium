// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/browser_crash_event_router.h"

#include "base/memory/singleton.h"
#include "chrome/browser/enterprise/connectors/reporting/crash_reporting_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace enterprise_connectors {

BrowserCrashEventRouter::BrowserCrashEventRouter(
    content::BrowserContext* context) {
#if !BUILDFLAG(IS_CHROMEOS)
  CrashReportingContext* crash_reporting_context =
      CrashReportingContext::GetInstance();
  Profile* profile = Profile::FromBrowserContext(context);
  crash_reporting_context->AddProfile(this, profile);

#endif
}

BrowserCrashEventRouter::~BrowserCrashEventRouter() {
#if !BUILDFLAG(IS_CHROMEOS)
  CrashReportingContext* crash_reporting_context =
      CrashReportingContext::GetInstance();
  crash_reporting_context->RemoveProfile(this);
#endif
}

// static
BrowserCrashEventRouterFactory* BrowserCrashEventRouterFactory::GetInstance() {
  return base::Singleton<BrowserCrashEventRouterFactory>::get();
}

// static
BrowserCrashEventRouter* BrowserCrashEventRouterFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<BrowserCrashEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

bool BrowserCrashEventRouterFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool BrowserCrashEventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

BrowserCrashEventRouterFactory::BrowserCrashEventRouterFactory()
    : ProfileKeyedServiceFactory(
          "BrowserCrashEventRouter",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

BrowserCrashEventRouterFactory::~BrowserCrashEventRouterFactory() = default;

KeyedService* BrowserCrashEventRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BrowserCrashEventRouter(context);
}

}  // namespace enterprise_connectors
