// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/telomere_event_router.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/enterprise/connectors/reporting/telomere_reporting_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace enterprise_connectors {

BASE_FEATURE(kTelomereReporting,
             "TelomereReporting",
             base::FEATURE_DISABLED_BY_DEFAULT);

TelomereEventRouter::TelomereEventRouter(content::BrowserContext* context) {
  TelomereReportingContext* telomere_reporting_context =
      TelomereReportingContext::GetInstance();
  Profile* profile = Profile::FromBrowserContext(context);
  telomere_reporting_context->AddProfile(this, profile);
}

TelomereEventRouter::~TelomereEventRouter() {
  TelomereReportingContext* telomere_reporting_context =
      TelomereReportingContext::GetInstance();
  telomere_reporting_context->RemoveProfile(this);
}

// static
TelomereEventRouterFactory* TelomereEventRouterFactory::GetInstance() {
  VLOG(2) << "enterprise.telomere_reporting: " << __func__;
  return base::Singleton<TelomereEventRouterFactory>::get();
}

// static
TelomereEventRouter* TelomereEventRouterFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<TelomereEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

bool TelomereEventRouterFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool TelomereEventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

TelomereEventRouterFactory::TelomereEventRouterFactory()
    : ProfileKeyedServiceFactory(
          "TelomereEventRouter",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

TelomereEventRouterFactory::~TelomereEventRouterFactory() = default;

std::unique_ptr<KeyedService>
TelomereEventRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<TelomereEventRouter>(context);
}

}  // namespace enterprise_connectors
